#include <pinc.h>
#include <stdint.h>
#include "libs/pinc_allocator.h"
#include "libs/pinc_string.h"
#include "pinc_error.h"
#include "pinc_opengl.h"
#include "pinc_types.h"
#include "platform/pinc_platform.h"
#include "pinc_main.h"

#include "pinc_sdl2.h"
#include "pinc_window.h"

// Implementations of things in pinc_main.h

void pinc_intern_callError(pincString message, PincErrorType type) {
    if(rootAllocator.vtable == 0) {
        char* errString = "Pinc received an error before initialization of the root allocator - Did you forget to call InitComplete()?";
        size_t errStringLen = pincStringLen(errString);
        if(staticState.userCallError) {
            staticState.userCallError((uint8_t const*) errString, errStringLen, PincErrorType_user);
        } else {
            pincPrintErrorLine((uint8_t const*) errString, errStringLen);
        }
        pincTriggerDebugger();
    }
    if(staticState.userCallError) {
        // Let's be nice and let the user have their null terminator
        uint8_t* msgNullTerm = (uint8_t*)pincString_marshalAlloc(message, tempAllocator);
        staticState.userCallError(msgNullTerm, message.len, type);
        pincAllocator_free(tempAllocator, msgNullTerm, message.len+1);
    } else {
        pincPrintErrorLine(message.str, message.len);
    }
    pincTriggerDebugger();
}

P_NORETURN void pinc_intern_callFatalError(pincString message, PincErrorType type) {
    if(rootAllocator.vtable == 0) {
        char* errString = "Pinc received a fatal error before initialization of the root allocator - Did you forget to call InitComplete()?";
        size_t errStringLen = pincStringLen(errString);
        if(staticState.userCallError) {
            staticState.userCallError((uint8_t const*) errString, errStringLen, type);
        } else {
            pincPrintErrorLine((uint8_t const*) errString, errStringLen);
        }
        pincAssertFail();
    }
    if(staticState.userCallError) {
        // Let's be nice and let the user have their null terminator
        uint8_t* msgNullTerm = (uint8_t*)pincString_marshalAlloc(message, tempAllocator);
        staticState.userCallError(msgNullTerm, message.len, type);
        pincAllocator_free(tempAllocator, msgNullTerm, message.len+1);
    } else {
        pincPrintErrorLine(message.str, message.len);
    }
    pincAssertFail();
}

PincStaticState pinc_intern_staticState = PINC_PREINIT_STATE;

// Implementation of pinc's root allocator on top of platform.h
static void* pinc_root_platform_allocate(void* obj, size_t size) {
    P_UNUSED(obj);
    return pincAlloc(size);
}

static void* pinc_root_platform_allocateAligned(void* obj, size_t size, size_t alignment) {
    P_UNUSED(obj);
    return pincAllocAligned(size, alignment);
}

static void* pinc_root_platform_reallocate(void* obj, void* ptr, size_t oldSize, size_t newSize) {
    P_UNUSED(obj);
    return pincRealloc(ptr, oldSize, newSize);
}

static void pinc_root_platform_free(void* obj, void* ptr, size_t size) {
    P_UNUSED(obj);
    pincFree(ptr, size);
}

static const pincAllocatorVtable pinc_platform_alloc_vtable = {
    .allocate = &pinc_root_platform_allocate,
    .allocateAligned = &pinc_root_platform_allocateAligned,
    .reallocate = &pinc_root_platform_reallocate,
    .free = &pinc_root_platform_free,
};

// Implementation of allocator based on the user callbacks
// These need to exist due to the potential ABI difference between this internal allocator, and the PINC_PROC_CALL setting.
// the object can be set to the user pointer though
static void* pinc_root_user_allocate(void* obj, size_t size) {
    return staticState.userAllocFn(obj, size);
}

static void* pinc_root_user_allocateAligned(void* obj, size_t size, size_t alignment) {
    return staticState.userAllocAlignedFn(obj, size, alignment);
}

static void* pinc_root_user_reallocate(void* obj, void* ptr, size_t oldSize, size_t newSize) {
    return staticState.userReallocFn(obj, ptr, oldSize, newSize);
}

static void pinc_root_user_free(void* obj, void* ptr, size_t size) {
    staticState.userFreeFn(obj, ptr, size);
}

static const pincAllocatorVtable pinc_user_alloc_vtable = {
    .allocate = &pinc_root_user_allocate,
    .allocateAligned = &pinc_root_user_allocateAligned,
    .reallocate = &pinc_root_user_reallocate,
    .free = &pinc_root_user_free,
};

// allocator implementation for temp allocator
static void* pinc_temp_allocate(void* obj, size_t size) {
    P_UNUSED(obj);
    return arena_alloc(&staticState.arenaAllocatorObject, size);
}

static void* pinc_temp_allocateAligned(void* obj, size_t size, size_t alignment) {
    P_UNUSED(obj);
    P_UNUSED(size);
    P_UNUSED(alignment);
    // TODO: implement this? Do we even need it?
    PErrorAssert(false, "Cannot allocate an aligned buffer on the temp allocator.");
    return 0;
}

static void* pinc_temp_reallocate(void* obj, void* ptr, size_t oldSize, size_t newSize) {
    P_UNUSED(obj);
    return arena_realloc(&staticState.arenaAllocatorObject, ptr, oldSize, newSize);
}

static void pinc_temp_free(void* obj, void* ptr, size_t size) {
    P_UNUSED(obj);
    P_UNUSED(ptr);
    P_UNUSED(size);
    // TODO: implement this - if this allocation happens to be the last one, then rewind the arena by size.
}

static const pincAllocatorVtable PincTempAllocatorVtable = {
    .allocate = &pinc_temp_allocate,
    .allocateAligned = &pinc_temp_allocateAligned,
    .reallocate = &pinc_temp_reallocate,
    .free = &pinc_temp_free,
};

uint32_t PincPool_alloc(PincPool* pool, size_t elementSize) {
    if(pool->objectsCapacity == pool->objectsNum) {
        if(!pool->objectsArray) {
            pool->objectsArray = pincAllocator_allocate(rootAllocator, elementSize * 8);
            pool->objectsCapacity = 8;
        } else {
            uint32_t newObjectsCapacity = pool->objectsCapacity * 2;
            pool->objectsArray = pincAllocator_reallocate(rootAllocator, pool->objectsArray, elementSize * pool->objectsCapacity, elementSize * newObjectsCapacity);
            pool->objectsCapacity = newObjectsCapacity;
        }
    }
    if(pool->freeArray && pool->freeArrayNum > 0) {
        pool->freeArrayNum--;
        return pool->freeArray[pool->freeArrayNum];
    } else {
        pool->objectsNum++;
        return pool->objectsNum-1;
    }
}

