#include "pinc_string.h"
#include "libs/pinc_allocator.h"
#include "platform/pinc_platform.h"

#include <pinc_error.h>

pincString pincString_makeDirect(char* str) {
    if(!str) {
        return (pincString) {.str = 0, .len = 0};
    }
    return (pincString) {
        .str = (uint8_t*)str,
        .len = pincStringLen(str),
    };
}

pincString pincString_makeAlloc(char const* str, pincAllocator alloc) {
    size_t len = pincStringLen(str);
    uint8_t* newStr = pincAllocator_allocate(alloc, len);
    pincMemCopy(str, newStr, len);
    return (pincString) {
        .str = newStr,
        .len = len,
    };
}

pincString pincString_copy(pincString const str, pincAllocator alloc) {
    uint8_t* newStr = pincAllocator_allocate(alloc, str.len);
    pincMemCopy(str.str, newStr, str.len);
    return (pincString) {
        .str = newStr,
        .len = str.len,
    };
}

char* pincString_marshalAlloc(pincString str, pincAllocator alloc) {
    // If the length is zero, we can't return a pointer to a zero byte
    // as the user explicitly called for it to be allocated on alloc.
    char* newStr = pincAllocator_allocate(alloc, str.len+1);
    pincMemCopy(str.str, newStr, str.len);
    newStr[str.len] = 0;
    return newStr;
}

void pincString_marshalDirect(pincString str, char* dest, size_t capacity) {
    if(capacity == 0) {
        return;   
    }
    if(capacity > str.len) {
        capacity = str.len;
    }
    pincMemCopy(str.str, dest, capacity);
    dest[capacity] = 0;
}

pincString pincString_slice(pincString str, size_t start, size_t len) {
    PincAssertAssert(str.len > start, "Invalid slice operation: start index is out of bounds", true, return (pincString){0, 0};);
    PincAssertAssert(str.len >= len+start, "Invalid slice operation: slice extends beyond end of source string", true, return (pincString){0, 0};);
    return (pincString) {
        .str = str.str+start,
        .len = len,
    };
}

void pincString_free(pincString* str, pincAllocator alloc) {
    pincAllocator_free(alloc, str->str, str->len);
    str->str = 0;
    str->len = 0;
}

pincString pincString_concat(size_t numStrings, pincString strings[], pincAllocator alloc) {
    size_t totalLen = 0;
    for(size_t index = 0; index < numStrings; ++index) {
        totalLen += strings[index].len;
    }
    pincString newStr = (pincString) {
        .len = totalLen,
        .str = pincAllocator_allocate(alloc, totalLen),
    };
    uint8_t* writePtr = newStr.str;
    for(size_t index = 0; index < numStrings; ++index) {
        pincString str1 = strings[index];
        if(str1.len == 0) {continue;}
        if(str1.str == 0) {continue;}
        pincMemCopy(str1.str, writePtr, str1.len);
        writePtr += str1.len;
    }
    return newStr;
}

pincString pincString_allocFormatUint32(uint32_t item, pincAllocator alloc) {
    char buffer[11];
    size_t len = pincBufPrintUint32(buffer, 11, item);
    pincString new;
    new.str = pincAllocator_allocate(alloc, len);
    pincMemCopy(buffer, new.str, len);
    PincAssertAssert(len, "Zero length string", true, return (pincString){0, 0};);
    new.len = len;
    return new;
}

pincString pincString_allocFormatInt32(int32_t value, pincAllocator alloc) {
    if(value == 0) {
        return pincString_makeAlloc("0", alloc);
    }
    bool sign = (value < 0);
    if(value < 0) { value = -value; }
    char buf[11] = "abcdefghijk";
    size_t index = 0;
    while(value > 0) {
        uint32_t decimal = (uint32_t)value % 10;
        value = value / 10;
        char character = (char)(decimal+0x30);
        index += 1;
        buf[11-index] = character;
    }
    if(sign) {
        index += 1;
        buf[11 - index] = '-';
    }
    PincAssertAssert(index <= 11, "", true, return (pincString){0, 0};);
    uint8_t* str = pincAllocator_allocate(alloc, index);
    pincMemCopy(&buf[11-index], str, index);
    return (pincString){
        .str = str,
        .len = index,
    };
}

pincString pincString_allocFormatUint64(uint64_t item, pincAllocator alloc) {
    char buffer[21];
    size_t len = pincBufPrintUint64(buffer, 21, item);
    pincString new;
    new.str = pincAllocator_allocate(alloc, len);
    pincMemCopy(buffer, new.str, len);
    PincAssertAssert(len, "Zero length string", true, return (pincString){0, 0};);
    new.len = len-1;
    return new;
}
