#include <SDL2/SDL_video.h>
#include "SDL2/SDL_stdinc.h"
#include "libs/dynamic_allocator.h"
#include "libs/pstring.h"
#include "pinc.h"
#include "pinc_error.h"
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
    PincWindowHandle frontHandle;
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
    // On my Linux mint system with libsdl2-dev installed, I get these:
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
    *this = (Sdl2WindowBackend){0};
    // The only thing required for SDL2 support is for the SDL2 library to be present
    void* lib = sdl2LoadLib();
    if(!lib) {
        char* msg = "SDL2 could not be loaded, disabling SDL2 backend.\n";
        pPrintDebug((uint8_t*)msg, pStringLen(msg));
        // sdl2UnloadLib(lib);
        return false;
    }
    this->sdl2Lib = lib;

    loadSdl2Functions(this->sdl2Lib, &this->libsdl2);
    SDL_version sdlVersion;
    this->libsdl2.getVersion(&sdlVersion);
    PString strings[] = {
        PString_makeDirect("Loaded SDL2 version: "),
        PString_allocFormatUint32(sdlVersion.major, tempAllocator),
        PString_makeDirect("."),
        PString_allocFormatUint32(sdlVersion.minor, tempAllocator),
        PString_makeDirect("."),
        PString_allocFormatUint32(sdlVersion.patch, tempAllocator),
    };
    PString msg = PString_concat(sizeof(strings) / sizeof(PString), strings, tempAllocator);
    pPrintDebugLine(msg.str, msg.len);
    if(sdlVersion.major < 2) {
        char* msg2 = "SDL version too old, disabling SDL2 backend\n";
        pPrintDebug((uint8_t*)msg2, pStringLen(msg2));
        sdl2UnloadLib(lib);
        this->sdl2Lib = 0;
        this->libsdl2 = (Sdl2Functions){0};
        return false;
    }
    this->libsdl2.init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    // Load all of the functions into the vtable
    #define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames) obj->vt.name = sdl2##name;
    #define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) obj->vt.name = sdl2##name;

    PINC_WINDOW_INTERFACE

    #undef PINC_WINDOW_INTERFACE_FUNCTION
    #undef PINC_WINDOW_INTERFACE_PROCEDURE
    return true;
}

// Clang, GCC, and MSVC optimize this into the most efficient form, when optimization flags are passed.
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
    PErrorAssert(this->dummyWindow, "SDL2 Backend: Could not create dummy window");
    return this->dummyWindow;
}

