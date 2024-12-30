#include "platform/platform.h"
#include "pinc_main.h"
#include "window.h"
#include "sdl2load.h"

typedef struct {
    SdlWindowHandle sdlWindow;
    uint8_t* titlePtr;
    size_t titleLen;
} Sdl2Window;

typedef struct {
    Sdl2Functions libsdl2;
    // May be null. Is reused as the first window the user creates.
    Sdl2Window* dummyWindow;
    // Whether the dummy window is also in use as a user-facing window object
    bool dummyWindowInUse;
} Sdl2WindowBackend;

static void* sdl2Lib = 0;

static void* sdl2LoadLib(void) {
    // TODO: what are all the library name possibilities?
    // Note: For now, we only use functions from the original 2.0.0 release
    // On my Linux mint system with libsdl2-dev installed, I get:
    // - libSDL2-2.0.so
    // - libSDL2-2.0.so.0
    // - libSDL2-2.0.so.0.3000.0
    // - libSDL2.so
    void* lib = pLoadLibrary((uint8_t*)"SDL2-2.0", 8);
    if(!lib) {
        lib = pLoadLibrary((uint8_t*)"SDL2", 4);
    }
    return lib;
}

// declare the sdl2 window functions

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames) type sdl2##name arguments;

PINC_WINDOW_INTERFACE

#undef PINC_WINDOW_INTERFACE_FUNCTION

bool psdl2Init(WindowBackend* obj) {
    if(sdl2Lib) {
        return true;
    }
    // The only thing required for SDL2 support is for the SDL2 library to be present
    void* lib = sdl2LoadLib();
    if(!lib) {
        pPrintFormat("SDL2 could not be loaded, disabling SDL2 backend\n\n");
        // TODO: clean up
        return false;
    }
    sdl2Lib = lib;
    obj->obj = Allocator_allocate(rootAllocator, sizeof(Sdl2WindowBackend));
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    loadSdl2Functions(sdl2Lib, &this->libsdl2);
    // TODO: warn for any functions that were not loaded
    SdlVersion sdlVersion;
    this->libsdl2.getVersion(&sdlVersion);
    pPrintFormat("Loaded SDL2 version: %i.%i.%i\n", sdlVersion.major, sdlVersion.minor, sdlVersion.patch);
    if(sdlVersion.major < 2) {
        pPrintFormat("SDL version too old, disabling SDL2 backend\n");
        // TODO: clean up
        return false;
    }
    // TODO: where do we need to actually initialize SDL? Ideally we would do lazy-loading if possible.
    this->libsdl2.init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    // Load all of the functions into the vtable
    #define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames) obj->vt.name = sdl2##name;

    PINC_WINDOW_INTERFACE

    #undef PINC_WINDOW_INTERFACE_FUNCTION
    return true;
}

void psdl2Deinit(WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Allocator_free(rootAllocator, this, sizeof(Sdl2WindowBackend));
    // TODO: quit sdl2
}

// Fingers crossed the compiler sees that this obviously counts the number of set bits and uses a more efficient method
// TODO: actually check this
static int bitCount32(uint32_t n) {
    int counter = 0;
    while(n) {
        counter++;
        n &= (n - 1);
    }
    return counter;
}

static Sdl2Window* _dummyWindow(Sdl2WindowBackend* this) {
    if(this->dummyWindow) {
        return this->dummyWindow;
    }
    char const* const title = "Pinc Dummy Window";
    size_t const titleLen = pStringLen(title);
    uint8_t* titlePtr = Allocator_allocate(rootAllocator, titleLen);
    pMemCopy(title, titlePtr, titleLen);
    IncompleteWindow windowSettings = {
        // Ownership is transferred to the window
        titlePtr, // uint8_t* titlePtr;
        titleLen, // size_t titleLen;
        false, // bool hasWidth;
        0, // uint32_t width;
        false, // bool hasHeight;
        0, // uint32_t height;
        true, // bool resizable;
        false, // bool minimized;
        false, // bool maximized;
        false, // bool fullscreen;
        false, // bool focused;
        true, // bool hidden;
        true, // bool vsync;
    };
    this->dummyWindow = sdl2completeWindow(this, &windowSettings);
}

