#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Zson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, result, format)\
    do {\
        test_count++;\
        if (equality) {\
            test_pass++;\
        }\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " result: " format "\n", __FILE__, __LINE__, expect, result);\
            main_ret = 1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, result) EXPECT_EQ_BASE((expect) == (result), expect, result, "%d")

static void test_parse_null() {
    zson_value v;
    v.type = ZSON_TRUE;
    EXPECT_EQ_INT(ZSON_PARSE_OK, zson_parse(&v, "null"));
    EXPECT_EQ_INT(ZSON_NULL, zson_get_type(&v));
}

static void test_parse() {
    test_parse_null();
}

int main() {
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}