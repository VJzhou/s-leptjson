//
// Created by vj-zhou on 2021/10/12.
//
#include "leptjson.h"
#include <assert.h> // assert
#include <stdlib.h> // NULL malloc realloc free strtod
#include <errno.h>  // errno, ERANGE
#include <math.h> // HUGE_VAL
#include <string.h> // memcpy
#include <stdio.h>


#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIG1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;


static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1; // 扩容1.5 倍
        c->stack = (char*)realloc(c->stack, c->size);
    }

    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}


static void lept_parse_whitespace (lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

static int lept_parse_literal (lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]); // c->json++

    for (i = 0; literal[i+1]; i++) {
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number (lept_context* c, lept_value* v) {
    const char* p = c->json;

    if (*p == '-') p++; // 如果是- 号跳过
    if (*p == '0') p++; // 如果是单值 0 跳过
    else {
        // 如果是以0开头的数字则非法
        if (!ISDIG1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++); // 遍历剩下的字符是不是数字
    }
    if (*p == '.') { // 遇到小数点跳过, 但是小数点后面必须跟数字
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE; // 如果不是数字非法
        for (p++; ISDIGIT(*p); p++);
    }

    if (*p == 'E' || *p == 'e') {
        p++;
        if (*p == '+' || *p == '-') p++;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}


static const char* lept_parse_hex4(const char* p, unsigned* u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; ++i) {
        char ch = *p++;
        *u <<= 4;
        if (ch >= '0' && ch <= '9') *u |= ch - '0';
        else if (ch >= 'a' && ch <= 'f') *u |= ch - ('a' - 10);
        else if (ch >= 'A' && ch <= 'F') *u |= ch - ('A' - 10);
        else return NULL;
    }
    return p;
}

static void lept_encode_utf8 (lept_context* c, unsigned u) {
    if (u <= 0x7F) {
        PUTC(c, u & 0xFF);
    } else if (u <= 0x7FF){
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF ));
        PUTC(c, 0x80 | ( u & 0x3F));
    } else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u  & 0x3F));
    } else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u  & 0x3F));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

static int lept_parse_string (lept_context* c, lept_value* v) {
    size_t head = c->top, len;
    unsigned u, u2;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch =  *p++;
        switch (ch) {
            case '\\':
                switch (*p++) {
                    case '\\':  PUTC(c, '\\');break;
                    case '\"':  PUTC(c, '\"');break;
                    case 'n':  PUTC(c, '\n');break;
                    case 't':  PUTC(c, '\t');break;
                    case 'r':  PUTC(c, '\r');break;
                    case 'b':  PUTC(c, '\b');break;
                    case 'f':  PUTC(c, '\f');break;
                    case '/':  PUTC(c, '/');break;
                    case 'u':
                        if (!(p = lept_parse_hex4(p, &u))) { // 解析4位16进制数字 \u2020
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX); // 解析失败返回解析错误
                        }
                        // 高代理  U+D800 至 U+DBFF     低代理 U+DC00 至 U+DFFF
                        // 检查是否存在低代理 || 低代理不在合法码点范围
                        // 计算码点
                        if (u >= 0xD800 && u <= 0xD8FF) {
                            if (*p++ != '\\') {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (*p++ != 'u') {
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u); // 写入缓冲区
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20) {
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch);
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v); // 向前申明
static int lept_parse_array (lept_context* c, lept_value* v) {
    size_t size = 0;
    int ret;
    EXPECT(c, '[');
    if (*c->json == ']') {
        v->type = LEPT_ARRAY;
        v->e = NULL;
        v->size = 0;
        c->json++;
        return LEPT_PARSE_OK;
    }
    for (;;) {
        lept_value e;
        lept_init(&e);
        if ( (ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK) {
            return ret;
        }

        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
        size++;
        if (*c->json == ',') {
            c->json++;
        } else if (*c->json == ']') {
            v->type = LEPT_ARRAY;
            c->json++;
            v->size = size;
            size *= sizeof(lept_value);
            memcpy(v->e = (lept_value*)malloc(size), lept_context_pop(c, size), size);
            return LEPT_PARSE_OK;
        } else {
            return LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
        }
    }

}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) { // *c->json => *(c->json)
        case '[': return lept_parse_array(c, v);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        case '"': return lept_parse_string(c, v);
        default:   return lept_parse_number(c,v);
    }
}

int lept_parse (lept_value* v, const char* json) {
    lept_context   c;
    int ret ;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') { // *c.json => *(c.json)
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return  ret;
}

void lept_free (lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->s);
    v->type = LEPT_NULL;
}


lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v){
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->n = n;
    v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value *v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->s;
}
size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->len;
}
void lept_set_string(lept_value* v, const char* s, size_t len){
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->s = (char*)malloc(len + 1); // + 1 是为了在结尾添加一个结束字符\0
    memcpy(v->s, s, len);
    v->s[len] = '\0';
    v->len = len;
    v->type = LEPT_STRING;
}


size_t  lept_get_array_size (const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->size;
}

lept_value* lept_get_array_element(const lept_value *v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return &v->e[index];
}