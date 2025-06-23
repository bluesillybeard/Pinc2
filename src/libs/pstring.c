#include "pstring.h"

#include <pinc_error.h>

PString PString_makeDirect(char* str) {
    if(!str) return (PString) {.str = 0, .len = 0};
    return (PString) {
        .str = (uint8_t*)str,
        .len = pStringLen(str),
    };
}

PString PString_makeAlloc(char const* str, Allocator alloc) {
    size_t len = pStringLen(str);
    uint8_t* newStr = Allocator_allocate(alloc, len);
    pMemCopy(str, newStr, len);
    return (PString) {
        .str = newStr,
        .len = len,
    };
}

PString PString_copy(PString const str, Allocator alloc) {
    uint8_t* newStr = Allocator_allocate(alloc, str.len);
    pMemCopy(str.str, newStr, str.len);
    return (PString) {
        .str = newStr,
        .len = str.len,
    };
}

char* PString_marshalAlloc(PString str, Allocator alloc) {
    // If the length is zero, we can't return a pointer to a zero byte
    // as the user explicitly called for it to be allocated on alloc.
    char* newStr = Allocator_allocate(alloc, str.len+1);
    pMemCopy(str.str, newStr, str.len);
    newStr[str.len] = 0;
    return newStr;
}

void PString_marshalDirect(PString str, char* dest, size_t capacity) {
    if(capacity == 0) return;
    if(capacity > str.len) {
        capacity = str.len;
    }
    pMemCopy(str.str, dest, capacity);
    dest[capacity] = 0;
}

PString PString_slice(PString str, size_t start, size_t len) {
    PErrorAssert(str.len > start, "Invalid slice operation: start index is out of bounds");
    PErrorAssert(str.len >= len+start, "Invalid slice operation: slice extends beyond end of source string");
    return (PString) {
        .str = str.str+start,
        .len = len,
    };
}

void PString_free(PString* str, Allocator alloc) {
    Allocator_free(alloc, str->str, str->len);
    str->str = 0;
    str->len = 0;
}

PString PString_concat(size_t numStrings, PString strings[], Allocator alloc) {
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
        if(str1.str == 0) continue;
        pMemCopy(str1.str, writePtr, str1.len);
        writePtr += str1.len;
    }
    return newStr;
}

PString PString_allocFormatUint32(uint32_t item, Allocator alloc) {
    char buffer[11];
    size_t len = pBufPrintUint32(buffer, 11, item);
    PString new;
    new.str = Allocator_allocate(alloc, len);
    pMemCopy(buffer, new.str, len);
    PErrorAssert(len, "Zero length string");
    new.len = len;
    return new;
}

PString PString_allocFormatUint64(uint64_t item, Allocator alloc) {
    char buffer[21];
    size_t len = pBufPrintUint64(buffer, 21, item);
    PString new;
    new.str = Allocator_allocate(alloc, len);
    pMemCopy(buffer, new.str, len);
    PErrorAssert(len, "Zero length string");
    new.len = len-1;
    return new;
}
