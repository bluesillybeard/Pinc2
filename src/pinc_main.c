#include <pinc.h>
#include "platform/platform.h"
#include "pinc_main.h"

#include "sdl2.h"

pinc_error_callback pfn_pinc_error_callback = 0;
pinc_error_callback pfn_pinc_panic_callback = 0;
pinc_error_callback pfn_pinc_user_error_callback = 0;

void* ptr_user_alloc = 0;
pinc_alloc_callback pfn_user_alloc = 0;
pinc_alloc_aligned_callback pfn_user_alloc_aligned = 0;
pinc_realloc_callback pfn_user_realloc = 0;
pinc_free_callback pfn_user_free = 0;

Allocator rootAllocator;
Allocator tempAllocator;

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

static AllocatorVtable platform_alloc_vtable = (AllocatorVtable) {
    .allocate = &pinc_root_platform_allocate,
    .allocateAligned = &pinc_root_platform_allocateAligned,
    .reallocate = &pinc_root_platform_reallocate,
    .free = &pinc_root_platform_free,
};

// Implementation of allocator based on the user callbacks
// These need to exist due to the potential ABI difference between this internal allocator, and the PINC_PROC_CALL setting.
// the object can be set to the user pointer though
static void* pinc_root_user_allocate(void* obj, size_t size) {
    return pfn_user_alloc(obj, size);
}

static void* pinc_root_user_allocateAligned(void* obj, size_t size, size_t alignment) {
    return pfn_user_alloc_aligned(obj, size, alignment);
}

static void* pinc_root_user_reallocate(void* obj, void* ptr, size_t oldSize, size_t newSize) {
    return pfn_user_realloc(obj, ptr, oldSize, newSize);
}

static void pinc_root_user_free(void* obj, void* ptr, size_t size) {
    pfn_user_free(obj, ptr, size);
}

static AllocatorVtable user_alloc_vtable = (AllocatorVtable) {
    .allocate = &pinc_root_user_allocate,
    .allocateAligned = &pinc_root_user_allocateAligned,
    .reallocate = &pinc_root_user_reallocate,
    .free = &pinc_root_user_free,
};

// Some other variables exclusive to this translation unit

static WindowBackend sdl2WindowBackend;

// On the root allocator
static FramebufferFormat* framebufferFormats = 0;

static size_t framebufferFormatNum;

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

static PincObject* objects = 0;
static size_t objectsNum = 0;
static size_t objectsCapacity = 0;

static pinc_object PincObject_allocate(void) {
    if(objectsCapacity - objectsNum == 0) {
        if(!objects) {
            objects = Allocator_allocate(rootAllocator, sizeof(PincObject) * 8);
            objectsCapacity = 8;
        } else {
            size_t newObjectsCapacity = objectsCapacity * 2;
            objects = Allocator_reallocate(rootAllocator, objects, sizeof(PincObject) * objectsCapacity, sizeof(PincObject) * newObjectsCapacity);
            objectsCapacity = newObjectsCapacity;
        }
    }
    // TODO: get an empty spot instead of growing the list indefinitely
    objectsNum++;
    if(objectsNum >= UINT32_MAX) PPANIC_NL("Integer Overflow\n");
    pinc_object id = (pinc_object)objectsNum;
    return id;
}

static PincObject* PincObject_ref(pinc_object handle) {
    if(handle == 0) PPANIC_NL("Object handle is null\n");
    if(handle > objectsNum) PPANIC_NL("Object handle is outside the range\n");
    return &objects[handle-1];
}

static void PincObject_free(pinc_object handle) {
    PincObject* obj = PincObject_ref(handle);
    pMemSet(0, (uint8_t*)obj, sizeof(PincObject));
    if(handle == objectsNum) {
        objectsNum--;
    }
}

bool windowBackendSet = false;

WindowBackend windowBackend;

// Below are the actual implementations of the Pinc API functions.

