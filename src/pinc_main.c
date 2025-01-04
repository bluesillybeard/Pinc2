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

Allocator rootAllocator;
Allocator tempAllocator;

// Implementation of pinc's root allocator on top of platform.h
static void* pinc_root_platform_allocate(void* obj, size_t size) {
    return pAlloc(size);
}

static void* pinc_root_platform_allocateAligned(void* obj, size_t size, size_t alignment) {
    return pAllocAligned(size, alignment);
}

static void* pinc_root_platform_reallocate(void* obj, void* ptr, size_t oldSize, size_t newSize) {
    return pRealloc(ptr, oldSize, newSize);
}

static void pinc_root_platform_free(void* obj, void* ptr, size_t size) {
    pFree(ptr, size);
}

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

// Some other variables exclusive to this translation unit

static WindowBackend sdl2WindowBackend;

static FramebufferFormat* framebufferFormats = 0;

static size_t framebufferFormatNum;

// pinc objects are a user-facing map from what the user sees, and the actual internal objects / handles.
// The real internal objects are usually done with opaque pointers managed by the backend itself.
typedef enum {
    // This needs to be zero so null-initialized objects are valid
    PincObjectDiscriminator_none = 0,
    PincObjectDiscriminator_incompleteWindow,
    PincObjectDiscriminator_window

} PincObjectDiscriminator;

