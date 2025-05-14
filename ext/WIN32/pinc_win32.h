#ifndef PINC_WIN32_H
#define PINC_WIN32_H

// see ../readme.md

// Navigating windows API types and macros can be a nightmare and a half
// Due to a history of x86-specific things, a lack of a desire to go through and fix things to be more clear, and the general age of the win32 API.
// So here's a little guide that may or may not be correct
// First, their official documentation which frequently disagrees with the mingw headers: https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types
// SIZE_T -> size_t
// SSIZE_T -> ssize_t
// LPVOID -> void* until it's time to add 16 bit x86 support
// PVOID -> void* until it's time to add 16 bit x86 support
// HANDLE -> void* as far as I can tell
// WORD -> uint16_t as far as I can tell
// DWORD -> uint32_t as far as I can tell
// QWORD -> uint64_t as far as I can tell

// DECLSPEC_ALLOCATOR -> WINBASEAPI [type] WINAPI (based on mingw headers being different than MS docs)
// WINBASEAPI -> DECLSPEC_IMPORT when _KERNEL32_ is not defined. the _KERNEL32_ macro appears to be an entirely undocumented.

#include <stdint.h>
#include <stddef.h>

// macros that are kept from the API
#define DECLSPEC_IMPORT __declspec(dllimport)
#define DECLSPEC_NORETURN __declspec(noreturn)

// Silly ARM breaking every ABI normality
// TODO: Apparently this can be defined as PASCAL for OLE? What does that even mean?
#if defined(_ARM_)
#define WINAPI
#else
#define WINAPI __stdcall
#endif

// Pretty sure WINBOOL is just int
typedef int WINBOOL;

typedef int (WINAPI *FARPROC) (void);

// heapapi.h
#define HEAP_NO_SERIALIZE 0x1
#define HEAP_GENERATE_EXCEPTIONS 0x4
#define HEAP_ZERO_MEMORY 0x8
#define HEAP_REALLOC_IN_PLACE_ONLY 0x00000010
#define HEAP_CREATE_ENABLE_EXECUTE 0x00040000

DECLSPEC_IMPORT void* WINAPI HeapAlloc (void* hHeap, uint32_t dwFlags, size_t dwBytes);
DECLSPEC_IMPORT WINBOOL WINAPI HeapFree (void* hHeap, uint32_t dwFlags, void* lpMem);
DECLSPEC_IMPORT void* WINAPI HeapCreate (uint32_t flOptions, size_t dwInitialSize, size_t dwMaximumSize);
DECLSPEC_IMPORT WINBOOL WINAPI HeapDestroy (void* hHeap);
DECLSPEC_IMPORT void* WINAPI HeapReAlloc (void* hHeap, uint32_t dwFlags, void* lpMem, size_t dwBytes);

// libloaderapi.h

DECLSPEC_IMPORT void* WINAPI LoadLibraryA(char const* lpLibFileName);
DECLSPEC_IMPORT FARPROC WINAPI GetProcAddress (void* hModule, char const* lpProcName);
DECLSPEC_IMPORT WINBOOL WINAPI FreeLibrary (void* hLibModule);

// debugapi.h

DECLSPEC_IMPORT void WINAPI DebugBreak(void);

// processthreadsapi.h

DECLSPEC_IMPORT DECLSPEC_NORETURN void WINAPI ExitProcess (unsigned int uExitCode);

// winbase.h

#define STD_INPUT_HANDLE ((uint32_t)UINT32_MAX-10)
#define STD_OUTPUT_HANDLE ((uint32_t)UINT32_MAX-11)
#define STD_ERROR_HANDLE ((uint32_t)UINT32_MAX-12)

// processenv.h

DECLSPEC_IMPORT void* WINAPI GetStdHandle (uint32_t nStdHandle);

// wincon.h

DECLSPEC_IMPORT WINBOOL WINAPI WriteConsoleA(void* hConsoleOutput, void const* lpBuffer, uint32_t nNumberOfCharsToWrite, uint32_t* lpNumberOfCharsWritten, void* lpReserved);

// sysinfoapi.h

DECLSPEC_IMPORT uint64_t WINAPI GetTickCount64(void);

#endif
