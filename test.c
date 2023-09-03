#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Zson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_EQ_STRING(expect, actual, alength) \
    EXPECT_EQ_BASE(sizeof(expect) - 1 == alength && memcmp(expect, actual, alength + 1) == 0, expect, actual, "%s")
#define EXPECT_TRUE(actual) EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_FALSE(actual) EXPECT_EQ_BASE((actual) == 0, "false", "true", "%s")

#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#endif

static void test_parse_null() {
    zson_value v;
    zson_init(&v);
    zson_set_boolean(&v, 0);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "null"));
    EXPECT_EQ_INT(ZSON_NULL, zson_get_type(&v));
    zson_free(&v);
}

static void test_parse_true() {
    zson_value v;
    zson_init(&v);
    zson_set_boolean(&v, 0);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "true"));
    EXPECT_EQ_INT(ZSON_TRUE, zson_get_type(&v));
    zson_free(&v);
}

static void test_parse_false() {
    zson_value v;
    zson_init(&v);
    zson_set_boolean(&v, 1);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "false"));
    EXPECT_EQ_INT(ZSON_FALSE, zson_get_type(&v));
    zson_free(&v);
}

#define TEST_NUMBER(expect, json)\
    do {\
        zson_value v;\
        zson_init(&v);\
        EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, json));\
        EXPECT_EQ_INT(ZSON_NUMBER, zson_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, zson_get_number(&v));\
        zson_free(&v);\
    } while(0)

static void test_parse_number() {
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1416, "3.1416");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000"); /* must underflow */

    TEST_NUMBER(1.0000000000000002, "1.0000000000000002"); /* the smallest number > 1 */
    TEST_NUMBER( 4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUMBER( 2.2250738585072009e-308, "2.2250738585072009e-308");  /* Max subnormal double */
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUMBER( 2.2250738585072014e-308, "2.2250738585072014e-308");  /* Min normal positive double */
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUMBER( 1.7976931348623157e+308, "1.7976931348623157e+308");  /* Max double */
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}

#define TEST_STRING(expect, json)\
    do {\
        zson_value v;\
        zson_init(&v);\
        EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, json));\
        EXPECT_EQ_INT(ZSON_STRING, zson_get_type(&v));\
        EXPECT_EQ_STRING(expect, zson_get_string(&v), zson_get_string_length(&v));\
        zson_free(&v);\
    } while(0)

static void test_parse_string() {
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
    TEST_STRING("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STRING("\x24", "\"\\u0024\"");         /* Dollar sign U+0024 */
    TEST_STRING("\xC2\xA2", "\"\\u00A2\"");     /* Cents sign U+00A2 */
    TEST_STRING("\xE2\x82\xAC", "\"\\u20AC\""); /* Euro sign U+20AC */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  /* G clef sign U+1D11E */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");  /* G clef sign U+1D11E */
}

static void test_parse_array() {
    size_t i, j;
    zson_value v;

    zson_init(&v);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "[ ]"));
    EXPECT_EQ_INT(ZSON_ARRAY, zson_get_type(&v));
    EXPECT_EQ_SIZE_T(0, zson_get_array_size(&v));
    zson_free(&v);

    zson_init(&v);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(ZSON_ARRAY, zson_get_type(&v));
    EXPECT_EQ_SIZE_T(5, zson_get_array_size(&v));
    EXPECT_EQ_INT(ZSON_NULL,   zson_get_type(zson_get_array_element(&v, 0)));
    EXPECT_EQ_INT(ZSON_FALSE,  zson_get_type(zson_get_array_element(&v, 1)));
    EXPECT_EQ_INT(ZSON_TRUE,   zson_get_type(zson_get_array_element(&v, 2)));
    EXPECT_EQ_INT(ZSON_NUMBER, zson_get_type(zson_get_array_element(&v, 3)));
    EXPECT_EQ_INT(ZSON_STRING, zson_get_type(zson_get_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, zson_get_number(zson_get_array_element(&v, 3)));
    EXPECT_EQ_STRING("abc", zson_get_string(zson_get_array_element(&v, 4)), zson_get_string_length(zson_get_array_element(&v, 4)));
    zson_free(&v);

    zson_init(&v);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(ZSON_ARRAY, zson_get_type(&v));
    EXPECT_EQ_SIZE_T(4, zson_get_array_size(&v));
    for (i = 0; i < 4; i++) {
        zson_value* a = zson_get_array_element(&v, i);
        EXPECT_EQ_INT(ZSON_ARRAY, zson_get_type(a));
        EXPECT_EQ_SIZE_T(i, zson_get_array_size(a));
        for (j = 0; j < i; j++) {
            zson_value* e = zson_get_array_element(a, j);
            EXPECT_EQ_INT(ZSON_NUMBER, zson_get_type(e));
            EXPECT_EQ_DOUBLE((double)j, zson_get_number(e));
        }
    }
    zson_free(&v);
}

