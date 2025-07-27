#ifndef PINC_MAIN_H
#define PINC_MAIN_H

#include <pinc.h>
#include "libs/pinc_allocator.h"
#include "pinc_options.h"
#include "pinc_error.h"
#include "pinc_window.h"
#include "pinc_types.h"
#include "pinc_arena.h"

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
    PincEventType type;
    PincWindowHandle currentWindow;
    int64_t timeUnixMillis;
    union PincEventUnion {
        struct PincEventCloseSignal {
            PincWindowHandle window;
        } closeSignal;
        struct PincEventMouseButton {
            uint32_t oldState;
            uint32_t state;
        } mouseButton;
        struct PincEventResize {
            PincWindowHandle window;
            uint32_t oldWidth;
            uint32_t oldHeight;
            uint32_t width;
            uint32_t height;
        } resize;
        struct PincEventFocus {
            // the old window is in currentWindow
            PincWindowHandle newWindow;
        } focus;
        struct PincEventExposure {
            PincWindowHandle window;
            uint32_t x;
            uint32_t y;
            uint32_t width;
            uint32_t height;
        } exposure;
        struct PincEventKeyboardButton {
            PincKeyboardKey key;
            bool state;
            bool repeat;
        } keyboardButton;
        struct PincEventCursorMove {
            PincWindowHandle window;
            uint32_t oldX;
            uint32_t oldY;
            uint32_t x;
            uint32_t y;
        } cursorMove;
        struct PincEventCursorTransition {
            PincWindowHandle oldWindow;
            uint32_t oldX;
            uint32_t oldY;
            PincWindowHandle window;
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
        struct PincEventClipboardChanged {
            PincMediaType type;
            // This data is on the temporary allocator
            char* data;
            size_t dataSize;
        } clipboard;
    } data;
} PincEvent;

// Call these functions to trigger events
// TODO: Would it possibly make sense to make these public? Is there are use case for that?
void PincEventCloseSignal(int64_t timeUnixMillis, PincWindowHandle window);

void PincEventMouseButton(int64_t timeUnixMillis, uint32_t oldState, uint32_t state);

void PincEventResize(int64_t timeUnixMillis, PincWindowHandle window, uint32_t oldWidth, uint32_t oldHeight, uint32_t width, uint32_t height);

void PincEventFocus(int64_t timeUnixMillis, PincWindowHandle window);

void PincEventExposure(int64_t timeUnixMillis, PincWindowHandle window, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void PincEventKeyboardButton(int64_t timeUnixMillis, PincKeyboardKey key, bool state, bool repeat);

void PincEventCursorMove(int64_t timeUnixMillis, PincWindowHandle window, uint32_t oldX, uint32_t oldY, uint32_t x, uint32_t y);

void PincEventCursorTransition(int64_t timeUnixMillis, PincWindowHandle oldWindow, uint32_t oldX, uint32_t oldY, PincWindowHandle window, uint32_t x, uint32_t y);

void PincEventTextInput(int64_t timeUnixMillis, uint32_t codepoint);

void PincEventScroll(int64_t timeUnixMillis, float vertical, float horizontal);

/// Note: the data given here is assumed to be already on the pinc temporary allocator
void PincEventClipboardChanged(int64_t timeUnixMillis, PincMediaType type, char* dataNullterm, size_t dataSize);

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
    pincAllocator alloc;
    // Memory for tempAlloc object
    Arena arenaAllocatorObject;
    // See doc for tempAllocator macro. Live for incomplete and init.
    pincAllocator tempAlloc;
    // Nullable, Lifetime separate from initState
    PincErrorCallback userCallError;
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
    // Live for init, type: RawOpenglContextObject
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
    PincWindowHandle currentWindow;

    // The real current window.
    PincWindowHandle realCurrentWindow;

    // The chosen framebuffer format
    PincFramebufferFormatHandle framebufferFormat;

    // Defined by the user, These are either all live or none live
    // userAllocObj can be null while these are live
    void* userAllocObj;
    PincAllocCallback userAllocFn;
    PincAllocAlignedCallback userAllocAlignedFn;
    PincReallocCallback userReallocFn;
    PincFreeCallback userFreeFn;

    // TODO: is this still needed?
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

PincObjectHandle PincObject_allocate(PincObjectDiscriminator discriminator);

// Destroy the old object and make a new object in its place with the same ID.
void PincObject_reallocate(PincObjectHandle id, PincObjectDiscriminator discriminator);

void PincObject_free(PincObjectHandle id);

static P_INLINE PincObjectDiscriminator PincObject_discriminator(PincObjectHandle id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    return ((PincObject*)staticState.objects.objectsArray)[id-1].discriminator;
}

static P_INLINE IncompleteWindow* PincObject_ref_incompleteWindow(PincObjectHandle id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_incompleteWindow, "Object must be an incomplete window");
    return &((IncompleteWindow*)staticState.incompleteWindowObjects.objectsArray)[obj.internalIndex];
}

static P_INLINE WindowHandle* PincObject_ref_window(PincObjectHandle id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_window, "Object must be a complete window");
    return &((WindowHandle*)staticState.windowHandleObjects.objectsArray)[obj.internalIndex];
}

static P_INLINE IncompleteGlContext* PincObject_ref_incompleteGlContext(PincObjectHandle id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    return &((IncompleteGlContext*)staticState.incompleteGlContextObjects.objectsArray)[obj.internalIndex];
}

static P_INLINE RawOpenglContextObject* PincObject_ref_glContext(PincObjectHandle id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_glContext, "Object must be a complete OpenGL context");
    return &((RawOpenglContextObject*)staticState.rawOpenglContextHandleObjects.objectsArray)[obj.internalIndex];
}

static P_INLINE FramebufferFormat* PincObject_ref_framebufferFormat(PincObjectHandle id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    PErrorUser(obj.discriminator == PincObjectDiscriminator_framebufferFormat, "Object must be a framebuffer format");
    return &((FramebufferFormat*)staticState.framebufferFormatObjects.objectsArray)[obj.internalIndex];
}

#endif
