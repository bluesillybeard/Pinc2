#include "pinc_utf8.h"

uint8_t pincCodepointUTF8Len(uint32_t codepoint) {
    if (codepoint < 0x80) { return 1; }
    if (codepoint < 0x800) { return 2; }
    if (codepoint < 0x10000) { return 3; }
    if (codepoint < 0x110000) { return 4; }
    return 0;
}

// Gotta prefix these local macros so the unity build works (turns out, they aren't so local)
#define utf8xx 0xF1 // Invalid, size 1
#define utf8as 0xF0 // ASCII, size 1
#define utf8s1 0x02 // accept 0, size 2
#define utf8s2 0x03 // accept 0, size 3
#define utf8s3 0x03
#define utf8s4 0x04
#define utf8s5 0x34 // accept 3, size 4
#define utf8s6 0x04
#define utf8s7 0x44

static const uint8_t first_utf8_byte_info[128] = {
    utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx,
    utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx,
    utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx,
    utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx,
    utf8xx, utf8xx, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1,
    utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1, utf8s1,
    utf8s2, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s3, utf8s4, utf8s3, utf8s3,
    utf8s5, utf8s6, utf8s6, utf8s6, utf8s7, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx, utf8xx,
};

bool pincValidateUTF8String(char const* str_ptr, size_t str_len) { //NOLINT
    // Implementation based on Zig's standard library: 0.14.1 -> unicode.zig -> utf8ValidateSlice.,
    // Seriously, Zig's standard library is a genuine gold mine for random things like this.
    // Everything is optimized, robust, well documented and easy to read.

    // Main differences from Zig impl:
    // - in C (obviously)
    // - surrogates is assumed to be can_encode_surrogate_half
    // - large lists are declared above and defined at the bottom of this file
    // - lookup table size was cut in half. The zig version could easily do this... so why don't they?
    uint8_t const* rem_ptr = (uint8_t const*)str_ptr;
    size_t rem_len = str_len;

    // Skip any ASCII stuff at the start
    // Zig uses a vectorized loop... using intrinsics sounds like a pain,
    // so hopefully the compiler can figure it out.
    while(rem_len > 0) {
        uint8_t const value = rem_ptr[0];
        if((value & (uint8_t)0x80) != 0) {
            break;
        }
        rem_ptr = &(rem_ptr[1]);
        rem_len -= 1;
    }

    // const min_continue = (uint8_t)0b10000000; // PSYCH, no binary literals in C (on some compilers)
    uint8_t const min_continue = (uint8_t)0x80;
    uint8_t const max_continue = (uint8_t)0xBF;

    size_t index = 0;

    while(index < rem_len) {
        uint8_t const first_byte = rem_ptr[index];

        if(first_byte < min_continue) {
            index += 1;
            continue;
        }

        uint8_t const info = first_utf8_byte_info[first_byte - 128];

        uint8_t const size = info & (uint8_t)7;
        if(index + size > rem_len) { return false; }

        uint8_t min_accept = min_continue;
        uint8_t max_accept = max_continue;
        switch(info << (uint8_t)4) {
            case 1: min_accept = 0xA0; break;
            case 2: max_accept = 0x9F; break;
            case 3: min_accept = 0x90; break;
            case 4: max_accept = 0x8F; break;
            case 0: break;
            default: return false;
        }

        uint8_t const second_byte = rem_ptr[index + 1];
        if(second_byte < min_accept || second_byte > max_accept) { return false; }

        switch(size) {
            case 2: index += 2; break;
            case 3: {
                uint8_t const third_byte = rem_ptr[index + 2];
                if(third_byte < min_continue || max_continue < third_byte) { return false; }
                index += 3;
                break;
            }
            case 4: {
                uint8_t const third_byte = rem_ptr[index + 2];
                if(third_byte < min_continue || max_continue < third_byte) { return false; }
                uint8_t const fourth_byte = rem_ptr[index + 3];
                if(fourth_byte < min_continue || max_continue < third_byte) { return false; }
                index += 4;
            }
            default: return false;
        }
    }

    return true;
}