static void test_parse_object() {
    zson_value v;
    size_t i;

    zson_init(&v);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, " { } "));
    EXPECT_EQ_INT(ZSON_OBJECT, zson_get_type(&v));
    EXPECT_EQ_SIZE_T(0, zson_get_object_size(&v));
    zson_free(&v);

    zson_init(&v);
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v,
        " { "
        "\"n\" : null , "
        "\"f\" : false , "
        "\"t\" : true , "
        "\"i\" : 123 , "
        "\"s\" : \"abc\", "
        "\"a\" : [ 1, 2, 3 ],"
        "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
        " } "
    ));
    EXPECT_EQ_INT(ZSON_OBJECT, zson_get_type(&v));
    EXPECT_EQ_SIZE_T(7, zson_get_object_size(&v));
    EXPECT_EQ_STRING("n", zson_get_object_key(&v, 0), zson_get_object_key_length(&v, 0));
    EXPECT_EQ_INT(ZSON_NULL,   zson_get_type(zson_get_object_value(&v, 0)));
    EXPECT_EQ_STRING("f", zson_get_object_key(&v, 1), zson_get_object_key_length(&v, 1));
    EXPECT_EQ_INT(ZSON_FALSE,  zson_get_type(zson_get_object_value(&v, 1)));
    EXPECT_EQ_STRING("t", zson_get_object_key(&v, 2), zson_get_object_key_length(&v, 2));
    EXPECT_EQ_INT(ZSON_TRUE,   zson_get_type(zson_get_object_value(&v, 2)));
    EXPECT_EQ_STRING("i", zson_get_object_key(&v, 3), zson_get_object_key_length(&v, 3));
    EXPECT_EQ_INT(ZSON_NUMBER, zson_get_type(zson_get_object_value(&v, 3)));
    EXPECT_EQ_DOUBLE(123.0, zson_get_number(zson_get_object_value(&v, 3)));
    EXPECT_EQ_STRING("s", zson_get_object_key(&v, 4), zson_get_object_key_length(&v, 4));
    EXPECT_EQ_INT(ZSON_STRING, zson_get_type(zson_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("abc", zson_get_string(zson_get_object_value(&v, 4)), zson_get_string_length(zson_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("a", zson_get_object_key(&v, 5), zson_get_object_key_length(&v, 5));
    EXPECT_EQ_INT(ZSON_ARRAY, zson_get_type(zson_get_object_value(&v, 5)));
    EXPECT_EQ_SIZE_T(3, zson_get_array_size(zson_get_object_value(&v, 5)));
    for (i = 0; i < 3; i++) {
        zson_value* e = zson_get_array_element(zson_get_object_value(&v, 5), i);
        EXPECT_EQ_INT(ZSON_NUMBER, zson_get_type(e));
        EXPECT_EQ_DOUBLE(i + 1.0, zson_get_number(e));
    }
    EXPECT_EQ_STRING("o", zson_get_object_key(&v, 6), zson_get_object_key_length(&v, 6));
    {
        zson_value* o = zson_get_object_value(&v, 6);
        EXPECT_EQ_INT(ZSON_OBJECT, zson_get_type(o));
        for (i = 0; i < 3; i++) {
            zson_value* ov = zson_get_object_value(o, i);
            EXPECT_TRUE('1' + i == zson_get_object_key(o, i)[0]);
            EXPECT_EQ_SIZE_T(1, zson_get_object_key_length(o, i));
            EXPECT_EQ_INT(ZSON_NUMBER, zson_get_type(ov));
            EXPECT_EQ_DOUBLE(i + 1.0, zson_get_number(ov));
        }
    }
    zson_free(&v);
}

#define TEST_PARSE_ERROR(error, json)\
    do {\
        zson_value v;\
        zson_init(&v);\
        v.type = ZSON_FALSE;\
        EXPECT_EQ_INT(error, zson_parse(&v, json));\
        EXPECT_EQ_INT(ZSON_NULL, zson_get_type(&v));\
        zson_free(&v);\
    } while(0)

static void test_parse_expect_value() {
    TEST_PARSE_ERROR(ZSON_PARSE_EXPECT_VALUE, "");
    TEST_PARSE_ERROR(ZSON_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value() {
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "nul");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "?");

    /* invalid number */
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "+0");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "+1");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, ".123"); /* at least one digit before '.' */
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "1.");   /* at least one digit after '.' */
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "INF");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "inf");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "NAN");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "nan");

    /* invalid value in array */
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "[1,]");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_VALUE, "[\"a\", nul]");
}