PINC_EXPORT void PINC_CALL pinc_preinit_set_error_callback(pinc_error_callback callback) {
    pfn_pinc_error_callback = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_panic_callback(pinc_error_callback callback) {
    pfn_pinc_panic_callback = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_user_error_callback(pinc_error_callback callback) {
    pfn_pinc_user_error_callback = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_alloc_callbacks(void* user_ptr, pinc_alloc_callback alloc, pinc_alloc_aligned_callback alloc_aligned, pinc_realloc_callback realloc, pinc_free_callback free) {
    ptr_user_alloc = user_ptr;
    pfn_user_alloc = alloc;
    pfn_user_alloc_aligned = alloc_aligned;
    pfn_user_realloc = realloc;
    pfn_user_free = free;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_incomplete_init(void) {
    // First up, allocator needs set up
    PUSEERROR_LIGHT_NL(
        (pfn_user_alloc && pfn_user_alloc_aligned && pfn_user_realloc && pfn_user_free)
        || !(pfn_user_alloc || pfn_user_alloc_aligned || pfn_user_realloc || pfn_user_free), 
        "Pinc allocator callbacks must either be all set or all null!\n");
    
    if(pfn_user_alloc) {
        // User callbacks are set
        rootAllocator = (Allocator) {
            .allocatorObjectPtr = ptr_user_alloc,
            .vtable = &user_alloc_vtable,
        };
    } else {
        // user callbacks are not set
        rootAllocator = (Allocator){
            .allocatorObjectPtr = 0,
            .vtable = &platform_alloc_vtable,
        };
    }

    // TODO: FIX THIS POOPY GARBAGE CRAP!!
    tempAllocator = rootAllocator;

    // Begin initialization of window backends
    // SDL2 is the only one that actually exists so yeah

    bool sdl2InitRes = psdl2Init(&sdl2WindowBackend);

    if(!sdl2InitRes) {
        PERROR_NL("No supported window backends available!\n");
        return pinc_return_code_error;
    }

    // Query collective information from window backends
    // Again, SDL2 is the only window backend, so some shortcuts can be taken
    // Once there are more window backends, all of their various framebuffer formats
    // need to be merged into one list and properly mapped to user-facing ID handles
    framebufferFormats = WindowBackend_queryFramebufferFormats(&sdl2WindowBackend, rootAllocator, &framebufferFormatNum);
    
    return pinc_return_code_pass;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_window_backends(pinc_window_backend* backend_dest, uint32_t capacity) {
    // TODO: Only window backend is SDL2, shortcuts are taken
    if(backend_dest && capacity < 1){
        *backend_dest = pinc_window_backend_sdl2;
    }
    return 1;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_graphics_backends(pinc_window_backend window_backend, pinc_graphics_backend* backend_dest, uint32_t capacity) {
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    size_t numGraphicsBackends;
    pinc_graphics_backend* supportedBackends = WindowBackend_queryGraphicsBackends(&sdl2WindowBackend, tempAllocator, &numGraphicsBackends);
    size_t numToCopy = capacity;
    if(numToCopy > numGraphicsBackends) {
        numToCopy = numGraphicsBackends;
    }
    if(backend_dest) {
        pMemCopy(supportedBackends, backend_dest, numToCopy);
    }
    if(numGraphicsBackends > UINT32_MAX) PPANIC_NL("Integer Overflow\n");
    return (uint32_t)numGraphicsBackends;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_ids(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format* ids_dest, uint32_t capacity) {
    P_UNUSED(window_backend);
    P_UNUSED(graphics_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    // TODO: sort framebuffers from best to worst, so applications can just loop from first to last and pick the first one they see that they like
    // TODO: is it really safe to assume that we can just filter the formats that are the same between the two backends and call all of those supported?
    // In other words, if both the window backend and graphics backend both support a given framebuffer format,
    // is it safe to assume that it can be used if that specific combination of window/graphics backend is chosen?
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow\n");
    if(capacity > INT32_MAX) PPANIC_NL("Integer Overflow\n");

    if(ids_dest) {
        // Since there's only one window backend and only one graphics backend, just return the list we have as-is
        for(int32_t i=0; i<(int32_t)framebufferFormatNum && i<(int32_t)capacity; ++i) {
            ids_dest[i] = i;
        }
    }
    return (uint32_t)framebufferFormatNum;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow");
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    PUSEERROR_LIGHT_NL(format_id < (int32_t)framebufferFormatNum && format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    return framebufferFormats[format_id].channels;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, uint32_t channel) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow");
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    PUSEERROR_LIGHT_NL(format_id < (int32_t)framebufferFormatNum && format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    FramebufferFormat* fmt = &framebufferFormats[format_id];
    PUSEERROR_LIGHT_NL(channel < fmt->channels, "channel index out of bounds - did you make sure it's less than what pinc_query_framebuffer_format_channels returns for this format?\n");
    return fmt->channel_bits[channel];
}

PINC_EXPORT pinc_color_space PINC_CALL pinc_query_framebuffer_format_color_space(pinc_framebuffer_format format_id) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow");
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    PUSEERROR_LIGHT_NL(format_id < (int32_t)framebufferFormatNum && format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    return framebufferFormats[format_id].color_space;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_graphics_samples_options(pinc_graphics_backend graphics_backend, pinc_framebuffer_format format_id, uint32_t* samples_dest, uint32_t capacity) {
    P_UNUSED(graphics_backend);
    P_UNUSED(format_id);
    P_UNUSED(samples_dest);
    P_UNUSED(capacity);
    PPANIC_NL("pinc_query_graphics_samples_options is not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_graphics_depth_options(pinc_graphics_backend graphics_backend, pinc_framebuffer_format format_id, uint32_t* depth_bits_dest, uint32_t capacity) {
    P_UNUSED(graphics_backend);
    P_UNUSED(format_id);
    P_UNUSED(depth_bits_dest);
    P_UNUSED(capacity);
    PPANIC_NL("pinc_query_graphics_depth_options is not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend) {
    P_UNUSED(window_backend);
    // TODO: only window backend is SDL2, shortcuts are taken
    return WindowBackend_queryMaxOpenWindows(&sdl2WindowBackend);
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_graphics_alpha_options(pinc_graphics_backend graphics_backend, pinc_framebuffer_format format_id, uint32_t* alpha_bits_dest, uint32_t capacity) {
    P_UNUSED(graphics_backend);
    P_UNUSED(format_id);
    P_UNUSED(alpha_bits_dest);
    P_UNUSED(capacity);
    PPANIC_NL("pinc_query_graphics_alpha_options is not implemented");
    return 0;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_complete_init(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format framebuffer_format_id, uint32_t samples, uint32_t depth_buffer_bits) {
    // TODO: only window backend is SDL2, shortcuts are taken
    PUSEERROR_LIGHT_NL(window_backend != pinc_window_backend_none, "Unsupported window backend\n");
    if(window_backend == pinc_window_backend_any) {
        window_backend = pinc_window_backend_sdl2;
    }
    // TODO: only graphics backend is raw opengl, shortcuts are taken
    if(graphics_backend == pinc_graphics_backend_any) {
        graphics_backend = pinc_graphics_backend_raw_opengl;
    }
    if(framebuffer_format_id == -1) {
        // TODO: actually choose the best one instead of just grabbing the first one
        framebuffer_format_id = 0;
    }
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow");
    PUSEERROR_LIGHT_NL(framebuffer_format_id < (int32_t)framebufferFormatNum && framebuffer_format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    FramebufferFormat framebuffer = framebufferFormats[framebuffer_format_id];
    pinc_return_code result = WindowBackend_completeInit(&sdl2WindowBackend, graphics_backend, framebuffer, samples, depth_buffer_bits);
    if(result == pinc_return_code_error) {
        return pinc_return_code_error;
    }
    windowBackend = sdl2WindowBackend;
    windowBackendSet = true;

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

    // Destroy framebuffer formats
    if(framebufferFormats && framebufferFormatNum) {
        Allocator_free(rootAllocator, framebufferFormats, framebufferFormatNum * sizeof(FramebufferFormat));
        framebufferFormats = NULL;
        framebufferFormatNum = 0;
    }

    if(objects && objectsCapacity) {
        // Go through every object and destroy it
        for(uint32_t i=0; i<objectsNum; ++i) {
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
                    PPANIC_NL("Not Implemented");
                    break;
                }
                case pinc_object_type_uniforms: {
                    // TODO
                    PPANIC_NL("Not Implemented");
                    break;
                }
                case pinc_object_type_shaders: {
                    // TODO
                    PPANIC_NL("Not Implemented");
                    break;
                }
                case pinc_object_type_pipeline: {
                    // TODO
                    PPANIC_NL("Not Implemented");
                    break;
                }
                case pinc_object_type_vertex_array: {
                    // TODO
                    PPANIC_NL("Not Implemented");
                    break;
                }
                case pinc_object_type_texture: {
                    // TODO
                    PPANIC_NL("Not Implemented");
                    break;
                }
                default:
                    PPANIC_NL("Invalid object type! This is an error in Pinc itself.\n");
            }
        }
    }

    // deinit backends
    // TODO: only window backend is sdl2, shortcuts are taken
    if(windowBackendSet) {
        psdl2Deinit(&windowBackend);
        windowBackendSet = false;
        windowBackend.obj = 0;
        sdl2WindowBackend.obj = 0;
    }
}

PINC_EXPORT pinc_window_backend PINC_CALL pinc_query_set_window_backend(void) {
    PPANIC_NL("pinc_query_set_window_backend not implemented");
    return 0;
}

PINC_EXPORT pinc_graphics_backend PINC_CALL pinc_query_set_graphics_backend(void) {
    PPANIC_NL("pinc_query_set_graphics_backend not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_set_framebuffer_format(void) {
    PPANIC_NL("pinc_query_set_framebuffer_format not implemented");
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
        PPANIC_NL("Invalid object type - this is an error within Pinc!\n");
    }
}

PINC_EXPORT bool PINC_CALL pinc_get_object_complete(pinc_object obj) {
    P_UNUSED(obj);
    PPANIC_NL("pinc_get_object_complete not implemented");
    return false;
}

PINC_EXPORT pinc_window PINC_CALL pinc_window_create_incomplete(void) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
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
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    PincObject* object = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_incompleteWindow, "Window is not an incomplete window object\n");
    WindowHandle handle = WindowBackend_completeWindow(&windowBackend, &object->data.incompleteWindow);
    if(!handle) {
        return pinc_return_code_error;
    }
    object->discriminator = PincObjectDiscriminator_window;
    object->data.window = handle;
    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_window_deinit(pinc_window window) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    PincObject* object = PincObject_ref(window);
    switch(object->discriminator) {
        case PincObjectDiscriminator_incompleteWindow:{
            PincObject_free(window);
            break;
        }
        case PincObjectDiscriminator_window:{
            WindowBackend_deinitWindow(&windowBackend, &object->data.window);
            break;
        }
        default:{
            PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_incompleteWindow, "Window is not a window object\n");
            break;
        }
    }
}

PINC_EXPORT void PINC_CALL pinc_window_set_title(pinc_window window, const char* title_buf, uint32_t title_len) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    PincObject* object = PincObject_ref(window);
    if(title_len == 0) {
        size_t realTitleLen = pStringLen(title_buf);
        if(realTitleLen > UINT32_MAX) PPANIC_NL("Integer overflow");
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
            WindowBackend_setWindowTitle(&windowBackend, object->data.window, titlePtr, title_len);
            break;
        default:
            PUSEERROR_LIGHT_NL(false, "Window is not a window object\n");
            break;
    }
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_title(pinc_window window, char* title_buf, uint32_t title_capacity) {
    P_UNUSED(window);
    P_UNUSED(title_buf);
    P_UNUSED(title_capacity);
    PPANIC_NL("pinc_window_get_title not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_width(pinc_window window, uint32_t width) {
    P_UNUSED(window);
    P_UNUSED(width);
    PPANIC_NL("pinc_window_set_width not implemented");
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_width(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_width not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_has_width(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_has_width not implemented");
    return 0;
}

PINC_EXPORT void PINC_CALL pinc_window_set_height(pinc_window window, uint32_t height) {
    P_UNUSED(window);
    P_UNUSED(height);
    PPANIC_NL("pinc_window_set_height not implemented");
}

PINC_EXPORT uint32_t PINC_CALL pinc_window_get_height(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_height not implemented");
    return 0;
}

PINC_EXPORT bool PINC_CALL pinc_window_has_height(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_has_height not implemented");
    return false;
}

PINC_EXPORT float PINC_CALL pinc_window_get_scale_factor(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_scale_factor not implemented");
    return 0;
}

PINC_EXPORT int PINC_CALL pinc_window_has_scale_factor(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_has_scale_factor not implemented");
    return 0;
}

PINC_EXPORT void PINC_CALL pinc_window_set_resizable(pinc_window window, bool resizable) {\
    P_UNUSED(window);
    P_UNUSED(resizable);
    PPANIC_NL("pinc_window_set_resizable not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_resizable(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_resizable not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_minimized(pinc_window window, bool minimized) {
    P_UNUSED(window);
    P_UNUSED(minimized);
    PPANIC_NL("pinc_window_set_minimized not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_minimized(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_minimized not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_maximized(pinc_window window, bool maximized) {
    P_UNUSED(window);
    P_UNUSED(maximized);
    PPANIC_NL("pinc_window_set_maximized not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_maximized(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_maximized not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_fullscreen(pinc_window window, bool fullscreen) {
    P_UNUSED(window);
    P_UNUSED(fullscreen);
    PPANIC_NL("pinc_window_set_fullscreen not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_fullscreen(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_fullscreen not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_focused(pinc_window window, bool focused) {
    P_UNUSED(window);
    P_UNUSED(focused);
    PPANIC_NL("pinc_window_set_focused not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_focused(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_focused not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_set_hidden(pinc_window window, bool hidden) {
    P_UNUSED(window);
    P_UNUSED(hidden);
    PPANIC_NL("pinc_window_set_hidden not implemented");
}

PINC_EXPORT bool PINC_CALL pinc_window_get_hidden(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_get_hidden not implemented");
    return false;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_set_vsync(bool sync) {
    P_UNUSED(sync);
    PPANIC_NL("pinc_set_vsync not implemented");
    return pinc_return_code_error;
}

PINC_EXPORT bool PINC_CALL pinc_get_vsync(void) {
    PPANIC_NL("pinc_get_vsync not implemented");
    return false;
}

PINC_EXPORT void PINC_CALL pinc_window_present_framebuffer(pinc_window window) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    PincObject* object = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_window, "Window must be a window object\n");
    WindowBackend_windowPresentFramebuffer(&windowBackend, object->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_mouse_button_get(int button) {
    P_UNUSED(button);
    PPANIC_NL("pinc_mouse_button_get not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_keyboard_key_get(pinc_keyboard_key button) {
    P_UNUSED(button);
    PPANIC_NL("pinc_keyboard_key_get not implemented");
    return false;
}

PINC_EXPORT uint32_t PINC_CALL pinc_get_cursor_x(void) {
    PPANIC_NL("pinc_get_cursor_x not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_get_cursor_y(void) {
    PPANIC_NL("pinc_get_cursor_y not implemented");
    return 0;
}

PINC_EXPORT pinc_window PINC_CALL pinc_get_cursor_window(void) {
    PPANIC_NL("pinc_get_cursor_window not implemented");
    return 0;
}

PINC_EXPORT void PINC_CALL pinc_step(void) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    WindowBackend_step(&windowBackend);
    // TODO: clear temp allocator
}

PINC_EXPORT bool PINC_CALL pinc_event_window_closed(pinc_window window) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    PincObject* object = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_window, "Window must be a window object\n");
    return WindowBackend_windowEventClosed(&windowBackend, object->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_event_window_mouse_button(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_mouse_button not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_resized(pinc_window window) {
    // TODO: properly implement this
    PincObject* obj = PincObject_ref(window);
    return WindowBackend_windowEventResized(&windowBackend, obj->data.window);
}

PINC_EXPORT bool PINC_CALL pinc_event_window_focused(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_focused not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_unfocused(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_unfocused not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_exposed(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_exposed not implemented");
    return false;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_window_keyboard_button_get(pinc_window window, pinc_keyboard_key* key_buffer, uint32_t capacity) {
    P_UNUSED(window);
    P_UNUSED(key_buffer);
    P_UNUSED(capacity);
    PPANIC_NL("pinc_event_window_keyboard_button_get not implemented");
    return 0;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_window_keyboard_button_get_repeat(pinc_window window, bool* repeat_buffer, uint32_t capacity) {
    P_UNUSED(window);
    P_UNUSED(repeat_buffer);
    P_UNUSED(capacity);
    PPANIC_NL("pinc_event_window_keyboard_button_get_repeat not implemented");
    return 0;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_cursor_move(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_cursor_move not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_cursor_exit(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_cursor_exit not implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_event_window_cursor_enter(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_event_window_cursor_enter not implemented");
    return false;
}

PINC_EXPORT uint32_t PINC_CALL pinc_event_window_text_get(pinc_window window, uint8_t* return_buf, uint32_t capacity) {
    P_UNUSED(window);
    P_UNUSED(return_buf);
    P_UNUSED(capacity);
    PPANIC_NL("pinc_event_window_text_get not implemented");
    return 0;
}

PINC_EXPORT float PINC_CALL pinc_window_scroll_vertical(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_scroll_vertical not implemented");
    return 0;
}

PINC_EXPORT float PINC_CALL pinc_window_scroll_horizontal(pinc_window window) {
    P_UNUSED(window);
    PPANIC_NL("pinc_window_scroll_horizontal not implemented");
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
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");

    return WindowBackend_queryRawGlVersionSupported(&sdl2WindowBackend, major, minor, es);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_accumulator_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t channel, uint32_t bits){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");
    // TODO: -1 for default framebuffer format
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer overflow");
    PUSEERROR_LIGHT_NL(framebuffer < (int32_t)framebufferFormatNum, "Invalid framebuffer format ID\n");
    FramebufferFormat framebufferFormat = framebufferFormats[framebuffer];
    return WindowBackend_queryRawGlAccumulatorBits(&sdl2WindowBackend, framebufferFormat, channel, bits);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_stereo_buffer(pinc_window_backend backend, pinc_framebuffer_format framebuffer) {
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");
    // TODO: -1 for default framebuffer format
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow");
    PUSEERROR_LIGHT_NL(framebuffer < (int32_t)framebufferFormatNum, "Invalid framebuffer format ID\n");
    FramebufferFormat framebufferFormat = framebufferFormats[framebuffer];
    return WindowBackend_queryRawGlStereoBuffer(&sdl2WindowBackend, framebufferFormat);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_context_debug(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");
    return WindowBackend_queryRawGlContextDebug(&sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_forward_compatible(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");
    return WindowBackend_queryRawGlForwardCompatible(&sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_robust_access(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");
    return WindowBackend_queryRawGlRobustAccess(&sdl2WindowBackend);
}

PINC_EXPORT pinc_raw_opengl_support_status PINC_CALL pinc_query_raw_opengl_reset_isolation(pinc_window_backend backend){
    // TODO: Only backend is sdl2, shortcuts are taken
    if(backend == pinc_window_backend_any) {
        backend = pinc_window_backend_sdl2;
    }
    PUSEERROR_LIGHT_NL(backend == pinc_window_backend_sdl2, "Unsupported window backend\n");
    PUSEERROR_LIGHT_NL(sdl2WindowBackend.obj, "No backends initialized - did you forget to call pinc_incomplete_init?\n");
    return WindowBackend_queryRawGlResetIsolation(&sdl2WindowBackend);
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
    PPANIC_NL("Not Implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_stereo_buffer(pinc_raw_opengl_context incomplete_context, bool stereo) {
    P_UNUSED(incomplete_context);
    P_UNUSED(stereo);
    // TODO
    PPANIC_NL("Not Implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_context_debug(pinc_raw_opengl_context incomplete_context, bool debug) {
    P_UNUSED(incomplete_context);
    P_UNUSED(debug);
    // TODO
    PPANIC_NL("Not Implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_forward_compatible(pinc_raw_opengl_context incomplete_context, bool compatible) {
    P_UNUSED(incomplete_context);
    P_UNUSED(compatible);
    // TODO
    PPANIC_NL("Not Implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_robust_access(pinc_raw_opengl_context incomplete_context, bool robust) {
    P_UNUSED(incomplete_context);
    P_UNUSED(robust);
    // TODO
    PPANIC_NL("Not Implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_reset_isolation(pinc_raw_opengl_context incomplete_context, bool isolation) {
    P_UNUSED(incomplete_context);
    P_UNUSED(isolation);
    // TODO
    PPANIC_NL("Not Implemented");
    return pinc_return_code_error;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_set_context_version(pinc_raw_opengl_context incomplete_context, uint32_t major, uint32_t minor, bool es, bool core) {
    // TODO validation
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    // TODO check that this version is available

    PincObject* contextObj = PincObject_ref(incomplete_context);
    PUSEERROR_LIGHT_NL(contextObj->discriminator == PincObjectDiscriminator_incompleteRawGlContext, "pinc_raw_opengl_set_context_version: Object must be an incomplete raw OpenGl context\n");
    contextObj->data.incompleteRawGlContext.versionMajor = major;
    contextObj->data.incompleteRawGlContext.versionMinor = minor;
    contextObj->data.incompleteRawGlContext.versionEs = es;
    contextObj->data.incompleteRawGlContext.core = core;
    contextObj->data.incompleteRawGlContext.forwardCompatible = !core;
    PUSEERROR_LIGHT_NL((!es) && (!core), "pinc_raw_opengl_set_context_version: OpenGl context can either be core or ES, not both\n");
    return pinc_return_code_pass;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_context_complete(pinc_raw_opengl_context incomplete_context) {
    // TODO validation
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");

    PincObject* contextObj = PincObject_ref(incomplete_context);
    PUSEERROR_LIGHT_NL(contextObj->discriminator == PincObjectDiscriminator_incompleteRawGlContext, "pinc_raw_opengl_context_complete: Object must be an incomplete raw OpenGl context\n");
    RawOpenglContextHandle contextHandle = WindowBackend_rawGlCompleteContext(&windowBackend, contextObj->data.incompleteRawGlContext);
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
    PPANIC_NL("Not Implemented");
    return 0;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_stereo_buffer(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC_NL("Not Implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_context_debug(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC_NL("Not Implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_forward_compatible(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC_NL("Not Implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_robust_access(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC_NL("Not Implemented");
    return false;
}

PINC_EXPORT bool PINC_CALL pinc_raw_opengl_get_context_reset_isolation(pinc_raw_opengl_context incomplete_context) {
    P_UNUSED(incomplete_context);
    // TODO
    PPANIC_NL("Not Implemented");
    return false;
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_raw_opengl_make_current(pinc_window window, pinc_raw_opengl_context context) {
    // TODO validation
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");

    PincObject* windowObj = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(windowObj->discriminator == PincObjectDiscriminator_window, "pinc_raw_opengl_make_current: window must be a complete window object\n");
    PincObject* contextObj = PincObject_ref(context);
    PUSEERROR_LIGHT_NL(windowObj->discriminator == PincObjectDiscriminator_rawGlContext, "pinc_raw_opengl_make_current: context must be a complete raw OpenGL object\n");

    return WindowBackend_rawGlMakeCurrent(&windowBackend, windowObj->data.window, contextObj->data.rawGlContext);
}

PINC_EXPORT pinc_window PINC_CALL pinc_raw_opengl_get_current(void) {
    // TODO
    PPANIC_NL("Not Implemented");
    return 0;
}

PINC_EXPORT void* PINC_CALL pinc_raw_opengl_get_proc(char const * procname) {
    // TODO validation
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");

    return WindowBackend_rawGlGetProc(&windowBackend, procname);
}
