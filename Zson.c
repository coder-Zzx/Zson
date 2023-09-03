#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "zson.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdio.h>   /* sprintf() */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */

#ifndef ZSON_PARSE_STACK_INIT_SIZE
#define ZSON_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef ZSON_PARSE_STRINGIFY_INIT_SIZE
#define ZSON_PARSE_STRINGIFY_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)zson_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len)     memcpy(zson_context_push(c, len), s, len)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}zson_context;

static void* zson_context_push(zson_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = ZSON_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* zson_context_pop(zson_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void zson_parse_whitespace(zson_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int zson_parse_literal(zson_context* c, zson_value* v, const char* literal, zson_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return ZSON_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return ZSON_PARSE_OK;
}

static int zson_parse_number(zson_context* c, zson_value* v) {
    const char* p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return ZSON_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return ZSON_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return ZSON_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return ZSON_PARSE_NUMBER_TOO_BIG;
    v->type = ZSON_NUMBER;
    c->json = p;
    return ZSON_PARSE_OK;
}

static const char* zson_parse_hex4(const char* p, unsigned* u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

static void zson_encode_utf8(zson_context* c, unsigned u) {
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

static int zson_parse_string_raw(zson_context* c, char** str, size_t* len) {
    size_t head = c->top;
    unsigned u, u2;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;
                *str = zson_context_pop(c, *len);
                c->json = p;
                return ZSON_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        if (!(p = zson_parse_hex4(p, &u)))
                            STRING_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = zson_parse_hex4(p, &u2)))
                                STRING_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        zson_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(ZSON_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(ZSON_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(ZSON_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int zson_parse_string(zson_context* c, zson_value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = zson_parse_string_raw(c, &s, &len)) == ZSON_PARSE_OK)
        zson_set_string(v, s, len);
    return ret;
}

static int zson_parse_value(zson_context* c, zson_value* v);

static int zson_parse_array(zson_context* c, zson_value* v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    zson_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        zson_set_array(v, 0);
        return ZSON_PARSE_OK;
    }
    for (;;) {
        zson_value e;
        zson_init(&e);
        if ((ret = zson_parse_value(c, &e)) != ZSON_PARSE_OK)
            break;
        memcpy(zson_context_push(c, sizeof(zson_value)), &e, sizeof(zson_value));
        size++;
        zson_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            zson_parse_whitespace(c);
        }
        else if (*c->json == ']') {
            c->json++;
            zson_set_array(v, size);
            memcpy(v->u.a.e, zson_context_pop(c, size * sizeof(zson_value)), size * sizeof(zson_value));
            v->u.a.size = size;
            return ZSON_PARSE_OK;
        }
        else {
            ret = ZSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    /* Pop and free values on the stack */
    for (i = 0; i < size; i++)
        zson_free((zson_value*)zson_context_pop(c, sizeof(zson_value)));
    return ret;
}

static int zson_parse_object(zson_context* c, zson_value* v) {
    size_t i, size;
    zson_member m;
    int ret;
    EXPECT(c, '{');
    zson_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        zson_set_object(v, 0);
        return ZSON_PARSE_OK;
    }
    m.k = NULL;
    size = 0;
    for (;;) {
        char* str;
        zson_init(&m.v);
        /* parse key */
        if (*c->json != '"') {
            ret = ZSON_PARSE_MISS_KEY;
            break;
        }
        if ((ret = zson_parse_string_raw(c, &str, &m.klen)) != ZSON_PARSE_OK)
            break;
        memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        /* parse ws colon ws */
        zson_parse_whitespace(c);
        if (*c->json != ':') {
            ret = ZSON_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        zson_parse_whitespace(c);
        /* parse value */
        if ((ret = zson_parse_value(c, &m.v)) != ZSON_PARSE_OK)
            break;
        memcpy(zson_context_push(c, sizeof(zson_member)), &m, sizeof(zson_member));
        size++;
        m.k = NULL; /* ownership is transferred to member on stack */
        /* parse ws [comma | right-curly-brace] ws */
        zson_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            zson_parse_whitespace(c);
        }
        else if (*c->json == '}') {
            c->json++;
            zson_set_object(v, size);
            memcpy(v->u.o.m, zson_context_pop(c, sizeof(zson_member) * size), sizeof(zson_member) * size);
            v->u.o.size = size;
            return ZSON_PARSE_OK;
        }
        else {
            ret = ZSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* Pop and free members on the stack */
    free(m.k);
    for (i = 0; i < size; i++) {
        zson_member* m = (zson_member*)zson_context_pop(c, sizeof(zson_member));
        free(m->k);
        zson_free(&m->v);
    }
    v->type = ZSON_NULL;
    return ret;
}

static int zson_parse_value(zson_context* c, zson_value* v) {
    switch (*c->json) {
        case 't':  return zson_parse_literal(c, v, "true", ZSON_TRUE);
        case 'f':  return zson_parse_literal(c, v, "false", ZSON_FALSE);
        case 'n':  return zson_parse_literal(c, v, "null", ZSON_NULL);
        default:   return zson_parse_number(c, v);
        case '"':  return zson_parse_string(c, v);
        case '[':  return zson_parse_array(c, v);
        case '{':  return zson_parse_object(c, v);
        case '\0': return ZSON_PARSE_EXPECT_VALUE;
    }
}

int zson_parse(zson_value* v, const char* json) {
    zson_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    zson_init(v);
    zson_parse_whitespace(&c);
    if ((ret = zson_parse_value(&c, v)) == ZSON_PARSE_OK) {
        zson_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = ZSON_NULL;
            ret = ZSON_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

static void zson_stringify_string(zson_context* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != NULL);
    p = head = zson_context_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}

static void zson_stringify_value(zson_context* c, const zson_value* v) {
    size_t i;
    switch (v->type) {
        case ZSON_NULL:   PUTS(c, "null",  4); break;
        case ZSON_FALSE:  PUTS(c, "false", 5); break;
        case ZSON_TRUE:   PUTS(c, "true",  4); break;
        case ZSON_NUMBER: c->top -= 32 - sprintf(zson_context_push(c, 32), "%.17g", v->u.n); break;
        case ZSON_STRING: zson_stringify_string(c, v->u.s.s, v->u.s.len); break;
        case ZSON_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                zson_stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case ZSON_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                zson_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                zson_stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

char* zson_stringify(const zson_value* v, size_t* length) {
    zson_context c;
    assert(v != NULL);
    c.stack = (char*)malloc(c.size = ZSON_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    zson_stringify_value(&c, v);
    if (length)
        *length = c.top;
    PUTC(&c, '\0');
    return c.stack;
}

void zson_copy(zson_value* dst, const zson_value* src) {
    assert(src != NULL && dst != NULL && src != dst);
    size_t i;
    switch (src->type) {
        case ZSON_STRING:
            zson_set_string(dst, src->u.s.s, src->u.s.len);
            break;
        case ZSON_ARRAY:
            zson_set_array(dst, src->u.a.size);
            for(i = 0; i < src->u.a.size; i++){
                zson_copy(&dst->u.a.e[i], &src->u.a.e[i]);
            }
            dst->u.a.size = src->u.a.size;
            break;
        case ZSON_OBJECT:
            zson_set_object(dst, src->u.o.size);

            for(i = 0; i < src->u.o.size; i++){
                zson_value * val = zson_set_object_value(dst, src->u.o.m[i].k, src->u.o.m[i].klen);
                zson_copy(val, &src->u.o.m[i].v);
            }
            dst->u.o.size = src->u.o.size;
            break;
        default:
            zson_free(dst);
            memcpy(dst, src, sizeof(zson_value));
            break;
    }
}

void zson_move(zson_value* dst, zson_value* src) {
    assert(dst != NULL && src != NULL && src != dst);
    zson_free(dst);
    memcpy(dst, src, sizeof(zson_value));
    zson_init(src);
}

void zson_swap(zson_value* lhs, zson_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        zson_value temp;
        memcpy(&temp, lhs, sizeof(zson_value));
        memcpy(lhs,   rhs, sizeof(zson_value));
        memcpy(rhs, &temp, sizeof(zson_value));
    }
}

void zson_free(zson_value* v) {
    size_t i;
    assert(v != NULL);
    switch (v->type) {
        case ZSON_STRING:
            free(v->u.s.s);
            break;
        case ZSON_ARRAY:
            for (i = 0; i < v->u.a.size; i++)
                zson_free(&v->u.a.e[i]);
            free(v->u.a.e);
            break;
        case ZSON_OBJECT:
            for (i = 0; i < v->u.o.size; i++) {
                free(v->u.o.m[i].k);
                zson_free(&v->u.o.m[i].v);
            }
            free(v->u.o.m);
            break;
        default: break;
    }
    v->type = ZSON_NULL;
}

zson_type zson_get_type(const zson_value* v) {
    assert(v != NULL);
    return v->type;
}

int zson_is_equal(const zson_value* lhs, const zson_value* rhs) {
    size_t i, index;
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type)
        return 0;
    switch (lhs->type) {
        case ZSON_STRING:
            return lhs->u.s.len == rhs->u.s.len && 
                memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
        case ZSON_NUMBER:
            return lhs->u.n == rhs->u.n;
        case ZSON_ARRAY:
            if (lhs->u.a.size != rhs->u.a.size)
                return 0;
            for (i = 0; i < lhs->u.a.size; i++)
                if (!zson_is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i]))
                    return 0;
            return 1;
        case ZSON_OBJECT:
            if (lhs->u.o.size != rhs->u.o.size)
                return 0;
            for (i = 0; i < lhs->u.o.size; i++) {
                index = zson_find_object_index(rhs, lhs->u.o.m[i].k, lhs->u.o.m[i].klen);
                if (index == ZSON_KEY_NOT_EXIST) return 0;
                if (!zson_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[index].v)) return 0;
            }
            return 1;
        default:
            return 1;
    }
}

int zson_get_boolean(const zson_value* v) {
    assert(v != NULL && (v->type == ZSON_TRUE || v->type == ZSON_FALSE));
    return v->type == ZSON_TRUE;
}

void zson_set_boolean(zson_value* v, int b) {
    zson_free(v);
    v->type = b ? ZSON_TRUE : ZSON_FALSE;
}

double zson_get_number(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_NUMBER);
    return v->u.n;
}

void zson_set_number(zson_value* v, double n) {
    zson_free(v);
    v->u.n = n;
    v->type = ZSON_NUMBER;
}

const char* zson_get_string(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_STRING);
    return v->u.s.s;
}

size_t zson_get_string_length(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_STRING);
    return v->u.s.len;
}

void zson_set_string(zson_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    zson_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = ZSON_STRING;
}

void zson_set_array(zson_value* v, size_t capacity) {
    assert(v != NULL);
    zson_free(v);
    v->type = ZSON_ARRAY;
    v->u.a.size = 0;
    v->u.a.capacity = capacity;
    v->u.a.e = capacity > 0 ? (zson_value*)malloc(capacity * sizeof(zson_value)) : NULL;
}

size_t zson_get_array_size(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    return v->u.a.size;
}

size_t zson_get_array_capacity(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    return v->u.a.capacity;
}

void zson_reserve_array(zson_value* v, size_t capacity) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    if (v->u.a.capacity < capacity) {
        v->u.a.capacity = capacity;
        v->u.a.e = (zson_value*)realloc(v->u.a.e, capacity * sizeof(zson_value));
    }
}

void zson_shrink_array(zson_value* v) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    if (v->u.a.capacity > v->u.a.size) {
        v->u.a.capacity = v->u.a.size;
        v->u.a.e = (zson_value*)realloc(v->u.a.e, v->u.a.capacity * sizeof(zson_value));
    }
}

