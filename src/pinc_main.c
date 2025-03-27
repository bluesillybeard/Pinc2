#include <pinc.h>
#include <stdint.h>
#include "libs/dynamic_allocator.h"
#include "pinc_error.h"
#include "pinc_types.h"
#include "platform/platform.h"
#include "pinc_main.h"

#include "sdl2.h"
#include "window.h"

// Implementations of things in pinc_main.h

void pinc_intern_callError(PString message, pinc_error_type type) {
    if(rootAllocator.vtable == 0) {
        // This assert is not part of the Pinc error system... because if it was, we would have an infinite loop!
        // The temp allocator is set before any errors have the potential to occur, so this should never happen.
        pAssertFail();
    }
    if(staticState.userCallError) {
        // Let's be nice and let the user have their null terminator
        uint8_t* msgNullTerm = (uint8_t*)PString_marshalAlloc(message, tempAllocator);
        staticState.userCallError(msgNullTerm, message.len, type);
        Allocator_free(tempAllocator, msgNullTerm, message.len+1);
    } else {
        pPrintError(message.str, message.len);
    }
    pTriggerDebugger();
}

P_NORETURN void pinc_intern_callFatalError(PString message, pinc_error_type type) {
    if(rootAllocator.vtable == 0) {
        // This assert is not part of the Pinc error system... because if it was, we would have an infinite loop!
        // The temp allocator is set before any errors have the potential to occur, so this should never happen.
        pAssertFail();
    }
    if(staticState.userCallError) {
        // Let's be nice and let the user have their null terminator
        uint8_t* msgNullTerm = (uint8_t*)PString_marshalAlloc(message, tempAllocator);
        staticState.userCallError(msgNullTerm, message.len, type);
        Allocator_free(tempAllocator, msgNullTerm, message.len+1);
    } else {
        pPrintError(message.str, message.len);
    }
    pAssertFail();
}

PincStaticState pinc_intern_staticState = PINC_PREINIT_STATE;

// Implementation of pinc's root allocator on top of platform.h
static void* pinc_root_platform_allocate(void* obj, size_t size) {
    P_UNUSED(obj);
    return pAlloc(size);
}

static void* pinc_root_platform_allocateAligned(void* obj, size_t size, size_t alignment) {
    P_UNUSED(obj);
    return pAllocAligned(size, alignment);
}

static void* pinc_root_platform_reallocate(void* obj, void* ptr, size_t oldSize, size_t newSize) {
    P_UNUSED(obj);
    return pRealloc(ptr, oldSize, newSize);
}

static void pinc_root_platform_free(void* obj, void* ptr, size_t size) {
    P_UNUSED(obj);
    pFree(ptr, size);
}

