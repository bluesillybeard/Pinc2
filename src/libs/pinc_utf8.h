#ifndef P_UTF8_H
#define P_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// TODO: Since Pinc needs a robust UTF8 impl anyway, why not expose it to the user?
// (theoretically as simple as moving this header to the public include path)

// Notes about Pinc's UTF8 implementation:
// - overlong encodings are decoded, but not encoded
// - surrogate halves are decoded, but not encoded (U+D800 - U+DFFF)

#define PINC_UTF8_REPLACEMENT_CHARACTER 0xFFFD

// Returns the number of bytes required to encode a single given codepoint
// Returns 0 when given an invalid codepoint
uint8_t pincCodepointUTF8Len(uint32_t codepoint);

bool pincValidateUTF8String(char const* str_ptr, size_t str_len);

// Decodes a single UTF8 codepoint
// use pincCodepointUTF8Len to determine how many characters to move forward for the next codepoint
// returns 0 if the encoding is invalid
uint32_t pincDecodeUTF8Single(char const* str_ptr, size_t str_len);

// Returns the number of codepoints in the string
// If the given string is invalid UTF8, it will decode nothing and return 0.
size_t pincDecodeUTF8String(char const* str_ptr, size_t str_len, uint32_t* out_ptr, size_t out_capacity);

#endif
