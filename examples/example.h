// Certain things commonly used by examples

#include <pinc.h>
#include <stdint.h>
#include <stdio.h>

void PINC_PROC_CALL example_error_callback(uint8_t const * message_buf, uintptr_t message_len) {
    // I swapped message_len and message_buf. It took me 40 minutes to find. What would I do without the compiler warnings...
    // Genuinely, printf using variadic arguments is one of the absolute most disastrous mistakes of the C standard
    printf("Pinc had an error! It says \"%.*s\"\n", (int)message_len, message_buf);
}