void zson_clear_array(zson_value* v) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    zson_erase_array_element(v, 0, v->u.a.size);
}

zson_value* zson_get_array_element(zson_value* v, size_t index) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

zson_value* zson_pushback_array_element(zson_value* v) {
    assert(v != NULL && v->type == ZSON_ARRAY);
    if (v->u.a.size == v->u.a.capacity)
        zson_reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    zson_init(&v->u.a.e[v->u.a.size]);
    return &v->u.a.e[v->u.a.size++];
}

void zson_popback_array_element(zson_value* v) {
    assert(v != NULL && v->type == ZSON_ARRAY && v->u.a.size > 0);
    zson_free(&v->u.a.e[--v->u.a.size]);
}

zson_value* zson_insert_array_element(zson_value* v, size_t index) {
    assert(v != NULL && v->type == ZSON_ARRAY && index <= v->u.a.size);
    if (v->u.a.size == v->u.a.capacity) zson_reserve_array(v, v->u.a.capacity == 0 ? 1: (v->u.a.size << 1));
    memcpy(&v->u.a.e[index + 1], &v->u.a.e[index], (v->u.a.size - index) * sizeof(zson_value));
    zson_init(&v->u.a.e[index]);
    v->u.a.size++;
    return &v->u.a.e[index];
}