static uint8_t pincUTF8SequenceLen(uint8_t first_byte) {
    if(first_byte < 0x7F) { return 1; }
    if((first_byte > 0xC0) && (first_byte < 0xDF)) { return 2; }
    if((first_byte > 0xE0) && (first_byte < 0xEF)) { return 3; }
    if((first_byte > 0xF0) && (first_byte < 0xF7)) { return 4; }
    return 0;
}

uint32_t pincDecodeUTF8Single(char const* str_ptr, size_t str_len) {
    uint8_t const* const str = (uint8_t const*)str_ptr;
    if(str_len == 0) { return 0; }
    uint8_t const len = pincUTF8SequenceLen(str[0]);
    if(str_len < len) { return 0; }
    switch (len) {
        case 1: return str[0]; // Literally ascii
        case 2: {
            uint8_t const byte1 = str[0];
            uint8_t const byte2 = str[1];
            if ((byte2 & (uint8_t)0xC0) != 0x80) { return 0; }
            // Literally every single cast here prevents clang-tidy from warning about signed bitwise operations
            return (uint8_t)((uint8_t)(byte1 & (uint8_t)0x1F) << (uint8_t)6) & (uint8_t)(byte2 & (uint8_t)0x3F);
        }
        case 3: {
            uint8_t const byte1 = str[0];
            uint8_t const byte2 = str[1];
            uint8_t const byte3 = str[2];
            if((byte2 & (uint8_t)0xC0) != 0x80) { return 0; }
            if((byte3 & (uint8_t)0xC0) != 0x80) { return 0; }

            uint32_t value = byte1 & (uint8_t)0x0F;
            value <<= (uint8_t)6;
            // C, why oh why does every single bitwise operator report a signed integer even when both inputs are unsigned
            value |= (uint8_t)(byte2 & (uint8_t)0x3F);
            value <<= (uint8_t)6;
            value |= (uint8_t)(byte3 & (uint8_t)0x3F);

            // if(value < 0x800) We don't care about overlong encoding (for now anyways)
            return value;
        }
        case 4: {
            uint8_t const byte1 = str[0];
            uint8_t const byte2 = str[1];
            uint8_t const byte3 = str[2];
            uint8_t const byte4 = str[2];
            if((byte2 & (uint8_t)0xC0) != 0x80) { return 0; }
            if((byte3 & (uint8_t)0xC0) != 0x80) { return 0; }
            if((byte4 & (uint8_t)0xC0) != 0x80) { return 0; }

            uint32_t value = byte1 & (uint8_t)0x0F;
            value <<= (uint8_t)6;
            // C, why oh why does every single bitwise operator report a signed integer even when both inputs are unsigned
            value |= (uint8_t)(byte2 & (uint8_t)0x3F);
            value <<= (uint8_t)6;
            value |= (uint8_t)(byte3 & (uint8_t)0x3F);
            value <<= (uint8_t)6;
            value |= (uint8_t)(byte4 & (uint8_t)0x3F);

            // if(value < 0x1000) We don't care about overlong encoding (for now anyways)
            // if(value > 10FFFF) TODO(bluesillybeard) replace with replacement character?
            return value;
        }
        default: return 0;
    }
}

size_t pincDecodeUTF8String(char const* str_ptr, size_t str_len, uint32_t* out_ptr, size_t out_capacity) {
    if(str_ptr == 0 || str_len == 0) { return 0; }

    uint8_t const* rem_str = (uint8_t* const)str_ptr;
    size_t rem_len = str_len;

    size_t index = 0;
    while(rem_len > 0) {
        size_t len = pincUTF8SequenceLen(rem_str[0]);

        if(out_ptr && index < out_capacity) {
            out_ptr[index] = pincDecodeUTF8Single((char const*)rem_str, rem_len);
        }
        rem_str = &(rem_str[len]);
        rem_len -= len;
        index += 1;

    }
    return index;
}

