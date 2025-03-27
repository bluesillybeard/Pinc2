// Pinc window interface.

#ifndef PINC_WINDOW_H
#define PINC_WINDOW_H

#include <pinc.h>
#include <pinc_opengl.h>
#include "pinc_types.h"
#include "pinc_error.h"

// Tip: if you don't know what's going on with macros, use the '-E' flag in gcc to preprocess the file without compiling it.

// This is required to get around annoying recursive structs, which can't be typedef'd for some reason
struct WindowBackend;

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames)
#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames)

#define PINC_WINDOW_INTERFACE \
    /* These functions are not part of the interface, and instead are handled differently for each backend. */ \
    /* PINC_WINDOW_INTERFACE_PROCEDURE((WindowBackend* obj), initIncomplete)*/ \
    /* PINC_WINDOW_INTERFACE_FUNCTION(bool, (WindowBackend* obj), isSupported) */ \
    /* ### Initialization / query functions ### */ \
    PINC_WINDOW_INTERFACE_FUNCTION(FramebufferFormat*, (struct WindowBackend* obj, Allocator allocator, size_t* outNumFormats), queryFramebufferFormats, (obj, allocator, outNumFormats)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, pinc_graphics_api api), queryGraphicsApiSupport, (obj, api)) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj), queryMaxOpenWindows, (obj)) \
    /* The window backend is in charge of initializing the graphics api at this point */ \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_return_code, (struct WindowBackend* obj, pinc_graphics_api graphicsApi, FramebufferFormat framebuffer, uint32_t samples, uint32_t depthBufferBits), completeInit, (obj, graphicsApi, framebuffer, samples, depthBufferBits)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj), deinit, (obj)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj), step, (obj)) \
    /* ### Window Property Functions ## */ \
    /* May return null in the case of an error */ \
    PINC_WINDOW_INTERFACE_FUNCTION(WindowHandle, (struct WindowBackend* obj, IncompleteWindow const * incomplete, pinc_window frontHandle), completeWindow, (obj, incomplete, frontHandle)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window), deinitWindow, (obj, window)) \
    /* This function takes ownership of the title's memory. It is assumed to be on the pinc root allocator. */ \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, uint8_t* title, size_t titleLen), setWindowTitle, (obj, window, title, titleLen)) \
    /* Returned memory is owned by the window */ \
    PINC_WINDOW_INTERFACE_FUNCTION(uint8_t const *, (struct WindowBackend* obj, WindowHandle window, size_t* outTitleLen), getWindowTitle, (obj, window, outTitleLen)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, uint32_t width), setWindowWidth, (obj, window, width)) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, WindowHandle window), getWindowWidth, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, uint32_t height), setWindowHeight, (obj, window, height)) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, WindowHandle window), getWindowHeight, (obj, window)) \
    /* Returns 0 if the scale factor is unset / unknown */ \
    PINC_WINDOW_INTERFACE_FUNCTION(float, (struct WindowBackend* obj, WindowHandle window), getWindowScaleFactor, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool resizable), setWindowResizable, (obj, window, resizable)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowResizable, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool minimized), setWindowMinimized, (obj, window, minimized)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowMinimized, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool maximized), setWindowMaximized, (obj, window, maximized)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowMaximized, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool fullscreen), setWindowFullscreen, (obj, window, fullscreen)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowFullscreen, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool focused), setWindowFocused, (obj, window, focused)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowFocused, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool hidden), setWindowHidden, (obj, window, hidden)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowHidden, (obj, window)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, bool vsync), setVsync, (obj, vsync)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj), getVsync, (obj)) \
    /* ### Other Window Functions ### */ \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window), windowPresentFramebuffer, (obj, window)) \
    /* ### OpenGL functions */ \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj, uint32_t major, uint32_t minor, bool es) , queryGlVersionSupported, (obj, major, minor, es)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t channel, uint32_t bits), queryGlAccumulatorBits, (obj, framebuffer, channel, bits)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits), queryGlAlphaBits, (obj, framebuffer, bits)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits), queryGlDepthBits, (obj, framebuffer, bits)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj, FramebufferFormat framebuffer), queryGlStereoBuffer, (obj, framebuffer)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj), queryGlContextDebug, (obj)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj), queryGlForwardCompatible, (obj)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj), queryGlRobustAccess, (obj)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_support_status, (struct WindowBackend* obj), queryGlResetIsolation, (obj)) \
    PINC_WINDOW_INTERFACE_FUNCTION(RawOpenglContextHandle, (struct WindowBackend* obj, IncompleteGlContext incompleteContext), glCompleteContext, (obj, incompleteContext)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_return_code, (struct WindowBackend* obj, WindowHandle window, RawOpenglContextHandle context), glMakeCurrent, (obj, window, context)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_window, (struct WindowBackend* obj), glGetCurrentWindow, (obj)) \
    PINC_WINDOW_INTERFACE_FUNCTION(pinc_opengl_context, (struct WindowBackend* obj), glGetCurrentContext, (obj)) \
    PINC_WINDOW_INTERFACE_FUNCTION(PINC_PFN, (struct WindowBackend* obj, char const* procname), glGetProc, (obj, procname)) \

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE
// Classic interfaces in C.
// TODO: evaluate if this actually saves time or is just a big mess
#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames) type (* name) arguments;
#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) void (* name) arguments;

struct WindowBackendVtable {
    PINC_WINDOW_INTERFACE
};

typedef struct WindowBackend {
    void* obj;
    // These are inlined instead of using a vtable, because there will only one of these.
    // It reduces the indirection a bit, at the cost of making this struct massive.
    struct WindowBackendVtable vt;
} WindowBackend;

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE

// nice wrapper functions for the WindowBackend functions

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames)\
    static type WindowBackend_##name arguments { \
        if(obj -> vt.name == 0) { \
            PErrorExternal(false, "Function " #name " Is not implemented for this window backend!"); \
        } \
        return obj -> vt.name argumentsNames;\
    }

#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames)\
    static void WindowBackend_##name arguments { \
        if(obj -> vt.name == 0) { \
            PErrorExternal(false, "Function " #name " Is not implemented for this window backend!"); \
        } \
        obj -> vt.name argumentsNames;\
    }

PINC_WINDOW_INTERFACE

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE

// These are declared here to avoid a circular reference between pinc_main.h and window.h
// However, the variables themselves are located in pinc_main.c

extern bool windowBackendSet;

extern WindowBackend windowBackend;

#endif