// Quick function for convenience
static inline void _framebufferFormatAdd(FramebufferFormat** formats, size_t* formatsNum, size_t* formatsCapacity, FramebufferFormat const* fmt) {
    for(size_t formatid=0; formatid<(*formatsNum); formatid++) {
        FramebufferFormat ft = *formats[formatid];
        if(ft.color_space != fmt->color_space) {
            continue;
        }
        if(ft.channels != fmt->channels) {
            continue;
        }
        if(ft.channel_bits[0] != fmt->channel_bits[0]) {
            continue;
        }
        if(ft.channels >= 2 && ft.channel_bits[1] != fmt->channel_bits[1]) {
            continue;
        }
        if(ft.channels >= 3 && ft.channel_bits[2] != fmt->channel_bits[2]) {
            continue;
        }
        if(ft.channels >= 4 && ft.channel_bits[3] != fmt->channel_bits[3]) {
            continue;
        }
        // This format is already in the list, don't add it again
        return;

    }
    if(formatsNum == formatsCapacity) {
        size_t newFormatsCapacity = *formatsCapacity*2;
        *formats = Allocator_reallocate(tempAllocator, *formats, sizeof(FramebufferFormat) * (*formatsCapacity), sizeof(FramebufferFormat) * newFormatsCapacity);
        *formatsCapacity = newFormatsCapacity;
    }
    *formats[*formatsNum] = *fmt;
    (*formatsNum)++;
}

// Implementation of the interface

FramebufferFormat* sdl2queryFramebufferFormats(struct WindowBackend* obj, Allocator allocator, size_t* outNumFormats) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // SDL2 has no nice way for us to get a list of possible framebuffer formats
    // However, SDL2 does have functionality to iterate through displays and go through what formats they support.
    // Ideally, SDL2 would report the list of Visuals from X or the list of pixel formats from win32, but this is the next best option

    // Dynamic list to hold the framebuffer formats
    // TODO: A proper set data structure for deduplication is a good idea
    FramebufferFormat* formats = Allocator_allocate(tempAllocator, sizeof(FramebufferFormat)*8);
    size_t formatsNum = 0;
    size_t formatsCapacity = 8;

    int numDisplays = this->libsdl2.getNumVideoDisplays();
    for(int displayIndex=0; displayIndex<numDisplays; ++displayIndex) {
        int numDisplayModes = this->libsdl2.getNumDisplayModes(displayIndex);
        for(int displayModeIndex=0; displayModeIndex<numDisplayModes; ++displayModeIndex) {
            SdlDisplayMode displayMode;
            this->libsdl2.getDisplayMode(displayIndex, displayModeIndex, &displayMode);
            // the pixel format is a bitfield, like so (in little endian order):
            // bytes*8, bits*8, layout*4, order*4, type*4, 1, 0*remaining
            // SDL2 has us covered though, with a nice function that decodes all of it
            SdlPixelFormat* format = this->libsdl2.allocFormat(displayMode.format);
            FramebufferFormat bufferFormat;
            // TODO: this is being refactored out of framebuffer format VERY VERY SOON - pretty much as soon as we have a window opening
            bufferFormat.depth_buffer_bits = 0;
            bufferFormat.max_samples = 1;
            // TODO: properly figure out transparent window support
            if(format->palette) {
                // This is a palette / index based format, which Pinc does not yet support
                // It is actually a planned feature, but for now we'll just ignore these
            } else {
                // There is absolutely no reason for assuming sRGB, other than SDL doesn't let us get what the real color space is.
                // sRGB is a fairly safe bet, and even if it's wrong, 99% of the time if it's not Srgb it's another similar perceptual color space.
                // There is a rare chance that it's a linear color space, but given SDL2's supported platforms, I find that incredibly unlikely.
                bufferFormat.color_space = pinc_color_space_srgb;
                // TODO: is this right? I think so
                bufferFormat.channel_bits[0] = bitCount32(format->Rmask);
                bufferFormat.channel_bits[1] = bitCount32(format->Gmask);
                bufferFormat.channel_bits[2] = bitCount32(format->Bmask);
                if(format->Amask == 0) {
                    // RGB
                    bufferFormat.channels = 3;
                    bufferFormat.channel_bits[3] = 0;
                } else {
                    // RGBA
                    bufferFormat.channels = 4;
                    bufferFormat.channel_bits[3] = bitCount32(format-> Amask);
                    if(bufferFormat.channel_bits[3] == 0) {
                        // An RGBA format that's secretly only RGB isn't actually an RGBA format.
                        bufferFormat.channels = 3;
                    }
                }
                _framebufferFormatAdd(&formats, &formatsNum, &formatsCapacity, &bufferFormat);
            }
            this->libsdl2.freeFormat(format);
        }
    }
    // Allocate the final returned value
    FramebufferFormat* actualFormats = Allocator_allocate(allocator, formatsNum * sizeof(FramebufferFormat));
    for(int fmi = 0; fmi<formatsNum; fmi++) {
        actualFormats[fmi] = formats[fmi];
    }
    // Even though the temp allocator os a bump allocator, we may as well treat it as a real one.
    Allocator_free(tempAllocator, formats, formatsCapacity * sizeof(FramebufferFormat));
    *outNumFormats = formatsNum;
    return actualFormats;
}