// Quick function for convenience
static void _framebufferFormatAdd(FramebufferFormat** formats, size_t* formatsNum, size_t* formatsCapacity, FramebufferFormat const* fmt) {
    for(size_t formatid=0; formatid<(*formatsNum); formatid++) {
        FramebufferFormat ft = (*formats)[formatid];
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
    // I somehow managed to lose several hours on forgetting to surround "*formats" with parentheses.
    // In all fairness, it only started showing causing problems when compiling for windows via GCC and running it through WINE.
    // Isn't C just the most absolutely amazing programming language ever?
    (*formats)[*formatsNum] = *fmt;
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
    if(numDisplays < 0) {
        PString strings[] = {
            PString_makeDirect("Pinc encountered fatal SDL2 error: "),
            PString_makeDirect((char*)this->libsdl2.getError()),
        };
        PString err = PString_concat(sizeof(strings) / sizeof(PString), strings, tempAllocator);
        PErrorExternalStr(numDisplays > 0, err);
        *outNumFormats = 0;
        return NULL;
    }
    for(int displayIndex=0; displayIndex<numDisplays; ++displayIndex) {
        int numDisplayModes = this->libsdl2.getNumDisplayModes(displayIndex);
        if(numDisplayModes < 0) {
            PString strings[] = {
                PString_makeDirect("Pinc encountered fatal SDL2 error: "),
                PString_makeDirect((char*)this->libsdl2.getError()),
            };
            PString err = PString_concat(sizeof(strings) / sizeof(PString), strings, tempAllocator);
            PErrorExternalStr(numDisplays > 0, err);
            *outNumFormats = 0;
            return NULL;
        }
        for(int displayModeIndex=0; displayModeIndex<numDisplayModes; ++displayModeIndex) {
            SDL_DisplayMode displayMode;
            this->libsdl2.getDisplayMode(displayIndex, displayModeIndex, &displayMode);
            // Strangeness is going on and I don't like it!
            if(!displayMode.format || !displayMode.w || !displayMode.h) {
                PString strings[] = {
                    PString_makeDirect("Pinc encountered non-fatal SDL2 error: Invalid display mode "),
                    PString_allocFormatUint64((uint64_t)displayModeIndex, tempAllocator),
                    PString_makeDirect(" For display "),
                    PString_allocFormatUint64((uint64_t)displayIndex, tempAllocator),
                };
                PString err = PString_concat(sizeof(strings) / sizeof(PString), strings, tempAllocator);
                pPrintErrorLine(err.str, err.len);
                PString_free(&err, tempAllocator);
                continue;
            }

            int bpp = 0;
            uint32_t rmask = 0;
            uint32_t gmask = 0;
            uint32_t bmask = 0;
            uint32_t amask = 0;
            if(this->libsdl2.pixelFormatEnumToMasks(displayMode.format, &bpp, &rmask, &gmask, &bmask, &amask) == SDL_FALSE){
                PString strings[] = {
                    PString_makeDirect("Pinc encountered non-fatal SDL2 error: "),
                    PString_makeDirect((char*)this->libsdl2.getError()),
                };
                PString err = PString_concat(sizeof(strings) / sizeof(PString), strings, tempAllocator);
                pPrintErrorLine(err.str, err.len);
                PString_free(&err, tempAllocator);
                continue;
            }

            // the pixel format is a bitfield, like so (in little endian order):
            // bytes*8, bits*8, layout*4, order*4, type*4, 1, 0*remaining
            // SDL2 has us covered though, with a nice function that decodes all of it
            FramebufferFormat bufferFormat;
            // TODO: properly figure out transparent window support
            // There is absolutely no reason for assuming sRGB, other than SDL doesn't let us get what the real color space is.
            // sRGB is a fairly safe bet, and even if it's wrong, 99% of the time if it's not Srgb it's another similar perceptual color space.
            // There is a rare chance that it's a linear color space, but given SDL2's supported platforms, I find that incredibly unlikely.
            bufferFormat.color_space = PincColorSpace_srgb;
            // TODO: is this right? I think so
            bufferFormat.channel_bits[0] = bitCount32(rmask);
            bufferFormat.channel_bits[1] = bitCount32(gmask);
            bufferFormat.channel_bits[2] = bitCount32(bmask);
            if(amask == 0) {
                // RGB
                bufferFormat.channels = 3;
                bufferFormat.channel_bits[3] = 0;
            } else {
                // RGBA?
                bufferFormat.channels = 4;
                bufferFormat.channel_bits[3] = bitCount32(amask);
            }
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

bool sdl2queryGraphicsApiSupport(struct WindowBackend* obj, PincGraphicsApi api) {
    P_UNUSED(obj);
    switch (api) {
        case PincGraphicsApi_opengl:
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

PincReturnCode sdl2completeInit(struct WindowBackend* obj, PincGraphicsApi graphicsBackend, FramebufferFormat framebuffer) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    // Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    switch (graphicsBackend)
    {
        case PincGraphicsApi_opengl:
            break;
        
        default:
            // We don't support this graphics api.
            // Technically this code should never run, because the user API frontend should have caught this
            PErrorUser(false, "Attempt to use SDL2 backend with an unsupported graphics api");
            return PincReturnCode_error;
    }
    return PincReturnCode_pass;
}

void sdl2deinit(struct WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // Make sure the frontend deleted all of the windows already
    PErrorAssert(this->windowsNum == 0, "Internal pinc error: the frontend didn't delete the windows before calling backend deinit");
    
    this->libsdl2.destroyWindow(this->dummyWindow->sdlWindow);
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
    int64_t timeOffset = pCurrentTimeMillis() - ((int64_t)this->libsdl2.getTicks64());

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

WindowHandle sdl2completeWindow(struct WindowBackend* obj, IncompleteWindow const * incomplete, PincWindowHandle frontHandle) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    this->libsdl2.resetHints();

    uint32_t realWidth = incomplete->width;
    uint32_t realHeight = incomplete->height;
    if(!incomplete->hasWidth) {
        realWidth = 256;
    }
    if(!incomplete->hasHeight) {
        realHeight = 256;
    }
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
    // Only graphics api is OpenGL, shortcuts are taken
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
        sdl2setWindowWidth(obj, this->dummyWindow, realWidth);
        sdl2setWindowHeight(obj, this->dummyWindow, realHeight);

        char* titleNullTerm = PString_marshalAlloc(incomplete->title, tempAllocator);
        this->libsdl2.setWindowTitle(dummyWindow->sdlWindow, titleNullTerm);
        Allocator_free(tempAllocator, titleNullTerm, incomplete->title.len+1);
        dummyWindow->frontHandle = frontHandle;
        // They gave us ownership
        // Sooner or later I'm going to change that
        PString_free((PString*)&incomplete->title, rootAllocator);
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
        SDL_Window* win = this->libsdl2.createWindow(titleNullTerm, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)realWidth, (int)realHeight, windowFlags);
        // I'm so paranoid, I actually went through the SDL2 source code to make sure it actually duplicates the window title to avoid a use-after-free
        // Better too worried than not enough I guess
        Allocator_free(tempAllocator, titleNullTerm, incomplete->title.len+1);

        Sdl2Window* windowObj = Allocator_allocate(rootAllocator, sizeof(Sdl2Window));
        *windowObj = (Sdl2Window){
            .sdlWindow = win,
            .frontHandle = frontHandle,
            .width = realWidth,
            .height = realHeight,
        };
        
        // They gave us ownership
        // Sooner or later I'm going to change that
        PString_free((PString*)&incomplete->title, rootAllocator);

        // So we can easily get one of our windows out of the SDL2 window handle
        this->libsdl2.setWindowData(win, "pincSdl2Window", windowObj);

        // Add it to the list of windows
        sdl2AddWindow(this, windowObj);

        // If the dummy window is not set, make this the dummy window
        if(!this->dummyWindow) {
            this->dummyWindow = windowObj;
            this->dummyWindowInUse = true;
        }
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
    Allocator_free(rootAllocator, window, sizeof(Sdl2Window));
}

void sdl2setWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, uint8_t* title, size_t titleLen) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    
    // It needs to be null terminated because reasons
    char* titleNullTerm = PString_marshalAlloc((PString){.str = title, .len = titleLen}, tempAllocator);
    this->libsdl2.setWindowTitle(window->sdlWindow, titleNullTerm);
    Allocator_free(tempAllocator, titleNullTerm, titleLen+1);
    // We take ownership of the title
    Allocator_free(rootAllocator, title, titleLen);
}

uint8_t const * sdl2getWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, size_t* outTitleLen) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* window = (Sdl2Window*)windowHandle;
    char const* title = this->libsdl2.getWindowTitle(window->sdlWindow);
    *outTitleLen = pStringLen(title);
    return (uint8_t const*)title;
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
        // Only graphics api is OpenGl, shortcuts are made
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

PincReturnCode sdl2setVsync(struct WindowBackend* obj, bool vsync) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // TODO: only graphics backend is OpenGL, shortcuts are taken
    if(vsync) {
        if(this->libsdl2.glSetSwapInterval(-1) == -1) {
            // Try again with non-adaptive vsync
            if(this->libsdl2.glSetSwapInterval(1) == -1) {
                // big sad
                return PincReturnCode_error;
            }
        }
    } else {
        if(this->libsdl2.glSetSwapInterval(0) == -1) {
            return PincReturnCode_error;
        }
    }
    return PincReturnCode_pass;
}

