#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Platform provides some base platform functionality

// Useful macros that depend on the compiler

# if __GNUC__ || __clang__
#   define P_UNUSED(var) (void) var
#   define P_NORETURN __attribute__ ((__noreturn__))
#   define P_INLINE inline
#   define P_RESTRICT restrict
# elif _MSC_VER
#   define P_UNUSED(var) (void) var
#   define P_NORETURN
#   define P_INLINE __inline
#   define P_RESTRICT __restrict
# else
// Assume other compilers do not support these at all
#   define P_UNUSED(var)
#   define P_NORETURN
#   define P_INLINE
#   define P_RESTRICT
#endif

typedef void (*pincPFN)(void);

// TODO(bluesillybeard): It is probably worth exposing these functions to the user, as an alternative to whatever other platform library they may use.
// We have to implement all of these nice functions for every supported platform anyway, may as well let the downstream developer make use of it,
// Although many of them are just wrappers of libc (for now)

// allocations

/// @brief Allocate some memory. Aligned depending on platform such that any structure can be placed into this memory.
///     Identical to libc's malloc function.
/// @param bytes Number of bytes to allocate.
/// @return A pointer to the memory.
void* pincAlloc(size_t bytes);

/// @brief Allocate some memory with explicit alignment.
/// @param bytes Number of bytes to allocate. Must be a multiple of alignment.
/// @param alignment Alignment requirement. Must be a power of 2.
/// @return A pointer to the allocated memory.
void* pincAllocAligned(size_t bytes, size_t alignment);

/// @brief Reallocate some memory with a different size
/// @param pointer Pointer to the memory to reallocate. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param oldSize Old size of the allocation. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
/// @param newSize The new size of the allocation
/// @return A pointer to this memory. May be the same or different from pointer.
void* pincRealloc(void* pointer, size_t oldSize, size_t newSize);

/// @brief Free some memory
/// @param pointer Pointer to free. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param bytes Number of bytes to free. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
void pincFree(void* pointer, size_t bytes);

// loading libraries

/// @brief Load a library.
///     Unlike Posix's dlopen or Windows' LoadLibrary[A/W] functions, this does not need to update any linker or symbol tables.
///     It is allowed to do so if the implementation requires it.
/// @param nameUtf8 The name of the library, as a UTF8 encoded string.
/// @param nameSize The number of bytes in the name.
/// @return An opaque pointer to the library object, or null if the library could not be loaded.
void* pincLoadLibrary(uint8_t const* nameUtf8, size_t nameSize);

/// @brief Load a symbol from a library
/// @param library The library to load from
/// @param symbolNameUtf8 The name of the symbol, encoded in UTF8
/// @param nameSize The number of bytes in the name
/// @return A pointer to that symbol.
pincPFN pincLibrarySymbol(void* library, uint8_t const* symbolNameUtf8, size_t nameSize);

/// @brief Unload a library that is no longer needed.
/// @param library The library to unload.
void pincUnloadLibrary(void* library);

// Utility functions that may have optimized versions on specific platforms

/// @brief Gets the length of a null-terminated string of bytes. Same thing as libc's strlen function.
/// @param str String to get the length of
/// @return length of the string, in bytes, not including the null terminator
size_t pincStringLen(char const* str);

/// @brief Copy memory from one location to another. Source and destination must not overlap.
/// @param source The source of the copy operation
/// @param destination Destination of the copy operation
/// @param numBytes The number of bytes to copy
void pincMemCopy(void const* P_RESTRICT source, void* P_RESTRICT destination, size_t numBytes);

/// @brief Same thing as pMemCopy, but the source and destination may overlap.
void pincMemMove(void const* source, void* destination, size_t numBytes);

void pincMemSet(uint8_t value, void* destination, size_t numBytes);

// debugging functionality

/// @brief Trigger a debugger breakpoint if possible, then continue execution, if possible
///        Triggering a debugger is incredibly architecture and compiler and debugger specific, so there are very few guarantees with this function
void pincTriggerDebugger(void);

/// @brief Call when an assertion fails.
P_NORETURN void pincAssertFail(void);

void pincPrintError(uint8_t const* message, size_t len);

void pincPrintDebug(uint8_t const* message, size_t len);

void pincPrintErrorLine(uint8_t const* message, size_t len);

void pincPrintDebugLine(uint8_t const* message, size_t len);

// TODO(bluesillybeard): these print functions should really be moved out of platform and into some kind of logging package

/// Returns the number of characters that would have been written given enough space, not including the null terminator
size_t pincBufPrintUint32(char* buf, size_t capacity, uint32_t value);

/// Returns the number of characters that would have been written given enough space, not including the null terminator
size_t pincBufPrintUint64(char* buf, size_t capacity, uint64_t value);

// A monotonic time counter in milliseconds.
// The only strict requirement is that it is relatively consistent so two time values can be compared with decent accuracy.
int64_t pincCurrentTimeMillis(void);

#endif