pinc_graphics_backend* sdl2queryGraphicsBackends(struct WindowBackend* obj, Allocator allocator, size_t* outNumBackends) {
    // The only one supported for now is raw opengl
    pinc_graphics_backend* backends = Allocator_allocate(allocator, sizeof(pinc_graphics_backend));
    *backends = pinc_graphics_backend_raw_opengl;
    *outNumBackends = 1;
    return backends;
}

uint32_t sdl2queryMaxOpenWindows(struct WindowBackend* obj, pinc_graphics_backend graphicsBackend) {
    // SDL2 has no (arbitrary) limit on how many windows can be open.
    return 0;
}

pinc_return_code sdl2completeInit(struct WindowBackend* obj, pinc_graphics_backend graphicsBackend, FramebufferFormat framebuffer, uint32_t samples) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    switch (graphicsBackend)
    {
        case pinc_graphics_backend_raw_opengl:

            break;
        
        default:
            // We don't support this graphics backend.
            // Technically this code should never run, because the user API frontend should have caught this
            PERROR_NL("Attempt to use SDL2 backend with an unsupported graphics backend\n");
            return pinc_return_code_error;
    }
    return pinc_return_code_pass;
}

void sdl2deinit(struct WindowBackend* obj) {
    // TODO
}

void sdl2step(struct WindowBackend* obj) {
    // TODO
}

WindowHandle sdl2completeWindow(struct WindowBackend* obj, IncompleteWindow const * incomplete) {
    // TODO: only graphics backend is OpenGL, shortcuts are taken
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // TODO: reuse dummy window
    // if(!this->dummyWindowInUse && this->dummyWindow) {
    //     this->dummyWindowInUse = true;
    //     if(incomplete->hasWidth) {
    //         sdl2setWindowWidth(this, this->dummyWindow, incomplete->width);
    //     }
    //     if(incomplete->hasHeight) {
    //         sdl2setWindowHeight(this, this->dummyWindow, incomplete->height);
    //     }    
    // }
    // SDL expects a null-terminated title, while our actual title is not null terminated
    // (How come nobody ever makes options for those using non null-terminated strings?)
    // TODO: should probably make some quick string marshalling functions for convenience
    // Something like Allocator_marshalToNullterm(allocator, string, length) and Allocator_marshalFromNullTerm(allocator, string, *outLen)
    // Reminder: SDL2 uses UTF8 encoding for pretty much all strings
    char* titleNullTerm = Allocator_allocate(tempAllocator, incomplete->titleLen+1);
    pMemCopy(incomplete->titlePtr, titleNullTerm, incomplete->titleLen);
    titleNullTerm[incomplete->titleLen] = 0;
    // TODO: is ResetHints a good idea?
    uint32_t windowFlags = 0;
    if(incomplete->resizable) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    if(incomplete->minimized) {
        windowFlags |= SDL_WINDOW_MINIMIZED;
    }
    if(incomplete->maximized) {
        windowFlags |= SDL_WINDOW_MAXIMIZED;
    }
    if(incomplete->fullscreen) {
        // TODO: do we want to use fullscreen or fullscreen_desktop? the difference between them does not seem to be properly documented.
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }
    if(incomplete->focused) {
        // TODO: Does keyboard grabbed just steal keyboard input entirely, or does it allow the user to move to other windows normally?
        // What even is keyboard grabbed? the SDL2 documentation is annoyingly not specific.
        windowFlags |= SDL_WINDOW_INPUT_FOCUS; // | SDL_WINDOW_KEYBOARD_GRABBED;
    }
    if(incomplete->hidden) {
        windowFlags |= SDL_WINDOW_HIDDEN;
    }
    if(incomplete->vsync) {
        if(this->libsdl2.glSetSwapInterval(-1) == -1) {
            // uh oh, we couldn't set the swap interval
            // Try again with a value of 1 (so normal non-adaptive vsync)
            if(this->libsdl2.glSetSwapInterval(1) == -1) {
                // So, we cannot set vsync at all
                // TODO: Pinc really needs a proper warning print system
                pPrintFormat("SDL2 Could not set the swap interval: %s\n", this->libsdl2.getError());
            }
        }
    }
    SdlWindowHandle win = this->libsdl2.createWindow(titleNullTerm, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 256, windowFlags);
    // I'm so paranoid, I actually went through the SDL2 source code to make sure it actually duplicates the window title to avoid a use-after-free
    // Better too worried than not enough I guess
    Allocator_free(tempAllocator, titleNullTerm, incomplete->titleLen+1);

    // Title's ownership is in the window object itself
}

