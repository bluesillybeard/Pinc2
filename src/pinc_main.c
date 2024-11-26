#include <pinc.h>
#include <stdio.h>

PINC_API pinc_return_code PINC_CALL pinc_incomplete_init(void) {
    printf("Hello There!\n");
    return pinc_return_code_pass;
}