static const AllocatorVtable platform_alloc_vtable = {
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

static const AllocatorVtable user_alloc_vtable = {
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

static const AllocatorVtable PincTempAllocatorVtable = {
    .allocate = &pinc_temp_allocate,
    .allocateAligned = &pinc_temp_allocateAligned,
    .reallocate = &pinc_temp_reallocate,
    .free = &pinc_temp_free,
};

uint32_t PincPool_alloc(PincPool* pool, size_t elementSize) {
    if(pool->objectsCapacity == pool->objectsNum) {
        if(!pool->objectsArray) {
            pool->objectsArray = Allocator_allocate(rootAllocator, elementSize * 8);
            pool->objectsCapacity = 8;
        } else {
            uint32_t newObjectsCapacity = pool->objectsCapacity * 2;
            pool->objectsArray = Allocator_reallocate(rootAllocator, pool->objectsArray, elementSize * pool->objectsCapacity, elementSize * newObjectsCapacity);
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
                pool->freeArray = Allocator_allocate(rootAllocator, elementSize * 8);
                pool->freeArrayCapacity = 8;
            } else {
                uint32_t newObjectsCapacity = pool->freeArrayCapacity * 2;
                pool->freeArray = Allocator_reallocate(rootAllocator, pool->freeArray, elementSize * pool->freeArrayCapacity, elementSize * newObjectsCapacity);
                pool->freeArrayCapacity = newObjectsCapacity;
            }
        }
        pool->freeArray[pool->freeArrayNum] = index;
        pool->freeArrayNum++;
    }
}

void PincPool_deinit(PincPool* pool, size_t elementSize) {
    if(pool->objectsArray) {
        Allocator_free(rootAllocator, pool->objectsArray, elementSize * pool->objectsCapacity);
    }
    if(pool->freeArray) {
        Allocator_free(rootAllocator, pool->freeArray, sizeof(uint32_t) * pool->freeArrayCapacity);
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

pinc_object PincObject_allocate(PincObjectDiscriminator discriminator) {
    pinc_object object_index = PincPool_alloc(&staticState.objects, sizeof(PincObject));
    PincObject *obj = &((PincObject*)staticState.objects.objectsArray)[object_index];
    PErrorSanitize(staticState.objects.objectsNum < UINT32_MAX, "Integer overflow");
    obj->discriminator = discriminator;
    obj->internalIndex = PincObject_allocateInternal(discriminator);
    obj->userData = 0;
    return object_index+1;
}

void PincObject_reallocate(pinc_object id, PincObjectDiscriminator discriminator) {
    PErrorAssert(staticState.objects.objectsNum >= id, "Object ID out of bounds");
    PincObject *obj = &((PincObject*)staticState.objects.objectsArray)[id-1];
    PincObject_freeInternal(obj->discriminator, obj->internalIndex);
    obj->discriminator = discriminator;
    obj->internalIndex = PincObject_allocateInternal(discriminator);
    // The user data is left as-is, so people don't confused of why their user data is reset when they complete an object.
}

void PincObject_free(pinc_object id) {
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
        staticState.eventsBufferBack = Allocator_allocate(rootAllocator, 8 * sizeof(PincEvent));
        staticState.eventsBufferBackCapacity = 8;
        staticState.eventsBufferBackNum = 0;
    } else {
        uint32_t newCapacity = staticState.eventsBufferBackCapacity * 2;
        staticState.eventsBufferBack = Allocator_reallocate(rootAllocator, staticState.eventsBufferBack, staticState.eventsBufferBackCapacity * sizeof(PincEvent), newCapacity * sizeof(PincEvent));
        staticState.eventsBufferBackCapacity = newCapacity;
    }
}

static inline void PincEventBackAppend(PincEvent* ev) {
    PincEventBackEnsureCapacity(staticState.eventsBufferBackNum+1);
    staticState.eventsBufferBack[staticState.eventsBufferBackNum] = *ev;
    staticState.eventsBufferBackNum += 1;
}

void PincEventCloseSignal(int64_t timeUnixMillis, pinc_window window) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_close_signal,
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
        .type = pinc_event_type_mouse_button,
        .data.mouseButton = {
            .oldState = oldState,
            .state = state,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventResize(int64_t timeUnixMillis, pinc_window window, uint32_t oldWidth, uint32_t oldHeight, uint32_t width, uint32_t height) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_resize,
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

void PincEventFocus(int64_t timeUnixMillis, pinc_window window) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_focus,
        .data.focus = {
            .newWindow = window,
        },
    };
    staticState.realCurrentWindow = window;
    PincEventBackAppend(&e);
}

void PincEventExposure(int64_t timeUnixMillis, pinc_window window, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_exposure,
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

void PincEventKeyboardButton(int64_t timeUnixMillis, pinc_keyboard_key key, bool state, bool repeat) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_keyboard_button,
        .data.keyboardButton = {
            .key = key,
            .state = state,
            .repeat = repeat,
        },
    };
    PincEventBackAppend(&e);
}

void PincEventCursorMove(int64_t timeUnixMillis, pinc_window window, uint32_t oldX, uint32_t oldY, uint32_t x, uint32_t y) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_cursor_move,
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

void PincEventCursorTransition(int64_t timeUnixMillis, pinc_window oldWindow, uint32_t oldX, uint32_t oldY, pinc_window window, uint32_t x, uint32_t y) {
    PincEvent e = {
        .currentWindow = staticState.realCurrentWindow,
        .timeUnixMillis = timeUnixMillis,
        .type = pinc_event_type_cursor_transition,
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
        .type = pinc_event_type_text_input,
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
        .type = pinc_event_type_scroll,
        .data.scroll = {
            .vertical = vertical,
            .horizontal = horizontal,
        },
    };
    PincEventBackAppend(&e);
}

// Below are the actual implementations of the Pinc API functions.

