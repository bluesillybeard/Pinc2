#include "SDL2/SDL_video.h"
#include "pinc_opengl.h"
#include "pinc_options.h"
#include "pinc_types.h"
// To make the build system as simple as possible, backend source files must remove themselves rather than rely on the build system
#if PINC_HAVE_WINDOW_SDL2==1

#include "platform/platform.h"
#include "pinc_main.h"
#include "window.h"
#include "sdl2load.h"

typedef struct {
    SDL_Window* sdlWindow;
    PString title;
    pinc_window frontHandle;
    uint32_t width;
    uint32_t height;
} Sdl2Window;

typedef struct {
    Sdl2Functions libsdl2;
    void* sdl2Lib;
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
    uint32_t mouseState;
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
    bool indexFound = false;
    uintptr_t index;
    for(uintptr_t i=0; i<this->windowsNum; i++) {
        if(this->windows[i] == window) {
            index = i;
            indexFound = true;
            break;
        }
    }
    if(!indexFound) return;
    if(this->windows[index] == this->dummyWindow) {
        this->dummyWindowInUse = false;
    }
    // Order of windows is not important, swap remove
    this->windows[index] = this->windows[this->windowsNum-1];
    this->windows[this->windowsNum-1] = NULL;
    this->windowsNum--;
}

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
#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) void sdl2##name arguments;

PINC_WINDOW_INTERFACE

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE

bool psdl2Init(WindowBackend* obj) {
    obj->obj = Allocator_allocate(rootAllocator, sizeof(Sdl2WindowBackend));
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    pMemSet(0, this, sizeof(Sdl2WindowBackend));
    // The only thing required for SDL2 support is for the SDL2 library to be present
    void* lib = sdl2LoadLib();
    if(!lib) {
        pPrintFormat("SDL2 could not be loaded, disabling SDL2 backend.\n");
        // sdl2UnloadLib(lib);
        return false;
    }
    this->sdl2Lib = lib;

    loadSdl2Functions(this->sdl2Lib, &this->libsdl2);
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
    #define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) obj->vt.name = sdl2##name;

    PINC_WINDOW_INTERFACE

    #undef PINC_WINDOW_INTERFACE_FUNCTION
    #undef PINC_WINDOW_INTERFACE_PROCEDURE
    return true;
}

// Fingers crossed the compiler sees that this obviously counts the number of set bits and uses a more efficient method
// TODO: actually check this
static uint32_t bitCount32(uint32_t n) {
    uint32_t counter = 0;
    while(n) {
        counter++;
        n &= (n - 1);
    }
    return counter;
}

static Sdl2Window* _dummyWindow(struct WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
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
    this->dummyWindow = sdl2completeWindow(obj, &windowSettings, 0);
    // sdl2completeWindow sets this to true, under the assumption the user called it.
    // We are requesting the dummy window not for the user's direct use, so it's NOT in use.
    this->dummyWindowInUse = false;
    // TODO: make this possibly recoverable?
    PErrorAssert(this->dummyWindow, "SDL2 Backend: Could not create dummy window");
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
    for(size_t fmi = 0; fmi<formatsNum; fmi++) {
        actualFormats[fmi] = formats[fmi];
    }
    // Even though the temp allocator os a bump allocator, we may as well treat it as a real one.
    Allocator_free(tempAllocator, formats, formatsCapacity * sizeof(FramebufferFormat));
    *outNumFormats = formatsNum;
    return actualFormats;
}

bool sdl2queryGraphicsApiSupport(struct WindowBackend* obj, pinc_graphics_api api) {
    P_UNUSED(obj);
    switch (api) {
        case pinc_graphics_api_opengl:
            return true;
        default:
            return false;
    }
}

uint32_t sdl2queryMaxOpenWindows(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no (arbitrary) limit on how many windows can be open.
    return 0;
}