void PincPool_free(PincPool* pool, uint32_t index, size_t elementSize) {
    if(index+1 == pool->objectsNum) {
        pool->objectsNum--;
    } else {
        if(pool->freeArrayCapacity == pool->freeArrayNum) {
            if(!pool->freeArray) {
                pool->freeArray = pincAllocator_allocate(rootAllocator, elementSize * 8);
                pool->freeArrayCapacity = 8;
            } else {
                uint32_t newObjectsCapacity = pool->freeArrayCapacity * 2;
                pool->freeArray = pincAllocator_reallocate(rootAllocator, pool->freeArray, elementSize * pool->freeArrayCapacity, elementSize * newObjectsCapacity);
                pool->freeArrayCapacity = newObjectsCapacity;
            }
        }
        pool->freeArray[pool->freeArrayNum] = index;
        pool->freeArrayNum++;
    }
}

void PincPool_deinit(PincPool* pool, size_t elementSize) {
    if(pool->objectsArray) {
        pincAllocator_free(rootAllocator, pool->objectsArray, elementSize * pool->objectsCapacity);
    }
    if(pool->freeArray) {
        pincAllocator_free(rootAllocator, pool->freeArray, sizeof(uint32_t) * pool->freeArrayCapacity);
    }
    *pool = (PincPool){0};
}

static uint32_t PincObject_allocateInternal(PincObjectDiscriminator discriminator) {
    switch(discriminator) {
        case PincObjectDiscriminator_none:{
            PPANIC("Cannot allocate object of no type");
            break;
        }
        case PincObjectDiscriminator_incompleteWindow: {
            return PincPool_alloc(&staticState.incompleteWindowObjects, sizeof(IncompleteWindow));
            break;
        }
        case PincObjectDiscriminator_window: {
            return PincPool_alloc(&staticState.windowHandleObjects, sizeof(WindowHandle));
            break;
        }
        case PincObjectDiscriminator_incompleteGlContext: {
            return PincPool_alloc(&staticState.incompleteGlContextObjects, sizeof(IncompleteGlContext));
            break;
        }
        case PincObjectDiscriminator_glContext: {
            return PincPool_alloc(&staticState.rawOpenglContextHandleObjects, sizeof(RawOpenglContextObject));
            break;
        }
        case PincObjectDiscriminator_framebufferFormat: {
            return PincPool_alloc(&staticState.framebufferFormatObjects, sizeof(FramebufferFormat));
            break;
        }
    }
    return 0;
}

static void PincObject_freeInternal(PincObjectDiscriminator discriminator, uint32_t index) {
    switch (discriminator)
    {
        case PincObjectDiscriminator_none: {
            PErrorAssert(false, "Not a valid object to reallocate");
            break;
        }
        case PincObjectDiscriminator_incompleteWindow:{
            PincPool_free(&staticState.incompleteWindowObjects, index, sizeof(IncompleteWindow));
            break;
        }
        case PincObjectDiscriminator_window:{
            PincPool_free(&staticState.windowHandleObjects, index, sizeof(WindowHandle));
            break;
        }
        case PincObjectDiscriminator_incompleteGlContext:{
            PincPool_free(&staticState.incompleteGlContextObjects, index, sizeof(IncompleteGlContext));
            break;
        }
        case PincObjectDiscriminator_glContext:{
            PincPool_free(&staticState.rawOpenglContextHandleObjects, index, sizeof(RawOpenglContextObject));
            break;
        }
        case PincObjectDiscriminator_framebufferFormat:{
            PincPool_free(&staticState.framebufferFormatObjects, index, sizeof(FramebufferFormat));
            break;
        }
    }
}

PincObjectHandle PincObject_allocate(PincObjectDiscriminator discriminator) {
    PincObjectHandle object_index = PincPool_alloc(&staticState.objects, sizeof(PincObject));
    PincObject *obj = &((PincObject*)staticState.objects.objectsArray)[object_index];
    PErrorSanitize(staticState.objects.objectsNum < UINT32_MAX, "Integer overflow");
    obj->discriminator = discriminator;
    obj->internalIndex = PincObject_allocateInternal(discriminator);
    obj->userData = 0;
    return object_index+1;
}

void PincObject_reallocate(PincObjectHandle id, PincObjectDiscriminator discriminator) {
    PErrorAssert(staticState.objects.objectsNum >= id, "Object ID out of bounds");
    PincObject *obj = &((PincObject*)staticState.objects.objectsArray)[id-1];
    PincObject_freeInternal(obj->discriminator, obj->internalIndex);
    obj->discriminator = discriminator;
    obj->internalIndex = PincObject_allocateInternal(discriminator);
    // The user data is left as-is, so people don't confused of why their user data is reset when they complete an object.
}

void PincObject_free(PincObjectHandle id) {
    PErrorAssert(staticState.objects.objectsNum >= id, "Object ID out of bounds");
    PincObject *obj = &((PincObject*)staticState.objects.objectsArray)[id-1];
    PincObject_freeInternal(obj->discriminator, obj->internalIndex);
    obj->discriminator = PincObjectDiscriminator_none;
    obj->internalIndex = 0;
    obj->userData = 0;
    PincPool_free(&staticState.objects, id-1, sizeof(PincObject));
}

static void PincEventBackEnsureCapacity(uint32_t capacity) {
    if(staticState.eventsBufferBackCapacity >= capacity) return;
    if(!staticState.eventsBufferBack) {
        staticState.eventsBufferBack = pincAllocator_allocate(rootAllocator, 8 * sizeof(PincEvent));
        staticState.eventsBufferBackCapacity = 8;
        staticState.eventsBufferBackNum = 0;
    } else {
        uint32_t newCapacity = staticState.eventsBufferBackCapacity * 2;
        staticState.eventsBufferBack = pincAllocator_reallocate(rootAllocator, staticState.eventsBufferBack, staticState.eventsBufferBackCapacity * sizeof(PincEvent), newCapacity * sizeof(PincEvent));
        staticState.eventsBufferBackCapacity = newCapacity;
    }
}

static P_INLINE void PincEventBackAppend(PincEvent* ev) {
    PincEventBackEnsureCapacity(staticState.eventsBufferBackNum+1);
    staticState.eventsBufferBack[staticState.eventsBufferBackNum] = *ev;
    staticState.eventsBufferBackNum += 1;
}

