// Implementation of platform.h for posix systems.

#include "pinc_platform.h"

// If you are here because you encountered include errors, use a compiler toolchain with the libc and posix headers

#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void* pincAlloc(size_t bytes) {
    return malloc(bytes);
}

void* pincAllocAligned(size_t bytes, size_t alignment) {
    // aligned_alloc is C11 or newer. We want to be compatible with C99 if doable.
    // Note: Pinc is purely compiled with C99, so this will never be true (for now)
    #if(__STDC_VERSION__ >= 201112L)
    return aligned_alloc(alignment, bytes);
    #else
    P_UNUSED(bytes);
    P_UNUSED(alignment);
    // Aligned alloc is not implemented for older than C11....
    // Oh well. Pinc is unlikely to use this. In other words: only bother fixing this if an error occurs.
    pincAssertFail();
    return 0;
    #endif
}

void* pincRealloc(void* pointer, size_t oldSize, size_t newSize) {
    P_UNUSED(oldSize);
    return realloc(pointer, newSize);
}

void pincFree(void* pointer, size_t bytes) {
    P_UNUSED(bytes);
    free(pointer);
}

void* pincLoadLibrary(uint8_t const* nameUtf8, size_t nameSize) {
    // Cannot assume name is null terminated, which is what dlopen needs
    // The input name also does not contain the file ending or lib prefix - only the name of the library (ex: "sdl2")
    uint8_t* nameUtf8NullTerm = pincAlloc(nameSize+7);
    pincMemCopy("lib", nameUtf8NullTerm, 3);
    pincMemCopy(nameUtf8, nameUtf8NullTerm+3, nameSize);
    pincMemCopy(".so\0", nameUtf8NullTerm+3+nameSize, 4);
    // TODO: does dlOpen accept utf8?
    void* lib = dlopen((char*)nameUtf8NullTerm, RTLD_LAZY);
    pincFree(nameUtf8NullTerm, nameSize+1);
    return lib;
    // TODO: Since this is on unix, there may be other things (ex: libsdl.so.2)
    // Probably best to change the function signature to specify a version of the library to load.
    // TODO: there will eventually be options to have Pinc link with libraries statically or with the normal OS linker instead of at runtime
}

pincPFN pincLibrarySymbol(void* library, uint8_t const* symbolNameUtf8, size_t nameSize) {
    // Cannot assume name is null terminated, which is what dlsym needs
    uint8_t* nameUtf8NullTerm = pincAlloc(nameSize+1);
    pincMemCopy(symbolNameUtf8, nameUtf8NullTerm, nameSize);
    nameUtf8NullTerm[nameSize] = 0;

    void* sym = dlsym(library, (char*)nameUtf8NullTerm);
    pincFree(nameUtf8NullTerm, nameSize+1);
    // GCC's -Wpedantic is extra strict and does not allow a void* to be cast into a function pointer.
    // This is an artifact of the ancient world back when a function pointer wasn't necessarily the same size as a pointer to memory.
    // (side not about wasm or function pointer sanitization or whatever)
    // It appears POSIX guarantees that fact, seeing as dlsym returns void* instead of a void function pointer.
    // Anyway, they gave us a nice way to disable that warning using __extension__.
    #ifdef __GNUC__
    return __extension__ (pincPFN)sym;
    #else
    return (PFN)sym;
    #endif
}

void pincUnloadLibrary(void* library) {
    dlclose(library);
}

size_t pincStringLen(char const* str) {
    return strlen(str);
}

void pincMemCopy(void const* restrict source, void* restrict destination, size_t numBytes) {
    memcpy(destination, source, numBytes);
}

void pincMemMove(void const* source, void* destination, size_t numBytes) {
    memmove(destination, source, numBytes);
}

void pincMemSet(uint8_t value, void* destination, size_t numBytes) {
    memset(destination, value, numBytes);
}

// TODO
uint32_t pincUtf8Unicode(uint8_t const* restrict str, size_t strLen, size_t* restrict outLen);

// TODO
size_t pincUnicodeUtf8(uint32_t codepoint, char* destStr);

// TODO
size_t pincUtf8UnicodeString(uint8_t const* restrict str, size_t strLen, uint32_t* restrict outUnicode);

// TODO
size_t pincUnicodeUtf8String(uint32_t const* restrict codepoints, size_t numCodepoints, uint8_t* restrict outUtf8);

// Source: https://github.com/scottt/debugbreak/blob/master/debugbreak.h
// Not a copy, but this has the parts needed from it
// We don't need any of the windows stuff, or the C++ stuff... Or most of it, actually.