pinc_return_code sdl2completeInit(struct WindowBackend* obj, pinc_graphics_api graphicsBackend, FramebufferFormat framebuffer, uint32_t samples, uint32_t depthBufferBits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(samples);
    P_UNUSED(depthBufferBits);
    // Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    switch (graphicsBackend)
    {
        case pinc_graphics_api_opengl:
            // TODO: probably need to store the samples and depth buffer bits somewhere?
            break;
        
        default:
            // We don't support this graphics api.
            // Technically this code should never run, because the user API frontend should have caught this
            PErrorUser(false, "Attempt to use SDL2 backend with an unsupported graphics api");
            return pinc_return_code_error;
    }
    return pinc_return_code_pass;
}

void sdl2deinit(struct WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // Make sure the frontend deleted all of the windows already
    PErrorAssert(this->windowsNum == 0, "Internal pinc error: the frontend didn't delete the windows before calling backend deinit");
    
    this->libsdl2.destroyWindow(this->dummyWindow->sdlWindow);
    PString_free(&this->dummyWindow->title, rootAllocator);
    Allocator_free(rootAllocator, this->dummyWindow, sizeof(Sdl2Window));

    this->libsdl2.quit();
    sdl2UnloadLib(this->sdl2Lib);
    Allocator_free(rootAllocator, this->windows, sizeof(Sdl2Window*) * this->windowsCapacity);
    Allocator_free(rootAllocator, this, sizeof(Sdl2WindowBackend));
}


void sdl2step(struct WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;

    // The offset between SDL2's getTicks() and platform's pCurrentTimeMillis()
    // so that getTicks + timeOffset == pCurrentTimeMillis() (with some margin of error)
    int64_t timeOffset = pCurrentTimeMillis() - ((int64_t)this->libsdl2.getTicks());
    // TODO: see if you can do something about the premature rollover with only 32 bits
    // This might already do that simply by the nature of calculating the offset based on the faulty getTicks,
    // However I'm not certain enough as of yet.

    SDL_Event event;
    // NEXTLOOP:
    while(this->libsdl2.pollEvent(&event)) {
        switch (event.type) {
            case SDL_WINDOWEVENT: {
                SDL_Window* sdlWin = this->libsdl2.getWindowFromId(event.window.windowID);
                // External -> caused by SDL2 giving us events for nonexistent windows
                PErrorExternal(sdlWin, "SDL2 window from WindowEvent is NULL!");
                Sdl2Window* windowObj = (Sdl2Window*)this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
                // Assert -> caused by Pinc not setting the window event data (supposedly)
                PErrorAssert(windowObj, "Pinc SDL2 window object from WindowEvent is NULL!");
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:{
                        PincEventCloseSignal(((int64_t)event.window.timestamp) + timeOffset, windowObj->frontHandle);
                        break;
                    }
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    case SDL_WINDOWEVENT_RESIZED:{
                        // Is this a good time to say just how much I dislike SDL2's event union struct thing?
                        // It's such a big confusing mess. What in the heck is data1 and data2?
                        // Everything is sorta half-manually baked together in weirdly signed mixed data types...
                        // SDL2's imperfect documentation does not make it any easier either.
                        PErrorAssert(event.window.data1 > 0, "Integer underflow");
                        PErrorAssert(event.window.data2 > 0, "Integer underflow");
                        PincEventResize(((int64_t)event.window.timestamp) + timeOffset, windowObj->frontHandle, windowObj->width, windowObj->height, (uint32_t)event.window.data1, (uint32_t)event.window.data2);
                        windowObj->width = (uint32_t)event.window.data1;
                        windowObj->height = (uint32_t)event.window.data2;
                        break;
                    }
                    default:{
                        // TODO: once all window events are handled, assert.
                        break;
                    }
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                // TODO: We only care about mouse 0?
                // What does it even mean to have multiple mice? Multiple cursors?
                // On my X11 based system, it seems all mouse events are merged into mouse 0,
                if(event.button.which == 0) {
                    // SDL2's event.button.button is mapped like this:
                    // 1 -> left
                    // 2 -> middle
                    // 3 -> right
                    // 4 -> back
                    // 5 -> forward
                    // However, Pinc maps the bits like this:
                    // 0 -> left    (1s place)
                    // 1 -> right   (2s place)
                    // 2 -> middle  (4s place)
                    // 3 -> back    (8s place)
                    // 4 -> forward (16s place)
                    uint32_t buttonBit;
                    switch (event.button.button)
                    {
                        case 1: buttonBit = 0; break;
                        case 2: buttonBit = 2; break;
                        case 3: buttonBit = 1; break;
                        case 4: buttonBit = 3; break;
                        case 5: buttonBit = 4; break;
                        // There are certainly more buttons, but for now I don't think they really exist.
                        default: PErrorAssert(false, "Invalid button index!");
                    }
                    uint32_t buttonBitMask = 1<<buttonBit;
                    // event.button.state is 1 when pressed, 0 when released. SDL2 is ABI stable, so this is safe.
                    PErrorAssert(event.button.state < 2, "It appears SDL2's ABI has changed. The universe as we know it is broken!");
                    // Cast here because for some reason the compiler thinks one of these is signed
                    uint32_t newState = (this->mouseState & ~buttonBitMask) | (((uint32_t)event.button.state)<<buttonBit);
                    PincEventMouseButton(((int64_t)event.button.timestamp) + timeOffset, this->mouseState, newState);
                    this->mouseState = newState;
                }
            }
            default:{
                // TODO: Once all SDL events are handled, assert.
                break;
            }
        }
    }
}

