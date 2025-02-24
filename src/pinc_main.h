#ifndef PINC_MAIN_H
#define PINC_MAIN_H

// most options are handled by the external handle (as they must be shared between pinc and the user)
#include <pinc.h>
#include "platform/platform.h"
#include "libs/pstring.h"
#include "libs/dynamic_allocator.h"
#include "pinc_options.h"
#include "pinc_error.h"
#include "window.h"
#include "pinc_types.h"

// pinc objects are a user-facing map from what the user sees, and the actual internal objects / handles.
// The real internal objects are usually done with opaque pointers managed by the backend itself.
typedef enum {
    // This needs to be zero so null-initialized objects are valid
    PincObjectDiscriminator_none = 0,
    PincObjectDiscriminator_incompleteWindow,
    PincObjectDiscriminator_window,
    PincObjectDiscriminator_incompleteRawGlContext,
    PincObjectDiscriminator_rawGlContext,

} PincObjectDiscriminator;

typedef struct {
    PincObjectDiscriminator discriminator;
    union PincObjectData {
        IncompleteWindow incompleteWindow;
        WindowHandle window;
        IncompleteRawGlContext incompleteRawGlContext;
        RawOpenglContextHandle rawGlContext;
    } data;
} PincObject;

// Pinc static state

typedef enum {
    PincState_preinit,
    PincState_incomplete,
    PincState_init,
} PincState;

typedef struct {
    // Keep track of what stage of initialization we're in
    PincState initState;
    // See doc for rootAllocator macro. Live for incomplete and init
    Allocator alloc;
    // See doc for tempAllocator macro. Live for incomplete and init.
    Allocator tempAlloc;
    // Nullable, Lifetime separate from initState
    pinc_error_callback userCallError;
    // Live for incomplete and init
    WindowBackend sdl2WindowBackend;
    // On the root allocator, Live for incomplete and init
    FramebufferFormat* framebufferFormats;
    size_t framebufferFormatNum;
    // On the root allocator, Live for init
    PincObject* objects;
    size_t objectsNum;
    size_t objectsCapacity;
    // On the root allocator, Live for init
    pinc_object* freeObjects;
    size_t freeObjectsNum;
    size_t freeObjectsCapacity;

    // Defined by the user, These are either all live or none live
    // userAllocObj can be null while these are live
    void* userAllocObj;
    pinc_alloc_callback userAllocFn;
    pinc_alloc_aligned_callback userAllocAlignedFn;
    pinc_realloc_callback userReallocFn;
    pinc_free_callback userFreeFn;

    bool windowBackendSet;
    WindowBackend windowBackend;
} PincStaticState;

#define PINC_PREINIT_STATE (PincStaticState) { \
    .initState = PincState_preinit, \
    .alloc = {.allocatorObjectPtr = 0, .vtable = 0}, \
    .tempAlloc = {.allocatorObjectPtr = 0, .vtable = 0}, \
    .userCallError = 0, \
    .sdl2WindowBackend = {.obj = 0, .vt = {0}}, \
    .framebufferFormats = 0, \
    .framebufferFormatNum = 0, \
    .objects = 0, \
    .objectsNum = 0, \
    .objectsCapacity = 0, \
    .windowBackendSet = false, \
    .windowBackend = {.obj = 0, .vt = {0}}, \
}

extern PincStaticState pinc_intern_staticState;

// shortcuts
// shortcut to the Pinc global static state struct.
#define staticState pinc_intern_staticState
// The primary allocator. This is either a wrapper of libs/platform.h, or the user-defined allocation callbacks
#define rootAllocator staticState.alloc
// A temporary allocator that is cleared at the end of pinc_step().
#define tempAllocator staticState.tempAlloc

#endif
