#ifndef PINC_TYPES_H
#define PINC_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pinc.h>
#include "libs/pinc_string.h"
#include "pinc_opengl.h"

// Internal versions of external Pinc api pieces

typedef struct {
    uint32_t channels;
    uint32_t channel_bits[4];
    PincColorSpace color_space;
} FramebufferFormat;

typedef struct {
    // Allocated on the root allocator
    pincString title;
    bool hasWidth;
    uint32_t width;
    bool hasHeight;
    uint32_t height;
    bool resizable;
    bool minimized;
    bool maximized;
    bool fullscreen;
    bool focused;
    bool hidden;
} IncompleteWindow;

typedef void* WindowHandle;

typedef struct {
    uint32_t accumulatorBits[4];
    uint32_t alphaBits;
    uint32_t depthBits;
    uint32_t stencilBits;
    uint32_t samples;
    bool stereo;
    bool debug;
    bool robustAccess;
    bool resetIsolation;
    uint32_t versionMajor;
    uint32_t versionMinor;
    PincOpenglContextProfile profile;
    bool shareWithCurrent;
} IncompleteGlContext;

typedef void* RawOpenglContextHandle;

typedef struct {
    RawOpenglContextHandle handle;
    PincOpenglContextHandle front_handle;
} RawOpenglContextObject;

#endif