WindowHandle sdl2completeWindow(struct WindowBackend* obj, IncompleteWindow const * incomplete, pinc_window frontHandle) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
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
    // TODO: Only graphics api is OpenGL, shortcuts are taken
    windowFlags |= SDL_WINDOW_OPENGL;

    if(!this->dummyWindowInUse && this->dummyWindow) {
        Sdl2Window* dummyWindow = this->dummyWindow;
        uint32_t realFlags = this->libsdl2.getWindowFlags(dummyWindow->sdlWindow);

        // If we need opengl but the dummy window doesn't have it,
        // Then, as long as Pinc doesn't start supporting a different graphics api for each window,
        // the dummy window is fully useless and it should be replaced.
        // SDL does not support changing a window to have opengl after it was created without it.
        if((windowFlags&SDL_WINDOW_OPENGL) && !(realFlags&SDL_WINDOW_OPENGL)) {
            this->libsdl2.destroyWindow(dummyWindow->sdlWindow);
            PString_free(&this->dummyWindow->title, rootAllocator);
            Allocator_free(rootAllocator, dummyWindow, sizeof(Sdl2Window));
            goto SDL_MAKE_NEW_WINDOW;
        }

        // All of the other flags can be changed
        if((windowFlags&SDL_WINDOW_RESIZABLE) != (realFlags&SDL_WINDOW_RESIZABLE)) {
            sdl2setWindowResizable(obj, dummyWindow, (windowFlags&SDL_WINDOW_RESIZABLE) != 0);
        }
        if((windowFlags&SDL_WINDOW_MINIMIZED) != (realFlags&SDL_WINDOW_MINIMIZED)) {
            sdl2setWindowMinimized(obj, dummyWindow, (windowFlags&SDL_WINDOW_MINIMIZED) != 0);
        }
        if((windowFlags&SDL_WINDOW_MAXIMIZED) != (realFlags&SDL_WINDOW_MAXIMIZED)) {
            sdl2setWindowMaximized(obj, dummyWindow, (windowFlags&SDL_WINDOW_MAXIMIZED) != 0);
        }
        if((windowFlags&SDL_WINDOW_FULLSCREEN) != (realFlags&SDL_WINDOW_FULLSCREEN)) {
            sdl2setWindowFullscreen(obj, dummyWindow, (windowFlags&SDL_WINDOW_FULLSCREEN) != 0);
        }
        if((windowFlags&SDL_WINDOW_INPUT_FOCUS) != (realFlags&SDL_WINDOW_INPUT_FOCUS)) {
            sdl2setWindowFocused(obj, dummyWindow, (windowFlags&SDL_WINDOW_INPUT_FOCUS) != 0);
        }
        if((windowFlags&SDL_WINDOW_HIDDEN) != (realFlags&SDL_WINDOW_HIDDEN)) {
            sdl2setWindowHidden(obj, dummyWindow, (windowFlags&SDL_WINDOW_HIDDEN) != 0);
        }

        this->dummyWindowInUse = true;
        if(incomplete->hasWidth) {
            sdl2setWindowWidth(obj, this->dummyWindow, incomplete->width);
        }
        if(incomplete->hasHeight) {
            sdl2setWindowHeight(obj, this->dummyWindow, incomplete->height);
        }
        // Title's ownership is in the window object itself
        dummyWindow->title = incomplete->title;
        char* titleNullTerm = PString_marshalAlloc(incomplete->title, tempAllocator);
        this->libsdl2.setWindowTitle(dummyWindow->sdlWindow, titleNullTerm);
        Allocator_free(tempAllocator, titleNullTerm, incomplete->title.len+1);
        dummyWindow->frontHandle = frontHandle;
        return dummyWindow;
    }
    SDL_MAKE_NEW_WINDOW:
    // Seriously, why did it take until C23 for us lowly C programmers to declare variables after a label
    // Now I have to put this in a scope
    {
        // SDL expects a null-terminated title, while our actual title is not null terminated
        // (How come nobody ever makes options for those using non null-terminated strings?)
        // Reminder: SDL2 uses UTF8 encoding for pretty much all strings
        char* titleNullTerm = PString_marshalAlloc(incomplete->title, tempAllocator);
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

        // If the dummy window is not set, make this the dummy window
        if(!this->dummyWindow) {
            this->dummyWindow = windowObj;
            this->dummyWindowInUse = true;
        }
        windowObj->frontHandle = frontHandle;
        return (WindowHandle)windowObj;
    }
}