bool sdl2getVsync(struct WindowBackend* obj) {
    // TODO: only graphics backend is OpenGL, shortcuts are taken
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    return this->libsdl2.glGetSwapInterval() != 0;
}

void sdl2windowPresentFramebuffer(struct WindowBackend* obj, WindowHandle window) {
    Sdl2Window* windowObj = (Sdl2Window*)window;
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    // TODO: only graphics backend is OpenGL, shortcuts are taken
    this->libsdl2.glSwapWindow(windowObj->sdlWindow);
}

PincOpenglSupportStatus sdl2queryGlVersionSupported(struct WindowBackend* obj, uint32_t major, uint32_t minor, PincOpenglContextProfile profile) {
    P_UNUSED(obj);
    P_UNUSED(major);
    P_UNUSED(minor);
    P_UNUSED(profile);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlAccumulatorBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t channel, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(channel);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlAlphaBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlDepthBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlStencilBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlSamples(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t samples) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(samples);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlStereoBuffer(struct WindowBackend* obj, FramebufferFormat framebuffer) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlContextDebug(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlRobustAccess(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus sdl2queryGlResetIsolation(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

RawOpenglContextHandle sdl2glCompleteContext(struct WindowBackend* obj, IncompleteGlContext incompleteContext) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    FramebufferFormat* fmt = PincObject_ref_framebufferFormat(staticState.framebufferFormat);
    // Due to reasons, we have to create a compatible RGB framebuffer format for the context
    // TODO: is this actually valid?
    uint32_t channel_bits[4] = { 0 };
    channel_bits[0] = fmt->channel_bits[0];
    switch(fmt->channels) {
        case 1:
        case 2:
            channel_bits[1] = fmt->channel_bits[0];
            channel_bits[2] = fmt->channel_bits[0];
            break;
        case 3:
        case 4:
            channel_bits[1] = fmt->channel_bits[1];
            channel_bits[2] = fmt->channel_bits[2];
            break;
        default:
            PPANIC("Invalid number of channels in framebuffer format");
    }
    channel_bits[3] = incompleteContext.alphaBits;
    this->libsdl2.glSetAttribute(SDL_GL_RED_SIZE, (int)channel_bits[0]);
    this->libsdl2.glSetAttribute(SDL_GL_GREEN_SIZE, (int)channel_bits[1]);
    this->libsdl2.glSetAttribute(SDL_GL_BLUE_SIZE, (int)channel_bits[2]);
    this->libsdl2.glSetAttribute(SDL_GL_ALPHA_SIZE, (int)channel_bits[3]);
    this->libsdl2.glSetAttribute(SDL_GL_DEPTH_SIZE, (int)incompleteContext.depthBits);
    this->libsdl2.glSetAttribute(SDL_GL_STENCIL_SIZE, (int)incompleteContext.stencilBits);
    this->libsdl2.glSetAttribute(SDL_GL_ACCUM_RED_SIZE, (int)incompleteContext.accumulatorBits[0]);
    this->libsdl2.glSetAttribute(SDL_GL_ACCUM_GREEN_SIZE, (int)incompleteContext.accumulatorBits[1]);
    this->libsdl2.glSetAttribute(SDL_GL_ACCUM_BLUE_SIZE, (int)incompleteContext.accumulatorBits[2]);
    this->libsdl2.glSetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, (int)incompleteContext.accumulatorBits[3]);
    int stereo = 0;
    if(incompleteContext.stereo) {
        stereo = 1;
    }
    this->libsdl2.glSetAttribute(SDL_GL_STEREO, stereo);
    // TODO: As far as I can tell, MULTISAMPLEBUFFERS is only 1 or 0. Nobody explains a use case with more than 1.
    // the ARB samples extension doesn't even mention the idea of more than one buffer, and has no way to set a number of them
    // But the ARB extension hasn't been modified in at least 15 years.
    if(incompleteContext.samples > 1) {
        this->libsdl2.glSetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    } else {
        this->libsdl2.glSetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    }
    this->libsdl2.glSetAttribute(SDL_GL_MULTISAMPLESAMPLES, (int)incompleteContext.samples);
    this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, (int)incompleteContext.versionMajor);
    this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (int)incompleteContext.versionMinor);
    int glFlags = 0;
    switch (incompleteContext.profile) {
        case PincOpenglContextProfile_legacy:
            PErrorUser(false, "SDL2 does not support creating a legacy context");
            return 0;
        case PincOpenglContextProfile_compatibility:
            this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            break;
        case PincOpenglContextProfile_core:
            this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            break;
        case PincOpenglContextProfile_forward:
            this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            glFlags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
            break;
    }
    if(incompleteContext.robustAccess) {
        glFlags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
    }
    if(incompleteContext.debug) {
        glFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
    }
    this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_FLAGS, glFlags);
    int share = 0;
    if(incompleteContext.shareWithCurrent) {
        share = 1;
    }
    this->libsdl2.glSetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, share);

    // TODO: SDL says it needs the attributes set before creating the window,
    // But is that actually true beyond just the framebuffer format?
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
    // This is to stop users from assuming the context will be current after completion, like what SDL2 does.
    this->libsdl2.glMakeCurrent(0, 0);
    // Unlike a window, an OpenGl context contains no other information than just the opaque pointer
    // So no need to wrap it in a struct or anything
    return sdlGlContext;
}

