#include "platform/platform.h"
#include "pinc_main.h"
#include "window.h"
#include "sdl2load.h"

typedef struct Sdl2WindowEvents {
    bool closed;
} Sdl2WindowEvents;

#define PINC_SDL2_EMPTY_WINDOW_EVENTS (Sdl2WindowEvents) {\
    .closed = false,\
}

typedef struct {
    SDL_Window* sdlWindow;
    PString title;
    Sdl2WindowEvents events;
} Sdl2Window;

typedef struct {
    Sdl2Functions libsdl2;
    // May be null.
    Sdl2Window* dummyWindow;
    // Whether the dummy window is also in use as a user-facing window object
    bool dummyWindowInUse;
    // List of windows.
    // TODO: instead of a list of pointers, just use a list of the struct itself
    // All external references to a window would need to change to be an ID / index into the list, instead of an arbitrary pointer.
    Sdl2Window** windows;
    size_t windowsNum;
    size_t windowsCapacity;
} Sdl2WindowBackend;

// Adds a window to the list of windows
void sdl2AddWindow(Sdl2WindowBackend* this, Sdl2Window* window) {
    if(!this->windows) {
        this->windows = Allocator_allocate(rootAllocator, sizeof(Sdl2Window*) * 8);
        this->windowsNum = 0;
        this->windowsCapacity = 8;
    }
    if(this->windowsCapacity == this->windowsNum) {
        this->windows = Allocator_reallocate(rootAllocator, this->windows, sizeof(Sdl2Window*) * this->windowsCapacity, sizeof(Sdl2Window*) * this->windowsCapacity * 2);
        this->windowsCapacity = this->windowsCapacity * 2;
    }
    this->windows[this->windowsNum] = window;
    this->windowsNum++;
}

// This IS NOT responsible for actually destroying the window.
// It only removes a window from the list of windows.
void sdl2RemoveWindow(Sdl2WindowBackend* this, Sdl2Window* window) {
    // Get the index of this window
    // Linear search is fine - this is cold code (hopefully), and the number of windows (should be) quite small.
    // Arguably it's the user's problem if they are constantly destroying windows and there are enough of them to make linear search slow.
    uintptr_t index;
    for(uintptr_t i=0; i<this->windowsNum; i++) {
        if(this->windows[i] == window) {
            index = i;
            break;
        }
    }
    // Order of windows is not important, swap remove
    this->windows[index] = this->windows[this->windowsNum-1];
    this->windows[this->windowsNum-1] = NULL;
    this->windowsNum--;
}

static void* sdl2Lib = 0;

static void* sdl2LoadLib(void) {
    // TODO: what are all the library name possibilities?
    // Note: For now, we only use functionality from the original 2.0.0 release
    // On my Linux mint system with libsdl2-dev installed, I get:
    // - libSDL2-2.0.so
    // - libSDL2-2.0.so.0
    // - libSDL2-2.0.so.0.3000.0
    // - libSDL2.so - this one only seems to be present with the dev package installed
    void* lib = pLoadLibrary((uint8_t*)"SDL2-2.0", 8);
    if(!lib) {
        lib = pLoadLibrary((uint8_t*)"SDL2", 4);
    }
    return lib;
}

static void sdl2UnloadLib(void* lib) {
    if(lib) {
        pUnloadLibrary(lib);
    }
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
        // sdl2UnloadLib(lib);
        return false;
    }
    sdl2Lib = lib;
    obj->obj = Allocator_allocate(rootAllocator, sizeof(Sdl2WindowBackend));
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    pMemSet(0, this, sizeof(Sdl2WindowBackend));
    loadSdl2Functions(sdl2Lib, &this->libsdl2);
    // TODO: warn for any functions that were not loaded
    SDL_version sdlVersion;
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
    this->libsdl2.quit();
    Allocator_free(rootAllocator, this->windows, sizeof(Sdl2Window*) * this->windowsCapacity);
    Allocator_free(rootAllocator, this, sizeof(Sdl2WindowBackend));
    sdl2UnloadLib(sdl2Lib);
    sdl2Lib = 0;
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
        .title = (PString) {.str = titlePtr, .len = titleLen},
        .hasWidth = false,
        .width = 0,
        .hasHeight = false,
        .height = 0,
        .resizable = true,
        .minimized = false,
        .maximized = false,
        .fullscreen = false,
        .focused = false,
        .hidden = true,
    };
    this->dummyWindow = sdl2completeWindow((struct WindowBackend*)this, &windowSettings);
    if(!this->dummyWindow) PPANIC_NL("Could not create dummy window\n");
    return this->dummyWindow;
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
            SDL_DisplayMode displayMode;
            this->libsdl2.getDisplayMode(displayIndex, displayModeIndex, &displayMode);
            // the pixel format is a bitfield, like so (in little endian order):
            // bytes*8, bits*8, layout*4, order*4, type*4, 1, 0*remaining
            // SDL2 has us covered though, with a nice function that decodes all of it
            SDL_PixelFormat* format = this->libsdl2.allocFormat(displayMode.format);
            FramebufferFormat bufferFormat;
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
                    // RGBA?
                    bufferFormat.channels = 4;
                    bufferFormat.channel_bits[3] = bitCount32(format-> Amask);
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

