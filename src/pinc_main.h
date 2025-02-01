#ifndef PINC_MAIN_H
#define PINC_MAIN_H

// most options are handled by the external handle (as they must be shared between pinc and the user)
#include <pinc.h>
#include "platform/platform.h"

// options only for library build

// This is so cmake options work correctly when put directly as defines
#define ON 1
#define OFF 0

#ifndef PINC_ERROR_SETTING
// 0 -> disabled
// 1 -> light
// 2 -> rigorous
#define PINC_ERROR_SETTING 2
#endif

#ifndef PINC_HAVE_WINDOW_SDL2
#define PINC_HAVE_WINDOW_SDL2 1
#endif

#ifndef PINC_HAVE_GRAPHICS_RAW_OPENGL
#define PINC_HAVE_GRAPHICS_RAW_OPENGL 1
#endif

// These are "implemented" in pinc_main.c
extern pinc_error_callback pfn_pinc_error_callback;
extern pinc_error_callback pfn_pinc_panic_callback;
extern pinc_error_callback pfn_pinc_user_error_callback;

// Call when some kind of external issue occurs
#define PERROR(message, len) \
    { \
        if(pfn_pinc_error_callback) { \
            pfn_pinc_error_callback((uint8_t*) (message), (len)); \
        } else { \
            pPrintError((uint8_t*) (message), (len)); \
        } \
    }

#define PERROR_NL(message) PERROR((message), pStringLen(message))

#define PPANIC(message, len) \
    { \
        if(pfn_pinc_panic_callback) { \
            pfn_pinc_panic_callback((uint8_t*) (message), (len)); \
        } else { \
            pPrintError((uint8_t*) (message), (len)); \
        } \
        pAssertFail(); \
    }

#define PPANIC_NL(message) PPANIC(message, pStringLen((char*)message))

#define PUSEERROR(message, len) \
    { \
        if(pfn_pinc_user_error_callback) { \
            pfn_pinc_user_error_callback((uint8_t*) (message), (len)); \
        } else { \
            pPrintError((uint8_t*)(message), (len)); \
        } \
        pAssertFail(); \
    }

#if PINC_ERROR_SETTING == 2

#define PERROR_RIGOROUS(expr, message, len) if(!(expr)) PERROR(message, len)

#define PERROR_LIGHT(expr, message, len) if(!(expr)) PERROR(message, len)

#define PERROR_RIGOROUS_NL(expr, message) if(!(expr)) PERROR(message, pStringLen((char*)(message)))

#define PERROR_LIGHT_NL(expr, message) if(!(expr)) PERROR(message, pStringLen((char*)(message)))

#define PUSEERROR_RIGOROUS(expr, message, len) if(!(expr)) PUSEERROR(message, len)

#define PUSEERROR_LIGHT(expr, message, len) if(!(expr)) PUSEERROR(message, len)

#define PUSEERROR_RIGOROUS_NL(expr, message) if(!(expr)) PUSEERROR(message, pStringLen((char*)(message)))

#define PUSEERROR_LIGHT_NL(expr, message) if(!(expr)) PUSEERROR(message, pStringLen((char*)(message)))

#elif PINC_ERROR_SETTING == 1

#define PERROR_RIGOROUS(expr, message, len) 

#define PERROR_LIGHT(expr, message, len) if(!(expr)) PERROR(message, len)

#define PERROR_RIGOROUS_NL(expr, message)

#define PERROR_LIGHT_NL(expr, message) if(!(expr)) PERROR(message, pStringLen((char*)(message)))

#define PUSEERROR_RIGOROUS(expr, message, len)

#define PUSEERROR_LIGHT(expr, message, len) if(!(expr)) PUSEERROR(message, len)

#define PUSEERROR_RIGOROUS_NL(expr, message)

#define PUSEERROR_LIGHT_NL(expr, message) if(!(expr)) PUSEERROR(message, pStringLen((char*)(message)))

#else

#define PERROR_RIGOROUS(expr, message, len) 

#define PERROR_LIGHT(expr, message, len)

#define PERROR_RIGOROUS_NL(expr, message)

#define PERROR_LIGHT_NL(expr, message)

#define PUSEERROR_RIGOROUS(expr, message, len)

#define PUSEERROR_LIGHT(expr, message, len)

#define PUSEERROR_RIGOROUS_NL(expr, message)

#define PUSEERROR_LIGHT_NL(expr, message)

#endif

// Zig-style allocator interface

typedef struct {
    void* (*allocate) (void* obj, size_t size);
    void* (*allocateAligned) (void* obj, size_t size, size_t alignment);
    void* (*reallocate) (void* obj, void* ptr, size_t oldSize, size_t newSize);
    void (*free) (void* obj, void* ptr, size_t size);
} AllocatorVtable;

typedef struct {
    void* allocatorObjectPtr;
    AllocatorVtable const* vtable;
} Allocator;

/// @brief Allocate some memory. Aligned depending on platform such that any structure can be placed into this memory.
///     Identical to libc's malloc function.
/// @param size Number of bytes to allocate.
/// @return A pointer to the memory.
static void* Allocator_allocate(Allocator a, size_t size) {
    return a.vtable->allocate(a.allocatorObjectPtr, size);
}

/// @brief Allocate some memory with explicit alignment.
/// @param size Number of bytes to allocate. Must be a multiple of alignment.
/// @param alignment Alignment requirement. Must be a power of 2.
/// @return A pointer to the allocated memory.
static void* Allocator_allocateAligned(Allocator a, size_t size, size_t alignment) {
    return a.vtable->allocateAligned(a.allocatorObjectPtr, size, alignment);
}

/// @brief Reallocate some memory with a different size
/// @param ptr Pointer to the memory to reallocate. Must be the exact pointer from to allocate, allocateAligned, or reallocate on the same allocator
/// @param oldSize Old size of the allocation. Must be the exact size given to allocate, allocateAligned, or reallocate on the same allocator for the respective pointer.
/// @param newSize The new size of the allocation
/// @return A pointer to this memory. May be the same or different from pointer.
static void* Allocator_reallocate(Allocator a, void* ptr, size_t oldSize, size_t newSize) {
    return a.vtable->reallocate(a.allocatorObjectPtr, ptr, oldSize, newSize);
}

/// @brief Free some memory
/// @param pointer Pointer to free. Must be the exact pointer from allocate, allocateAligned, or reallocate on the same allocator
/// @param bytes Number of bytes to free. Must be the exact size given to allocate, allocateAligned, or reallocate on the same allocator
static void Allocator_free(Allocator a, void* ptr, size_t size) {
    return a.vtable->free(a.allocatorObjectPtr, ptr, size);
}

/// @brief Pinc primary "root" allocator
/// @remarks 
///     This is either a wrapper around the platform.h functions, or user-defined allocator functions (which themselves might just be wrappers of platform.h)
///     It is undefined until pinc_incomplete_init is called by the user.
extern Allocator rootAllocator;

/// @brief Pinc temporary allocator. This is an arena/bump allocator that is cleared at the start of pinc_step. undefined until pinc_incomplete_init is called by the user.
extern Allocator tempAllocator;

#endif
