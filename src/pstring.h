// Basic strings in C - a header only library, at least for now since all of the functions are super simple.
// This is because doing char* to a null terminated string is kinda just bad,
// and having to have a length variable with every char* is just annoying.

#ifndef P_STRING_H
#define P_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <platform/platform.h>
#include <pinc_main.h>

/// @brief A basic string type. This is a basic C string, but there is no null terminator, and the length is held in the struct.
typedef struct PString {
    uint8_t* str;
    size_t len;
} PString;

static inline PString PString_makeDirect(char* str) {
    return (PString) {
        .str = (uint8_t*)str,
        .len = pStringLen(str),
    };
}

static inline PString PString_makeAlloc(char const* str, Allocator alloc) {
    size_t len = pStringLen(str);
    uint8_t* newStr = Allocator_allocate(alloc, len);
    pMemCopy(str, newStr, len);
    return (PString) {
        .str = newStr,
        .len = len,
    };
}

static inline PString PString_copy(PString const str, Allocator alloc) {
    uint8_t* newStr = Allocator_allocate(alloc, str.len);
    pMemCopy(str.str, newStr, str.len);
    return (PString) {
        .str = newStr,
        .len = str.len,
    };
}

// Makes a new traditional C string from a PString
static inline char* PString_marshalAlloc(PString str, Allocator alloc) {
    char* newStr = Allocator_allocate(alloc, str.len+1);
    pMemCopy(str.str, newStr, str.len);
    newStr[str.len] = 0;
    return newStr;
}

// Marshals str into dest, where dest has capacity characters it can hold (including null terminator)
static inline void PString_marshalDirect(PString str, char* dest, size_t capacity) {
    if(capacity > str.len) {
        capacity = str.len;
    }
    pMemCopy(str.str, dest, capacity);
    dest[capacity] = 0;
}

static inline PString PString_slice(PString str, size_t start, size_t len) {
    if(str.len <= start) PPANIC_NL("Slice invalid: start index is out of bounds");
    if(str.len < len+start) PPANIC_NL("Slice invalid: slice extends beyond end of source string");
    return (PString) {
        .str = str.str+start,
        .len = len,
    };
}

static inline void PString_free(PString* str, Allocator alloc) {
    Allocator_free(alloc, str->str, str->len);
    str->str = 0;
    str->len = 0;
}

#endif