void zson_erase_array_element(zson_value* v, size_t index, size_t count) {
    assert(v != NULL && v->type == ZSON_ARRAY && index + count <= v->u.a.size);
    size_t i;
    for(i = index; i < index + count; i++){
        zson_free(&v->u.a.e[i]);
    }
    memcpy(v->u.a.e + index, v->u.a.e + index + count, (v->u.a.size - index - count) * sizeof(zson_value));
    for(i = v->u.a.size - count; i < v->u.a.size; i++)
        zson_init(&v->u.a.e[i]);
    v->u.a.size -= count;
}

void zson_set_object(zson_value* v, size_t capacity) {
    assert(v != NULL);
    zson_free(v);
    v->type = ZSON_OBJECT;
    v->u.o.size = 0;
    v->u.o.capacity = capacity;
    v->u.o.m = capacity > 0 ? (zson_member*)malloc(capacity * sizeof(zson_member)) : NULL;
}

size_t zson_get_object_size(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    return v->u.o.size;
}

size_t zson_get_object_capacity(const zson_value* v) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    return v->u.o.capacity;
    return 0;
}

void zson_reserve_object(zson_value* v, size_t capacity) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    if(v->u.o.capacity < capacity){
        v->u.o.capacity = capacity;
        v->u.o.m = (zson_member *) realloc(v->u.o.m, capacity * sizeof(zson_member));
    }
}