static void test_parse_root_not_singular() {
    TEST_PARSE_ERROR(ZSON_PARSE_ROOT_NOT_SINGULAR, "null x");

    /* invalid number */
    TEST_PARSE_ERROR(ZSON_PARSE_ROOT_NOT_SINGULAR, "0123"); /* after zero should be '.' or nothing */
    TEST_PARSE_ERROR(ZSON_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_PARSE_ERROR(ZSON_PARSE_ROOT_NOT_SINGULAR, "0x123");
}

static void test_parse_number_too_big() {
    TEST_PARSE_ERROR(ZSON_PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_PARSE_ERROR(ZSON_PARSE_NUMBER_TOO_BIG, "-1e309");
}

static void test_parse_miss_quotation_mark() {
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_QUOTATION_MARK, "\"abc");
}

static void test_parse_invalid_string_escape() {
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}

static void test_parse_invalid_string_char() {
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

static void test_parse_invalid_unicode_hex() {
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_HEX, "\"\\u 123\"");
}

static void test_parse_invalid_unicode_surrogate() {
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_PARSE_ERROR(ZSON_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

static void test_parse_miss_comma_or_square_bracket() {
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1}");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1 2");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[[]");
}

static void test_parse_miss_key() {
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{1:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{true:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{false:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{null:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{[]:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{{}:1,");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_KEY, "{\"a\":1,");
}

static void test_parse_miss_colon() {
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COLON, "{\"a\"}");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket() {
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_PARSE_ERROR(ZSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

static void test_parse() {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_parse_object();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();
    test_parse_miss_quotation_mark();
    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();
    test_parse_miss_comma_or_square_bracket();
    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();
}

#define TEST_ROUNDTRIP(json)\
    do {\
        zson_value v;\
        char* json2;\
        size_t length;\
        zson_init(&v);\
        EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, json));\
        json2 = zson_stringify(&v, &length);\
        EXPECT_EQ_STRING(json, json2, length);\
        zson_free(&v);\
        free(json2);\
    } while(0)

static void test_stringify_number() {
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("-1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
    TEST_ROUNDTRIP("1e+20");
    TEST_ROUNDTRIP("1.234e+20");
    TEST_ROUNDTRIP("1.234e-20");

    TEST_ROUNDTRIP("1.0000000000000002"); /* the smallest number > 1 */
    TEST_ROUNDTRIP("4.9406564584124654e-324"); /* minimum denormal */
    TEST_ROUNDTRIP("-4.9406564584124654e-324");
    TEST_ROUNDTRIP("2.2250738585072009e-308");  /* Max subnormal double */
    TEST_ROUNDTRIP("-2.2250738585072009e-308");
    TEST_ROUNDTRIP("2.2250738585072014e-308");  /* Min normal positive double */
    TEST_ROUNDTRIP("-2.2250738585072014e-308");
    TEST_ROUNDTRIP("1.7976931348623157e+308");  /* Max double */
    TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

static void test_stringify_string() {
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
    TEST_ROUNDTRIP("\"Hello\\u0000World\"");
}

static void test_stringify_array() {
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

static void test_stringify_object() {
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

static void test_stringify() {
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");
    test_stringify_number();
    test_stringify_string();
    test_stringify_array();
    test_stringify_object();
}

#define TEST_EQUAL(json1, json2, equality) \
    do {\
        zson_value v1, v2;\
        zson_init(&v1);\
        zson_init(&v2);\
        EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v1, json1));\
        EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v2, json2));\
        EXPECT_EQ_INT(equality, zson_is_equal(&v1, &v2));\
        zson_free(&v1);\
        zson_free(&v2);\
    } while(0)

static void test_equal() {
    TEST_EQUAL("true", "true", 1);
    TEST_EQUAL("true", "false", 0);
    TEST_EQUAL("false", "false", 1);
    TEST_EQUAL("null", "null", 1);
    TEST_EQUAL("null", "0", 0);
    TEST_EQUAL("123", "123", 1);
    TEST_EQUAL("123", "456", 0);
    TEST_EQUAL("\"abc\"", "\"abc\"", 1);
    TEST_EQUAL("\"abc\"", "\"abcd\"", 0);
    TEST_EQUAL("[]", "[]", 1);
    TEST_EQUAL("[]", "null", 0);
    TEST_EQUAL("[1,2,3]", "[1,2,3]", 1);
    TEST_EQUAL("[1,2,3]", "[1,2,3,4]", 0);
    TEST_EQUAL("[[]]", "[[]]", 1);
    TEST_EQUAL("{}", "{}", 1);
    TEST_EQUAL("{}", "null", 0);
    TEST_EQUAL("{}", "[]", 0);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2}", 1);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"b\":2,\"a\":1}", 1);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":3}", 0);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2,\"c\":3}", 0);
    TEST_EQUAL("{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":{}}}}", 1);
    TEST_EQUAL("{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":[]}}}", 0);
}