void sdl2setWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, uint8_t* title, size_t titleLen) {
    // We take ownership of the title
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    Allocator_free(rootAllocator, window->titlePtr, window->titleLen);
    window->titlePtr = title;
    window->titleLen = titleLen;
    // Let SDL2 know that the title was changed
    // It needs to me null terminated because reasons
    // TODO: let SDL2 have the window title and not keep our own copy of it?
    char* titleNullTerm = Allocator_allocate(tempAllocator, titleLen+1);
    pMemCopy(title, titleNullTerm, titleLen);
    titleNullTerm[titleLen] = 0;
    this->libsdl2.setWindowTitle(window->sdlWindow, titleNullTerm);
    Allocator_free(tempAllocator, titleNullTerm, titleLen+1);
}

uint8_t const * sdl2getWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, size_t* outTitleLen) {
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    *outTitleLen = window->titleLen;
    return window->titlePtr;
}

void sdl2setWindowWidth(struct WindowBackend* obj, WindowHandle window, uint32_t width) {
    // TODO
}

uint32_t sdl2getWindowWidth(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return 0;
}

void sdl2setWindowHeight(struct WindowBackend* obj, WindowHandle window, uint32_t height) {
    // TODO
}

uint32_t sdl2getWindowHeight(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return 0;
}

float sdl2getWindowScaleFactor(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return 0;
}

void sdl2setWindowResizable(struct WindowBackend* obj, WindowHandle window, bool resizable) {
    // TODO
}

bool sdl2getWindowResizable(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

void sdl2setWindowMinimized(struct WindowBackend* obj, WindowHandle window, bool minimized) {
    // TODO
}

bool sdl2getWindowMinimized(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

void sdl2setWindowMaximized(struct WindowBackend* obj, WindowHandle window, bool maximized) {
    // TODO
}

bool sdl2getWindowMaximized(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

void sdl2setWindowFullscreen(struct WindowBackend* obj, WindowHandle window, bool fullscreen) {
    // TODO
}

bool sdl2getWindowFullscreen(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

void sdl2setWindowFocused(struct WindowBackend* obj, WindowHandle window, bool focused) {
    // TODO
}

bool sdl2getWindowFocused(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

void sdl2setWindowHidden(struct WindowBackend* obj, WindowHandle window, bool hidden) {
    // TODO
}

bool sdl2getWindowHidden(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

void sdl2setWindowVsync(struct WindowBackend* obj, WindowHandle window, bool vsync) {
    // TODO
}

bool sdl2getWindowVsync(struct WindowBackend* obj, WindowHandle window) {
    // TODO
    return false;
}

bool sdl2windowEventClosed(struct WindowBackend* obj, WindowHandle window) {
    
    // TODO
    return false;
}

void sdl2windowPresentFramebuffer(struct WindowBackend* obj, WindowHandle window) {
    // TODO
}

