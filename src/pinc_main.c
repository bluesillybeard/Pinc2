#include <pinc.h>
#include "platform/platform.h"
#include "pinc_main.h"

#include "sdl2.h"

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

static pinc_object PincObject_allocate(void) {
    if(staticState.objectsCapacity == staticState.objectsNum) {
        if(!staticState.objects) {
            staticState.objects = Allocator_allocate(rootAllocator, sizeof(PincObject) * 8);
            staticState.objectsCapacity = 8;
        } else {
            size_t newObjectsCapacity = staticState.objectsCapacity * 2;
            staticState.objects = Allocator_reallocate(rootAllocator, staticState.objects, sizeof(PincObject) * staticState.objectsCapacity, sizeof(PincObject) * newObjectsCapacity);
            staticState.objectsCapacity = newObjectsCapacity;
        }
    }
    if(staticState.freeObjects && staticState.freeObjectsNum > 0) {
        staticState.freeObjectsNum--;
        pinc_object id = (pinc_object) staticState.freeObjects[staticState.freeObjectsNum];
        return id;
    }
    staticState.objectsNum++;
    PErrorSanitize(staticState.objectsNum < UINT32_MAX, "Integer overflow");
    pinc_object id = (pinc_object)staticState.objectsNum;
    return id;
}

static PincObject* PincObject_ref(pinc_object handle) {
    // These are set as user errors because I can't imagine these happening in any other case.
    // Another option would be to return null in either of these cases, and let downstream functions deal with it.
    PErrorUser(handle != 0, "Object handle is null");
    PErrorUser(handle <= staticState.objectsNum, "Object is invalid");
    return &staticState.objects[handle-1];
}