static void test_copy() {
    zson_value v1, v2;
    zson_init(&v1);
    zson_parse(&v1, "{\"t\":true,\"f\":false,\"n\":null,\"d\":1.5,\"a\":[1,2,3]}");
    zson_init(&v2);
    zson_copy(&v2, &v1);
    EXPECT_TRUE(zson_is_equal(&v2, &v1));
    zson_free(&v1);
    zson_free(&v2);
}

static void test_move() {
    zson_value v1, v2, v3;
    zson_init(&v1);
    zson_parse(&v1, "{\"t\":true,\"f\":false,\"n\":null,\"d\":1.5,\"a\":[1,2,3]}");
    zson_init(&v2);
    zson_copy(&v2, &v1);
    zson_init(&v3);
    zson_move(&v3, &v2);
    EXPECT_EQ_INT(ZSON_NULL, zson_get_type(&v2));
    EXPECT_TRUE(zson_is_equal(&v3, &v1));
    zson_free(&v1);
    zson_free(&v2);
    zson_free(&v3);
}

static void test_swap() {
    zson_value v1, v2;
    zson_init(&v1);
    zson_init(&v2);
    zson_set_string(&v1, "Hello",  5);
    zson_set_string(&v2, "World!", 6);
    zson_swap(&v1, &v2);
    EXPECT_EQ_STRING("World!", zson_get_string(&v1), zson_get_string_length(&v1));
    EXPECT_EQ_STRING("Hello",  zson_get_string(&v2), zson_get_string_length(&v2));
    zson_free(&v1);
    zson_free(&v2);
}

static void test_access_null() {
    zson_value v;
    zson_init(&v);
    zson_set_string(&v, "a", 1);
    zson_set_null(&v);
    EXPECT_EQ_INT(ZSON_NULL, zson_get_type(&v));
    zson_free(&v);
}