uint32_t sdl2queryMaxOpenWindows(struct WindowBackend* obj) {
    // SDL2 has no (arbitrary) limit on how many windows can be open.
    return 0;
}

pinc_return_code sdl2completeInit(struct WindowBackend* obj, pinc_graphics_backend graphicsBackend, FramebufferFormat framebuffer, uint32_t samples, uint32_t depthBufferBits) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    switch (graphicsBackend)
    {
        case pinc_graphics_backend_raw_opengl:
            // TODO: probably need to store the samples and depth buffer bits somewhere?
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
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;

    // Iterate all windows and reset their event data
    // TODO: would it be a good idea to make an 'official' way to do this, or is it best for the backend to store its own list of windows?
    for(uintptr_t windowIndex = 0; windowIndex < this->windowsNum; ++windowIndex) {
        this->windows[windowIndex]->events = PINC_SDL2_EMPTY_WINDOW_EVENTS;
    }

    SDL_Event event;
    NEXTLOOP:
    while(this->libsdl2.pollEvent(&event)) {
        switch (event.type) {
            case SDL_WINDOWEVENT: {
                SDL_Window* sdlWin = this->libsdl2.getWindowFromId(event.window.windowID);
                if(!sdlWin) PPANIC_NL("SDL2 window from WindowEvent is NULL!\n");
                Sdl2Window* windowObj = (Sdl2Window*)this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
                if(!windowObj) PPANIC_NL("Pinc SDL2 window object from WindowEvent is NULL!\n");
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:{
                        windowObj->events.closed = true;
                        break;
                    }
                    default:{
                        // TODO: once all window events are handled, assert.
                        break;
                    }
                }
                break;
            }
            default:{
                // TODO: Once all SDL events are handled, assert.
                break;
            }
        }
    }
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
    // Reminder: SDL2 uses UTF8 encoding for pretty much all strings
    char* titleNullTerm = PString_marshalAlloc(incomplete->title, tempAllocator);
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
    SDL_Window* win = this->libsdl2.createWindow(titleNullTerm, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 256, windowFlags);
    // I'm so paranoid, I actually went through the SDL2 source code to make sure it actually duplicates the window title to avoid a use-after-free
    // Better too worried than not enough I guess
    Allocator_free(tempAllocator, titleNullTerm, incomplete->title.len+1);

    Sdl2Window* windowObj = Allocator_allocate(rootAllocator, sizeof(Sdl2Window));
    
    // Title's ownership is in the window object itself
    windowObj->sdlWindow = win;
    windowObj->title = incomplete->title;

    // So we can easily get one of our windows out of the SDL2 window handle
    this->libsdl2.setWindowData(win, "pincSdl2Window", windowObj);

    // Add it to the list of windows
    sdl2AddWindow(this, windowObj);

    return (WindowHandle)windowObj;
}

void sdl2deinitWindow(struct WindowBackend* obj, WindowHandle windowHandle) {
    // TODO
}

void sdl2setWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, uint8_t* title, size_t titleLen) {
    // We take ownership of the title
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    PString_free(&window->title, rootAllocator);
    window->title = (PString){.str = title, .len = titleLen};
    // Let SDL2 know that the title was changed
    // It needs to be null terminated because reasons
    // TODO: let SDL2 have the window title and not keep our own copy of it?
    char* titleNullTerm = PString_marshalAlloc((PString){.str = title, .len = titleLen}, tempAllocator);
    this->libsdl2.setWindowTitle(window->sdlWindow, titleNullTerm);
    Allocator_free(tempAllocator, titleNullTerm, titleLen+1);
}

uint8_t const * sdl2getWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, size_t* outTitleLen) {
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    *outTitleLen = window->title.len;
    return window->title.str;
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

void sdl2setVsync(struct WindowBackend* obj, bool vsync) {
    // TODO
}

bool sdl2getVsync(struct WindowBackend* obj) {
    // TODO
    return false;
}

bool sdl2windowEventClosed(struct WindowBackend* obj, WindowHandle window) {
    Sdl2Window* windowObj = (Sdl2Window*)window;
    return windowObj->events.closed;
}

void sdl2windowPresentFramebuffer(struct WindowBackend* obj, WindowHandle window) {
    // TODO
}