void sdl2deinitWindow(struct WindowBackend* obj, WindowHandle windowHandle) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    // TODO: add validation that the dummy window in use variable is synchronized with the list of windows
    // if the dummy window is in use, it should be in the list. If not, it should not be in the list.
    sdl2RemoveWindow(this, window);
    if(window == this->dummyWindow) {
        // Don't want to accidentally delete the dummy window
        this->dummyWindowInUse = false;
        return;
    }
    this->libsdl2.destroyWindow(window->sdlWindow);
    PString_free(&window->title, rootAllocator);
    Allocator_free(rootAllocator, window, sizeof(Sdl2Window));
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
    P_UNUSED(obj);
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    *outTitleLen = window->title.len;
    return window->title.str;
}

void sdl2setWindowWidth(struct WindowBackend* obj, WindowHandle window, uint32_t width) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(width);
    // TODO
}

uint32_t sdl2getWindowWidth(struct WindowBackend* obj, WindowHandle window) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* windowObj = (Sdl2Window*)window;
    // SDL has a history of annoying issues around the size of a window in actual pixels
    int width = 0;
    if(this->libsdl2.getWindowSizeInPixels) {
        this->libsdl2.getWindowSizeInPixels(windowObj->sdlWindow, &width, NULL);
    } else if(this->libsdl2.glGetDrawableSize) {
        // TODO: only graphics api is OpenGl, shortcuts are made
        this->libsdl2.glGetDrawableSize(windowObj->sdlWindow, &width, NULL);
    } else {
        // If the previously tried functions don't work, then it means this is a version of SDL from before it became hidpi aware.
        // There's not a lot we can do. If I'm not mistaken, non-dpi aware applications (in windows) are just fed incorrect window size values,
        // which can cause all kinds of issues.
        // Just assume the window size is equal to pixels. It's unlikely anyone is using an SDL2 version this old anyway.
        this->libsdl2.getWindowSize(windowObj->sdlWindow, &width, NULL);
    }
    PErrorSanitize(width <= UINT32_MAX && width > 0, "Integer overflow");
    return (uint32_t)width;
}

