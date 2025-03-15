// Certain things commonly used by examples

#include <pinc.h>
#include <stdint.h>
#include <stdio.h>

void PINC_PROC_CALL example_error_callback(uint8_t const * message_buf, uintptr_t message_len, pinc_error_type error_type) {
    (void)error_type;
    fprintf(stderr, "Pinc had an error! It says \"%.*s\"\n", (int)message_len, message_buf);
}