static void PincObject_free(pinc_object handle) {
    PincObject* obj = PincObject_ref(handle);
    pMemSet(0, (uint8_t*)obj, sizeof(PincObject));
    if(handle == staticState.objectsNum) {
        staticState.objectsNum--;
    } else {
        if(staticState.freeObjectsCapacity == staticState.freeObjectsNum) {
            if(!staticState.freeObjects) {
                staticState.freeObjects = Allocator_allocate(rootAllocator, sizeof(pinc_object) * 8);
                staticState.freeObjectsCapacity = 8;
            } else {
                size_t newObjectsCapacity = staticState.freeObjectsCapacity * 2;
                staticState.freeObjects = Allocator_reallocate(rootAllocator, staticState.freeObjects, sizeof(pinc_object) * staticState.freeObjectsCapacity, sizeof(pinc_object) * newObjectsCapacity);
                staticState.freeObjectsCapacity = newObjectsCapacity;
            }
        }
        staticState.freeObjects[staticState.freeObjectsNum] = handle;
        staticState.freeObjectsNum++;
    }
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
    // Once there are more window backends, all of their various framebuffer formats
    // need to be merged into one list and properly mapped to user-facing ID handles
    staticState.framebufferFormats = WindowBackend_queryFramebufferFormats(&staticState.sdl2WindowBackend, rootAllocator, &staticState.framebufferFormatNum);
    
    return pinc_return_code_pass;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_window_backends(pinc_window_backend* backend_dest, uint32_t capacity) {
    // TODO: Only window backend is SDL2, shortcuts are taken
    if(backend_dest && capacity < 1){
        *backend_dest = pinc_window_backend_sdl2;
    }
    return 1;
}

PINC_EXPORT pinc_window_backend PINC_CALL pinc_query_window_backend_default(void) {
    // TODO: only window backend is SDL2, shortcuts are taken
    return pinc_window_backend_sdl2;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_graphics_backends(pinc_window_backend window_backend, pinc_graphics_backend* backend_dest, uint32_t capacity) {
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    size_t numGraphicsBackends;
    pinc_graphics_backend* supportedBackends = WindowBackend_queryGraphicsBackends(&staticState.sdl2WindowBackend, tempAllocator, &numGraphicsBackends);
    size_t numToCopy = capacity;
    if(numToCopy > numGraphicsBackends) {
        numToCopy = numGraphicsBackends;
    }
    if(backend_dest) {
        pMemCopy(supportedBackends, backend_dest, numToCopy);
    }
    PErrorSanitize(numGraphicsBackends <= UINT32_MAX, "Integer overflow");
    return (uint32_t)numGraphicsBackends;
}

PINC_EXPORT pinc_graphics_backend PINC_CALL pinc_query_graphics_backend_default(pinc_window_backend window_backend) {
    // TODO: only backend is raw opengl, shortcuts are taken
    P_UNUSED(window_backend);
    return pinc_graphics_backend_raw_opengl;
}

PINC_EXPORT pinc_framebuffer_format pinc_query_framebuffer_format_default(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend) {
    P_UNUSED(window_backend);
    P_UNUSED(graphics_backend);
    // TODO: actually chose one instead of just grabbing the first one
    return 1;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_ids(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format* ids_dest, uint32_t capacity) {
    P_UNUSED(window_backend);
    P_UNUSED(graphics_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    // TODO: sort framebuffers from best to worst, so applications can just loop from first to last and pick the first one they see that they like
    // TODO: is it really safe to assume that we can just filter the formats that are the same between the two backends and call all of those supported?
    // TODO: completely finish refactoring framebuffer formats into Pinc objects
    // In other words, if both the window backend and graphics backend both support a given framebuffer format,
    // is it safe to assume that it can be used if that specific combination of window/graphics backend is chosen?
    PErrorUser(staticState.framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?");
    PErrorSanitize(staticState.framebufferFormatNum < INT32_MAX, "Integer overflow");
    PErrorSanitize(capacity < INT32_MAX, "Integer overflow");

    if(ids_dest) {
        // Since there's only one window backend and only one graphics backend, just return the list we have as-is
        for(uint32_t i=0; i==(uint32_t)staticState.framebufferFormatNum && i<(uint32_t)capacity; ++i) {
            ids_dest[i] = i+1;
        }
    }
    return (uint32_t)staticState.framebufferFormatNum;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    PErrorSanitize(staticState.framebufferFormatNum < INT32_MAX, "Integer overflow");
    PErrorUser(staticState.framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?");
    PErrorUser(format_id <= (uint32_t)staticState.framebufferFormatNum && format_id >= 1, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?");
    return staticState.framebufferFormats[format_id-1].channels;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, uint32_t channel) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    PErrorSanitize(staticState.framebufferFormatNum < INT32_MAX, "Integer overflow");
    PErrorUser(staticState.framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?");
    PErrorUser(format_id <= (uint32_t)staticState.framebufferFormatNum && format_id >= 1, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?");
    FramebufferFormat* fmt = &staticState.framebufferFormats[format_id-1];
    PErrorUser(channel < fmt->channels, "channel index out of bounds - did you make sure it's less than what pinc_query_framebuffer_format_channels returns for this format?");
    return fmt->channel_bits[channel];
}

PINC_EXPORT pinc_color_space PINC_CALL pinc_query_framebuffer_format_color_space(pinc_framebuffer_format format_id) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    PErrorSanitize(staticState.framebufferFormatNum < INT32_MAX, "Integer overflow");
    PErrorUser(staticState.framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?");
    PErrorUser(format_id <= (uint32_t)staticState.framebufferFormatNum && format_id >= 1, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?");
    return staticState.framebufferFormats[format_id-1].color_space;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend) {
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    return WindowBackend_queryMaxOpenWindows(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_complete_init(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format framebuffer_format_id, uint32_t samples, uint32_t depth_buffer_bits) {
    // TODO: only window backend is SDL2, shortcuts are taken
    PErrorUser(window_backend != pinc_window_backend_none, "Unsupported window backend");
    if(window_backend == pinc_window_backend_any) {
        window_backend = pinc_window_backend_sdl2;
    }
    // TODO: only graphics backend is raw opengl, shortcuts are taken
    if(graphics_backend == pinc_graphics_backend_any) {
        graphics_backend = pinc_graphics_backend_raw_opengl;
    }
    if(framebuffer_format_id == 0) {
        framebuffer_format_id = pinc_query_framebuffer_format_default(window_backend, graphics_backend);
    }
    PErrorUser(staticState.framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?");
    PErrorSanitize(staticState.framebufferFormatNum <= INT32_MAX, "Integer overflow");
    PErrorUser(framebuffer_format_id <= (uint32_t)staticState.framebufferFormatNum && framebuffer_format_id >= 1, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?");
    FramebufferFormat framebuffer = staticState.framebufferFormats[framebuffer_format_id-1];
    pinc_return_code result = WindowBackend_completeInit(&staticState.sdl2WindowBackend, graphics_backend, framebuffer, samples, depth_buffer_bits);
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
    // - destroy framebuffer formats
    // - destroy any objects
    // - deinit all backends (there may be multiple as this can be called before one is chosen)

    // If the allocator is not initialized, then it means Pinc is not initialized in any capacity and we can just return
    if(rootAllocator.vtable == NULL) {
        return;
    }

    if(staticState.objects && staticState.objectsCapacity) {
        // Go through every object and destroy it
        for(uint32_t i=0; i<staticState.objectsNum; ++i) {
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
                case pinc_object_type_vertex_attributes: {
                    // TODO
                    PPANIC("Not Implemented");
                    break;
                }
                case pinc_object_type_uniforms: {
                    // TODO
                    PPANIC("Not Implemented");
                    break;
                }
                case pinc_object_type_shaders: {
                    // TODO
                    PPANIC("Not Implemented");
                    break;
                }
                case pinc_object_type_pipeline: {
                    // TODO
                    PPANIC("Not Implemented");
                    break;
                }
                case pinc_object_type_vertex_array: {
                    // TODO
                    PPANIC("Not Implemented");
                    break;
                }
                case pinc_object_type_texture: {
                    // TODO
                    PPANIC("Not Implemented");
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

    if(staticState.framebufferFormats && staticState.framebufferFormatNum) {
        Allocator_free(rootAllocator, staticState.framebufferFormats, staticState.framebufferFormatNum * sizeof(FramebufferFormat));
    }

    if(staticState.objects) {
        Allocator_free(rootAllocator, staticState.objects, staticState.objectsNum * sizeof(PincObject));
    }

    if(staticState.freeObjects) {
        Allocator_free(rootAllocator, staticState.freeObjects, staticState.freeObjectsNum * sizeof(pinc_object));
    }

    if(staticState.tempAlloc.vtable){
        arena_free(&staticState.arenaAllocatorObject);
    }


    // Full reset the state
    staticState = (PincStaticState) PINC_PREINIT_STATE;
}

PINC_EXPORT pinc_window_backend PINC_CALL pinc_query_set_window_backend(void) {
    PPANIC("pinc_query_set_window_backend not implemented");
    return 0;
}

PINC_EXPORT pinc_graphics_backend PINC_CALL pinc_query_set_graphics_backend(void) {
    PPANIC("pinc_query_set_graphics_backend not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_set_framebuffer_format(void) {
    PPANIC("pinc_query_set_framebuffer_format not implemented");
    return 0;
}

PINC_EXPORT pinc_object_type PINC_CALL pinc_get_object_type(pinc_object obj) {
    PincObject* object = PincObject_ref(obj);
    switch (object->discriminator)
    {
    case PincObjectDiscriminator_none:
        return pinc_object_type_none;
    case PincObjectDiscriminator_incompleteWindow:
    case PincObjectDiscriminator_window:
        return pinc_object_type_window;
    // Objects that are not normal Pinc objects.
    case PincObjectDiscriminator_rawGlContext:
    case PincObjectDiscriminator_incompleteRawGlContext:
        return pinc_object_type_none;
    default:
        PErrorAssert(false, "Invalid object type - this is an error within Pinc!");
    }
}

PINC_EXPORT bool PINC_CALL pinc_get_object_complete(pinc_object obj) {
    P_UNUSED(obj);
    PPANIC("pinc_get_object_complete not implemented");
    return false;
}

PINC_EXPORT pinc_window PINC_CALL pinc_window_create_incomplete(void) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    pinc_window handle = PincObject_allocate();
    PincObject* window = PincObject_ref(handle);
    window->discriminator = PincObjectDiscriminator_incompleteWindow;
    // TODO: Add integer formatting to PString, or get a decent libc-free formatting library
    char namebuf[24];
    size_t nameLen = 0;
    pMemCopy("Pinc window ", namebuf, 12);
    nameLen += 12;
    nameLen += pBufPrintUint32(namebuf+12, 12, handle);
    char* name = Allocator_allocate(rootAllocator, nameLen);
    pMemCopy(namebuf, name, nameLen);
    window->data.incompleteWindow = (IncompleteWindow){
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

PINC_EXPORT pinc_return_code PINC_CALL pinc_window_complete(pinc_window window) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    PincObject* object = PincObject_ref(window);
    PErrorUser(object->discriminator == PincObjectDiscriminator_incompleteWindow, "Window is not an incomplete window object");
    WindowHandle handle = WindowBackend_completeWindow(&staticState.windowBackend, &object->data.incompleteWindow);
    if(!handle) {
        return pinc_return_code_error;
    }
    object->discriminator = PincObjectDiscriminator_window;
    object->data.window = handle;
    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_window_deinit(pinc_window window) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    PincObject* object = PincObject_ref(window);
    switch(object->discriminator) {
        case PincObjectDiscriminator_incompleteWindow:{
            PincObject_free(window);
            break;
        }
        case PincObjectDiscriminator_window:{
            WindowBackend_deinitWindow(&staticState.windowBackend, object->data.window);
            PincObject_free(window);
            break;
        }
        default:{
            PErrorUser(object->discriminator == PincObjectDiscriminator_incompleteWindow, "Window is not a window object");
            break;
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_title(pinc_window window, const char* title_buf, uint32_t title_len) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    PincObject* object = PincObject_ref(window);
    if(title_len == 0) {
        size_t realTitleLen = pStringLen(title_buf);
        PErrorSanitize(realTitleLen <= UINT32_MAX, "Integer overflow");
        title_len = (uint32_t)realTitleLen;
    }
    // this is over here because reasons
    uint8_t* titlePtr;
    switch (object->discriminator)
    {
        case PincObjectDiscriminator_incompleteWindow:
            // TODO: potential optimization here (this code is quite cold though)
            PString_free(&object->data.incompleteWindow.title, rootAllocator);
            object->data.incompleteWindow.title = PString_copy((PString){.str = (uint8_t*)title_buf, .len = title_len}, rootAllocator);
            break;
        case PincObjectDiscriminator_window:
            // Window takes ownership of the pointer, but we don't have ownership of title_buf
            titlePtr = (uint8_t*)Allocator_allocate(rootAllocator, title_len);
            pMemCopy(title_buf, titlePtr, title_len);
            WindowBackend_setWindowTitle(&staticState.windowBackend, object->data.window, titlePtr, title_len);
            break;
        default:
            PErrorUser(false, "Window is not a window object");
            break;
    }
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_title(pinc_window window, char* title_buf, uint32_t title_capacity) {
    P_UNUSED(window);
    P_UNUSED(title_buf);
    P_UNUSED(title_capacity);
    PPANIC("pinc_window_get_title not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_width(pinc_window window, uint32_t width) {
    P_UNUSED(window);
    P_UNUSED(width);
    PPANIC("pinc_window_set_width not implemented");
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_width(pinc_window window) {
    // TODO: implement this properly
    PincObject* windowObj = PincObject_ref(window);
    return WindowBackend_getWindowWidth(&staticState.windowBackend, windowObj->data.window);
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_has_width(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_has_width not implemented");
    return 0;
}

PINC_EXPORT void PINC_CALL pinc_window_set_height(pinc_window window, uint32_t height) {
    P_UNUSED(window);
    P_UNUSED(height);
    PPANIC("pinc_window_set_height not implemented");
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_height(pinc_window window) {
    // TODO: implement this properly
    PincObject* windowObj = PincObject_ref(window);
    return WindowBackend_getWindowHeight(&staticState.windowBackend, windowObj->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_window_has_height(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_has_height not implemented");
    return false;
}

PINC_EXPORT float PINC_CALL pinc_window_get_scale_factor(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_scale_factor not implemented");
    return 0;
}

PINC_EXPORT int PINC_CALL pinc_window_has_scale_factor(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_has_scale_factor not implemented");
    return 0;
}

PINC_EXPORT void PINC_CALL pinc_window_set_resizable(pinc_window window, bool resizable) {\
    P_UNUSED(window);
    P_UNUSED(resizable);
    PPANIC("pinc_window_set_resizable not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_resizable(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_resizable not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_minimized(pinc_window window, bool minimized) {
    P_UNUSED(window);
    P_UNUSED(minimized);
    PPANIC("pinc_window_set_minimized not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_minimized(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_minimized not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_maximized(pinc_window window, bool maximized) {
    P_UNUSED(window);
    P_UNUSED(maximized);
    PPANIC("pinc_window_set_maximized not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_maximized(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_maximized not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_fullscreen(pinc_window window, bool fullscreen) {
    P_UNUSED(window);
    P_UNUSED(fullscreen);
    PPANIC("pinc_window_set_fullscreen not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_fullscreen(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_fullscreen not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_focused(pinc_window window, bool focused) {
    P_UNUSED(window);
    P_UNUSED(focused);
    PPANIC("pinc_window_set_focused not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_focused(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_focused not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_hidden(pinc_window window, bool hidden) {
    P_UNUSED(window);
    P_UNUSED(hidden);
    PPANIC("pinc_window_set_hidden not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_hidden(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_get_hidden not implemented");
    return false;
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
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    PincObject* object = PincObject_ref(window);
    PErrorUser(object->discriminator == PincObjectDiscriminator_window, "Window must be a window object");
    WindowBackend_windowPresentFramebuffer(&staticState.windowBackend, object->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_mouse_button_get(int button) {
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

PINC_EXPORT void PINC_CALL pinc_step(void) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    WindowBackend_step(&staticState.windowBackend);
    arena_reset(&staticState.arenaAllocatorObject);
}

PINC_EXPORT bool PINC_CALL pinc_event_window_closed(pinc_window window) {
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    PincObject* object = PincObject_ref(window);
    PErrorUser(object->discriminator == PincObjectDiscriminator_window, "Window must be a window object");
    return WindowBackend_windowEventClosed(&staticState.windowBackend, object->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_event_window_mouse_button(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_mouse_button not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_resized(pinc_window window) {
    // TODO: properly implement this
    PincObject* obj = PincObject_ref(window);
    return WindowBackend_windowEventResized(&staticState.windowBackend, obj->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_event_window_focused(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_focused not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_unfocused(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_unfocused not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_exposed(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_exposed not implemented");
    return false;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_window_keyboard_button_get(pinc_window window, pinc_keyboard_key* key_buffer, uint32_t capacity) {
    P_UNUSED(window);
    P_UNUSED(key_buffer);
    P_UNUSED(capacity);
    PPANIC("pinc_event_window_keyboard_button_get not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_window_keyboard_button_get_repeat(pinc_window window, bool* repeat_buffer, uint32_t capacity) {
    P_UNUSED(window);
    P_UNUSED(repeat_buffer);
    P_UNUSED(capacity);
    PPANIC("pinc_event_window_keyboard_button_get_repeat not implemented");
    return 0;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_cursor_move(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_cursor_move not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_cursor_exit(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_cursor_exit not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_cursor_enter(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_event_window_cursor_enter not implemented");
    return false;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_window_text_get(pinc_window window, uint8_t* return_buf, uint32_t capacity) {
    P_UNUSED(window);
    P_UNUSED(return_buf);
    P_UNUSED(capacity);
    PPANIC("pinc_event_window_text_get not implemented");
    return 0;
}

PINC_EXPORT float PINC_CALL pinc_window_scroll_vertical(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_scroll_vertical not implemented");
    return 0;
}

PINC_EXPORT float PINC_CALL pinc_window_scroll_horizontal(pinc_window window) {
    P_UNUSED(window);
    PPANIC("pinc_window_scroll_horizontal not implemented");
    return 0;
}

// ### RAW OPENGL FUNCTIONS ###
#include <pinc_opengl.h>

// Unlike with the general window management functions,
// The raw opengl functions pretty much call directly into the window backend with nothing between it and the user.

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_version_supported(pinc_window_backend backend, uint32_t major, uint32_t minor, bool es) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");

    return WindowBackend_queryRawGlVersionSupported(&staticState.sdl2WindowBackend, major, minor, es);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_accumulator_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t channel, uint32_t bits){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    // TODO: -1 for default framebuffer format
    // TODO: Actually, I want to change 0 to be default / null, and turn framebuffer formats (more or less) into regular Pinc objects
    PErrorSanitize(framebufferFormatNum <= INT32_MAX, "Integer overflow");
    PErrorUser(framebuffer <= (uint32_t)staticState.framebufferFormatNum, "Invalid framebuffer format ID");
    FramebufferFormat framebufferFormat = staticState.framebufferFormats[framebuffer-1];
    return WindowBackend_queryRawGlAccumulatorBits(&staticState.sdl2WindowBackend, framebufferFormat, channel, bits);
}

// TODO
PINC_EXTERN pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_alpha_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t bits);
PINC_EXTERN pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_depth_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t bits);

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_stereo_buffer(pinc_window_backend backend, pinc_framebuffer_format framebuffer) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    // TODO: -1 for default framebuffer format
    PErrorSanitize(framebufferFormatNum <= INT32_MAX, "Integer overflow");
    PErrorUser(framebuffer <= (uint32_t)staticState.framebufferFormatNum, "Invalid framebuffer format ID");
    FramebufferFormat framebufferFormat = staticState.framebufferFormats[framebuffer-1];
    return WindowBackend_queryRawGlStereoBuffer(&staticState.sdl2WindowBackend, framebufferFormat);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_context_debug(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryRawGlContextDebug(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_forward_compatible(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryRawGlForwardCompatible(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_robust_access(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryRawGlRobustAccess(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_reset_isolation(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PErrorUser(backend == pinc_window_backend_sdl2, "Unsupported window backend");
    PErrorUser(staticState.sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?");
    return WindowBackend_queryRawGlResetIsolation(&staticState.sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_context PINC_CALL pinc_raw_opengl_create_context_incomplete(void) {
    pinc_raw_opengl_context id = PincObject_allocate();
    PincObject* obj = PincObject_ref(id);
    
    obj->discriminator = PincObjectDiscriminator_incompleteRawGlContext;
    obj->data.incompleteRawGlContext = (IncompleteRawGlContext) {
        .accumulatorBits = {0, 0, 0, 0},
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

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_accumulator_bits(pinc_raw_opengl_context incomplete_context, uint32_t channel, uint32_t bits) {
    P_UNUSED(incomplete_context);
    P_UNUSED(channel);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

// TODO
PINC_EXTERN pinc_return_code PINC_CALL pinc_raw_opengl_set_context_alpha_bits(pinc_raw_opengl_context incomplete_context, uint32_t bits);
PINC_EXTERN pinc_return_code PINC_CALL pinc_raw_opengl_set_context_depth_bits(pinc_raw_opengl_context incomplete_context, uint32_t bits);

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_stereo_buffer(pinc_raw_opengl_context incomplete_context, bool stereo) {
    P_UNUSED(incomplete_context);
    P_UNUSED(stereo);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_context_debug(pinc_raw_opengl_context incomplete_context, bool debug) {
    P_UNUSED(incomplete_context);
    P_UNUSED(debug);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_forward_compatible(pinc_raw_opengl_context incomplete_context, bool compatible) {
    P_UNUSED(incomplete_context);
    P_UNUSED(compatible);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_robust_access(pinc_raw_opengl_context incomplete_context, bool robust) {
    P_UNUSED(incomplete_context);
    P_UNUSED(robust);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_reset_isolation(pinc_raw_opengl_context incomplete_context, bool isolation) {
    P_UNUSED(incomplete_context);
    P_UNUSED(isolation);
    // TODO
    PPANIC("Not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_version(pinc_raw_opengl_context incomplete_context, uint32_t major, uint32_t minor, bool es, bool core) {
    // TODO validation
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");
    // TODO check that this version is available

    PincObject* contextObj = PincObject_ref(incomplete_context);
    PErrorUser(contextObj->discriminator == PincObjectDiscriminator_incompleteRawGlContext, "pinc_raw_opengl_set_context_version: Object must be an incomplete raw OpenGl context");
    contextObj->data.incompleteRawGlContext.versionMajor = major;
    contextObj->data.incompleteRawGlContext.versionMinor = minor;
    contextObj->data.incompleteRawGlContext.versionEs = es;
    contextObj->data.incompleteRawGlContext.core = core;
    contextObj->data.incompleteRawGlContext.forwardCompatible = !core;
    PErrorUser((!es) && (!core), "pinc_raw_opengl_set_context_version: OpenGl context can either be core or ES, not both");
    return pinc_return_code_pass;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_context_complete(pinc_raw_opengl_context incomplete_context) {
    // TODO validation
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");

    PincObject* contextObj = PincObject_ref(incomplete_context);
    PErrorUser(contextObj->discriminator == PincObjectDiscriminator_incompleteRawGlContext, "pinc_raw_opengl_context_complete: Object must be an incomplete raw OpenGl context");
    RawOpenglContextHandle contextHandle = WindowBackend_rawGlCompleteContext(&staticState.windowBackend, contextObj->data.incompleteRawGlContext);
    if(contextHandle == 0) {
        return pinc_return_code_error;
    }
    contextObj->discriminator = PincObjectDiscriminator_rawGlContext;
    contextObj->data.rawGlContext = contextHandle;

    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_raw_opengl_context_deinit(pinc_raw_opengl_context context) {
    P_UNUSED(context);
    // TODO
}

PINC_EXPORT uint32_t PINC_CALL pinc_raw_opengl_get_context_accumulator_bits(pinc_raw_opengl_context incomplete_context, uint32_t channel) {
    P_UNUSED(incomplete_context);
    P_UNUSED(channel);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

// TODO
PINC_EXTERN uint32_t PINC_CALL pinc_raw_opengl_get_context_alpha_bits(pinc_raw_opengl_context incomplete_context);
PINC_EXTERN uint32_t PINC_CALL pinc_raw_opengl_get_context_depth_bits(pinc_raw_opengl_context incomplete_context);

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_stereo_buffer(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_context_debug(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_forward_compatible(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_robust_access(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_reset_isolation(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC("Not implemented");
    return false;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_make_current(pinc_window window, pinc_raw_opengl_context context) {
    // TODO validation
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");

    PincObject* windowObj = PincObject_ref(window);
    PErrorUser(windowObj->discriminator == PincObjectDiscriminator_window, "pinc_raw_opengl_make_current: window must be a complete window object");
    PincObject* contextObj = PincObject_ref(context);
    PErrorUser(contextObj->discriminator == PincObjectDiscriminator_rawGlContext, "pinc_raw_opengl_make_current: context must be a complete raw OpenGL object");

    return WindowBackend_rawGlMakeCurrent(&staticState.windowBackend, windowObj->data.window, contextObj->data.rawGlContext);
}

PINC_EXPORT pinc_window PINC_CALL pinc_raw_opengl_get_current(void) {
    // TODO
    PPANIC("Not implemented");
    return 0;
}

PINC_EXPORT PINC_PFN PINC_CALL pinc_raw_opengl_get_proc(char const * procname) {
    // TODO validation
    PErrorUser(staticState.windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?");

    return WindowBackend_rawGlGetProc(&staticState.windowBackend, procname);
}