void sdl2setWindowHeight(struct WindowBackend* obj, WindowHandle window, uint32_t height) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(height);
    // TODO
}

uint32_t sdl2getWindowHeight(struct WindowBackend* obj, WindowHandle window) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* windowObj = (Sdl2Window*)window;
    // SDL has a history of annoying issues around the size of a window in actual pixels
    int height = 0;
    if(this->libsdl2.getWindowSizeInPixels) {
        this->libsdl2.getWindowSizeInPixels(windowObj->sdlWindow, NULL, &height);
    } else if(this->libsdl2.glGetDrawableSize) {
        // TODO: only graphics api is OpenGl, shortcuts are made
        this->libsdl2.glGetDrawableSize(windowObj->sdlWindow, NULL, &height);
    } else {
        // If the previously tried functions don't work, then it means this is a version of SDL from before it became hidpi aware.
        // There's not a lot we can do. If I'm not mistaken, non-dpi aware applications (in windows) are just fed incorrect window size values,
        // which can cause all kinds of issues.
        // Just assume the window size is equal to pixels. It's unlikely anyone is using an SDL2 version this old anyway.
        this->libsdl2.getWindowSize(windowObj->sdlWindow, NULL, &height);
    }
    PErrorSanitize(height <= UINT32_MAX && height > 0, "Integer overflow");
    return (uint32_t)height;
}

float sdl2getWindowScaleFactor(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return 0;
}

void sdl2setWindowResizable(struct WindowBackend* obj, WindowHandle window, bool resizable) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(resizable);
    // TODO
}

bool sdl2getWindowResizable(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void sdl2setWindowMinimized(struct WindowBackend* obj, WindowHandle window, bool minimized) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(minimized);
    // TODO
}

bool sdl2getWindowMinimized(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void sdl2setWindowMaximized(struct WindowBackend* obj, WindowHandle window, bool maximized) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(maximized);
    // TODO
}

bool sdl2getWindowMaximized(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void sdl2setWindowFullscreen(struct WindowBackend* obj, WindowHandle window, bool fullscreen) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(fullscreen);
    // TODO
}

bool sdl2getWindowFullscreen(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void sdl2setWindowFocused(struct WindowBackend* obj, WindowHandle window, bool focused) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(focused);
    // TODO
}

bool sdl2getWindowFocused(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void sdl2setWindowHidden(struct WindowBackend* obj, WindowHandle window, bool hidden) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(hidden);
    // TODO
}

