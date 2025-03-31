// Basic strings in C - a header only library, at least for now since all of the functions are super simple.
// This is because doing char* to a null terminated string is kinda just bad,
// and having to have a length variable with every char* is just annoying.

#ifndef P_STRING_H
#define P_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <platform/platform.h>
#include "dynamic_allocator.h"

/// @brief A basic string type. This is a basic C string, but there is no null terminator, and the length is held in the struct.
typedef struct PString {
    uint8_t* str;
    size_t len;
} PString;

static P_INLINE PString PString_makeDirect(char* str) {
    return (PString) {
        .str = (uint8_t*)str,
        .len = pStringLen(str),
    };
}

static P_INLINE PString PString_makeAlloc(char const* str, Allocator alloc) {
    size_t len = pStringLen(str);
    uint8_t* newStr = Allocator_allocate(alloc, len);
    pMemCopy(str, newStr, len);
    return (PString) {
        .str = newStr,
        .len = len,
    };
}

static P_INLINE PString PString_copy(PString const str, Allocator alloc) {
    uint8_t* newStr = Allocator_allocate(alloc, str.len);
    pMemCopy(str.str, newStr, str.len);
    return (PString) {
        .str = newStr,
        .len = str.len,
    };
}

// Makes a new traditional C string from a PString.
// Returns null if the given string is null
static P_INLINE char* PString_marshalAlloc(PString str, Allocator alloc) {
    // If the length is zero, we can't return a pointer to a zero byte
    // as the user explicitly called for it to be allocated on alloc.
    char* newStr = Allocator_allocate(alloc, str.len+1);
    pMemCopy(str.str, newStr, str.len);
    newStr[str.len] = 0;
    return newStr;
}

// Marshals str into dest, where dest has capacity characters it can hold (including null terminator)
static P_INLINE void PString_marshalDirect(PString str, char* dest, size_t capacity) {
    if(capacity == 0) return;
    if(capacity > str.len) {
        capacity = str.len;
    }
    pMemCopy(str.str, dest, capacity);
    dest[capacity] = 0;
}

static P_INLINE PString PString_slice(PString str, size_t start, size_t len) {
    // TODO: make these asserts nicer. Maybe integrate them with Pinc's error system with macros or something
    if(str.len <= start) pAssertFail(); // "Slice invalid: start index is out of bounds"
    if(str.len < len+start) pAssertFail(); // "Slice invalid: slice extends beyond end of source string"
    return (PString) {
        .str = str.str+start,
        .len = len,
    };
}

static P_INLINE void PString_free(PString* str, Allocator alloc) {
    Allocator_free(alloc, str->str, str->len);
    str->str = 0;
    str->len = 0;
}

/// @brief Concatenate multiple strings together
/// @param numStrings the number of strings in the array to concatenate together 
/// @param strings the strings to concatenate
/// @param alloc The allocator that the new string will be allocated on. This function is guaranteed to only make a single allocation.
/// @return the result of the concatenation, allocated with alloc.
static P_INLINE PString PString_concat(size_t numStrings, PString strings[], Allocator alloc) {
    size_t totalLen = 0;
    for(size_t index = 0; index < numStrings; ++index) {
        totalLen += strings[index].len;
    }
    PString newStr = (PString) {
        .len = totalLen,
        .str = Allocator_allocate(alloc, totalLen),
    };
    uint8_t* writePtr = newStr.str;
    for(size_t index = 0; index < numStrings; ++index) {
        PString str1 = strings[index];
        if(str1.len == 0) continue;
        pMemCopy(str1.str, writePtr, str1.len);
        writePtr += str1.len;
    }
    return newStr;
}

#endif