void PincEventCloseSignal(int64_t timeUnixMillis, PincWindowHandle window) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_closeSignal,
        .data.closeSignal = {
            .window = window,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventMouseButton(int64_t timeUnixMillis, uint32_t oldState, uint32_t state) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_mouseButton,
        .data.mouseButton = {
            .oldState = oldState,
            .state = state,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventResize(int64_t timeUnixMillis, PincWindowHandle window, uint32_t oldWidth, uint32_t oldHeight, uint32_t width, uint32_t height) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_resize,
        .data.resize = {
            .window = window,
            .oldWidth = oldWidth,
            .oldHeight = oldHeight,
            .width = width,
            .height = height,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventFocus(int64_t timeUnixMillis, PincWindowHandle window) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_focus,
        .data.focus = {
            .newWindow = window,
        },
    };
    staticState.realCurrentWindow = window;
    PincEventBackAppend(&e);
}

void PincEventExposure(int64_t timeUnixMillis, PincWindowHandle window, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_exposure,
        .data.exposure = {
            .window = window,
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventKeyboardButton(int64_t timeUnixMillis, PincKeyboardKey key, bool state, bool repeat) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_keyboardButton,
        .data.keyboardButton = {
            .key = key,
            .state = state,
            .repeat = repeat,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventCursorMove(int64_t timeUnixMillis, PincWindowHandle window, uint32_t oldX, uint32_t oldY, uint32_t x, uint32_t y) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_cursorMove,
        .data.cursorMove = {
            .window = window,
            .oldX = oldX,
            .oldY = oldY,
            .x = x,
            .y = y,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventCursorTransition(int64_t timeUnixMillis, PincWindowHandle oldWindow, uint32_t oldX, uint32_t oldY, PincWindowHandle window, uint32_t x, uint32_t y) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_cursorTransition,
        .data.cursorTransition = {
            .oldWindow = oldWindow,
            .oldX = oldX,
            .oldY = oldY,
            .window = window,
            .x = x,
            .y = y,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventTextInput(int64_t timeUnixMillis, uint32_t codepoint) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_textInput,
        .data.textInput = {
            .codepoint = codepoint,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventScroll(int64_t timeUnixMillis, float vertical, float horizontal) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = PincEventType_scroll,
        .data.scroll = {
            .vertical = vertical,
            .horizontal = horizontal,
        },
    };
    PincEventBackAppend(&e);
}

// StateValidMacroForConvenience
#define SttVld(_expr, _message) if(!(_expr)) {pincPrintErrorEZ(_message); return false;}
static bool PincStateValidForIncomplete(void) {
    #if PINC_ENABLE_ERROR_ASSERT
    // Easy validation with little cost
    SttVld(staticState.alloc.vtable, "Allocator not live")
    SttVld(staticState.tempAlloc.vtable, "Temp allocator not live")
    // TODO: Only window backend is sdl2, shortcuts are taken
    SttVld(staticState.sdl2WindowBackend.obj, "SDL2 backend not live");
    #endif
    // TODO: More difficult validation that costs significant performance
    return true;
}

static bool PincStateValidForComplete(void) {
    // Easy validation with little cost
    #if PINC_ENABLE_ERROR_ASSERT
    SttVld(staticState.alloc.vtable, "Allocator not live");
    SttVld(staticState.tempAlloc.vtable, "Temp Allocator not live");
    // TODO: only window backend is sdl2, shortcuts are taken
    SttVld(staticState.sdl2WindowBackend.obj, "SDL2 backend not live");
    SttVld(staticState.framebufferFormat, "Framebuffer format not live");
    SttVld(staticState.windowBackend.obj, "Window backend not live");
    #endif
    // TODO: More difficult validation that costs significant performance
    return true;
}
#undef SttVld

// asserts (regular assert) if the state is invalid
static void PincValidateForState(PincState state) {
    switch (state) {
        case PincState_preinit: {
            // Nothing to validate, other than this is Pinc's actual state
            PErrorAssert(staticState.initState == PincState_preinit, "Pinc state is not preinit: The user may have called a preinit function after initialization");
            break;
        }
        case PincState_incomplete: {
            PErrorAssert(staticState.initState == PincState_incomplete, "Pinc state is not incomplete: The user may have called a function at the wrong time");
            PErrorAssert(PincStateValidForIncomplete(), "Pinc state is invalid! See error log for details.");
            break;
        }
        case PincState_init: {
            PErrorAssert(staticState.initState == PincState_init, "Pinc state is not complete: The user may have called a function before complete initialization");
            PErrorAssert(PincStateValidForComplete(), "Pinc state is invalid! See error log for details.");
            break;
        }
    }
}

// Asserts (regular assert) if the state invalid for either of the given states
// This is for query functions which can be called both in incomplete and complete init states.
static void PincValidateForStates(PincState st1, PincState st2) {
    PincState realState;
    if(staticState.initState == st1) {
        realState = st1;
    } else {
        realState = st2;
    }
    PincValidateForState(realState);
}

// Below are the actual implementations of the Pinc API functions.

PINC_EXPORT void PINC_CALL pincPreinitSetErrorCallback(PincErrorCallback callback) {
    PincValidateForState(PincState_preinit);
    staticState.userCallError = callback;
}

PINC_EXPORT void PINC_CALL pincPreinitSetAllocCallbacks(void* user_ptr, PincAllocCallback alloc, PincAllocAlignedCallback alloc_aligned, PincReallocCallback realloc, PincFreeCallback free) {
    PincValidateForState(PincState_preinit);
    staticState.userAllocObj = user_ptr;
    staticState.userAllocFn = alloc;
    staticState.userAllocAlignedFn = alloc_aligned;
    staticState.userReallocFn = realloc;
    staticState.userFreeFn = free;
}

PINC_EXPORT PincReturnCode PINC_CALL pincInitIncomplete(void) {
    PincValidateForState(PincState_preinit);
    // First up, allocator needs set up
    PErrorUser(
        (staticState.userAllocFn && staticState.userAllocAlignedFn && staticState.userReallocFn && staticState.userFreeFn)
        || !(staticState.userAllocFn || staticState.userAllocAlignedFn || staticState.userReallocFn || staticState.userFreeFn), 
        "Pinc allocator callbacks must either be all set or all null!");
    
    if(staticState.userAllocFn) {
        // User callbacks are set
        rootAllocator = (pincAllocator) {
            .allocatorObjectPtr = staticState.userAllocObj,
            .vtable = &pinc_user_alloc_vtable,
        };
    } else {
        // user callbacks are not set
        rootAllocator = (pincAllocator){
            .allocatorObjectPtr = 0,
            .vtable = &pinc_platform_alloc_vtable,
        };
    }

    staticState.arenaAllocatorObject = (Arena){0};

    tempAllocator = (pincAllocator) {
        .allocatorObjectPtr = 0,
        .vtable = &PincTempAllocatorVtable,
    };

    // Begin initialization of window backends
    // TODO: SDL2 is the only one that actually exists so yeah

    bool sdl2InitRes = pincSdl2Init(&staticState.sdl2WindowBackend);

    if(!sdl2InitRes) {
        PErrorExternal(false, "No supported window backends available!");
        return PincReturnCode_error;
    }

    // Query collective information from window backends
    // Again, SDL2 is the only window backend, so some shortcuts can be taken
    size_t numFramebufferFormats;
    FramebufferFormat* framebufferFormats = pincWindowBackend_queryFramebufferFormats(&staticState.sdl2WindowBackend, tempAllocator, &numFramebufferFormats);
    for(size_t i=0; i<numFramebufferFormats; ++i) {
        PincFramebufferFormatHandle handle = PincObject_allocate(PincObjectDiscriminator_framebufferFormat);
        FramebufferFormat* reference = PincObject_ref_framebufferFormat(handle);
        *reference = framebufferFormats[i];
    }

    pincAllocator_free(tempAllocator, framebufferFormats, numFramebufferFormats*sizeof(FramebufferFormat));
    staticState.initState = PincState_incomplete;
    PincValidateForState(PincState_incomplete);
    return PincReturnCode_pass;
}

PINC_EXPORT bool PINC_CALL pincQueryWindowBackendSupport(PincWindowBackend window_backend) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // We're assuming that there's at least one supported backend here
    if(window_backend == PincWindowBackend_any) return true;
    // TODO: only backend is SDL2, shortcuts are taken
    #if PINC_HAVE_WINDOW_SDL2 == 1
    if(window_backend == PincWindowBackend_sdl2) return true;
    #endif
    return false;
}

PINC_EXPORT PincWindowBackend PINC_CALL pincQueryWindowBackendDefault(void) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: only window backend is SDL2, shortcuts are taken
    return PincWindowBackend_sdl2;
}

PINC_EXPORT bool PINC_CALL pincQueryGraphicsApiSupport(PincWindowBackend window_backend, PincGraphicsApi graphics_api) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    P_UNUSED(window_backend);
    if(graphics_api == PincGraphicsApi_any) {
        graphics_api = pincQueryGraphicsApiDefault(window_backend);
    }
    // TODO: only window backend is SDL2, shortcuts are taken
    return pincWindowBackend_queryGraphicsApiSupport(&staticState.sdl2WindowBackend, graphics_api);
}

PINC_EXPORT PincGraphicsApi PINC_CALL pincQueryGraphicsApiDefault(PincWindowBackend window_backend) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: only api is opengl, shortcuts are taken
    P_UNUSED(window_backend);
    return PincGraphicsApi_opengl;
}

PINC_EXPORT PincFramebufferFormatHandle pincQueryFramebufferFormatDefault(PincWindowBackend window_backend, PincGraphicsApi graphics_api) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    if(graphics_api == PincGraphicsApi_any) {
        graphics_api = pincQueryGraphicsApiDefault(window_backend);
    }
    // Use the front-end API to do what a user would effectively do
    uint32_t numFramebufferFormats = pincQueryFramebufferFormats(window_backend, graphics_api, 0, 0);
    PErrorExternal(numFramebufferFormats, "No framebuffer formats available");
    uint32_t* ids = pincAllocator_allocate(tempAllocator, numFramebufferFormats * sizeof(uint32_t));
    pincQueryFramebufferFormats(window_backend, graphics_api, ids, numFramebufferFormats);
    
    // framebuffer format default can be tuned here
    uint32_t const score_per_channel = 2;
    uint32_t const score_per_bit = 1;
    uint32_t const score_for_srgb = 16;

    PincFramebufferFormatHandle bestFormatHandle = ids[0];
    uint32_t bestScore = 0;
    for(uint32_t i=1; i<numFramebufferFormats; ++i) {
        PincFramebufferFormatHandle fmt = ids[i];
        uint32_t channels = pincQueryFramebufferFormatChannels(fmt);
        PErrorAssert(channels <= 4, "Invalid number of channels");
        uint32_t score = channels * score_per_channel;
        for(uint32_t c=0; c<channels; ++c) {
            score += pincQueryFramebufferFormatChannelBits(fmt, c) * score_per_bit;
        }
        if(pincQueryFramebufferFormatColorSpace(fmt) == PincColorSpace_srgb) {
            score += score_for_srgb;
        }
        if(score > bestScore) {
            bestFormatHandle = fmt;
            bestScore = score;
        }
    }
    return bestFormatHandle;
}

