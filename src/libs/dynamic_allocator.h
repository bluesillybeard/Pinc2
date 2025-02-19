#ifndef DYNAMIC_ALLOCATOR_H
#define DYNAMIC_ALLOCATOR_H

#include <stddef.h>
// Zig style allocators

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
static inline void* Allocator_allocate(Allocator a, size_t size) {
    return a.vtable->allocate(a.allocatorObjectPtr, size);
}

/// @brief Allocate some memory with explicit alignment.
/// @param size Number of bytes to allocate. Must be a multiple of alignment.
/// @param alignment Alignment requirement. Must be a power of 2.
/// @return A pointer to the allocated memory.
static inline void* Allocator_allocateAligned(Allocator a, size_t size, size_t alignment) {
    return a.vtable->allocateAligned(a.allocatorObjectPtr, size, alignment);
}

/// @brief Reallocate some memory with a different size
/// @param ptr Pointer to the memory to reallocate. Must be the exact pointer from to allocate, allocateAligned, or reallocate on the same allocator
/// @param oldSize Old size of the allocation. Must be the exact size given to allocate, allocateAligned, or reallocate on the same allocator for the respective pointer.
/// @param newSize The new size of the allocation
/// @return A pointer to this memory. May be the same or different from pointer.
static inline void* Allocator_reallocate(Allocator a, void* ptr, size_t oldSize, size_t newSize) {
    return a.vtable->reallocate(a.allocatorObjectPtr, ptr, oldSize, newSize);
}

/// @brief Free some memory
/// @param pointer Pointer to free. Must be the exact pointer from allocate, allocateAligned, or reallocate on the same allocator
/// @param bytes Number of bytes to free. Must be the exact size given to allocate, allocateAligned, or reallocate on the same allocator
static inline void Allocator_free(Allocator a, void* ptr, size_t size) {
    a.vtable->free(a.allocatorObjectPtr, ptr, size);
}

#endif
