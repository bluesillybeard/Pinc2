#include "pinc_error.h"
#include <stdint.h>

// If you are here because you encountered include errors, either:
// 1. Set up Pinc's include directories properly within the build system
// 2. Use a compiler toolchain with the windows headers (We recommend MinGW + Clang or MinGW + GCC, but MSVC should work too)

// NOLINTBEGIN: Windows api headers are stupid and order-dependent
// Microsoft and their infinite wisdom thought putting literally everything in one header was the best idea ever.
// Here we are decades later and still dealing with the consequences instead of just fixing the problem.
// Gotta love Microsoft and their infinite wisdom.
// Do you have any idea how long it took so Pinc can be compiled with a toolchain that doesn't have the windows headers?
// Yes, there are Windows C compilers out there that don't have windows.h.
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
// NOLINTEND

#include "pinc_platform.h"

// TODO(bluesillybeard): does it make any sense to expose this to the user?
static void* pincHeap = NULL; //NOLINT

void* pincAlloc(size_t bytes) {
    if(!pincHeap) {
        // TODO(bluesillybeard): should we use getProcessHeap instead?
        pincHeap = HeapCreate(0, 0, 0);
    }
    return HeapAlloc(pincHeap, 0, bytes);
}

void* pincAllocAligned(size_t bytes, size_t alignment) {
    if(!pincHeap) {
        pincHeap = HeapCreate(0, 0, 0);
    }
    (void)bytes;
    (void)alignment;
    pincPrintError((uint8_t*)"Aligned allocations are not implemented on Windows", sizeof("Aligned allocations are not implemented on Windows"));
    pincAssertFail();
    return NULL;
}

void* pincRealloc(void* pointer, size_t oldSize, size_t newSize) {
    // TODO(bluesillybeard): assert that oldSize matches the size of the allocation?
    (void)oldSize;
    return HeapReAlloc(pincHeap, 0, pointer, newSize);
}

void pincFree(void* pointer, size_t bytes) {
    // TODO(bluesillybeard): assert that bytes matches the size of the allocation?
    (void)bytes;
    HeapFree(pincHeap, 0, pointer);
}

void* pincLoadLibrary(uint8_t const* nameUtf8, size_t nameSize) {
    char* nameNullTerm = pincAlloc(nameSize);
    pincMemCopy(nameUtf8, nameNullTerm, nameSize+1);
    nameNullTerm[nameSize] = 0;
    // TODO(bluesillybeard): handle utf8?
    void* lib = LoadLibraryA(nameNullTerm);
    return lib;
}

pincPFN pincLibrarySymbol(void* library, uint8_t const* symbolNameUtf8, size_t nameSize) {
    char* nameNullTerm = pincAlloc(nameSize);
    pincMemCopy(symbolNameUtf8, nameNullTerm, nameSize+1);
    nameNullTerm[nameSize] = 0;
    return (pincPFN) GetProcAddress(library, nameNullTerm);
}

void pincUnloadLibrary(void* library) {
    FreeLibrary(library);
    // TODO(bluesillybeard): getLastError and trigger debugger
}

size_t pincStringLen(char const* str) {
    // It appears we have no win32 equivalent for strlen
    // TODO(bluesillybeard): properly optimize this, assuming the compiler doesn't do that already
    size_t length=0;
    while(str[length] != 0) {
        ++length;
    }
    return length;
}

void pincMemCopy(void const* P_RESTRICT source, void* P_RESTRICT destination, size_t numBytes) {
    // CopyMemory is a macro to a macro to something that depends on the compiler
    // TODO(bluesillybeard): optimization
    uint8_t const* P_RESTRICT src = (uint8_t*) source;
    uint8_t* P_RESTRICT dst = (uint8_t*) destination;
    for(size_t i=0; i<numBytes; ++i) {
        dst[i] = src[i];
    }
}

void pincMemMove(void const* source, void* destination, size_t numBytes) {
    // MoveMemory is a macro to a macro to something that depends on the compiler
    // TODO(bluesillybeard): optimization, also I think this implementation is actually not correct either
    uint8_t* src = (uint8_t*) source;
    uint8_t* dst = (uint8_t*) destination;
    for(size_t i=0; i<numBytes; ++i) {
        dst[i] = src[i];
    }
}