PINC_EXPORT uint32_t PINC_CALL pincQueryFramebufferFormats(PincWindowBackend window_backend, PincGraphicsApi graphics_api, PincFramebufferFormatHandle* ids_dest, uint32_t capacity) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    P_UNUSED(window_backend);
    P_UNUSED(graphics_api);
    // TODO: only window backend is SDL2, shortcuts are taken
    // TODO: sort framebuffers from best to worst, so applications can just loop from first to last and pick the first one they see that they like
    // - Note: probably best to do this in init when all of the framebuffer formats are queried to begin with
    // - Note 2: Actually, do we need to query framebuffer formats in init? Why not lazy-query them here?
    PincObjectHandle currentObject = 1;
    uint32_t currentIndex = 0;

    while(currentObject <= staticState.objects.objectsNum) {
        PincObject obj = ((PincObject*)staticState.objects.objectsArray)[currentObject-1];
        if(obj.discriminator == PincObjectDiscriminator_framebufferFormat) {
            if(currentIndex < capacity && ids_dest) {
                ids_dest[currentIndex] = currentObject;
            }
            currentIndex++;
        }
        currentObject++;
    }
    return currentIndex;
}

PINC_EXPORT uint32_t PINC_CALL pincQueryFramebufferFormatChannels(PincFramebufferFormatHandle format_id) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    return PincObject_ref_framebufferFormat(format_id)->channels;
}

PINC_EXPORT uint32_t PINC_CALL pincQueryFramebufferFormatChannelBits(PincFramebufferFormatHandle format_id, uint32_t channel) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    FramebufferFormat* obj = PincObject_ref_framebufferFormat(format_id);

    PErrorUser(channel < obj->channels, "channel index out of bounds - did you make sure it's less than what pincQueryFramebufferFormatChannels returns for this format?");
    return obj->channel_bits[channel];
}

PINC_EXPORT PincColorSpace PINC_CALL pincQueryFramebufferFormatColorSpace(PincFramebufferFormatHandle format_id) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    return PincObject_ref_framebufferFormat(format_id)->color_space;
}

PINC_EXPORT uint32_t PINC_CALL pincQueryMaxOpenWindows(PincWindowBackend window_backend) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    return pincWindowBackend_queryMaxOpenWindows(&staticState.sdl2WindowBackend);
}

PINC_EXPORT PincReturnCode PINC_CALL pincInitComplete(PincWindowBackend window_backend, PincGraphicsApi graphics_api, PincFramebufferFormatHandle framebuffer_format_id) {
    if(staticState.initState == PincState_preinit) pincInitIncomplete();
    PincValidateForState(PincState_incomplete);
    if(window_backend == PincWindowBackend_any) {
        // TODO: only window backend is SDL2, shortcuts are taken
        window_backend = PincWindowBackend_sdl2;
    }
    PErrorUser(pincQueryWindowBackendSupport(window_backend), "Unsupported window backend");
    if(graphics_api == PincGraphicsApi_any) {
        graphics_api = pincQueryGraphicsApiDefault(window_backend);
    }
    PErrorUser(pincQueryGraphicsApiSupport(window_backend, graphics_api), "Unsupported graphics api");
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pincQueryFramebufferFormatDefault(window_backend, graphics_api);
    }
    FramebufferFormat* framebuffer = PincObject_ref_framebufferFormat(framebuffer_format_id);
    staticState.framebufferFormat = framebuffer_format_id;
    PincReturnCode result = pincWindowBackend_completeInit(&staticState.sdl2WindowBackend, graphics_api, *framebuffer);
    if(result == PincReturnCode_error) {
        return PincReturnCode_error;
    }
    staticState.windowBackend = staticState.sdl2WindowBackend;
    staticState.windowBackendSet = true;
    staticState.initState = PincState_init;

    PincValidateForState(PincState_init);

    return PincReturnCode_pass;
}

