#ifndef ZSON_H__
#define ZSON_H__

typedef enum {
    ZSON_NULL,
    ZSON_TRUE,
    ZSON_FALSE,
    ZSON_NUMBER,
    ZSON_STRING,
    ZSON_ARRARY,
    ZSON_OBJECT
} zson_type;

enum {
    ZSON_PARSE_OK = 0,
    ZSON_PARSE_EXPECT_VALUE,
    ZSON_PARSE_INVALID_VALUE,
    ZSON_PARSE_ROOT_NOT_SINGULAR
};

typedef struct {
    double n;
    zson_type type;
} zson_value;

typedef struct {
    const char* json;
} zson_context;

int zson_parse(zson_value* z, const char* json);

zson_type zson_get_type(const zson_value* z);

#endif