// "Implementation" for arena.h

#include "pinc_main.h"

// This has to be here because pinc_main.h includes arena.h which defines a default value for ARENA_BACKEND.
// This value has no effect on the regular use of arena.h, but it does change what implementation is used.
#undef ARENA_BACKEND
#define ARENA_BACKEND ARENA_BACKEND_USER_CUSTOM
#define ARENA_USER_CUSTOM_MALLOC(_size) Allocator_allocate(rootAllocator, _size)
#define ARENA_USER_CUSTOM_FREE(_ptr, _size) Allocator_free(rootAllocator, _ptr, _size)
#define ARENA_IMPLEMENTATION 1
#include "arena.h"
