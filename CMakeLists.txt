cmake_minimum_required (VERSION 2.6)
project (Zjson_test C)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -pedantic -Wall")
endif()

add_library(Zson Zson.c)
add_executable(Zson_test test.c)
target_link_libraries(Zson_test Zson)