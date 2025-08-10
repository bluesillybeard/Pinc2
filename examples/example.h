// Certain things commonly used by examples

#include <pinc.h>
#include <stdint.h>
#include <stdio.h>

// I would recommend putting a breakpoint on this function, so you can see full stacktrace of errors rather than just "error oh no!"
// And at runtime, if possible, print the whole stacktrace here so errors "in the field" can be debugged easier.
void PINC_PROC_CALL exampleErrorCallback(uint8_t const * message_buf, uintptr_t message_len, PincErrorCode error_type, bool recoverable) {
    (void)error_type;
    (void)recoverable;
    (void)fprintf(stderr, "Pinc had an error! It says \"%.*s\"\n", (int)message_len, message_buf);
}
