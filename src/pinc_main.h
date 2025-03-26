#ifndef PINC_MAIN_H
#define PINC_MAIN_H

// most options are handled by the external handle (as they must be shared between pinc and the user)
#include <pinc.h>
#include "libs/dynamic_allocator.h"
#include "pinc_options.h"
#include "pinc_error.h"
#include "window.h"
#include "pinc_types.h"
#include "arena.h"

// pinc objects are a user-facing map from what the user sees, and the actual internal objects / handles.
// The real internal objects are usually done with opaque pointers managed by the backend itself.
typedef enum {
    // This needs to be zero so null-initialized objects are valid
    PincObjectDiscriminator_none = 0,
    PincObjectDiscriminator_incompleteWindow,
    PincObjectDiscriminator_window,
    PincObjectDiscriminator_incompleteGlContext,
    PincObjectDiscriminator_glContext,
    PincObjectDiscriminator_framebufferFormat,

} PincObjectDiscriminator;

typedef struct {
    // What type of object this is
    PincObjectDiscriminator discriminator;
    // Where in the internal object-specific array this is
    uint32_t internalIndex;
    // user data
    void* userData;
} PincObject;

// Object pool struct
typedef struct {
    // C with generics (I mean actual generics that aren't evil magic macros) would be quite nice
    void* objectsArray;
    uint32_t objectsNum;
    uint32_t objectsCapacity;
    uint32_t* freeArray;
    uint32_t freeArrayNum;
    uint32_t freeArrayCapacity;
} PincPool;

// Object pool methods

// Returns the index of the newly allocated object
uint32_t PincPool_alloc(PincPool* pool, size_t elementSize);

void PincPool_free(PincPool* pool, uint32_t index, size_t elementSize);

void PincPool_deinit(PincPool* pool, size_t elementSize);

// Compound structs are my best friend
typedef struct {
    pinc_event_type type;
    pinc_window currentWindow;
    int64_t timeUnixMillis;
    union PincEventUnion {
        struct PincEventCloseSignal {
            pinc_window window;
        } closeSignal;
        struct PincEventMouseButton {
            uint32_t oldState;
            uint32_t state;
        } mouseButton;
        struct PincEventResize {
            pinc_window window;
            uint32_t oldWidth;
            uint32_t oldHeight;
            uint32_t width;
            uint32_t height;
        } resize;
        struct PincEventFocus {
            // the old window is in currentWindow
            pinc_window newWindow;
        } focus;
        struct PincEventExposure {
            pinc_window window;
            uint32_t x;
            uint32_t y;
            uint32_t width;
            uint32_t height;
        } exposure;
        struct PincEventKeyboardButton {
            pinc_keyboard_key key;
            bool state;
            bool repeat;
        } keyboardButton;
        struct PincEventCursorMove {
            pinc_window window;
            uint32_t oldX;
            uint32_t oldY;
            uint32_t x;
            uint32_t y;
        } cursorMove;
        struct PincEventCursorTransition {
            pinc_window oldWindow;
            uint32_t oldX;
            uint32_t oldY;
            pinc_window window;
            uint32_t x;
            uint32_t y;
        } cursorTransition;
        struct PincEventTextInput {
            uint32_t codepoint;
        } textInput;
        struct PincEventScroll {
            float vertical;
            float horizontal;
        } scroll;
    } data;
} PincEvent;

// Call these functions to trigger events
// TODO: Would it possibly make sense to make these public? Is there are use case for that?
void PincEventCloseSignal(int64_t timeUnixMillis, pinc_window window);

void PincEventMouseButton(int64_t timeUnixMillis, uint32_t oldState, uint32_t state);

void PincEventResize(int64_t timeUnixMillis, pinc_window window, uint32_t oldWidth, uint32_t oldHeight, uint32_t width, uint32_t height);

void PincEventFocus(int64_t timeUnixMillis, pinc_window window);