static void test_access_boolean() {
    zson_value v;
    zson_init(&v);
    zson_set_string(&v, "a", 1);
    zson_set_boolean(&v, 1);
    EXPECT_TRUE(zson_get_boolean(&v));
    zson_set_boolean(&v, 0);
    EXPECT_FALSE(zson_get_boolean(&v));
    zson_free(&v);
}

static void test_access_number() {
    zson_value v;
    zson_init(&v);
    zson_set_string(&v, "a", 1);
    zson_set_number(&v, 1234.5);
    EXPECT_EQ_DOUBLE(1234.5, zson_get_number(&v));
    zson_free(&v);
}

static void test_access_string() {
    zson_value v;
    zson_init(&v);
    zson_set_string(&v, "", 0);
    EXPECT_EQ_STRING("", zson_get_string(&v), zson_get_string_length(&v));
    zson_set_string(&v, "Hello", 5);
    EXPECT_EQ_STRING("Hello", zson_get_string(&v), zson_get_string_length(&v));
    zson_free(&v);
}

static void test_access_array() {
    zson_value a, e;
    size_t i, j;

    zson_init(&a);

    for (j = 0; j <= 5; j += 5) {
        zson_set_array(&a, j);
        EXPECT_EQ_SIZE_T(0, zson_get_array_size(&a));
        EXPECT_EQ_SIZE_T(j, zson_get_array_capacity(&a));
        for (i = 0; i < 10; i++) {
            zson_init(&e);
            zson_set_number(&e, i);
            zson_move(zson_pushback_array_element(&a), &e);
            zson_free(&e);
        }

        EXPECT_EQ_SIZE_T(10, zson_get_array_size(&a));
        for (i = 0; i < 10; i++)
            EXPECT_EQ_DOUBLE((double)i, zson_get_number(zson_get_array_element(&a, i)));
    }

    zson_popback_array_element(&a);
    EXPECT_EQ_SIZE_T(9, zson_get_array_size(&a));
    for (i = 0; i < 9; i++)
        EXPECT_EQ_DOUBLE((double)i, zson_get_number(zson_get_array_element(&a, i)));

    zson_erase_array_element(&a, 4, 0);
    EXPECT_EQ_SIZE_T(9, zson_get_array_size(&a));
    for (i = 0; i < 9; i++)
        EXPECT_EQ_DOUBLE((double)i, zson_get_number(zson_get_array_element(&a, i)));

    zson_erase_array_element(&a, 8, 1);
    EXPECT_EQ_SIZE_T(8, zson_get_array_size(&a));
    for (i = 0; i < 8; i++)
        EXPECT_EQ_DOUBLE((double)i, zson_get_number(zson_get_array_element(&a, i)));

    zson_erase_array_element(&a, 0, 2);
    EXPECT_EQ_SIZE_T(6, zson_get_array_size(&a));
    for (i = 0; i < 6; i++)
        EXPECT_EQ_DOUBLE((double)i + 2, zson_get_number(zson_get_array_element(&a, i)));

#if 1
    for (i = 0; i < 2; i++) {
        zson_init(&e);
        zson_set_number(&e, i);
        zson_move(zson_insert_array_element(&a, i), &e);
        zson_free(&e);
    }
#endif
    
    EXPECT_EQ_SIZE_T(8, zson_get_array_size(&a));
    for (i = 0; i < 8; i++)
        EXPECT_EQ_DOUBLE((double)i, zson_get_number(zson_get_array_element(&a, i)));

    EXPECT_TRUE(zson_get_array_capacity(&a) > 8);
    zson_shrink_array(&a);
    EXPECT_EQ_SIZE_T(8, zson_get_array_capacity(&a));
    EXPECT_EQ_SIZE_T(8, zson_get_array_size(&a));
    for (i = 0; i < 8; i++)
        EXPECT_EQ_DOUBLE((double)i, zson_get_number(zson_get_array_element(&a, i)));

    zson_set_string(&e, "Hello", 5);
    zson_move(zson_pushback_array_element(&a), &e);     /* Test if element is freed */
    zson_free(&e);

    i = zson_get_array_capacity(&a);
    zson_clear_array(&a);
    EXPECT_EQ_SIZE_T(0, zson_get_array_size(&a));
    EXPECT_EQ_SIZE_T(i, zson_get_array_capacity(&a));   /* capacity remains unchanged */
    zson_shrink_array(&a);
    EXPECT_EQ_SIZE_T(0, zson_get_array_capacity(&a));

    zson_free(&a);
}

