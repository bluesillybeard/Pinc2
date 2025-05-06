// TODO: do all cross compilers have win32 headers?
// Tested compilers:
// - x86_64-gcc-w64-mingw32

// I love just having a single header with the entire API from heap allocation to writing to the console to opening a window to drawing graphics
// It's super cool and awesome and totally not weird or outdated or anything
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "platform.h"

// TODO: does it make any sense to expose this to the user?
static HANDLE pincHeap = NULL;

void* pAlloc(size_t bytes) {
    if(!pincHeap) {
        // TODO: should we use getProcessHeap instead?
        pincHeap = HeapCreate(0, 0, 0);
    }
    return HeapAlloc(pincHeap, 0, bytes);
}

void* pAllocAligned(size_t bytes, size_t alignment) {
    if(!pincHeap) {
        pincHeap = HeapCreate(0, 0, 0);
    }
    (void)bytes;
    (void)alignment;
    pPrintError((uint8_t*)"Aligned allocations are not implemented on Windows", sizeof("Aligned allocations are not implemented on Windows"));
    pAssertFail();
    return NULL;
}

void* pRealloc(void* pointer, size_t oldSize, size_t newSize) {
    // TODO: assert that oldSize matches the size of the allocation?
    (void)oldSize;
    return HeapReAlloc(pincHeap, 0, pointer, newSize);
}

void pFree(void* pointer, size_t bytes) {
    // TODO: assert that bytes matches the size of the allocation?
    (void)bytes;
    HeapFree(pincHeap, 0, pointer);
}

void* pLoadLibrary(uint8_t const* nameUtf8, size_t nameSize) {
    char* nameNullTerm = pAlloc(nameSize);
    pMemCopy(nameUtf8, nameNullTerm, nameSize+1);
    nameNullTerm[nameSize] = 0;
    // TODO: handle utf8?
    HMODULE lib = LoadLibraryA(nameNullTerm);
    // TODO: handle when STRICT is defined, as that changes the type definition of HMODULE
    return (void*) lib;
}

PFN pLibrarySymbol(void* library, uint8_t const* symbolNameUtf8, size_t nameSize) {
    char* nameNullTerm = pAlloc(nameSize);
    pMemCopy(symbolNameUtf8, nameNullTerm, nameSize+1);
    nameNullTerm[nameSize] = 0;
    return (PFN) GetProcAddress((HMODULE)library, nameNullTerm);
}

void pUnloadLibrary(void* library) {
    FreeLibrary((HMODULE)library);
    // TODO: getLastError and trigger debugger
}

size_t pStringLen(char const* str) {
    // It appears we have no win32 equivalent for strlen
    // TODO: properly optimize this, assuming the compiler doesn't do that already
    size_t l=0;
    while(str[l] != 0) {
        ++l;
    }
    return l;
}

void pMemCopy(void const* P_RESTRICT source, void* P_RESTRICT destination, size_t numBytes) {
    // It seems GCC specifically makes this just a macro to memcpy
    CopyMemory(destination, source, numBytes);
}

void pMemMove(void const* source, void* destination, size_t numBytes) {
    // It seems GCC specifically makes this just a macro to memmove
    MoveMemory(destination, source, numBytes);
}

void pMemSet(uint8_t value, void* destination, size_t numBytes) {
    // It seems win32 has no equivalent for memset
    // TODO: optimize this
    uint8_t* P_RESTRICT destBytes = destination;
    for(size_t i=0; i<numBytes; ++i) {
        destBytes[i] = value;
    }
}

// TODO: unicode functions
// Arguably these go in their own header, as UTF8 and unicode should not be platform specific
uint32_t pUtf8Unicode(uint8_t const* str, size_t strLen, size_t* outLen);
size_t pUnicodeUtf8(uint32_t codepoint, char* destStr);
size_t pUtf8UnicodeString(uint8_t const* str, size_t strLen, uint32_t* outUnicode);
size_t pUnicodeUtf8String(uint32_t const* codepoints, size_t numCodepoints, uint8_t* outUtf8);

void pTriggerDebugger(void) {
    // Windows W right here, doing this reliably on posix is rather annoying
    DebugBreak();
}

P_NORETURN void pAssertFail(void) {
    // TODO: is this the correct way to do this?
    DebugBreak();
    ExitProcess(255);
}

void pPrintError(uint8_t const* message, size_t len) {
    // TODO: utf8
    HANDLE console = GetStdHandle(STD_ERROR_HANDLE);
    DWORD charsWritten;
    // Uh oh, what is the max value of DWORD?
    // TODO: handle the potential integer overflow of casting size_t to DWORD
    WriteConsoleA(console, message, (DWORD)len, &charsWritten, 0);
}

void pPrintErrorLine(uint8_t const* message, size_t len) {
    pPrintError(message, len);
    pPrintError("\n", 1);
}

void pPrintDebugLine(uint8_t const* message, size_t len) {
    pPrintDebug(message, len);
    pPrintDebug("\n", 1);
}

void pPrintDebug(uint8_t const* message, size_t len) {
    // TODO: utf8
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD charsWritten;
    // Uh oh, what is the max value of DWORD?
    // TODO: handle the potential integer overflow of casting size_t to DWORD
    WriteConsoleA(console, message, (DWORD)len, &charsWritten, 0);
}

size_t pBufPrintUint32(char* buf, size_t capacity, uint32_t v) {
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
        pMemCopy(&realbuf[10-i], buf, num);
        if(num > 0){
            buf[num-1] = 0;
        }
    }
    return i+1;
}

int64_t pCurrentTimeMillis(void) {
    // This is *probably* good enough, in the accuracy of 10-20 ms
    // TODO: However, it is almost certainly a good idea to get something that is accurate to 1-2 ms
    ULONGLONG ticks = GetTickCount64();
    if(ticks > INT64_MAX) {
        pPrintError((uint8_t*)"Integer Overflow", sizeof("Integer Overflow"));
        pAssertFail();
        return 0;
    }
    return (int64_t)ticks;
}