void pincTriggerDebugger(void) {
    // TODO: does it make sense to make this a Pinc option? I don't think so.
    #if !defined(DEBUG) && !defined(NDEBUG)
    #if defined(__i386__) || defined(__x86_64__)
    // x86
    __asm__ volatile("int $0x03");
    #elif defined(__thumb__)
    // arm short instructions
    // Apparently this can cause issues in some debuggers - can't guarantee you will be able to continue debugging after this is called
    __asm__ volatile(".inst 0xde01");
    #elif defined(__arm__)
    // arm normal instructions
    // Apparently this can cause issues in some debuggers - can't guarantee you will be able to continue debugging after this is called
    __asm__ volatile(".inst 0xe7f001f0");
    #elif defined(__arch64__)
    // 64 bit arm
    // TODO: Apparently this is not the best way on macos? Really, macos should have it's own platform implementation
    __asm__ volatile(".inst 0xd4200000");
    #elif defined(__powerpc__)
    // powerpc
    // Apparently this can cause issues in some debuggers - can't guarantee you will be able to continue debugging after this is called
    __asm__ volatile(".4byte 0x7d821008");
    #elif defined(__riscv)
    // riscv being different and only having underscores at the beginning
    __asm__ volatile(".4byte 0x00100073");
    #else
    // Oops, we don't know what this is. Assert instead.
    pincAssertFail();
    #endif
    #endif
}

// Guys, seriously. WHY does the C standard have assert(). BUT NOT ASSERT FAIL. WHAT IS THIS.
// Thankfully, this here handy function is baked into the posix standard - so it is safe to use.

/* This prints an "Assertion failed" message and aborts.  */
extern void __assert_fail (const char *__assertion, const char *__file,
			   unsigned int __line, const char *__function)
     __THROW __attribute__ ((__noreturn__));

P_NORETURN void pincAssertFail(void) {
    __assert_fail("Pinc platform assert", __FILE__, __LINE__, __func__);
}

void pincPrintError(uint8_t const* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stderr);
}

void pincPrintDebug(uint8_t const* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stdout);
}

void pincPrintErrorLine(uint8_t const* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stderr);
    fputc('\n', stderr);
}

void pincPrintDebugLine(uint8_t const* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stdout);
    fputc('\n', stdout);
}

#include <inttypes.h>

size_t pincBufPrintUint32(char* buf, size_t capacity, uint32_t v) {
    if(capacity == 1 && buf) buf[0] = 0;
    if(v == 0) {
        if(capacity >= 2) {
            if(buf) (buf[0] = '0');
            if(buf) (buf[1] = 0);
        }
        return 1;
    }
    // uint32_t can be at most 10 decimal digits
    // One more for null terminator
    char realbuf[11] = { 0 };
    // Assuming ASCII or something similar
    size_t i = 0;
    while(v > 0) {
        uint32_t d = v % 10;
        v = v / 10;
        char c = (char)(d+0x30);
        i += 1;
        realbuf[10-i] = c;
    }
    // Bro why are there so many edge cases like wth
    if(buf){
        size_t num = i+1;
        if(num > capacity) {
            num = capacity;
        }
        pincMemCopy(&realbuf[10-i], buf, num);
        if(num > 0){
            buf[num-1] = 0;
        }
    }
    return i;
}

size_t pincBufPrintUint64(char* buf, size_t capacity, uint64_t v) {
    if(capacity == 1 && buf) buf[0] = 0;
    if(v == 0) {
        if(capacity >= 2) {
            if(buf) (buf[0] = '0');
            if(buf) (buf[1] = 0);
        }
        return 1;
    }
    // uint64_t can be at most 20 decimal digits
    // One more for null terminator
    char realbuf[21] = { 0 };
    // Assuming ASCII or something similar
    size_t i = 0;
    while(v > 0) {
        uint64_t d = v % 10;
        v = v / 10;
        char c = (char)(d+0x30);
        i += 1;
        realbuf[20-i] = c;
    }
    // Bro why are there so many edge cases like wth
    if(buf){
        size_t num = i+1;
        if(num > capacity) {
            num = capacity;
        }
        pincMemCopy(&realbuf[20-i], buf, num);
        if(num > 0){
            buf[num-1] = 0;
        }
    }
    return i;
}

#include <time.h>

int64_t pincCurrentTimeMillis(void) {
    // This is in the posix standard, so it should be extremely well supported
    struct timespec theTime;
    clock_gettime(CLOCK_MONOTONIC, &theTime);
    return theTime.tv_sec * 1000 + theTime.tv_nsec / 1000000;
}