void pincMemSet(uint8_t value, void* destination, size_t numBytes) {
    // It seems win32 has no equivalent for memset
    // TODO(bluesillybeard): optimize this
    uint8_t* P_RESTRICT destBytes = destination;
    for(size_t i=0; i<numBytes; ++i) {
        destBytes[i] = value;
    }
}

void pincTriggerDebugger(void) {
    // Windows W right here, doing this reliably on posix is rather annoying
    DebugBreak();
}

P_NORETURN void pincAssertFail(void) {
    // TODO(bluesillybeard): is this the correct way to do this?
    DebugBreak();
    ExitProcess(255);
}

void pincWin32Print(uint8_t const* message, size_t len, DWORD handle) {
    // TODO(bluesillybeard): utf8
    void* console = GetStdHandle(handle);
    DWORD charsWritten = 0;
    if(len > UINT32_MAX) {
        PincAssertAssert(false, "Integer Overflow", false, ;);
        return;
    }
    // What if I told you WriteConsoleA indirectly requires wingdi.h?
    // We aren't doing any window or graphics stuff here, in theory this is just an IPC channel to another process,
    // But here we are anyway. Isn't the windows SDK absolutely great and completely organized?
    // Not that Pinc has anything to say here, our entire windowing API is stuffed into a single header file.
    WriteConsoleA(console, message, (uint32_t)len, &charsWritten, 0);
}

void pincPrintError(uint8_t const* message, size_t len) {
    pincWin32Print(message, len, STD_ERROR_HANDLE);
}

void pincPrintErrorLine(uint8_t const* message, size_t len) {
    pincPrintError(message, len);
    pincPrintError((uint8_t const*)"\n", 1);
}

void pincPrintDebugLine(uint8_t const* message, size_t len) {
    pincPrintDebug(message, len);
    pincPrintDebug((uint8_t const*)"\n", 1);
}

void pincPrintDebug(uint8_t const* message, size_t len) {
    pincWin32Print(message, len, STD_OUTPUT_HANDLE);
}

// These print functions really need to get moved out of here
size_t pincBufPrintUint32(char* buf, size_t capacity, uint32_t value) {
    if(capacity == 1 && buf) { buf[0] = 0; }
    if(value == 0) {
        if(capacity >= 2) {
            if(buf) { buf[0] = '0'; }
            if(buf) { buf[1] = 0; }
        }
        return 1;
    }
    // uint32_t can be at most 10 decimal digits
    // One more for null terminator
    char realbuf[11] = { 0 };
    // Assuming ASCII or something similar
    size_t index = 0;
    while(value > 0) {
        uint32_t decimal = value % 10;
        value = value / 10;
        char character = (char)(decimal+0x30);
        index += 1;
        realbuf[10-index] = character;
    }
    // Bro why are there so many edge cases like wth
    if(buf){
        size_t num = index+1;
        if(num > capacity) {
            num = capacity;
        }
        pincMemCopy(&realbuf[10-index], buf, num);
        if(num > 0){
            buf[num-1] = 0;
        }
    }
    return index;
}

size_t pincBufPrintUint64(char* buf, size_t capacity, uint64_t value) {
    if(capacity == 1 && buf) { buf[0] = 0; }
    if(value == 0) {
        if(capacity >= 2) {
            if(buf) { buf[0] = '0'; }
            if(buf) { buf[1] = 0; }
        }
        return 1;
    }
    // uint64_t can be at most 20 decimal digits
    // One more for null terminator
    char realbuf[21] = { 0 };
    // Assuming ASCII or something similar
    size_t index = 0;
    while(value > 0) {
        uint64_t decimal = value % 10;
        value = value / 10;
        char character = (char)(decimal+0x30);
        index += 1;
        realbuf[20-index] = character;
    }
    // Bro why are there so many edge cases like wth
    if(buf){
        size_t num = index+1;
        if(num > capacity) {
            num = capacity;
        }
        pincMemCopy(&realbuf[20-index], buf, num);
        if(num > 0){
            buf[num-1] = 0;
        }
    }
    return index;
}

int64_t pincCurrentTimeMillis(void) {
    // This is *probably* good enough, in the accuracy of 10-20 ms
    // TODO(bluesillybeard): However, it is almost certainly a good idea to get something that is accurate to 1-2 ms
    uint64_t ticks = GetTickCount64();
    if(ticks > INT64_MAX) {
        pincPrintError((uint8_t*)"Integer Overflow", sizeof("Integer Overflow"));
        pincAssertFail();
        return 0;
    }
    return (int64_t)ticks;
}