void zson_shrink_object(zson_value* v) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    if(v->u.o.capacity > v->u.o.size) {
        v->u.o.capacity = v->u.o.size;
        v->u.o.m = (zson_member *)realloc(v->u.o.m, v->u.o.capacity * sizeof(zson_value));
    }
}

void zson_clear_object(zson_value* v) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    size_t i;
    for(i = 0; i < v->u.o.size; i++){
        free(v->u.o.m[i].k);
        v->u.o.m[i].k = NULL;
        v->u.o.m[i].klen = 0;
        zson_free(&v->u.o.m[i].v);
    }
    v->u.o.size = 0;
}

const char* zson_get_object_key(const zson_value* v, size_t index) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t zson_get_object_key_length(const zson_value* v, size_t index) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

zson_value* zson_get_object_value(zson_value* v, size_t index) {
    assert(v != NULL && v->type == ZSON_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}

size_t zson_find_object_index(const zson_value* v, const char* key, size_t klen) {
    size_t i;
    assert(v != NULL && v->type == ZSON_OBJECT && key != NULL);
    for (i = 0; i < v->u.o.size; i++)
        if (v->u.o.m[i].klen == klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
            return i;
    return ZSON_KEY_NOT_EXIST;
}

zson_value* zson_find_object_value(zson_value* v, const char* key, size_t klen) {
    size_t index = zson_find_object_index(v, key, klen);
    return index != ZSON_KEY_NOT_EXIST ? &v->u.o.m[index].v : NULL;
}

zson_value* zson_set_object_value(zson_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == ZSON_OBJECT && key != NULL);
    size_t i, index;
    index = zson_find_object_index(v, key, klen);
    if(index != ZSON_KEY_NOT_EXIST)
        return &v->u.o.m[index].v;
    if(v->u.o.size == v->u.o.capacity){
        zson_reserve_object(v, v->u.o.capacity == 0? 1: (v->u.o.capacity << 1));
    }
    i = v->u.o.size;
    v->u.o.m[i].k = (char *)malloc((klen + 1));
    memcpy(v->u.o.m[i].k, key, klen);
    v->u.o.m[i].k[klen] = '\0';
    v->u.o.m[i].klen = klen;
    zson_init(&v->u.o.m[i].v);
    v->u.o.size++;
    return &v->u.o.m[i].v;
}

void zson_remove_object_value(zson_value* v, size_t index) {
    assert(v != NULL && v->type == ZSON_OBJECT && index < v->u.o.size);
    free(v->u.o.m[index].k);
    zson_free(&v->u.o.m[index].v);
    memcpy(v->u.o.m + index, v->u.o.m + index + 1, (v->u.o.size - index - 1) * sizeof(zson_member));
    v->u.o.m[--v->u.o.size].k = NULL;
    v->u.o.m[v->u.o.size].klen = 0;
    zson_init(&v->u.o.m[v->u.o.size].v);
}