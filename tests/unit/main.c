#include "test.h"

#include <stdio.h>

int main(void) {
    test_arena();
    test_vec();
    test_map();
    test_str();
    test_error();
    test_platform();
    test_source();
    test_lexer();
    test_parser();
    test_sema();
    test_calculator_example();
    (void)puts("all unit tests passed");
    return 0;
}