void sdl2glDeinitContext(struct WindowBackend* obj, RawOpenglContextObject context) {
    P_UNUSED(obj);
    P_UNUSED(context);
    // TODO
}

uint32_t sdl2glGetContextAccumulatorBits(struct WindowBackend* obj, RawOpenglContextObject context, uint32_t channel){
    P_UNUSED(obj);
    P_UNUSED(context);
    P_UNUSED(channel);
    // TODO
    return 0;
}

uint32_t sdl2glGetContextAlphaBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t sdl2glGetContextDepthBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t sdl2glGetContextStencilBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t sdl2glGetContextSamples(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

bool sdl2glGetContextStereoBuffer(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool sdl2glGetContextDebug(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool sdl2glGetContextRobustAccess(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool sdl2glGetContextResetIsolation(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}


PincReturnCode sdl2glMakeCurrent(struct WindowBackend* obj, WindowHandle window, RawOpenglContextHandle context) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Sdl2Window* windowObj = (Sdl2Window*)window;
    // Window may be null to indicate any window.
    // SDL2 does not state it needs a window, but it also does not state that no window is an option
    // So, in the event of a null window, get the dummy window
    if(!windowObj) {
        windowObj = _dummyWindow(obj);
    }
    // Unlike a window, an OpenGl context contains no other information than just the opaque pointer
    // So no need to wrap it in a struct or anything
    // NOTE: context may be null to indicate no context should be current
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
        return PincReturnCode_error;
    }
    return PincReturnCode_pass;
}

PincWindowHandle sdl2glGetCurrentWindow(struct WindowBackend* obj) {
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

PincOpenglContextHandle sdl2glGetCurrentContext(struct WindowBackend* obj) {
    // TODO: I'm not too confident about this, it should be tested properly
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    SDL_GLContext sdlContext = this->libsdl2.glGetCurrentContext();
    if(!sdlContext) {
        return 0;
    }
    // We need to turn this into a pinc opengl context object
    // Unlike with windows, SDL2 does not have user data on opengl contexts (much sad)
    // Linear search is probably the fastest solution actually, most programs will have at most 1 or 2 opengl contexts
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

PincPfn sdl2glGetProc(struct WindowBackend* obj, char const* procname) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    PErrorUser(this->libsdl2.glGetCurrentContext(), "Cannot get proc address of an OpenGL function without a current context");
    return this->libsdl2.glGetProcAddress(procname);
}

#endif
