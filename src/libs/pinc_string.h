// A quick string management thingy.
// This is because doing char* to a null terminated string is kinda just bad,
// and having to have a length variable with every char* is just annoying.
// This also provides a number of string manipulation functions that are independent of (and, in my opinion, better than) libc.

#ifndef P_STRING_H
#define P_STRING_H

#include "pinc_allocator.h"
#include <platform/pinc_platform.h>

/// @brief A basic string type. This is a basic C string, but there is no null terminator, and the length is held in the struct.
typedef struct {
    uint8_t* str;
    size_t len;
} PincString;

PincString pincString_makeDirect(char* str);

PincString pincString_makeAlloc(char const* str, pincAllocator alloc);

PincString pincString_copy(PincString str, pincAllocator alloc);

/// Makes a new traditional C string from a PString.
/// Returns null if the given string is null.
char* pincString_marshalAlloc(PincString str, pincAllocator alloc);

/// Marshals str into dest, where dest has capacity characters it can hold (including null terminator)
void pincString_marshalDirect(PincString str, char* dest, size_t capacity);

PincString pincString_slice(PincString str, size_t start, size_t len);

void pincString_free(PincString* str, pincAllocator alloc);

/// @brief Concatenate multiple strings together
/// @param numStrings the number of strings in the array to concatenate together 
/// @param strings the strings to concatenate
/// @param alloc The allocator that the new string will be allocated on. This function is guaranteed to only make a single allocation.
/// @return the result of the concatenation, allocated with alloc.
PincString pincString_concat(size_t numStrings, PincString strings[], pincAllocator alloc);

PincString pincString_allocFormatUint32(uint32_t item, pincAllocator alloc);

PincString pincString_allocFormatInt32(int32_t value, pincAllocator alloc);

PincString pincString_allocFormatUint64(uint64_t item, pincAllocator alloc);

#endif