void PincEventExposure(int64_t timeUnixMillis, pinc_window window, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void PincEventKeyboardButton(int64_t timeUnixMillis, pinc_keyboard_key key, bool state, bool repeat);

void PincEventCursorMove(int64_t timeUnixMillis, pinc_window window, uint32_t oldX, uint32_t oldY, uint32_t x, uint32_t y);

void PincEventCursorTransition(int64_t timeUnixMillis, pinc_window oldWindow, uint32_t oldX, uint32_t oldY, pinc_window window, uint32_t x, uint32_t y);

void PincEventTextInput(int64_t timeUnixMillis, uint32_t codepoint);

void PincEventScroll(int64_t timeUnixMillis, float vertical, float horizontal);

// Pinc static state

typedef enum {
    PincState_preinit,
    PincState_incomplete,
    PincState_init,
} PincState;

typedef struct {
    // TODO: Keep track of what stage of initialization we're in
    PincState initState;
    // See doc for rootAllocator macro. Live for incomplete and init
    Allocator alloc;
    // Memory for tempAlloc object
    Arena arenaAllocatorObject;
    // See doc for tempAllocator macro. Live for incomplete and init.
    Allocator tempAlloc;
    // Nullable, Lifetime separate from initState
    pinc_error_callback userCallError;
    // Live for incomplete and init
    WindowBackend sdl2WindowBackend;
    // Live for init, type: PincObject
    PincPool objects;
    // Live for init, type: IncompleteWindow
    PincPool incompleteWindowObjects;
    // Live for init, type: WindowHandle
    PincPool windowHandleObjects;
    // Live for init, type: IncompleteGlContext
    PincPool incompleteGlContextObjects;
    // Live for init, type: RawOpenglContextHandle
    PincPool rawOpenglContextHandleObjects;
    // Live for init, type: FramebufferFormat
    PincPool framebufferFormatObjects;

    PincEvent* eventsBuffer;
    uint32_t eventsBufferNum;
    uint32_t eventsBufferCapacity;

    PincEvent* eventsBufferBack;
    uint32_t eventsBufferBackNum;
    uint32_t eventsBufferBackCapacity;

    // Current window as far as the user cares, so it only changes within pinc_step
    pinc_window currentWindow;

    // The real current window.
    pinc_window realCurrentWindow;

    // The chosen framebuffer format
    pinc_framebuffer_format framebufferFormat;

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


#define PINC_PREINIT_STATE {0}

extern PincStaticState pinc_intern_staticState;

// shortcuts
// shortcut to the Pinc global static state struct.
#define staticState pinc_intern_staticState
// The primary allocator. This is either a wrapper of libs/platform.h, or the user-defined allocation callbacks
#define rootAllocator staticState.alloc
// A temporary allocator that is cleared at the end of pinc_step().
#define tempAllocator staticState.tempAlloc

pinc_object PincObject_allocate(PincObjectDiscriminator discriminator);

// Destroy the old object and make a new object in its place with the same ID.
void PincObject_reallocate(pinc_object id, PincObjectDiscriminator discriminator);

void PincObject_free(pinc_object id);

static inline PincObjectDiscriminator PincObject_discriminator(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    return ((PincObject*)staticState.objects.objectsArray)[id-1].discriminator;
}

static inline IncompleteWindow* PincObject_ref_incompleteWindow(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_incompleteWindow, "Object must be an incomplete window");
    return &((IncompleteWindow*)staticState.incompleteWindowObjects.objectsArray)[obj.internalIndex];
}

static inline WindowHandle* PincObject_ref_window(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_window, "Object must be a complete window");
    return &((WindowHandle*)staticState.windowHandleObjects.objectsArray)[obj.internalIndex];
}

static inline IncompleteGlContext* PincObject_ref_incompleteGlContext(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    return &((IncompleteGlContext*)staticState.incompleteGlContextObjects.objectsArray)[obj.internalIndex];
}

static inline RawOpenglContextHandle* PincObject_ref_glContext(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_glContext, "Object must be a complete OpenGL context");
    return &((RawOpenglContextHandle*)staticState.rawOpenglContextHandleObjects.objectsArray)[obj.internalIndex];
}

static inline FramebufferFormat* PincObject_ref_framebufferFormat(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_framebufferFormat, "Object must be a framebuffer format");
    return &((FramebufferFormat*)staticState.framebufferFormatObjects.objectsArray)[obj.internalIndex];
}

#endif
