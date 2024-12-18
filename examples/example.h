// Certain things commonly used by examples

#include <pinc.h>
#include <stdint.h>
#include <stdio.h>

void PINC_PROC_CALL example_error_callback(uint8_t const * message_buf, uintptr_t message_len) {
    printf("Pinc had an error! It says \"%s\"", message_buf);
}