bool sdl2getWindowHidden(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void sdl2setVsync(struct WindowBackend* obj, bool vsync) {
    P_UNUSED(obj);
    P_UNUSED(vsync);
    // TODO
}

bool sdl2getVsync(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // TODO
    return false;
}

void sdl2windowPresentFramebuffer(struct WindowBackend* obj, WindowHandle window) {
    Sdl2Window* windowObj = (Sdl2Window*)window;
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    this->libsdl2.glSwapWindow(windowObj->sdlWindow);
}

pinc_opengl_support_status sdl2queryGlVersionSupported(struct WindowBackend* obj, uint32_t major, uint32_t minor, bool es) {
    P_UNUSED(obj);
    P_UNUSED(major);
    P_UNUSED(minor);
    P_UNUSED(es);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    // TODO: document in pinc.h that when this function returns 'maybe',
    // that one should fall-back to using the tried-and-true method of trying to make a context with said version
    // to see if it works. Also document that this function does not attempt to create a context to query the version.
    return pinc_opengl_support_status_maybe;
}

pinc_opengl_support_status sdl2queryGlAccumulatorBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t channel, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(channel);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlAlphaBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlDepthBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlStereoBuffer(struct WindowBackend* obj, FramebufferFormat framebuffer) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlContextDebug(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlForwardCompatible(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlRobustAccess(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

pinc_opengl_support_status sdl2queryGlResetIsolation(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // TODO
    PPANIC("Not implemented");
    return 0;
}

RawOpenglContextHandle sdl2glCompleteContext(struct WindowBackend* obj, IncompleteGlContext incompleteContext) {
    // TODO: Actually use the context information
    P_UNUSED(incompleteContext);
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* dummyWindow = _dummyWindow(obj);
    SDL_GLContext sdlGlContext = this->libsdl2.glCreateContext(dummyWindow->sdlWindow);
    if(!sdlGlContext) {
        // TODO: don't do this when external errors are disable - PErrorExternal will do nothing, but this string is still created
        PString errorMsg = PString_concat(2, (PString[]){
            PString_makeDirect((char*)"SDL2 backend: Could not create OpenGl context: "),
            // const is not an issue, this string will not be modified
            PString_makeDirect((char*)this->libsdl2.getError()),
        },tempAllocator);
        PErrorExternalStr(false, errorMsg);
        PString_free(&errorMsg, tempAllocator);
        return 0;
    }
    // Unlike a window, an OpenGl context contains no other information than just the opaque pointer
    // So no need to wrap it in a struct or anything
    return sdlGlContext;
}

pinc_return_code sdl2glMakeCurrent(struct WindowBackend* obj, WindowHandle window, RawOpenglContextHandle context) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* windowObj = (Sdl2Window*)window;
    // Unlike a window, an OpenGl context contains no other information than just the opaque pointer
    // So no need to wrap it in a struct or anything
    SDL_GLContext contextObj = (SDL_GLContext)context;
    int result = this->libsdl2.glMakeCurrent(windowObj->sdlWindow, contextObj);
    // TODO: Only do this when external errors are enabled - when disabled, the string will still be created, just never actually used
    if(result != 0) {
        PString errorMsg = PString_concat(2, (PString[]){
            PString_makeDirect((char*)"SDL2 backend: Could not make context current: "),
            PString_makeDirect((char*)this->libsdl2.getError()),
        },tempAllocator);
        PErrorExternalStr(false, errorMsg);
        PString_free(&errorMsg, tempAllocator);
        return pinc_return_code_error;
    }
    return pinc_return_code_pass;
}

pinc_window sdl2glGetCurrentWindow(struct WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    SDL_Window* sdlWin = this->libsdl2.glGetCurrentWindow();
    if(!sdlWin) {
        return 0;
    }
    Sdl2Window* thisWin = this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
    if(!thisWin) {
        return 0;
    }
    return thisWin->frontHandle;
}

pinc_opengl_context sdl2glGetCurrentContext(struct WindowBackend* obj) {
    // TODO: I'm not too confident about this, it should be tested properly
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    SDL_GLContext sdlContext = this->libsdl2.glGetCurrentContext();
    if(!sdlContext) {
        return 0;
    }
    // We need to turn this into a pinc opengl context object
    // Unlike with windows, SDL2 does not have user data on opengl contexts (much sad)
    for(size_t i=0; i<staticState.rawOpenglContextHandleObjects.objectsNum; ++i) {
        // Unlike a window, an OpenGl context contains no other information than just the opaque pointer
        // So no need to wrap it in a struct or anything
        RawOpenglContextObject iobject = ((RawOpenglContextObject*)staticState.rawOpenglContextHandleObjects.objectsArray)[i];
        if(sdlContext == iobject.handle) {
            return iobject.front_handle;
        }
    }
    // Assume no context is current
    return 0;
}

PINC_PFN sdl2glGetProc(struct WindowBackend* obj, char const* procname) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // make sure the context is current.
    // If there is no current context, that is a user error that should be reported.
    // TODO: is it a good idea to print out what SDL_GetError() returns as well?
    PErrorUser(this->libsdl2.glGetCurrentContext(), "Cannot get proc address of an OpenGL function without a current context");
    return this->libsdl2.glGetProcAddress(procname);
}

#endif
