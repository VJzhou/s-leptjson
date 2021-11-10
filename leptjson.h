//
// Created by vj-zhou on 2021/10/12.
//

#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h> // size_t

typedef enum {LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT} lept_type;

typedef struct lept_value lept_value; //前置申明
typedef struct lept_member lept_member; //前置申明

struct lept_value{
    union { // 使用联合体可以节省内存
        struct { // string
            char* s; // 8 字节
            size_t  len; // 4字节
        }; // 占用12 个字节
        struct { // array
            lept_value *e; // 使用了自身类型指针, 需要前置申明
            size_t  size, capacity;
        };

        struct { // object
            lept_member *m;
            size_t  size, capacity;
        } o;

        double n; // 8 字节
    };
    lept_type   type; // 4
};


struct lept_member {
    char* k; // key
    size_t klen; // key length
    lept_value v; // val
};

enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE, // 1
    LEPT_PARSE_INVALID_VALUE, // 2
    LEPT_PARSE_ROOT_NOT_SINGULAR, //3
    LEPT_PARSE_NUMBER_TOO_BIG, // 4
    LEPT_PARSE_MISS_QUOTATION_MARK, // 5
    LEPT_PARSE_INVALID_STRING_ESCAPE, // 6
    LEPT_PARSE_INVALID_STRING_CHAR, // 7,
    LEPT_PARSE_INVALID_UNICODE_HEX, // 8
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,// 9
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, // 10
    LEPT_PARSE_MISS_KEY, // 11
    LEPT_PARSE_MISS_COLON, // 12
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET // 13

};

#define lept_init(v) do {(v)->type = LEPT_NULL; (v)->capacity = 0;} while(0)
#define LEPT_KEY_NOT_EXIST ((size_t) -1)

int lept_parse(lept_value* v, const char* json);

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value *v);

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const  lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

size_t  lept_get_array_size (const lept_value* v);
lept_value* lept_get_array_element(const lept_value *v, size_t index);


size_t lept_get_object_size(const lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(const lept_value* v, size_t index);


void lept_move (lept_value* dst, lept_value* src);
void lept_swap(lept_value* lhs, lept_value* rhs);


void lept_set_array (lept_value* v, size_t capacity);
size_t lept_get_array_capacity(const lept_value* v);
lept_value*  lept_pushback_array_element (lept_value* v);
void lept_popback_array_element (lept_value* v);
lept_value* lept_insert_array_element(lept_value* v, size_t index);
void lept_erase_array_element (lept_value* v, size_t index, size_t count);
void lept_shrink_array (lept_value* v);
void lept_clear_array(lept_value* v);

int lept_is_equal(const lept_value* lhs, const lept_value* rhs);
void lept_copy (lept_value* dst, const lept_value* src);


void lept_set_object(lept_value* v, size_t capacity);
size_t lept_get_object_capacity(const lept_value* v);
void lept_reserve_object(lept_value* v, size_t capacity);
void lept_shrink_object(lept_value* v);
void lept_clear_object(lept_value* v);
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);
void lept_remove_object_value(lept_value* v, size_t index);
size_t lept_find_object_index (lept_value* v, const char* key, size_t klen);
lept_value* lept_find_object_value (lept_value* v, const char* key, size_t klen);
#endif