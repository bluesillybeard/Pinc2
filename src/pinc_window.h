// Pinc window interface.

#ifndef PINC_WINDOW_H
#define PINC_WINDOW_H

#include <pinc.h>
#include <pinc_opengl.h>

#include "pinc_types.h"
#include "pinc_error.h"

// Tip: if you don't know what's going on with macros, use the '-E' flag in gcc to preprocess the file without compiling it.

// This random pseudo opaque struct is required
struct WindowBackend;

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames, defaultReturn)
#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames)

#define PINC_WINDOW_INTERFACE \
    /* These functions are not part of the interface, and instead are handled differently for each backend. */ \
    /* PINC_WINDOW_INTERFACE_PROCEDURE((WindowBackend* obj), initIncomplete)*/ \
    /* PINC_WINDOW_INTERFACE_FUNCTION(bool, (WindowBackend* obj), isSupported) */ \
    /* ### Initialization / query functions ### */ \
    PINC_WINDOW_INTERFACE_FUNCTION(FramebufferFormat*, (struct WindowBackend* obj, pincAllocator allocator, size_t* outNumFormats), queryFramebufferFormats, (obj, allocator, outNumFormats), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, PincGraphicsApi api), queryGraphicsApiSupport, (obj, api), false) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj), queryMaxOpenWindows, (obj), 0) \
    /* The window backend is in charge of initializing the graphics api at this point */ \
    PINC_WINDOW_INTERFACE_FUNCTION(PincReturnCode, (struct WindowBackend* obj, PincGraphicsApi graphicsApi, FramebufferFormat framebuffer), completeInit, (obj, graphicsApi, framebuffer), PincReturnCode_error) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj), deinit, (obj)) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj), step, (obj)) \
    /* ### Window Property Functions ## */ \
    /* May return null in the case of an error */ \
    PINC_WINDOW_INTERFACE_FUNCTION(WindowHandle, (struct WindowBackend* obj, IncompleteWindow const * incomplete, PincWindowHandle frontHandle), completeWindow, (obj, incomplete, frontHandle), 0) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window), deinitWindow, (obj, window)) \
    /* This function takes ownership of the title's memory. It is assumed to be on the pinc root allocator. */ \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, uint8_t* title, size_t titleLen), setWindowTitle, (obj, window, title, titleLen)) \
    /* Returned memory is owned by the window */ \
    PINC_WINDOW_INTERFACE_FUNCTION(uint8_t const *, (struct WindowBackend* obj, WindowHandle window, size_t* outTitleLen), getWindowTitle, (obj, window, outTitleLen), 0) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, uint32_t width), setWindowWidth, (obj, window, width)) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, WindowHandle window), getWindowWidth, (obj, window), 0) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, uint32_t height), setWindowHeight, (obj, window, height)) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, WindowHandle window), getWindowHeight, (obj, window), 0) \
    /* Returns 0 if the scale factor is unset / unknown */ \
    PINC_WINDOW_INTERFACE_FUNCTION(float, (struct WindowBackend* obj, WindowHandle window), getWindowScaleFactor, (obj, window), 0) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool resizable), setWindowResizable, (obj, window, resizable)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowResizable, (obj, window), false) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool minimized), setWindowMinimized, (obj, window, minimized)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowMinimized, (obj, window), false) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool maximized), setWindowMaximized, (obj, window, maximized)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowMaximized, (obj, window), false) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool fullscreen), setWindowFullscreen, (obj, window, fullscreen)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowFullscreen, (obj, window), false) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool focused), setWindowFocused, (obj, window, focused)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowFocused, (obj, window), false) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window, bool hidden), setWindowHidden, (obj, window, hidden)) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, WindowHandle window), getWindowHidden, (obj, window), false) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincReturnCode, (struct WindowBackend* obj, bool vsync), setVsync, (obj, vsync), PincReturnCode_error) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj), getVsync, (obj), false) \
    /* ### Other Window Functions ### */ \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, WindowHandle window), windowPresentFramebuffer, (obj, window)) \
    /* ### OpenGL functions */ \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, uint32_t major, uint32_t minor, PincOpenglContextProfile profile) , queryGlVersionSupported, (obj, major, minor, profile), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t channel, uint32_t bits), queryGlAccumulatorBits, (obj, framebuffer, channel, bits), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits), queryGlAlphaBits, (obj, framebuffer, bits), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits), queryGlDepthBits, (obj, framebuffer, bits), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits), queryGlStencilBits, (obj, framebuffer, bits), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t samples), queryGlSamples, (obj, framebuffer, samples), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj, FramebufferFormat framebuffer), queryGlStereoBuffer, (obj, framebuffer), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj), queryGlContextDebug, (obj), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj), queryGlRobustAccess, (obj), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglSupportStatus, (struct WindowBackend* obj), queryGlResetIsolation, (obj), PincOpenglSupportStatus_maybe) \
    PINC_WINDOW_INTERFACE_FUNCTION(RawOpenglContextHandle, (struct WindowBackend* obj, IncompleteGlContext incompleteContext), glCompleteContext, (obj, incompleteContext), 0) \
    PINC_WINDOW_INTERFACE_PROCEDURE((struct WindowBackend* obj, RawOpenglContextObject context), glDeinitContext, (obj, context)) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, RawOpenglContextObject context, uint32_t channel), glGetContextAccumulatorBits, (obj, context, channel), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextAlphaBits, (obj, context), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextDepthBits, (obj, context), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextStencilBits, (obj, context), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(uint32_t, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextSamples, (obj, context), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextStereoBuffer, (obj, context), false) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextDebug, (obj, context), false) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextRobustAccess, (obj, context), false) \
    PINC_WINDOW_INTERFACE_FUNCTION(bool, (struct WindowBackend* obj, RawOpenglContextObject context), glGetContextResetIsolation, (obj, context), false) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincReturnCode, (struct WindowBackend* obj, WindowHandle window, RawOpenglContextHandle context), glMakeCurrent, (obj, window, context), PincReturnCode_error) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincWindowHandle, (struct WindowBackend* obj), glGetCurrentWindow, (obj), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincOpenglContextHandle, (struct WindowBackend* obj), glGetCurrentContext, (obj), 0) \
    PINC_WINDOW_INTERFACE_FUNCTION(PincPfn, (struct WindowBackend* obj, char const* procname), glGetProc, (obj, procname), 0) \

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE
// Classic interfaces in C.
// TODO(bluesillybeard): evaluate if this actually saves time or is just a big mess
#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames, defaultReturn) type (* name) arguments; //NOLINT these macros are intentionally non-parenthesized. Homework: can you figure out why?
#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) void (* name) arguments; //NOLINT these macros are intentionally non-parenthesized. Homework: can you figure out why?

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

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames, defaultReturn)\
    static type pincWindowBackend_##name arguments { \
        if(obj -> vt.name == 0) { \
            PErrorExternal(false, "Function " #name " Is not implemented for this window backend!"); \
            return (defaultReturn); \
        } \
        return obj -> vt.name argumentsNames;\
    }

#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames)\
    static void pincWindowBackend_##name arguments { \
        if(obj -> vt.name == 0) { \
            PErrorExternal(false, "Function " #name " Is not implemented for this window backend!"); \
            return; \
        } \
        obj -> vt.name argumentsNames;\
    }

PINC_WINDOW_INTERFACE

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE

// These are declared here to avoid a circular reference between pinc_main.h and window.h
// However, the variables themselves are located in pinc_main.c
// TODO(bluesillybeard): wait, is that actually a problem? I'm genuinely confused.

extern bool windowBackendSet; //NOLINT: see above

extern WindowBackend windowBackend; //NOLINT: see above

#endif