PINC_EXPORT void PINC_CALL pincDeinit(void) {
    // Pinc deinit should work when called at ANY POINT AT ALL
    // Essentially, it is a safe function that says "call me at any point to completely reset Pinc to ground zero"
    // As such, there is quite a lot of work to do.
    // - destroy any objects
    // - deinit all backends (there may be multiple as this can be called before one is chosen)
    // - reset state

    // If the allocator is not initialized, then it means Pinc is not initialized in any capacity and we can just return
    if(rootAllocator.vtable == NULL) {
        return;
    }

    if(staticState.objects.objectsArray && staticState.objects.objectsCapacity) {
        // Go through every object and destroy it
        for(uint32_t i=0; i<staticState.objects.objectsNum; ++i) {
            PincObjectHandle id = i+1;
            switch (pincGetObjectType(id))
            {
                case PincObjectType_window: {
                    pincWindowDeinit(id);
                    break;
                }
                case PincObjectType_glContext: {
                    pincOpenglDeinitContext(id);
                    break;
                }
                // For these pure data types, destroying the object pool will automatically destroy them too
                case PincObjectType_none:
                case PincObjectType_framebufferFormat:
                case PincObjectType_incompleteGlContext:
                break;
            }
        }
    }

    // deinit backends
    // TODO: only window backend is sdl2, shortcuts are taken
    if(staticState.windowBackendSet) {
        pincWindowBackend_deinit(&staticState.windowBackend);
        staticState.windowBackendSet = false;
        staticState.windowBackend.obj = 0;
        staticState.sdl2WindowBackend.obj = 0;
    }
    
    // Destroy any remaining pieces

    PincPool_deinit(&staticState.objects, sizeof(PincObject));
    PincPool_deinit(&staticState.incompleteWindowObjects, sizeof(IncompleteWindow));
    PincPool_deinit(&staticState.windowHandleObjects, sizeof(WindowHandle));
    PincPool_deinit(&staticState.incompleteGlContextObjects, sizeof(IncompleteGlContext));
    PincPool_deinit(&staticState.rawOpenglContextHandleObjects, sizeof(RawOpenglContextObject));
    PincPool_deinit(&staticState.framebufferFormatObjects, sizeof(FramebufferFormat));

    pincAllocator_free(rootAllocator, staticState.eventsBuffer, staticState.eventsBufferNum * sizeof(PincEvent));
    pincAllocator_free(rootAllocator, staticState.eventsBufferBack, staticState.eventsBufferBackNum * sizeof(PincEvent));

    if(staticState.tempAlloc.vtable){
        arena_free(&staticState.arenaAllocatorObject);
    }

    // Full reset the state
    staticState = (PincStaticState) PINC_PREINIT_STATE;
}

PINC_EXPORT PincWindowBackend PINC_CALL pincQuerySetWindowBackend(void) {
    PincValidateForState(PincState_init);
    // TODO: only backend is SDL2, shortcuts are taken
    return PincWindowBackend_sdl2;
}

PINC_EXPORT PincGraphicsApi PINC_CALL pincQuerySetGraphicsApi(void) {
    PincValidateForState(PincState_init);
    // TODO: only backend is opengl, shortcuts are taken
    return PincGraphicsApi_opengl;
}

PINC_EXPORT PincFramebufferFormatHandle PINC_CALL pincQuerySetFramebufferFormat(void) {
    PincValidateForState(PincState_init);
    return staticState.framebufferFormat;
}

PINC_EXPORT PincObjectType PINC_CALL pincGetObjectType(PincObjectHandle id) {
    PincValidateForState(PincState_init);
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    switch (obj.discriminator)
    {
    case PincObjectDiscriminator_none:
        return PincObjectType_none;
    case PincObjectDiscriminator_incompleteWindow:
    case PincObjectDiscriminator_window:
        return PincObjectType_window;
    case PincObjectDiscriminator_glContext:
    case PincObjectDiscriminator_incompleteGlContext:
        return PincObjectType_none;
    case PincObjectDiscriminator_framebufferFormat:
        return PincObjectType_none;
    }
    return PincObjectType_none;
}

PINC_EXPORT bool PINC_CALL pincGetObjectComplete(PincObjectHandle id) {
    PincValidateForState(PincState_init);
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    switch (obj.discriminator)
    {
    case PincObjectDiscriminator_none:
    case PincObjectDiscriminator_incompleteWindow:
    case PincObjectDiscriminator_incompleteGlContext:
        return false;
    case PincObjectDiscriminator_window:
    case PincObjectDiscriminator_glContext:
    case PincObjectDiscriminator_framebufferFormat:
        return true;
    }
    // This should be impossible, but GCC complains about it
    return false;
}

PINC_EXPORT void PINC_CALL pincSetObjectUserData(PincObjectHandle obj, void* user_data) {
    PincValidateForState(PincState_init);
    PErrorUser(obj <= staticState.objects.objectsNum, "Invalid object ID");
    PincObject* object = &((PincObject*)staticState.objects.objectsArray)[obj-1];
    PErrorUser(object->discriminator != PincObjectDiscriminator_none, "Cannot set user data of empty object");
    object->userData = user_data;
}

PINC_EXPORT void* PINC_CALL pincGetObjectUserData(PincObjectHandle obj) {
    PincValidateForState(PincState_init);
    PErrorUser(obj <= staticState.objects.objectsNum, "Invalid object ID");
    PincObject* object = &((PincObject*)staticState.objects.objectsArray)[obj-1];
    PErrorUser(object->discriminator != PincObjectDiscriminator_none, "Cannot get user data of empty object");
    return object->userData;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincWindowCreateIncomplete(void) {
    PincValidateForState(PincState_init);
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pincInitComplete?");
    PincWindowHandle handle = PincObject_allocate(PincObjectDiscriminator_incompleteWindow);
    IncompleteWindow* window = PincObject_ref_incompleteWindow(handle);
    // TODO: Add integer formatting to PString, or get a decent libc-free formatting library
    pincString strings[] = {
        pincString_makeDirect("Pinc Window "),
        pincString_allocFormatUint32(handle, tempAllocator),
    };
    pincString name = pincString_concat(sizeof(strings) / sizeof(pincString), strings, rootAllocator);
    *window = (IncompleteWindow){
        .title = name,
        .hasWidth = false,
        .width = 0,
        .hasHeight = false,
        .height = 0,
        .resizable = true,
        .minimized = false,
        .maximized = false,
        .fullscreen = false,
        .focused = false,
        .hidden = false,
    };
    return handle;
}

PINC_EXPORT PincReturnCode PINC_CALL pincWindowComplete(PincWindowHandle window) {\
    PincValidateForState(PincState_init);
    IncompleteWindow* object = PincObject_ref_incompleteWindow(window);
    WindowHandle handle = pincWindowBackend_completeWindow(&staticState.windowBackend, object, window);
    if(!handle) {
        return PincReturnCode_error;
    }
    PincObject_reallocate(window, PincObjectDiscriminator_window);
    WindowHandle* object2 = PincObject_ref_window(window);
    *object2 = handle;
    return PincReturnCode_pass;
}

PINC_EXPORT void PINC_CALL pincWindowDeinit(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    PincObjectDiscriminator tp = PincObject_discriminator(window);
    switch(tp) {
        case PincObjectDiscriminator_incompleteWindow:{
            PincObject_free(window);
            break;
        }
        case PincObjectDiscriminator_window:{
            WindowHandle* object = PincObject_ref_window(window);
            pincWindowBackend_deinitWindow(&staticState.windowBackend, *object);
            PincObject_free(window);
            break;
        }
        default:{
            PErrorUser(false, "Window is not a window object");
            break;
        }
    }
}

PINC_EXPORT void PINC_CALL pincWindowSetTitle(PincWindowHandle window, const char* title_buf, uint32_t title_len) {
    PincValidateForState(PincState_init);
    if(title_len == 0) {
        size_t realTitleLen = pincStringLen(title_buf);
        PErrorSanitize(realTitleLen <= UINT32_MAX, "Integer overflow");
        title_len = (uint32_t)realTitleLen;
    }
    switch (PincObject_discriminator(window))
    {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* object = PincObject_ref_incompleteWindow(window);
            if(title_len == object->title.len) {
                pincMemCopy(title_buf, object->title.str, title_len);
            } else {
                pincString_free(&object->title, rootAllocator);
                object->title = pincString_copy((pincString){.str = (uint8_t*)title_buf, .len = title_len}, rootAllocator);
            }
            break;
        }
        case PincObjectDiscriminator_window:{
            WindowHandle* object = PincObject_ref_window(window);
            // Window takes ownership of the pointer, but we don't have ownership of title_buf
            uint8_t* titlePtr = (uint8_t*)pincAllocator_allocate(rootAllocator, title_len);
            pincMemCopy(title_buf, titlePtr, title_len);
            pincWindowBackend_setWindowTitle(&staticState.windowBackend, *object, titlePtr, title_len);
            break;
        }
        default:
            PErrorUser(false, "Window is not a window object");
            break;
    }
}

