#ifndef PINC_TYPES_H
#define PINC_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pinc.h>
#include "libs/pstring.h"

// Internal versions of external Pinc api pieces

typedef struct {
    uint32_t channels;
    uint32_t channel_bits[4];
    pinc_color_space color_space;
} FramebufferFormat;

typedef struct {
    // Allocated on the root allocator
    PString title;
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
    bool stereo;
    bool debug;
    bool forwardCompatible;
    bool robustAccess;
    uint32_t versionMajor;
    uint32_t versionMinor;
    bool versionEs;
    bool core;
} IncompleteRawGlContext;

typedef void* RawOpenglContextHandle;

#endif
