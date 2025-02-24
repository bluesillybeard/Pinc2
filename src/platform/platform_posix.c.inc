// Implementation of platform.h for posix systems.

#include "platform.h"

#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void* pAlloc(size_t bytes) {
    return malloc(bytes);
}

void* pAllocAligned(size_t bytes, size_t alignment) {
    // aligned_alloc is C11 or newer. We want to be compatible with C99 if doable.
    // Note: Pinc is purely compiled with C99, so this will never be true (for now)
    #if(__STDC_VERSION__ >= 201112L)
    return aligned_alloc(alignment, bytes);
    #else
    P_UNUSED(bytes);
    P_UNUSED(alignment);
    // Aligned alloc is not implemented for older than C11....
    // Oh well. Pinc is unlikely to use this. In other words: only bother fixing this if an error occurs.
    pAssertFail();
    return 0;
    #endif
}

void* pRealloc(void* pointer, size_t oldSize, size_t newSize) {
    P_UNUSED(oldSize);
    return realloc(pointer, newSize);
}

void pFree(void* pointer, size_t bytes) {
    P_UNUSED(bytes);
    free(pointer);
}

void* pLoadLibrary(uint8_t const* nameUtf8, size_t nameSize) {
    // Cannot assume name is null terminated, which is what dlopen needs
    // The input name also does not contain the file ending or lib prefix - only the name of the library (ex: "sdl2")
    uint8_t* nameUtf8NullTerm = pAlloc(nameSize+7);
    pMemCopy("lib", nameUtf8NullTerm, 3);
    pMemCopy(nameUtf8, nameUtf8NullTerm+3, nameSize);
    pMemCopy(".so\0", nameUtf8NullTerm+3+nameSize, 4);
    // TODO: does dlOpen accept utf8?
    void* lib = dlopen((char*)nameUtf8NullTerm, RTLD_LAZY);
    pFree(nameUtf8NullTerm, nameSize+1);
    return lib;
    // TODO: Since this is on unix, there may be other things (ex: libsdl.so.2)
    // Probably best to change the function signature to specify a version of the library to load.
    // TODO: there will eventually be options to have Pinc link with libraries statically or with the normal OS linker instead of at runtime
}

void* pLibrarySymbol(void* library, uint8_t* symbolNameUtf8, size_t nameSize) {
    // Cannot assume name is null terminated, which is what dlsym needs
    uint8_t* nameUtf8NullTerm = pAlloc(nameSize+1);
    pMemCopy(symbolNameUtf8, nameUtf8NullTerm, nameSize);
    nameUtf8NullTerm[nameSize] = 0;
    void* sym = dlsym(library, (char*)nameUtf8NullTerm);
    pFree(nameUtf8NullTerm, nameSize+1);
    return sym;
}

void pUnloadLibrary(void* library) {
    dlclose(library);
}

size_t pStringLen(char const* str) {
    return strlen(str);
}

void pMemCopy(void const* restrict source, void* restrict destination, size_t numBytes) {
    memcpy(destination, source, numBytes);
}

void pMemMove(void const* source, void* destination, size_t numBytes) {
    memmove(destination, source, numBytes);
}

void pMemSet(uint8_t value, void* destination, size_t numBytes) {
    memset(destination, value, numBytes);
}

// TODO
uint32_t pUtf8Unicode(uint8_t const* restrict str, size_t strLen, size_t* restrict outLen);

// TODO
size_t pUnicodeUtf8(uint32_t codepoint, char* destStr);

// TODO
size_t pUtf8UnicodeString(uint8_t const* restrict str, size_t strLen, uint32_t* restrict outUnicode);

// TODO
size_t pUnicodeUtf8String(uint32_t const* restrict codepoints, size_t numCodepoints, uint8_t* restrict outUtf8);

// Source: https://github.com/scottt/debugbreak/blob/master/debugbreak.h
// Not a copy, but this has the parts needed from it
// We don't need any of the windows stuff, or the C++ stuff... Or most of it, actually.

void pTriggerDebugger(void) {
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
    pAssertFail();
    #endif
    #endif
}

// Guys, seriously. WHY does the C standard have assert(). BUT NOT ASSERT FAIL. WHAT IS THIS.
// Thankfully, this here handy function is baked into the posix standard - so it is safe to use.

/* This prints an "Assertion failed" message and aborts.  */
extern void __assert_fail (const char *__assertion, const char *__file,
			   unsigned int __line, const char *__function)
     __THROW __attribute__ ((__noreturn__));

P_NORETURN void pAssertFail(void) {
    __assert_fail("Pinc platform assert", __FILE__, __LINE__, __func__);
}

void pPrintError(uint8_t const* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stderr);
}

void pPrintDebug(uint8_t const* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stdout);
}

void pPrintFormat(char const* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#include <inttypes.h>

size_t pBufPrintUint32(char* buf, size_t capacity, uint32_t v) {
    int res = snprintf(buf, capacity, "%" PRIu32, v);
    if(res < 0) pAssertFail();
    return (size_t)res;
}