PINC_EXPORT uint32_t PINC_CALL pincWindowGetTitle(PincWindowHandle window, char* title_buf, uint32_t title_capacity) {
    PincValidateForState(PincState_init);
    WindowHandle* win = PincObject_ref_window(window);
    size_t len;
    uint8_t const* title = pincWindowBackend_getWindowTitle(&staticState.windowBackend, *win, &len);
    PErrorAssert(len > UINT32_MAX, "Integer Overflow");
    if(title_buf) {
        uint32_t amountToWrite = title_capacity;
        if(title_capacity > len) {
            amountToWrite = (uint32_t)len;
        }
        pincMemCopy(title, title_buf, amountToWrite);
    }
    return (uint32_t) len;
}

PINC_EXPORT void PINC_CALL pincWindowSetWidth(PincWindowHandle window, uint32_t width) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->width = width;
            ob->hasWidth = true;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowWidth(&staticState.windowBackend, *ob, width);
            break;
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT uint32_t PINC_CALL pincWindowGetWidth(PincWindowHandle window) {
    PincValidateForState(PincState_init);
        switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            PErrorUser(ob->hasWidth, "Window does not have its width set");
            return ob->width;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowWidth(&staticState.windowBackend, *ob);
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT bool PINC_CALL pincWindowHasWidth(PincWindowHandle window) {
    PincValidateForState(PincState_init);
        switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->hasWidth;
        }
        case PincObjectDiscriminator_window: {
            return true;
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT void PINC_CALL pincWindowSetHeight(PincWindowHandle window, uint32_t height) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->height = height;
            ob->hasHeight = true;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowHeight(&staticState.windowBackend, *ob, height);
            break;
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT uint32_t PINC_CALL pincWindowGetHeight(PincWindowHandle window) {
    PincValidateForState(PincState_init);
        switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            PErrorUser(ob->hasHeight, "Window does not have its height set");
            return ob->height;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowHeight(&staticState.windowBackend, *ob);
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT bool PINC_CALL pincWindowHasHeight(PincWindowHandle window) {
    PincValidateForState(PincState_init);
        switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->hasHeight;
        }
        case PincObjectDiscriminator_window: {
            return true;
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT float PINC_CALL pincWindowGetScaleFactor(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    P_UNUSED(window);
    // TODO: probably want to refactor how scale factors work anyway
    return 1;
}

PINC_EXPORT bool PINC_CALL pincWindowHasScaleFactor(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    P_UNUSED(window);
    // TODO: probably want to refactor how scale factors work anyway
    return false;
}

PINC_EXPORT void PINC_CALL pincWindowSetResizable(PincWindowHandle window, bool resizable) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->resizable = resizable;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowResizable(&staticState.windowBackend, *ob, resizable);
            break;
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pincWindowGetResizable(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->resizable;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowResizable(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT void PINC_CALL pincWindowSetMinimized(PincWindowHandle window, bool minimized) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->minimized = minimized;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowMinimized(&staticState.windowBackend, *ob, minimized);
            break;
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pincWindowGetMinimized(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->minimized;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowMinimized(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT void PINC_CALL pincWindowSetMaximized(PincWindowHandle window, bool maximized) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->maximized = maximized;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowMaximized(&staticState.windowBackend, *ob, maximized);
            break;
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pincWindowGetMaximized(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->maximized;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowMaximized(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT void PINC_CALL pincWindowSetFullscreen(PincWindowHandle window, bool fullscreen) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->fullscreen = fullscreen;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowFullscreen(&staticState.windowBackend, *ob, fullscreen);
            break;
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pincWindowGetFullscreen(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->fullscreen;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowFullscreen(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT void PINC_CALL pincWindowSetFocused(PincWindowHandle window, bool focused) {
    PincValidateForState(PincState_init);
    // TODO: should this trigger a focus changed event?
    // TODO: Either way, it needs to set the current window.
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->focused = focused;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowFocused(&staticState.windowBackend, *ob, focused);
            break;
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pincWindowGetFocused(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->resizable;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowResizable(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT void PINC_CALL pincWindowSetHidden(PincWindowHandle window, bool hidden) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->hidden = hidden;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            pincWindowBackend_setWindowHidden(&staticState.windowBackend, *ob, hidden);
            break;
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pincWindowGetHidden(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->hidden;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return pincWindowBackend_getWindowHidden(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
    return 0;
}

PINC_EXPORT PincReturnCode PINC_CALL pincSetVsync(bool sync) {
    PincValidateForState(PincState_init);
    return pincWindowBackend_setVsync(&staticState.windowBackend, sync);
}

PINC_EXPORT bool PINC_CALL pincGetVsync(void) {
    PincValidateForState(PincState_init);
    return pincWindowBackend_getVsync(&staticState.windowBackend);
}

PINC_EXPORT void PINC_CALL pincWindowPresentFramebuffer(PincWindowHandle window) {
    PincValidateForState(PincState_init);
    WindowHandle* object = PincObject_ref_window(window);
    pincWindowBackend_windowPresentFramebuffer(&staticState.windowBackend, *object);
}

PINC_EXPORT void PINC_CALL pincStep(void) {
    PincValidateForState(PincState_init);
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pincInitComplete?");
    pincWindowBackend_step(&staticState.windowBackend);
    arena_reset(&staticState.arenaAllocatorObject);
    // Event buffer swap
    PincEvent* te = staticState.eventsBuffer;
    uint32_t tc = staticState.eventsBufferCapacity;
    staticState.eventsBuffer = staticState.eventsBufferBack;
    staticState.eventsBufferNum = staticState.eventsBufferBackNum;
    staticState.eventsBufferCapacity = staticState.eventsBufferBackCapacity;
    staticState.eventsBufferBack = te;
    staticState.eventsBufferBackCapacity = tc;
    staticState.eventsBufferBackNum = 0;

    staticState.currentWindow = staticState.realCurrentWindow;
}

PINC_EXPORT uint32_t PINC_CALL pincEventGetNum(void) {
    PincValidateForState(PincState_init);
    return staticState.eventsBufferNum;
}

PINC_EXPORT PincEventType PINC_CALL pincEventGetType(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    return staticState.eventsBuffer[event_index].type;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventGetWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    return staticState.eventsBuffer[event_index].currentWindow;
}

PINC_EXPORT int64_t PINC_CALL pincEventGetTimestampUnixMillis(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    return staticState.eventsBuffer[event_index].timeUnixMillis;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventCloseSignalWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_closeSignal, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.closeSignal.window;
}

PINC_EXPORT uint32_t PINC_CALL pincEventMouseButtonOldState(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_mouseButton, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.mouseButton.oldState;
}

PINC_EXPORT uint32_t PINC_CALL pincEventMouseButtonState(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_mouseButton, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.mouseButton.state;
}

PINC_EXPORT uint32_t PINC_CALL pincEventResizeOldWidth(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.oldWidth;
}

PINC_EXPORT uint32_t PINC_CALL pincEventResizeOldHeight(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.oldHeight;
}

PINC_EXPORT uint32_t PINC_CALL pincEventResizeWidth(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.width;
}

PINC_EXPORT uint32_t PINC_CALL pincEventResizeHeight(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.height;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventResizeWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.window;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventFocusOldWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_focus, "Wrong event type");
    return staticState.eventsBuffer[event_index].currentWindow;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventFocusWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_focus, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.focus.newWindow;
}

PINC_EXPORT uint32_t PINC_CALL pincEventExposureX(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.x;
}

PINC_EXPORT uint32_t PINC_CALL pincEventExposureY(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.y;
}

PINC_EXPORT uint32_t PINC_CALL pincEventExposureWidth(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.width;
}

PINC_EXPORT uint32_t PINC_CALL pincEventExposureHeight(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.height;
}

PINC_EXPORT uint32_t PINC_CALL pincEventExposureWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.window;
}

PINC_EXPORT PincKeyboardKey PINC_CALL pincEventKeyboardButtonKey(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_keyboardButton, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.keyboardButton.key;
}

PINC_EXPORT bool PINC_CALL pincEventKeyboardButtonState(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_keyboardButton, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.keyboardButton.state;
}

PINC_EXPORT bool PINC_CALL pincEventKeyboardButtonRepeat(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_keyboardButton, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.keyboardButton.repeat;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorMoveOldX(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorMove, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.oldX;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorMoveOldY(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorMove, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.oldY;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorMoveX(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorMove, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.x;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorMoveY(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorMove, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.y;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventCursorMoveWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorMove, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.window;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorTransitionOldX(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorTransition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.oldX;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorTransitionOldY(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorTransition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.oldY;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventCursorTransitionOldWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorTransition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.oldWindow;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorTransitionX(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorTransition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.x;
}

PINC_EXPORT uint32_t PINC_CALL pincEventCursorTransitionY(uint32_t event_index){
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorTransition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.y;
}

PINC_EXPORT PincWindowHandle PINC_CALL pincEventCursorTransitionWindow(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_cursorTransition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.window;
}

PINC_EXPORT uint32_t PINC_CALL pincEventTextInputCodepoint(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_textInput, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.textInput.codepoint;
}

PINC_EXPORT float PINC_CALL pincEventScrollVertical(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_scroll, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.scroll.vertical;
}

PINC_EXPORT float PINC_CALL pincEventScrollHorizontal(uint32_t event_index) {
    PincValidateForState(PincState_init);
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == PincEventType_scroll, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.scroll.horizontal;
}

// ### OPENGL FUNCTIONS ###
#include <pinc_opengl.h>

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglVersionSupported(PincWindowBackend backend, uint32_t major, uint32_t minor, PincOpenglContextProfile profile) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    PErrorUser(backend == PincWindowBackend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");

    return pincWindowBackend_queryGlVersionSupported(&staticState.sdl2WindowBackend, major, minor, profile);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglAccumulatorBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer_format_id, uint32_t channel, uint32_t bits){
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pincQueryFramebufferFormatDefault(backend, PincGraphicsApi_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return pincWindowBackend_queryGlAccumulatorBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, channel, bits);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglAlphaBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer_format_id, uint32_t bits) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pincQueryFramebufferFormatDefault(backend, PincGraphicsApi_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return pincWindowBackend_queryGlAlphaBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, bits);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglDepthBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer_format_id, uint32_t bits) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pincQueryFramebufferFormatDefault(backend, PincGraphicsApi_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return pincWindowBackend_queryGlDepthBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, bits);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglStencilBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer_format_id, uint32_t bits) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pincQueryFramebufferFormatDefault(backend, PincGraphicsApi_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return pincWindowBackend_queryGlStencilBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, bits);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglSamples(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer_format_handle, uint32_t samples) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    if(framebuffer_format_handle == 0) {
        framebuffer_format_handle = pincQueryFramebufferFormatDefault(backend, PincGraphicsApi_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_handle);
    return pincWindowBackend_queryGlSamples(&staticState.sdl2WindowBackend, *framebufferFormatObj, samples);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglStereoBuffer(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer_format_id) {
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pincQueryFramebufferFormatDefault(backend, PincGraphicsApi_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return pincWindowBackend_queryGlStereoBuffer(&staticState.sdl2WindowBackend, *framebufferFormatObj);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglContextDebug(PincWindowBackend backend){
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    PErrorUser(backend == PincWindowBackend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return pincWindowBackend_queryGlContextDebug(&staticState.sdl2WindowBackend);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglRobustAccess(PincWindowBackend backend){
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    PErrorUser(backend == PincWindowBackend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return pincWindowBackend_queryGlRobustAccess(&staticState.sdl2WindowBackend);
}

PINC_EXPORT PincOpenglSupportStatus PINC_CALL pincQueryOpenglResetIsolation(PincWindowBackend backend){
    PincValidateForStates(PincState_init, PincState_incomplete);
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == PincWindowBackend_any) {
        backend = PincWindowBackend_sdl2;
    }
    PErrorUser(backend == PincWindowBackend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return pincWindowBackend_queryGlResetIsolation(&staticState.sdl2WindowBackend);
}

PINC_EXPORT PincOpenglContextHandle PINC_CALL pincOpenglCreateContextIncomplete(void) {
    PincValidateForState(PincState_init);
    PincOpenglContextHandle id = PincObject_allocate(PincObjectDiscriminator_incompleteGlContext);
    IncompleteGlContext* obj = PincObject_ref_incompleteGlContext(id);
    
    *obj = (IncompleteGlContext) {
        .accumulatorBits = {0, 0, 0, 0},
        .alphaBits = 0,
        .depthBits = 16,
        .stencilBits = 0,
        .stereo = false,
        .debug = false,
        .robustAccess = false,
        // The reasoning for this default is that OpenGL 1.2 is basically the absolute most widely supported version - more or less 100% of OpenGL hardware supports it.
        // With that said, 99% of users should override the default.
        .versionMajor = 1,
        .versionMinor = 2,
        .profile = PincOpenglContextProfile_core,
    };

    return id;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextAccumulatorBits(PincOpenglContextHandle incomplete_context, uint32_t channel, uint32_t bits) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    PErrorUser(channel < 4, "Invalid channel index: must be less than 4");
    contextObject->accumulatorBits[channel] = bits;
    return PincReturnCode_pass;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextAlphaBits(PincOpenglContextHandle incomplete_context, uint32_t bits) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->alphaBits = bits;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextDepthBits(PincOpenglContextHandle incomplete_context, uint32_t bits) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->depthBits = bits;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextStencilBits(PincOpenglContextHandle incomplete_context, uint32_t bits) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->stencilBits = bits;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextSamples(PincOpenglContextHandle incomplete_context, uint32_t samples) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->samples = samples;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextStereoBuffer(PincOpenglContextHandle incomplete_context, bool stereo) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->stereo = stereo;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextDebug(PincOpenglContextHandle incomplete_context, bool debug) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->debug = debug;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextRobustAccess(PincOpenglContextHandle incomplete_context, bool robust) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->robustAccess = robust;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextResetIsolation(PincOpenglContextHandle incomplete_context, bool isolation) {
    PincValidateForState(PincState_init);
    PErrorUser(PincObject_discriminator(incomplete_context) == PincObjectDiscriminator_incompleteGlContext, "Object must be an incomplete OpenGL context");
    IncompleteGlContext* contextObject = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObject->resetIsolation = isolation;
    return PincReturnCode_error;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglSetContextVersion(PincOpenglContextHandle incomplete_context, uint32_t major, uint32_t minor, PincOpenglContextProfile profile) {
    PincValidateForState(PincState_init);
    if(pincQueryOpenglVersionSupported(PincWindowBackend_any, major, minor, profile) == PincOpenglSupportStatus_none) {
        PErrorExternal(false, "Opengl version not supported");
        return PincReturnCode_error;
    }

    IncompleteGlContext* contextObj = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObj->versionMajor = major;
    contextObj->versionMinor = minor;
    contextObj->profile = profile;
    return PincReturnCode_pass;
}

PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextShareWithCurrent(PincOpenglContextHandle incomplete_context_handle, bool share) {
    PincValidateForState(PincState_init);
    IncompleteGlContext* contextObj = PincObject_ref_incompleteGlContext(incomplete_context_handle);
    contextObj->shareWithCurrent = share;
    return PincReturnCode_pass;
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglCompleteContext(PincOpenglContextHandle incomplete_context) {
    PincValidateForState(PincState_init);
    IncompleteGlContext* contextObj = PincObject_ref_incompleteGlContext(incomplete_context);
    RawOpenglContextHandle contextHandle = pincWindowBackend_glCompleteContext(&staticState.windowBackend, *contextObj);
    if(contextHandle == 0) {
        return PincReturnCode_error;
    }
    PincObject_reallocate(incomplete_context, PincObjectDiscriminator_glContext);
    RawOpenglContextObject* handleObject = PincObject_ref_glContext(incomplete_context);
    handleObject->handle = contextHandle;
    handleObject->front_handle = incomplete_context;
    return PincReturnCode_pass;
}

PINC_EXPORT void PINC_CALL pincOpenglDeinitContext(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            PincObject_free(context);
            break;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            pincWindowBackend_glDeinitContext(&staticState.windowBackend, *object);
            PincObject_free(context);
            break;
        }
        default:
            PErrorUser(false, "Object must be an opengl context");
    }
}

PINC_EXPORT uint32_t PINC_CALL pincOpenglGetContextAccumulatorBits(PincOpenglContextHandle context, uint32_t channel) {
    PincValidateForState(PincState_init);
    PErrorUser(channel < 4, "Invalid channel index: must be less than 4");
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->accumulatorBits[channel];
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextAccumulatorBits(&staticState.windowBackend, *object, channel);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT uint32_t PINC_CALL pincOpenglGetContextAlphaBits(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->alphaBits;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextAlphaBits(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT uint32_t PINC_CALL pincOpenglGetContextDepthBits(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->depthBits;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextDepthBits(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT uint32_t PINC_CALL pincOpenglGetContextStencilBits(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->stencilBits;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextStencilBits(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT uint32_t PINC_CALL pincOpenglGetContextSamples(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->samples;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextSamples(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT bool PINC_CALL pincOpenglGetContextStereoBuffer(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->stereo;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextStereoBuffer(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT bool PINC_CALL pincOpenglGetContextDebug(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->debug;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextDebug(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT bool PINC_CALL pincOpenglGetContextRobustAccess(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->robustAccess;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextRobustAccess(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT bool PINC_CALL pincOpenglGetContextResetIsolation(PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    switch(PincObject_discriminator(context)) {
        case PincObjectDiscriminator_incompleteGlContext: {
            IncompleteGlContext* object = PincObject_ref_incompleteGlContext(context);
            return object->resetIsolation;
        }
        case PincObjectDiscriminator_glContext: {
            RawOpenglContextObject* object = PincObject_ref_glContext(context);
            return pincWindowBackend_glGetContextResetIsolation(&staticState.windowBackend, *object);
        }
        default:
            PErrorUser(false, "Object must be an OpenGL context");
            return 0;
    }
}

PINC_EXPORT PincReturnCode PINC_CALL pincOpenglMakeCurrent(PincWindowHandle window, PincOpenglContextHandle context) {
    PincValidateForState(PincState_init);
    WindowHandle windowObj = 0;
    if(window != 0) {
        windowObj = *PincObject_ref_window(window);
    }
    RawOpenglContextObject contextObj = { 0 };
    if(context != 0){
        contextObj = *PincObject_ref_glContext(context);
    }
    return pincWindowBackend_glMakeCurrent(&staticState.windowBackend, windowObj, contextObj.handle);
}

PINC_EXPORT PincWindowHandle PINC_CALL pincOpenglGetCurrentWindow(void) {
    PincValidateForState(PincState_init);
    return pincWindowBackend_glGetCurrentWindow(&staticState.windowBackend);
}

PINC_EXPORT PincOpenglContextHandle PINC_CALL pincOpenglGetCurrentContext(void) {
    PincValidateForState(PincState_init);
    return pincWindowBackend_glGetCurrentContext(&staticState.windowBackend);
}

PINC_EXPORT PincPfn PINC_CALL pincOpenglGetProc(char const * procname) {
    PincValidateForState(PincState_init);
    return pincWindowBackend_glGetProc(&staticState.windowBackend, procname);
}