typedef struct {
    PincObjectDiscriminator discriminator;
    union {
        IncompleteWindow incompleteWindow;
        WindowHandle window;
    };
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
    // TODO: search for an empty spot instead of growing the list indefinitely
    objectsNum++;
    pinc_object id = objectsNum;
    if(id != objectsNum) PPANIC_NL("Integer Overflow\n");
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

PINC_EXPORT pinc_return_code PINC_CALL pinc_incomplete_init(void) {
    // First up, allocator needs set up
    PUSEERROR_LIGHT_NL(
        (pfn_user_alloc && pfn_user_alloc_aligned && pfn_user_realloc && pfn_user_free)
        || !(pfn_user_alloc || pfn_user_alloc_aligned || pfn_user_realloc || pfn_user_free), 
        "Pinc allocator callbacks must either be all set or all null!\n");
    
    if(pfn_user_alloc) {
        // User callbacks are set
        rootAllocator = (Allocator){
            ptr_user_alloc, &pinc_root_user_allocate, &pinc_root_user_allocateAligned, &pinc_root_user_reallocate, &pinc_root_user_free
        };
    } else {
        // user callbacks are not set
        rootAllocator = (Allocator){
            0, &pinc_root_platform_allocate, &pinc_root_platform_allocateAligned, &pinc_root_platform_reallocate, &pinc_root_platform_free
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
    // TODO: only window backend is SDL2, shortcuts are taken
    // TODO: sort framebuffers from best to worst, so applications can just loop from first to last and pick the first one they see that they like
    // TODO: is it really safe to assume that we can just filter the formats that are the same between the two backends and call all of those supported?
    // In other words, if both the window backend and graphics backend both support a given framebuffer format,
    // is it safe to assume that it can be used if that specific combination of window/graphics backend is chosen?
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    if(framebufferFormatNum > INT32_MAX) PPANIC_NL("Integer Overflow\n");

    if(ids_dest) {
        // Since there's only one window backend and only one graphics backend, just return the list we have as-is
        for(int32_t i=0; i<framebufferFormatNum && i<capacity; ++i) {
            ids_dest[i] = i;
        }
    }
    return framebufferFormatNum;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    PUSEERROR_LIGHT_NL(format_id < framebufferFormatNum && format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    return framebufferFormats[format_id].channels;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, uint32_t channel) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    PUSEERROR_LIGHT_NL(format_id < framebufferFormatNum && format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    FramebufferFormat* fmt = &framebufferFormats[format_id];
    PUSEERROR_LIGHT_NL(channel < fmt->channels, "channel index out of bounds - did you make sure it's less than what pinc_query_framebuffer_format_channels returns for this format?\n");
    return fmt->channel_bits[channel];
}

PINC_EXPORT pinc_color_space PINC_CALL pinc_query_framebuffer_format_color_space(pinc_framebuffer_format format_id) {
    // TODO: shortcuts were taken, see pinc_query_framebuffer_format_ids
    PUSEERROR_LIGHT_NL(framebufferFormats != NULL, "Framebuffer formats is null - did you forget to call pinc_incomplete_init?\n");
    PUSEERROR_LIGHT_NL(format_id < framebufferFormatNum && format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    return framebufferFormats[format_id].color_space;
}

PINC_EXPORT uint32_t PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend) {
    // TODO: only window backend is SDL2, shortcuts are taken
    return WindowBackend_queryMaxOpenWindows(&sdl2WindowBackend);
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
    PUSEERROR_LIGHT_NL(framebuffer_format_id < framebufferFormatNum && framebuffer_format_id >= 0, "format_id is not a valid framebuffer format id - did it come from pinc_query_framebuffer_format_ids?\n");
    FramebufferFormat framebuffer = framebufferFormats[framebuffer_format_id];
    pinc_return_code result = WindowBackend_completeInit(&sdl2WindowBackend, graphics_backend, framebuffer, samples, depth_buffer_bits);
    if(result == pinc_return_code_error) {
        return pinc_return_code_error;
    }
    windowBackend = sdl2WindowBackend;
    windowBackendSet = true;

    return pinc_return_code_pass;
}

PINC_EXPORT pinc_window PINC_CALL pinc_window_create_incomplete(void) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_complete_init?\n");
    pinc_window handle = PincObject_allocate();
    PincObject* window = PincObject_ref(handle);
    window->discriminator = PincObjectDiscriminator_incompleteWindow;
    char namebuf[24];
    size_t nameLen = 0;
    pMemCopy("Pinc window ", namebuf, 12);
    nameLen += 12;
    nameLen += pBufPrintUint32(namebuf+12, 12, handle);
    char* name = Allocator_allocate(rootAllocator, nameLen);
    pMemCopy(namebuf, name, nameLen);
    window->incompleteWindow = (IncompleteWindow){
        (uint8_t*)name, // uint8_t* titlePtr;
        nameLen, // size_t titleLen;
        false, // bool hasWidth;
        0, // uint32_t width;
        false, // bool hasHeight;
        0, // uint32_t height;
        true, // bool resizable;
        false, // bool minimized;
        false, // bool maximized;
        false, // bool fullscreen;
        false, // bool focused;
        false, // bool hidden;
    };
    return handle;
}

PINC_EXPORT void PINC_CALL pinc_window_set_title(pinc_window window, const char* title_buf, uint32_t title_len) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_window_create_complete?\n");
    PincObject* object = PincObject_ref(window);
    if(title_len == 0) {
        title_len = pStringLen(title_buf);
    }
    switch (object->discriminator)
    {
        case PincObjectDiscriminator_incompleteWindow:
            // TODO: potential optimization here (this code is quite cold though)
            Allocator_free(rootAllocator, object->incompleteWindow.titlePtr, object->incompleteWindow.titleLen);
            object->incompleteWindow.titlePtr = Allocator_allocate(rootAllocator, title_len);
            object->incompleteWindow.titleLen = title_len;
            pMemCopy(title_buf, object->incompleteWindow.titlePtr, title_len);
            break;
        case PincObjectDiscriminator_window:
            // Window takes ownership of the pointer, but we don't have ownership of title_buf
            uint8_t* titlePtr = Allocator_allocate(rootAllocator, title_len);
            pMemCopy(title_buf, titlePtr, title_len);
            WindowBackend_setWindowTitle(&windowBackend, object->window, titlePtr, title_len);
            break;
        default:
            PUSEERROR_LIGHT_NL(false, "Invalid Object (pinc_window_set_title): Window is not a window object\n");
            break;
    }
}

PINC_EXPORT pinc_return_code PINC_CALL pinc_window_complete(pinc_window window) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_window_create_complete?\n");
    PincObject* object = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_incompleteWindow, "Invalid Object (pinc_window_complete): Window is not an incomplete window object\n");
    WindowHandle handle = WindowBackend_completeWindow(&windowBackend, &object->incompleteWindow);
    if(!handle) {
        return pinc_return_code_error;
    }
    object->discriminator = PincObjectDiscriminator_window;
    object->window = handle;
    return pinc_return_code_pass;
}

PINC_EXPORT void PINC_CALL pinc_step(void) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_window_create_complete?\n");
    WindowBackend_step(&windowBackend);
    // TODO: clear temp allocator
}

PINC_EXPORT bool PINC_CALL pinc_event_window_closed(pinc_window window) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_window_create_complete?\n");
    PincObject* object = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_window, "Invalid Object (pinc_event_window_closed): must be a window object\n");
    return WindowBackend_windowEventClosed(&windowBackend, object->window);
}

PINC_EXPORT void PINC_CALL pinc_window_present_framebuffer(pinc_window window) {
    PUSEERROR_LIGHT_NL(windowBackendSet, "Window backend not set. Did you forget to call pinc_window_create_complete?\n");
    PincObject* object = PincObject_ref(window);
    PUSEERROR_LIGHT_NL(object->discriminator == PincObjectDiscriminator_window, "Invalid Object (pinc_event_window_closed): must be a window object\n");
    WindowBackend_windowPresentFramebuffer(&windowBackend, object->window);
}