PINC_EXPORT void PINC_CALL pinc_preinit_set_error_callback(pinc_error_callback callback) {
    staticState.userCallError = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_alloc_callbacks(void* user_ptr, pinc_alloc_callback alloc, pinc_alloc_aligned_callback alloc_aligned, pinc_realloc_callback realloc, pinc_free_callback free) {
    staticState.userAllocObj = user_ptr;
    staticState.userAllocFn = alloc;
    staticState.userAllocAlignedFn = alloc_aligned;
    staticState.userReallocFn = realloc;
    staticState.userFreeFn = free;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_incomplete_init(void) {
    // First up, allocator needs set up
    PErrorUser(
        (staticState.userAllocFn && staticState.userAllocAlignedFn && staticState.userReallocFn && staticState.userFreeFn)
        || !(staticState.userAllocFn || staticState.userAllocAlignedFn || staticState.userReallocFn || staticState.userFreeFn), 
        "Pinc allocator callbacks must either be all set or all null!");
    
    if(staticState.userAllocFn) {
        // User callbacks are set
        rootAllocator = (Allocator) {
            .allocatorObjectPtr = staticState.userAllocObj,
            .vtable = &user_alloc_vtable,
        };
    } else {
        // user callbacks are not set
        rootAllocator = (Allocator){
            .allocatorObjectPtr = 0,
            .vtable = &platform_alloc_vtable,
        };
    }

    staticState.arenaAllocatorObject = (Arena){0};

    tempAllocator = (Allocator) {
        .allocatorObjectPtr = 0,
        .vtable = &PincTempAllocatorVtable,
    };

    // Begin initialization of window backends
    // TODO: SDL2 is the only one that actually exists so yeah

    bool sdl2InitRes = psdl2Init(&staticState.sdl2WindowBackend);

    if(!sdl2InitRes) {
        PErrorExternal(false, "No supported window backends available!");
        return pinc_return_code_error;
    }

    // Query collective information from window backends
    // Again, SDL2 is the only window backend, so some shortcuts can be taken
    size_t numFramebufferFormats;
    FramebufferFormat* framebufferFormats = WindowBackend_queryFramebufferFormats(&staticState.sdl2WindowBackend, tempAllocator, &numFramebufferFormats);
    for(size_t i=0; i<numFramebufferFormats; ++i) {
        pinc_framebuffer_format handle = PincObject_allocate(PincObjectDiscriminator_framebufferFormat);
        FramebufferFormat* reference = PincObject_ref_framebufferFormat(handle);
        *reference = framebufferFormats[i];
    }

    Allocator_free(tempAllocator, framebufferFormats, numFramebufferFormats*sizeof(FramebufferFormat));
    
    return pinc_return_code_pass;
}

PINC_EXPORT bool PINC_CALL pinc_query_window_backend_support(pinc_window_backend window_backend) {
    // TODO: only backend is SDL2, shortcuts are taken
    #if PINC_HAVE_WINDOW_SDL2 == 1
    if(window_backend == pinc_window_backend_sdl2) return true;
    #endif
    return false;
}

PINC_EXPORT pinc_window_backend PINC_CALL pinc_query_window_backend_default(void) {
    // TODO: only window backend is SDL2, shortcuts are taken
    return pinc_window_backend_sdl2;
}

PINC_EXPORT bool PINC_CALL pinc_query_graphics_api_support(pinc_window_backend window_backend, pinc_graphics_api graphics_api) {
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    return WindowBackend_queryGraphicsApiSupport(&staticState.sdl2WindowBackend, graphics_api);
}

PINC_EXPORT pinc_graphics_api PINC_CALL pinc_query_graphics_api_default(pinc_window_backend window_backend) {
    // TODO: only api is opengl, shortcuts are taken
    P_UNUSED(window_backend);
    return pinc_graphics_api_opengl;
}

PINC_EXPORT pinc_framebuffer_format pinc_query_framebuffer_format_default(pinc_window_backend window_backend, pinc_graphics_api graphics_api) {
    // Use the front-end API to do what a user would effectively do
    uint32_t numFramebufferFormats = pinc_query_framebuffer_format_ids(window_backend, graphics_api, 0, 0);
    PErrorExternal(numFramebufferFormats, "No framebuffer formats available");
    uint32_t* ids = Allocator_allocate(tempAllocator, numFramebufferFormats * sizeof(uint32_t));
    pinc_query_framebuffer_format_ids(window_backend, graphics_api, ids, numFramebufferFormats);
    
    // framebuffer format default can be tuned here
    uint32_t const score_per_channel = 2;
    uint32_t const score_per_bit = 1;
    uint32_t const score_for_srgb = 16;

    pinc_framebuffer_format bestFormatHandle = ids[0];
    uint32_t bestScore = 0;
    for(uint32_t i=1; i<numFramebufferFormats; ++i) {
        pinc_framebuffer_format fmt = ids[i];
        uint32_t channels = pinc_query_framebuffer_format_channels(fmt);
        PErrorAssert(channels <= 4, "Invalid number of channels");
        uint32_t score = channels * score_per_channel;
        for(uint32_t c=0; c<channels; ++c) {
            score += pinc_query_framebuffer_format_channel_bits(fmt, c) * score_per_bit;
        }
        if(pinc_query_framebuffer_format_color_space(fmt) == pinc_color_space_srgb) {
            score += score_for_srgb;
        }
        if(score > bestScore) {
            bestFormatHandle = fmt;
            bestScore = score;
        }
    }
    return bestFormatHandle;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_ids(pinc_window_backend window_backend, pinc_graphics_api graphics_api, pinc_framebuffer_format* ids_dest, uint32_t capacity) {
    P_UNUSED(window_backend);
    P_UNUSED(graphics_api);
    // TODO: only window backend is SDL2, shortcuts are taken
    // TODO: sort framebuffers from best to worst, so applications can just loop from first to last and pick the first one they see that they like
    // - Note: probably best to do this in init when all of the framebuffer formats are queried to begin with
    // TODO: it's probably a good idea to make all object types have a way to get their ID instead of having to search the whole map
    // - While you're at it, it might be worth adding generation info to the handle to catch use after frees
    pinc_object currentObject = 1;
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

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id) {
    return PincObject_ref_framebufferFormat(format_id)->channels;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, uint32_t channel) {
    FramebufferFormat* obj = PincObject_ref_framebufferFormat(format_id);

    PErrorUser(channel < obj->channels, "channel index out of bounds - did you make sure it's less than what pinc_query_framebuffer_format_channels returns for this format?");
    return obj->channel_bits[channel];
}

PINC_EXPORT pinc_color_space PINC_CALL pinc_query_framebuffer_format_color_space(pinc_framebuffer_format format_id) {
    return PincObject_ref_framebufferFormat(format_id)->color_space;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend) {
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    return WindowBackend_queryMaxOpenWindows(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_complete_init(pinc_window_backend window_backend, pinc_graphics_api graphics_api, pinc_framebuffer_format framebuffer_format_id, uint32_t samples, uint32_t depth_buffer_bits) {
    // TODO: only window backend is SDL2, shortcuts are taken
    PErrorUser(window_backend != pinc_window_backend_none, "Unsupported window backend");
    if(window_backend == pinc_window_backend_any) {
        window_backend = pinc_window_backend_sdl2;
    }
    // TODO: only graphics api is opengl, shortcuts are taken
    if(graphics_api == pinc_graphics_api_any) {
        graphics_api = pinc_graphics_api_opengl;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pinc_query_framebuffer_format_default(window_backend, graphics_api);
    }
    FramebufferFormat* framebuffer = PincObject_ref_framebufferFormat(framebuffer_format_id);
    staticState.framebufferFormat = framebuffer_format_id;
    pinc_return_code result = WindowBackend_completeInit(&staticState.sdl2WindowBackend, graphics_api, *framebuffer, samples, depth_buffer_bits);
    if(result == pinc_return_code_error) {
        return pinc_return_code_error;
    }
    staticState.windowBackend = staticState.sdl2WindowBackend;
    staticState.windowBackendSet = true;

    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_deinit(void) {
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
            pinc_object id = i+1;
            switch (pinc_get_object_type(id))
            {
                case pinc_object_type_none: {
                    break;
                }
                case pinc_object_type_window: {
                    pinc_window_deinit(id);
                    break;
                }
                default:
                    PErrorAssert(false, "Invalid object type! This is an error in Pinc itself.");
            }
        }
    }

    // deinit backends
    // TODO: only window backend is sdl2, shortcuts are taken
    if(staticState.windowBackendSet) {
        WindowBackend_deinit(&staticState.windowBackend);
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

    Allocator_free(rootAllocator, staticState.eventsBuffer, staticState.eventsBufferNum * sizeof(PincEvent));
    Allocator_free(rootAllocator, staticState.eventsBufferBack, staticState.eventsBufferBackNum * sizeof(PincEvent));

    if(staticState.tempAlloc.vtable){
        arena_free(&staticState.arenaAllocatorObject);
    }

    // Full reset the state
    staticState = (PincStaticState) PINC_PREINIT_STATE;
}

PINC_EXPORT pinc_window_backend PINC_CALL pinc_query_set_window_backend(void) {
    // TODO: only backend is SDL2, shortcuts are taken
    return pinc_window_backend_sdl2;
}

PINC_EXPORT pinc_graphics_api PINC_CALL pinc_query_set_graphics_api(void) {
    // TODO: only backend is opengl, shortcuts are taken
    return pinc_graphics_api_opengl;
}

PINC_EXPORT pinc_framebuffer_format PINC_CALL pinc_query_set_framebuffer_format(void) {
    if(staticState.framebufferFormat) {
        return staticState.framebufferFormat;
    } else {
        PErrorUser(false, "Framebuffer format is not determined until pinc_complete_init");
        return 0;
    }
}

PINC_EXPORT pinc_object_type PINC_CALL pinc_get_object_type(pinc_object id) {
    PErrorUser(id <= staticState.objects.objectsNum, "Invalid object id");
    PincObject obj = ((PincObject*)staticState.objects.objectsArray)[id-1];
    switch (obj.discriminator)
    {
    case PincObjectDiscriminator_none:
        return pinc_object_type_none;
    case PincObjectDiscriminator_incompleteWindow:
    case PincObjectDiscriminator_window:
        return pinc_object_type_window;
    case PincObjectDiscriminator_glContext:
    case PincObjectDiscriminator_incompleteGlContext:
        return pinc_object_type_none;
    case PincObjectDiscriminator_framebufferFormat:
        return pinc_object_type_none;
    }
}

PINC_EXPORT bool PINC_CALL pinc_get_object_complete(pinc_object id) {
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
}

PINC_EXPORT void PINC_CALL pinc_set_object_user_data(pinc_object obj, void* user_data) {
    PErrorUser(obj <= staticState.objects.objectsNum, "Invalid object ID");
    PincObject* object = &((PincObject*)staticState.objects.objectsArray)[obj-1];
    PErrorUser(object->discriminator != PincObjectDiscriminator_none, "Cannot set user data of empty object");
    object->userData = user_data;
}

PINC_EXPORT void* PINC_CALL pinc_get_object_user_data(pinc_object obj) {
    PErrorUser(obj <= staticState.objects.objectsNum, "Invalid object ID");
    PincObject* object = &((PincObject*)staticState.objects.objectsArray)[obj-1];
    PErrorUser(object->discriminator != PincObjectDiscriminator_none, "Cannot get user data of empty object");
    return object->userData;
}

PINC_EXPORT pinc_window PINC_CALL pinc_window_create_incomplete(void) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    pinc_window handle = PincObject_allocate(PincObjectDiscriminator_incompleteWindow);
    IncompleteWindow* window = PincObject_ref_incompleteWindow(handle);
    // TODO: Add integer formatting to PString, or get a decent libc-free formatting library
    char namebuf[24];
    size_t nameLen = 0;
    pMemCopy("Pinc window ", namebuf, 12);
    nameLen += 12;
    nameLen += pBufPrintUint32(namebuf+12, 12, handle);
    char* name = Allocator_allocate(rootAllocator, nameLen);
    pMemCopy(namebuf, name, nameLen);
    *window = (IncompleteWindow){
        .title = (PString){ .str = (uint8_t*)name, .len = nameLen},
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

PINC_EXPORT pinc_return_code PINC_CALL pinc_window_complete(pinc_window window) {\
    IncompleteWindow* object = PincObject_ref_incompleteWindow(window);
    WindowHandle handle = WindowBackend_completeWindow(&staticState.windowBackend, object, window);
    if(!handle) {
        return pinc_return_code_error;
    }
    PincObject_reallocate(window, PincObjectDiscriminator_window);
    WindowHandle* object2 = PincObject_ref_window(window);
    *object2 = handle;
    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_window_deinit(pinc_window window) {
    PincObjectDiscriminator tp = PincObject_discriminator(window);
    switch(tp) {
        case PincObjectDiscriminator_incompleteWindow:{
            PincObject_free(window);
            break;
        }
        case PincObjectDiscriminator_window:{
            WindowHandle* object = PincObject_ref_window(window);
            WindowBackend_deinitWindow(&staticState.windowBackend, *object);
            PincObject_free(window);
            break;
        }
        default:{
            PErrorUser(false, "Window is not a window object");
            break;
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_title(pinc_window window, const char* title_buf, uint32_t title_len) {
    if(title_len == 0) {
        size_t realTitleLen = pStringLen(title_buf);
        PErrorSanitize(realTitleLen <= UINT32_MAX, "Integer overflow");
        title_len = (uint32_t)realTitleLen;
    }
    switch (PincObject_discriminator(window))
    {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* object = PincObject_ref_incompleteWindow(window);
            if(title_len == object->title.len) {
                pMemCopy(title_buf, object->title.str, title_len);
            } else {
                PString_free(&object->title, rootAllocator);
                object->title = PString_copy((PString){.str = (uint8_t*)title_buf, .len = title_len}, rootAllocator);
            }
            break;
        }
        case PincObjectDiscriminator_window:{
            WindowHandle* object = PincObject_ref_window(window);
            // Window takes ownership of the pointer, but we don't have ownership of title_buf
            uint8_t* titlePtr = (uint8_t*)Allocator_allocate(rootAllocator, title_len);
            pMemCopy(title_buf, titlePtr, title_len);
            WindowBackend_setWindowTitle(&staticState.windowBackend, *object, titlePtr, title_len);
            break;
        }
        default:
            PErrorUser(false, "Window is not a window object");
            break;
    }
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_title(pinc_window window, char* title_buf, uint32_t title_capacity) {
    WindowHandle* win = PincObject_ref_window(window);
    size_t len;
    uint8_t const* title = WindowBackend_getWindowTitle(&staticState.windowBackend, *win, &len);
    PErrorAssert(len > UINT32_MAX, "Integer Overflow");
    if(title_buf) {
        uint32_t amountToWrite = title_capacity;
        if(title_capacity > len) {
            amountToWrite = (uint32_t)len;
        }
        pMemCopy(title, title_buf, amountToWrite);
    }
    return (uint32_t) len;
}

PINC_EXPORT void PINC_CALL pinc_window_set_width(pinc_window window, uint32_t width) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->width = width;
            ob->hasWidth = true;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowWidth(&staticState.windowBackend, *ob, width);
            break;
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_width(pinc_window window) {
        switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            PErrorUser(ob->hasWidth, "Window does not have its width set");
            return ob->width;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowWidth(&staticState.windowBackend, *ob);
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_has_width(pinc_window window) {
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
}

PINC_EXPORT void PINC_CALL pinc_window_set_height(pinc_window window, uint32_t height) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->height = height;
            ob->hasHeight = true;
            break;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowHeight(&staticState.windowBackend, *ob, height);
            break;
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_height(pinc_window window) {
        switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow:{
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            PErrorUser(ob->hasHeight, "Window does not have its height set");
            return ob->height;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowHeight(&staticState.windowBackend, *ob);
        }
        default: {
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_has_height(pinc_window window) {
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
}

PINC_EXPORT float PINC_CALL pinc_window_get_scale_factor(pinc_window window) {
    P_UNUSED(window);
    // TODO: probably want to refactor how scale factors work anyway
    return 1;
}

PINC_EXPORT bool PINC_CALL pinc_window_has_scale_factor(pinc_window window) {
    P_UNUSED(window);
    // TODO: probably want to refactor how scale factors work anyway
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_resizable(pinc_window window, bool resizable) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->resizable = resizable;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowResizable(&staticState.windowBackend, *ob, resizable);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_get_resizable(pinc_window window) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->resizable;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowResizable(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_minimized(pinc_window window, bool minimized) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->minimized = minimized;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowMinimized(&staticState.windowBackend, *ob, minimized);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_get_minimized(pinc_window window) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->minimized;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowMinimized(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_maximized(pinc_window window, bool maximized) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->maximized = maximized;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowMaximized(&staticState.windowBackend, *ob, maximized);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_get_maximized(pinc_window window) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->maximized;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowMaximized(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_fullscreen(pinc_window window, bool fullscreen) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->fullscreen = fullscreen;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowFullscreen(&staticState.windowBackend, *ob, fullscreen);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_get_fullscreen(pinc_window window) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->fullscreen;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowFullscreen(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_focused(pinc_window window, bool focused) {
    // TODO: should this trigger a focus changed event?
    // TODO: Either way, it needs to set the current window.
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->focused = focused;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowFocused(&staticState.windowBackend, *ob, focused);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_get_focused(pinc_window window) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->resizable;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowResizable(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_hidden(pinc_window window, bool hidden) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            ob->hidden = hidden;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            WindowBackend_setWindowHidden(&staticState.windowBackend, *ob, hidden);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT bool PINC_CALL pinc_window_get_hidden(pinc_window window) {
    switch (PincObject_discriminator(window)) {
        case PincObjectDiscriminator_incompleteWindow: {
            IncompleteWindow* ob = PincObject_ref_incompleteWindow(window);
            return ob->hidden;
        }
        case PincObjectDiscriminator_window: {
            WindowHandle* ob = PincObject_ref_window(window);
            return WindowBackend_getWindowHidden(&staticState.windowBackend, *ob);
        }
        default:{
            PErrorUser(false, "Not a window object");
        }
    }
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_set_vsync(bool sync) {
    P_UNUSED(sync);
    PPANIC("pinc_set_vsync not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT bool PINC_CALL pinc_get_vsync(void) {
    PPANIC("pinc_get_vsync not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_present_framebuffer(pinc_window window) {
    WindowHandle* object = PincObject_ref_window(window);
    WindowBackend_windowPresentFramebuffer(&staticState.windowBackend, *object);
}

PINC_EXPORT bool PINC_CALL pinc_mouse_button_get(uint32_t button) {
    P_UNUSED(button);
    PPANIC("pinc_mouse_button_get not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_keyboard_key_get(pinc_keyboard_key button) {
    P_UNUSED(button);
    PPANIC("pinc_keyboard_key_get not implemented");
    return false;
}

PINC_EXPORT uint32_t PINC_CALL pinc_get_cursor_x(void) {
    PPANIC("pinc_get_cursor_x not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_get_cursor_y(void) {
    PPANIC("pinc_get_cursor_y not implemented");
    return 0;
}

PINC_EXPORT pinc_window PINC_CALL pinc_get_cursor_window(void) {
    PPANIC("pinc_get_cursor_window not implemented");
    return 0;
}

PINC_EXPORT pinc_window PINC_CALL pinc_get_focus_window(void) {
    return staticState.currentWindow;
}

PINC_EXPORT void PINC_CALL pinc_step(void) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    WindowBackend_step(&staticState.windowBackend);
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

PINC_EXPORT uint32_t PINC_CALL pinc_event_get_num(void) {
    return staticState.eventsBufferNum;
}

PINC_EXPORT pinc_event_type PINC_CALL pinc_event_get_type(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    return staticState.eventsBuffer[event_index].type;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_get_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    return staticState.eventsBuffer[event_index].currentWindow;
}

PINC_EXPORT int64_t PINC_CALL pinc_event_get_timestamp_unix_millis(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    return staticState.eventsBuffer[event_index].timeUnixMillis;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_close_signal_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_close_signal, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.closeSignal.window;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_mouse_button_old_state(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_mouse_button, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.mouseButton.oldState;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_mouse_button_state(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_mouse_button, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.mouseButton.state;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_resize_old_width(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.oldWidth;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_resize_old_height(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.oldHeight;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_resize_width(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.width;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_resize_height(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.height;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_resize_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_resize, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.resize.window;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_focus_old_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_focus, "Wrong event type");
    return staticState.eventsBuffer[event_index].currentWindow;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_focus_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_focus, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.focus.newWindow;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_exposure_x(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.x;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_exposure_y(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.y;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_exposure_width(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.width;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_exposure_height(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.height;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_exposure_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_exposure, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.exposure.window;
}

PINC_EXPORT pinc_keyboard_key PINC_CALL pinc_event_keyboard_button(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_keyboard_button, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.keyboardButton.key;
}

PINC_EXPORT bool PINC_CALL pinc_event_keyboard_button_state(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_keyboard_button, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.keyboardButton.state;
}

PINC_EXPORT bool PINC_CALL pinc_event_keyboard_button_repeat(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_keyboard_button, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.keyboardButton.repeat;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_move_old_x(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_move, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.oldX;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_move_old_y(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_move, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.oldY;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_move_x(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_move, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.x;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_move_y(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_move, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.y;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_cursor_move_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_move, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorMove.window;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_transition_old_x(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_transition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.oldX;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_transition_old_y(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_transition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.oldY;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_cursor_transition_old_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_transition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.oldWindow;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_transition_x(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_transition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.x;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_cursor_transition_y(uint32_t event_index){
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_transition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.y;
}

PINC_EXPORT pinc_window PINC_CALL pinc_event_cursor_transition_window(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_cursor_transition, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.cursorTransition.window;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_text_input_codepoint(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_text_input, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.textInput.codepoint;
}

PINC_EXPORT float PINC_CALL pinc_event_scroll_vertical(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_scroll, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.scroll.vertical;
}

PINC_EXPORT float PINC_CALL pinc_event_scroll_horizontal(uint32_t event_index) {
    PErrorUser(event_index < staticState.eventsBufferNum, "Event index out of bounds");
    PErrorUser(staticState.eventsBuffer[event_index].type == pinc_event_type_scroll, "Wrong event type");
    return staticState.eventsBuffer[event_index].data.scroll.horizontal;
}

// ### OPENGL FUNCTIONS ###
#include <pinc_opengl.h>

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_version_supported(pinc_window_backend backend, uint32_t major, uint32_t minor, bool es) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");

    return WindowBackend_queryGlVersionSupported(&staticState.sdl2WindowBackend, major, minor, es);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_accumulator_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer_format_id, uint32_t channel, uint32_t bits){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pinc_query_framebuffer_format_default(backend, pinc_graphics_api_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return WindowBackend_queryGlAccumulatorBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, channel, bits);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_alpha_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer_format_id, uint32_t bits) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pinc_query_framebuffer_format_default(backend, pinc_graphics_api_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return WindowBackend_queryGlAlphaBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, bits);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_depth_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer_format_id, uint32_t bits) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pinc_query_framebuffer_format_default(backend, pinc_graphics_api_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return WindowBackend_queryGlDepthBits(&staticState.sdl2WindowBackend, *framebufferFormatObj, bits);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_stereo_buffer(pinc_window_backend backend, pinc_framebuffer_format framebuffer_format_id) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pinc_query_framebuffer_format_default(backend, pinc_graphics_api_opengl);
    }
    FramebufferFormat* framebufferFormatObj = PincObject_ref_framebufferFormat(framebuffer_format_id);
    return WindowBackend_queryGlStereoBuffer(&staticState.sdl2WindowBackend, *framebufferFormatObj);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_context_debug(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryGlContextDebug(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_forward_compatible(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryGlForwardCompatible(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_robust_access(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryGlRobustAccess(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_opengl_support_status PINC_CALL pinc_query_opengl_reset_isolation(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryGlResetIsolation(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_opengl_context PINC_CALL pinc_opengl_create_context_incomplete(void) {
    pinc_opengl_context id = PincObject_allocate(PincObjectDiscriminator_incompleteGlContext);
    IncompleteGlContext* obj = PincObject_ref_incompleteGlContext(id);
    
    *obj = (IncompleteGlContext) {
        .accumulatorBits = {0, 0, 0, 0},
        .alphaBits = 0,
        .depthBits = 16,
        .stereo = false,
        .debug = false,
        .forwardCompatible = false,
        .robustAccess = false,
        .versionMajor = 1,
        .versionMinor = 0,
        .versionEs = false,
        .core = false,
    };

    return id;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_accumulator_bits(pinc_opengl_context incomplete_context, uint32_t channel, uint32_t bits) {
    P_UNUSED(incomplete_context);
    P_UNUSED(channel);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_alpha_bits(pinc_opengl_context incomplete_context, uint32_t bits) {
    P_UNUSED(incomplete_context);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}
PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_depth_bits(pinc_opengl_context incomplete_context, uint32_t bits) {
    P_UNUSED(incomplete_context);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_stereo_buffer(pinc_opengl_context incomplete_context, bool stereo) {
    P_UNUSED(incomplete_context);
    P_UNUSED(stereo);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_context_debug(pinc_opengl_context incomplete_context, bool debug) {
    P_UNUSED(incomplete_context);
    P_UNUSED(debug);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_forward_compatible(pinc_opengl_context incomplete_context, bool compatible) {
    P_UNUSED(incomplete_context);
    P_UNUSED(compatible);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_robust_access(pinc_opengl_context incomplete_context, bool robust) {
    P_UNUSED(incomplete_context);
    P_UNUSED(robust);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_reset_isolation(pinc_opengl_context incomplete_context, bool isolation) {
    P_UNUSED(incomplete_context);
    P_UNUSED(isolation);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_set_context_version(pinc_opengl_context incomplete_context, uint32_t major, uint32_t minor, bool es, bool core) {
    if(pinc_query_opengl_version_supported(pinc_window_backend_any, major, minor, es) == pinc_opengl_support_status_none) {
        PErrorExternal(false, "Opengl version not supported");
        return pinc_return_code_error;
    }

    IncompleteGlContext* contextObj = PincObject_ref_incompleteGlContext(incomplete_context);
    contextObj->versionMajor = major;
    contextObj->versionMinor = minor;
    contextObj->versionEs = es;
    contextObj->core = core;
    contextObj->forwardCompatible = !core;
    PErrorUser((!es) && (!core), "pinc_opengl_set_context_version: OpenGl context can either be core or ES, not both");
    return pinc_return_code_pass;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_context_complete(pinc_opengl_context incomplete_context) {
    IncompleteGlContext* contextObj = PincObject_ref_incompleteGlContext(incomplete_context);
    RawOpenglContextHandle contextHandle = WindowBackend_glCompleteContext(&staticState.windowBackend, *contextObj);
    if(contextHandle == 0) {
        return pinc_return_code_error;
    }
    PincObject_reallocate(incomplete_context, PincObjectDiscriminator_glContext);
    RawOpenglContextObject* handleObject = PincObject_ref_glContext(incomplete_context);
    handleObject->handle = contextHandle;
    handleObject->front_handle = incomplete_context;
    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_opengl_context_deinit(pinc_opengl_context context) {
    P_UNUSED(context);
    // TODO
}

PINC_EXPORT uint32_t PINC_CALL pinc_opengl_get_context_accumulator_bits(pinc_opengl_context incomplete_context, uint32_t channel) {
    P_UNUSED(incomplete_context);
    P_UNUSED(channel);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

// TODO
PINC_EXTERN uint32_t PINC_CALL pinc_opengl_get_context_alpha_bits(pinc_opengl_context incomplete_context);
PINC_EXTERN uint32_t PINC_CALL pinc_opengl_get_context_depth_bits(pinc_opengl_context incomplete_context);

PINC_EXPORT bool PINC_CALL pinc_opengl_get_context_stereo_buffer(pinc_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_opengl_get_context_context_debug(pinc_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_opengl_get_context_forward_compatible(pinc_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_opengl_get_context_robust_access(pinc_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_opengl_get_context_reset_isolation(pinc_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_opengl_make_current(pinc_window window, pinc_opengl_context context) {
    WindowHandle* windowObj = PincObject_ref_window(window);
    RawOpenglContextObject* contextObj = PincObject_ref_glContext(context);
    return WindowBackend_glMakeCurrent(&staticState.windowBackend, *windowObj, contextObj->handle);
}

PINC_EXPORT pinc_window PINC_CALL pinc_opengl_get_current_window(void) {
    return WindowBackend_glGetCurrentWindow(&staticState.windowBackend);
}

PINC_EXPORT pinc_opengl_context PINC_CALL pinc_opengl_get_current_context(void) {
    return WindowBackend_glGetCurrentContext(&staticState.windowBackend);
}

PINC_EXPORT PINC_PFN PINC_CALL pinc_opengl_get_proc(char const * procname) {
    // TODO validation
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");

    return WindowBackend_glGetProc(&staticState.windowBackend, procname);
}
