#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Platform provides some base platform functionality

// Useful macros that depend on the compiler

# if __GNUC__ || __clang__
#   define P_UNUSED(var) (void) var
#   define P_NORETURN __attribute__ ((__noreturn__))
# else
// Assume other compilers are not supported
#   define P_UNUSED(var)
#   define P_NORETURN
#endif

typedef void (*PFN)(void);

// TODO: It is probably worth exposing these functions to the user, as an alternative to whatever other platform library they may use.
// We have to implement all of these nice functions for every supported platform anyway, may as well let the downstream developer make use of it,
// Although many of them are just wrappers of libc (for now)

// allocations

/// @brief Allocate some memory. Aligned depending on platform such that any structure can be placed into this memory.
///     Identical to libc's malloc function.
/// @param bytes Number of bytes to allocate.
/// @return A pointer to the memory.
void* pAlloc(size_t bytes);

/// @brief Allocate some memory with explicit alignment.
/// @param bytes Number of bytes to allocate. Must be a multiple of alignment.
/// @param alignment Alignment requirement. Must be a power of 2.
/// @return A pointer to the allocated memory.
void* pAllocAligned(size_t bytes, size_t alignment);

/// @brief Reallocate some memory with a different size
/// @param pointer Pointer to the memory to reallocate. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param oldSize Old size of the allocation. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
/// @param newSize The new size of the allocation
/// @return A pointer to this memory. May be the same or different from pointer.
void* pRealloc(void* pointer, size_t oldSize, size_t newSize);

/// @brief Free some memory
/// @param pointer Pointer to free. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param bytes Number of bytes to free. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
void pFree(void* pointer, size_t bytes);

// loading libraries

/// @brief Load a library.
///     Unlike Posix's dlopen or Windows' LoadLibrary[A/W] functions, this does not need to update any linker or symbol tables.
///     It is allowed to do so if the implementation requires it.
/// @param nameUtf8 The name of the library, as a UTF8 encoded string.
/// @param nameSize The number of bytes in the name.
/// @return An opaque pointer to the library object, or null if the library could not be loaded.
void* pLoadLibrary(uint8_t const* nameUtf8, size_t nameSize);

/// @brief Load a symbol from a library
/// @param library The library to load from
/// @param symbolNameUtf8 The name of the symbol, encoded in UTF8
/// @param nameSize The number of bytes in the name
/// @return A pointer to that symbol.
PFN pLibrarySymbol(void* library, uint8_t* symbolNameUtf8, size_t nameSize);

/// @brief Unload a library that is no longer needed.
/// @param library The library to unload.
void pUnloadLibrary(void* library);

// Utility functions that may have optimized versions on specific platforms

/// @brief Gets the length of a null-terminated string of bytes. Same thing as libc's strlen function.
/// @param str String to get the length of
/// @return length of the string, in bytes, not including the null terminator
size_t pStringLen(char const* str);

/// @brief Copy memory from one location to another. Source and destination must not overlap.
/// @param source The source of the copy operation
/// @param destination Destination of the copy operation
/// @param numBytes The number of bytes to copy
void pMemCopy(void const* restrict source, void* restrict destination, size_t numBytes);

/// @brief Same thing as pMemCopy, but the source and destination may overlap.
void pMemMove(void const* source, void* destination, size_t numBytes);

void pMemSet(uint8_t value, void* destination, size_t numBytes);

/// @brief Convert the first UTF8 encoded codepoint
/// @param str String to convert
/// @param strLen The length of the string, in bytes. To avoid reading out of bounds for invalid UTF8.
/// @param outLen Place to output the number of bytes converted, or null if that is not needed.
/// @return the unicode point
uint32_t pUtf8Unicode(uint8_t const* str, size_t strLen, size_t* outLen);

/// @brief Encode a unicode point to UTF8
/// @param codepoint the codepoint to encode
/// @param destStr a pointer to write the codepoint. May be null so only the length is returned.
/// @return the number of bytes required to encode this codepoint
size_t pUnicodeUtf8(uint32_t codepoint, char* destStr);

/// @brief Convert a UTF8 string to a unicode string
/// @param str UTF8 string to convert
/// @param strLen length of UTF8 string in bytes
/// @param outUnicode where to write output characters, or null to only query length
/// @return the number of unicode points in the string
size_t pUtf8UnicodeString(uint8_t const* str, size_t strLen, uint32_t* outUnicode);

/// @brief Convert a unicode string into a UTF8 string
/// @param codepoints unicode string to convert
/// @param numCodepoints the length of the unicode string, in codepoints
/// @param outUtf8 where to write the output string, or null to only query length
/// @return the number of UTF8 bytes in the string
size_t pUnicodeUtf8String(uint32_t const* codepoints, size_t numCodepoints, uint8_t* outUtf8);

// debugging functionality

/// @brief Trigger a debugger breakpoint if possible, then continue execution, if possible
///        Triggering a debugger is incredibly architecture and compiler and debugger specific, so there are very few guarantees with this function
void pTriggerDebugger(void);

/// @brief Call when an assertion fails.
P_NORETURN void pAssertFail(void);

void pPrintError(uint8_t const* message, size_t len);

void pPrintDebug(uint8_t const* message, size_t len);

// TODO: replace this with a proper formatting system that isn't just a wrapper of libc's printf
// Variadic arguments feel hacky and are annoying to deal with.
void pPrintFormat(char const* fmt, ...);

// Returns the number of characters that would have been written given enough space
size_t pBufPrintUint32(char* buf, size_t capacity, uint32_t v);

#endif