static void test_access_object() {
#if 0
    zson_value o, v, *pv;
    size_t i, j, index;

    zson_init(&o);

    for (j = 0; j <= 5; j += 5) {
        zson_set_object(&o, j);
        EXPECT_EQ_SIZE_T(0, zson_get_object_size(&o));
        EXPECT_EQ_SIZE_T(j, zson_get_object_capacity(&o));
        for (i = 0; i < 10; i++) {
            char key[2] = "a";
            key[0] += i;
            zson_init(&v);
            zson_set_number(&v, i);
            zson_move(zson_set_object_value(&o, key, 1), &v);
            zson_free(&v);
        }
        EXPECT_EQ_SIZE_T(10, zson_get_object_size(&o));
        for (i = 0; i < 10; i++) {
            char key[] = "a";
            key[0] += i;
            index = zson_find_object_index(&o, key, 1);
            EXPECT_TRUE(index != ZSON_KEY_NOT_EXIST);
            pv = zson_get_object_value(&o, index);
            EXPECT_EQ_DOUBLE((double)i, zson_get_number(pv));
        }
    }

    index = zson_find_object_index(&o, "j", 1);    
    EXPECT_TRUE(index != ZSON_KEY_NOT_EXIST);
    zson_remove_object_value(&o, index);
    index = zson_find_object_index(&o, "j", 1);
    EXPECT_TRUE(index == ZSON_KEY_NOT_EXIST);
    EXPECT_EQ_SIZE_T(9, zson_get_object_size(&o));

    index = zson_find_object_index(&o, "a", 1);
    EXPECT_TRUE(index != ZSON_KEY_NOT_EXIST);
    zson_remove_object_value(&o, index);
    index = zson_find_object_index(&o, "a", 1);
    EXPECT_TRUE(index == ZSON_KEY_NOT_EXIST);
    EXPECT_EQ_SIZE_T(8, zson_get_object_size(&o));

    EXPECT_TRUE(zson_get_object_capacity(&o) > 8);
    zson_shrink_object(&o);
    EXPECT_EQ_SIZE_T(8, zson_get_object_capacity(&o));
    EXPECT_EQ_SIZE_T(8, zson_get_object_size(&o));
    for (i = 0; i < 8; i++) {
        char key[] = "a";
        key[0] += i + 1;
        EXPECT_EQ_DOUBLE((double)i + 1, zson_get_number(zson_get_object_value(&o, zson_find_object_index(&o, key, 1))));
    }

    zson_set_string(&v, "Hello", 5);
    zson_move(zson_set_object_value(&o, "World", 5), &v); /* Test if element is freed */
    zson_free(&v);

    pv = zson_find_object_value(&o, "World", 5);
    EXPECT_TRUE(pv != NULL);
    EXPECT_EQ_STRING("Hello", zson_get_string(pv), zson_get_string_length(pv));

    i = zson_get_object_capacity(&o);
    zson_clear_object(&o);
    EXPECT_EQ_SIZE_T(0, zson_get_object_size(&o));
    EXPECT_EQ_SIZE_T(i, zson_get_object_capacity(&o)); /* capacity remains unchanged */
    zson_shrink_object(&o);
    EXPECT_EQ_SIZE_T(0, zson_get_object_capacity(&o));

    zson_free(&o);
#endif
}

static void test_access() {
    test_access_null();
    test_access_boolean();
    test_access_number();
    test_access_string();
    test_access_array();
    test_access_object();
}

int main() {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    test_parse();
    test_stringify();
    test_equal();
    test_copy();
    test_move();
    test_swap();
    test_access();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}