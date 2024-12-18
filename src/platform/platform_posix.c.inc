// Implementation of platform.h for posix systems.

#include "platform.h"

#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

void* pAlloc(size_t bytes) {
    return malloc(bytes);
}

void* pAllocAligned(size_t bytes, size_t alignment) {
    return aligned_alloc(alignment, bytes);
}

void* pRealloc(void* pointer, size_t oldSize, size_t newSize) {
    return realloc(pointer, newSize);
}

void pFree(void* pointer, size_t bytes) {
    free(pointer);
}

void* pLoadLibrary(uint8_t* nameUtf8, size_t nameSize) {
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

size_t pStringLen(char* str) {
    return strlen(str);
}

void pMemCopy(void* source, void* destination, size_t numBytes) {
    memcpy(destination, source, numBytes);
}

void pMemMove(void* source, void* destination, size_t numBytes) {
    memmove(destination, source, numBytes);
}

// TODO
uint32_t pUtf8Unicode(uint8_t* str, size_t strLen, size_t* outLen);

// TODO
size_t pUnicodeUtf8(uint32_t codepoint, char* destStr);

// TODO
size_t pUtf8UnicodeString(uint8_t* str, size_t strLen, uint32_t* outUnicode);

// TODO
size_t pUnicodeUtf8String(uint32_t* codepoints, size_t numCodepoints, uint8_t* outUtf8);

void pAssertFail(void) {
    // For now, use libc's assert
    assert(0);
}

void pPrintError(uint8_t* message, size_t len) {
    fwrite(message, sizeof(uint8_t), len, stderr);
}
