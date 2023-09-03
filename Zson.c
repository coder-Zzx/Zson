#include <stdlib.h>
#include <assert.h>
#include "Zson.h"

#define EXPECT(c, ch) do {assert(*c->json == (ch)); c->json++; } while(0)    

static void zson_parse_whitespace(zson_context* context) {
    const char* p = context->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    context->json = p;
}

static int zson_parse_null(zson_context* context, zson_value* z) {
    EXPECT(context, 'n');
    if (context->json[0] != 'u' || context->json[1] != 'l' || context->json[2] != 'l') {
        return ZSON_PARSE_INVALID_VALUE;
    }
    context->json += 3;
    z->type = ZSON_NULL;
    return ZSON_PARSE_OK;
}

static int zson_parse_true(zson_context* context, zson_value* z) {
    EXPECT(context, 't');
    if (context->json[0] != 'r' || context->json[1] != 'u' || context->json[2] != 'e') {
        return ZSON_PARSE_INVALID_VALUE;
    }
    context->json += 3;
    z->type = ZSON_TRUE;
    return ZSON_PARSE_OK;
}

static int zson_parse_false(zson_context* context, zson_value* z) {
    EXPECT(context, 'f');
    if (context->json[0] != 'a' || context->json[1] != 'l' || context->json[2] != 's' || context->json[3] != 'e') {
        return ZSON_PARSE_INVALID_VALUE;
    }
    context->json += 4;
    z->type = ZSON_FALSE;
    return ZSON_PARSE_OK;
}


static int zson_parse_value(zson_context* context, zson_value* z) {
    switch (*context->json) {
    case 'n': return zson_parse_null(context, z);
    case 't': return zson_parse_true(context, z);
    case 'f': return zson_parse_false(context, z);
    case '0': return ZSON_PARSE_EXPECT_VALUE;
    default:  return ZSON_PARSE_INVALID_VALUE;
    }
}

int zson_parse(zson_value* z, const char* json) {
    int ret;
    zson_context context;
    assert(z != NULL);
    context.json = json;
    z->type = ZSON_NULL;
    zson_parse_whitespace(&context);
    if ((ret = zson_parse_value(&context, z)) == ZSON_PARSE_OK) {
        zson_parse_whitespace(&context);
        if (*context.json != '\0') {
            ret = ZSON_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

zson_type zson_get_type(const zson_value* z) {
    assert(z != NULL);
    return z->type;
}