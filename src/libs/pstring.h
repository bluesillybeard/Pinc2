// A quick string management thingy.
// This is because doing char* to a null terminated string is kinda just bad,
// and having to have a length variable with every char* is just annoying.
// This also provides a number of string manipulation functions that are independent of (and, in my opinion, better than) libc.

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

PString PString_makeDirect(char* str);

PString PString_makeAlloc(char const* str, Allocator alloc);

PString PString_copy(PString const str, Allocator alloc);

/// Makes a new traditional C string from a PString.
/// Returns null if the given string is null.
char* PString_marshalAlloc(PString str, Allocator alloc);

/// Marshals str into dest, where dest has capacity characters it can hold (including null terminator)
void PString_marshalDirect(PString str, char* dest, size_t capacity);

PString PString_slice(PString str, size_t start, size_t len);

void PString_free(PString* str, Allocator alloc);

/// @brief Concatenate multiple strings together
/// @param numStrings the number of strings in the array to concatenate together 
/// @param strings the strings to concatenate
/// @param alloc The allocator that the new string will be allocated on. This function is guaranteed to only make a single allocation.
/// @return the result of the concatenation, allocated with alloc.
PString PString_concat(size_t numStrings, PString strings[], Allocator alloc);

PString PString_allocFormatUint32(uint32_t item, Allocator alloc);

#endif
