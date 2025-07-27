// Certain things commonly used by examples

#include <pinc.h>
#include <stdint.h>
#include <stdio.h>

void PINC_PROC_CALL exampleErrorCallback(uint8_t const * message_buf, uintptr_t message_len, PincErrorType error_type) {
    (void)error_type;
    (void)fprintf(stderr, "Pinc had an error! It says \"%.*s\"\n", (int)message_len, message_buf);
}
