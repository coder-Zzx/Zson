#ifndef ZSON_H__
#define ZSON_H__

#include <stddef.h> /* size_t */

typedef enum { ZSON_NULL, ZSON_FALSE, ZSON_TRUE, ZSON_NUMBER, ZSON_STRING, ZSON_ARRAY, ZSON_OBJECT } zson_type;

#define ZSON_KEY_NOT_EXIST ((size_t)-1)

typedef struct zson_value zson_value;
typedef struct zson_member zson_member;

struct zson_value {
    union {
        struct { zson_member* m; size_t size, capacity; }o; /* object: members, member count, capacity */
        struct { zson_value*  e; size_t size, capacity; }a; /* array:  elements, element count, capacity */
        struct { char* s; size_t len; }s;                   /* string: null-terminated string, string length */
        double n;                                           /* number */
    }u;
    zson_type type;
};

struct zson_member {
    char* k; size_t klen;   /* member key string, key string length */
    zson_value v;           /* member value */
};

enum {
    ZSON_PARSE_OK = 0,
    ZSON_PARSE_EXPECT_VALUE,
    ZSON_PARSE_INVALID_VALUE,
    ZSON_PARSE_ROOT_NOT_SINGULAR,
    ZSON_PARSE_NUMBER_TOO_BIG,
    ZSON_PARSE_MISS_QUOTATION_MARK,
    ZSON_PARSE_INVALID_STRING_ESCAPE,
    ZSON_PARSE_INVALID_STRING_CHAR,
    ZSON_PARSE_INVALID_UNICODE_HEX,
    ZSON_PARSE_INVALID_UNICODE_SURROGATE,
    ZSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    ZSON_PARSE_MISS_KEY,
    ZSON_PARSE_MISS_COLON,
    ZSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET
};

#define zson_init(v) do { (v)->type = ZSON_NULL; } while(0)

int zson_parse(zson_value* v, const char* json);
char* zson_stringify(const zson_value* v, size_t* length);

void zson_copy(zson_value* dst, const zson_value* src);
void zson_move(zson_value* dst, zson_value* src);
void zson_swap(zson_value* lhs, zson_value* rhs);

void zson_free(zson_value* v);

zson_type zson_get_type(const zson_value* v);
int zson_is_equal(const zson_value* lhs, const zson_value* rhs);

#define zson_set_null(v) zson_free(v)

int zson_get_boolean(const zson_value* v);
void zson_set_boolean(zson_value* v, int b);

double zson_get_number(const zson_value* v);
void zson_set_number(zson_value* v, double n);

const char* zson_get_string(const zson_value* v);
size_t zson_get_string_length(const zson_value* v);
void zson_set_string(zson_value* v, const char* s, size_t len);

void zson_set_array(zson_value* v, size_t capacity);
size_t zson_get_array_size(const zson_value* v);
size_t zson_get_array_capacity(const zson_value* v);
void zson_reserve_array(zson_value* v, size_t capacity);
void zson_shrink_array(zson_value* v);
void zson_clear_array(zson_value* v);
zson_value* zson_get_array_element(zson_value* v, size_t index);
zson_value* zson_pushback_array_element(zson_value* v);
void zson_popback_array_element(zson_value* v);
zson_value* zson_insert_array_element(zson_value* v, size_t index);
void zson_erase_array_element(zson_value* v, size_t index, size_t count);

void zson_set_object(zson_value* v, size_t capacity);
size_t zson_get_object_size(const zson_value* v);
size_t zson_get_object_capacity(const zson_value* v);
void zson_reserve_object(zson_value* v, size_t capacity);
void zson_shrink_object(zson_value* v);
void zson_clear_object(zson_value* v);
const char* zson_get_object_key(const zson_value* v, size_t index);
size_t zson_get_object_key_length(const zson_value* v, size_t index);
zson_value* zson_get_object_value(zson_value* v, size_t index);
size_t zson_find_object_index(const zson_value* v, const char* key, size_t klen);
zson_value* zson_find_object_value(zson_value* v, const char* key, size_t klen);
zson_value* zson_set_object_value(zson_value* v, const char* key, size_t klen);
void zson_remove_object_value(zson_value* v, size_t index);

#endif /* ZSON_H__ */