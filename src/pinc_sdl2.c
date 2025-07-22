// To make the build system as simple as possible, backend source files must remove themselves rather than rely on the build system
#include "SDL2/SDL_events.h"
#include "SDL2/SDL_mouse.h"
#include "pinc_options.h"
#if PINC_HAVE_WINDOW_SDL2

#include <SDL2/SDL_video.h>
#include "SDL2/SDL_stdinc.h"
#include "libs/pinc_allocator.h"
#include "libs/pinc_string.h"
#include "pinc.h"
#include "pinc_error.h"
#include "pinc_opengl.h"
#include "pinc_types.h"

#include "platform/pinc_platform.h"
#include "pinc_main.h"
#include "pinc_window.h"
#include "pinc_sdl2load.h"

typedef struct {
    SDL_Window* sdlWindow;
    PincWindowHandle frontHandle;
    uint32_t width;
    uint32_t height;
} PincSdl2Window;

typedef struct {
    Sdl2Functions libsdl2;
    void* sdl2Lib;
    // May be null.
    PincSdl2Window* dummyWindow;
    // Whether the dummy window is also in use as a user-facing window object
    bool dummyWindowInUse;
    // List of windows.
    // TODO: instead of a list of pointers, just use a list of the struct itself
    // All external references to a window would need to change to be an ID / index into the list, instead of an arbitrary pointer.
    PincSdl2Window** windows;
    size_t windowsNum;
    size_t windowsCapacity;
    uint32_t mouseState;
} PincSdl2WindowBackend;

// Adds a window to the list of windows
void pincSdl2AddWindow(PincSdl2WindowBackend* this, PincSdl2Window* window) {
    if(!this->windows) {
        this->windows = pincAllocator_allocate(rootAllocator, sizeof(PincSdl2Window*) * 8);
        this->windowsNum = 0;
        this->windowsCapacity = 8;
    }
    if(this->windowsCapacity == this->windowsNum) {
        this->windows = pincAllocator_reallocate(rootAllocator, this->windows, sizeof(PincSdl2Window*) * this->windowsCapacity, sizeof(PincSdl2Window*) * this->windowsCapacity * 2);
        this->windowsCapacity = this->windowsCapacity * 2;
    }
    this->windows[this->windowsNum] = window;
    this->windowsNum++;
}

// This IS NOT responsible for actually destroying the window.
// It only removes a window from the list of windows.
void pincSdl2RemoveWindow(PincSdl2WindowBackend* this, PincSdl2Window* window) {
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

static void* pincSdl2LoadLib(void) {
    // On my Linux mint system with libsdl2-dev installed, I get these:
    // - libSDL2-2.0.so
    // - libSDL2-2.0.so.0
    // - libSDL2-2.0.so.0.3000.0
    // - libSDL2.so - this one only seems to be present with the dev package installed
    void* lib = pincLoadLibrary((uint8_t*)"SDL2-2.0", 8);
    if(!lib) {
        lib = pincLoadLibrary((uint8_t*)"SDL2", 4);
    }
    return lib;
}

static void pincSdl2UnloadLib(void* lib) {
    if(lib) {
        pincUnloadLibrary(lib);
    }
}

// declare the sdl2 window functions

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames) type pincSdl2##name arguments;
#define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) void pincSdl2##name arguments;

PINC_WINDOW_INTERFACE

#undef PINC_WINDOW_INTERFACE_FUNCTION
#undef PINC_WINDOW_INTERFACE_PROCEDURE

bool pincSdl2Init(WindowBackend* obj) {
    obj->obj = pincAllocator_allocate(rootAllocator, sizeof(PincSdl2WindowBackend));
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    *this = (PincSdl2WindowBackend){0};
    // The only thing required for SDL2 support is for the SDL2 library to be present
    void* lib = pincSdl2LoadLib();
    if(!lib) {
        char* msg = "SDL2 could not be loaded, disabling SDL2 backend.\n";
        pincPrintDebug((uint8_t*)msg, pincStringLen(msg));
        // pincSdl2UnloadLib(lib);
        return false;
    }
    this->sdl2Lib = lib;

    pincLoadSdl2Functions(this->sdl2Lib, &this->libsdl2);
    SDL_version sdlVersion;
    this->libsdl2.getVersion(&sdlVersion);
    pincString strings[] = {
        pincString_makeDirect("Loaded SDL2 version: "),
        pincString_allocFormatUint32(sdlVersion.major, tempAllocator),
        pincString_makeDirect("."),
        pincString_allocFormatUint32(sdlVersion.minor, tempAllocator),
        pincString_makeDirect("."),
        pincString_allocFormatUint32(sdlVersion.patch, tempAllocator),
    };
    pincString msg = pincString_concat(sizeof(strings) / sizeof(pincString), strings, tempAllocator);
    pincPrintDebugLine(msg.str, msg.len);
    if(sdlVersion.major < 2) {
        char* msg2 = "SDL version too old, disabling SDL2 backend\n";
        pincPrintDebug((uint8_t*)msg2, pincStringLen(msg2));
        pincSdl2UnloadLib(lib);
        this->sdl2Lib = 0;
        this->libsdl2 = (Sdl2Functions){0};
        return false;
    }
    this->libsdl2.init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    // Load all of the functions into the vtable
    #define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames) obj->vt.name = pincSdl2##name;
    #define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) obj->vt.name = pincSdl2##name;

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

static PincSdl2Window* _dummyWindow(struct WindowBackend* obj) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    if(this->dummyWindow) {
        return this->dummyWindow;
    }
    char const* const title = "Pinc Dummy Window";
    size_t const titleLen = pincStringLen(title);
    uint8_t* titlePtr = pincAllocator_allocate(rootAllocator, titleLen);
    pincMemCopy(title, titlePtr, titleLen);
    IncompleteWindow windowSettings = {
        // Ownership is transferred to the window
        .title = (pincString) {.str = titlePtr, .len = titleLen},
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
    this->dummyWindow = pincSdl2completeWindow(obj, &windowSettings, 0);
    // pincSdl2completeWindow sets this to true, under the assumption the user called it.
    // We are requesting the dummy window not for the user's direct use, so it's NOT in use.
    this->dummyWindowInUse = false;
    PErrorAssert(this->dummyWindow, "SDL2 Backend: Could not create dummy window");
    return this->dummyWindow;
}

// Quick function for convenience
static void _pincFramebufferFormatAdd(FramebufferFormat** formats, size_t* formatsNum, size_t* formatsCapacity, FramebufferFormat const* fmt) {
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
        *formats = pincAllocator_reallocate(tempAllocator, *formats, sizeof(FramebufferFormat) * (*formatsCapacity), sizeof(FramebufferFormat) * newFormatsCapacity);
        *formatsCapacity = newFormatsCapacity;
    }
    // I somehow managed to lose several hours on forgetting to surround "*formats" with parentheses.
    // In all fairness, it only started showing causing problems when compiling for windows via GCC and running it through WINE.
    // Isn't C just the most absolutely amazing programming language ever?
    (*formats)[*formatsNum] = *fmt;
    (*formatsNum)++;
}

// Implementation of the interface

FramebufferFormat* pincSdl2queryFramebufferFormats(struct WindowBackend* obj, pincAllocator allocator, size_t* outNumFormats) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    // SDL2 has no nice way for us to get a list of possible framebuffer formats
    // However, SDL2 does have functionality to iterate through displays and go through what formats they support.
    // Ideally, SDL2 would report the list of Visuals from X or the list of pixel formats from win32, but this is the next best option

    // Dynamic list to hold the framebuffer formats
    // TODO: A proper set data structure for deduplication is a good idea
    FramebufferFormat* formats = pincAllocator_allocate(tempAllocator, sizeof(FramebufferFormat)*8);
    size_t formatsNum = 0;
    size_t formatsCapacity = 8;

    int numDisplays = this->libsdl2.getNumVideoDisplays();
    if(numDisplays < 0) {
        pincString strings[] = {
            pincString_makeDirect("Pinc encountered fatal SDL2 error: "),
            pincString_makeDirect((char*)this->libsdl2.getError()),
        };
        pincString err = pincString_concat(sizeof(strings) / sizeof(pincString), strings, tempAllocator);
        PErrorExternalStr(numDisplays > 0, err);
        *outNumFormats = 0;
        return NULL;
    }
    for(int displayIndex=0; displayIndex<numDisplays; ++displayIndex) {
        int numDisplayModes = this->libsdl2.getNumDisplayModes(displayIndex);
        if(numDisplayModes < 0) {
            pincString strings[] = {
                pincString_makeDirect("Pinc encountered fatal SDL2 error: "),
                pincString_makeDirect((char*)this->libsdl2.getError()),
            };
            pincString err = pincString_concat(sizeof(strings) / sizeof(pincString), strings, tempAllocator);
            PErrorExternalStr(numDisplays > 0, err);
            *outNumFormats = 0;
            return NULL;
        }
        for(int displayModeIndex=0; displayModeIndex<numDisplayModes; ++displayModeIndex) {
            SDL_DisplayMode displayMode;
            this->libsdl2.getDisplayMode(displayIndex, displayModeIndex, &displayMode);
            // Strangeness is going on and I don't like it!
            if(!displayMode.format || !displayMode.w || !displayMode.h) {
                pincString strings[] = {
                    pincString_makeDirect("Pinc encountered non-fatal SDL2 error: Invalid display mode "),
                    pincString_allocFormatUint64((uint64_t)displayModeIndex, tempAllocator),
                    pincString_makeDirect(" For display "),
                    pincString_allocFormatUint64((uint64_t)displayIndex, tempAllocator),
                };
                pincString err = pincString_concat(sizeof(strings) / sizeof(pincString), strings, tempAllocator);
                pincPrintErrorLine(err.str, err.len);
                pincString_free(&err, tempAllocator);
                continue;
            }

            int bpp = 0;
            uint32_t rmask = 0;
            uint32_t gmask = 0;
            uint32_t bmask = 0;
            uint32_t amask = 0;
            if(this->libsdl2.pixelFormatEnumToMasks(displayMode.format, &bpp, &rmask, &gmask, &bmask, &amask) == SDL_FALSE){
                pincString strings[] = {
                    pincString_makeDirect("Pinc encountered non-fatal SDL2 error: "),
                    pincString_makeDirect((char*)this->libsdl2.getError()),
                };
                pincString err = pincString_concat(sizeof(strings) / sizeof(pincString), strings, tempAllocator);
                pincPrintErrorLine(err.str, err.len);
                pincString_free(&err, tempAllocator);
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
            _pincFramebufferFormatAdd(&formats, &formatsNum, &formatsCapacity, &bufferFormat);
        }
    }
    // Allocate the final returned value
    FramebufferFormat* actualFormats = pincAllocator_allocate(allocator, formatsNum * sizeof(FramebufferFormat));
    for(size_t fmi = 0; fmi<formatsNum; fmi++) {
        actualFormats[fmi] = formats[fmi];
    }
    // Even though the temp allocator os a bump allocator, we may as well treat it as a real one.
    pincAllocator_free(tempAllocator, formats, formatsCapacity * sizeof(FramebufferFormat));
    *outNumFormats = formatsNum;
    return actualFormats;
}

bool pincSdl2queryGraphicsApiSupport(struct WindowBackend* obj, PincGraphicsApi api) {
    P_UNUSED(obj);
    switch (api) {
        case PincGraphicsApi_opengl:
            return true;
        default:
            return false;
    }
}

uint32_t pincSdl2queryMaxOpenWindows(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no (arbitrary) limit on how many windows can be open.
    return 0;
}

PincReturnCode pincSdl2completeInit(struct WindowBackend* obj, PincGraphicsApi graphicsBackend, FramebufferFormat framebuffer) {
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

void pincSdl2deinit(struct WindowBackend* obj) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    // Make sure the frontend deleted all of the windows already
    PErrorAssert(this->windowsNum == 0, "Internal pinc error: the frontend didn't delete the windows before calling backend deinit");
    
    this->libsdl2.destroyWindow(this->dummyWindow->sdlWindow);
    pincAllocator_free(rootAllocator, this->dummyWindow, sizeof(PincSdl2Window));

    this->libsdl2.quit();
    pincSdl2UnloadLib(this->sdl2Lib);
    pincAllocator_free(rootAllocator, this->windows, sizeof(PincSdl2Window*) * this->windowsCapacity);
    pincAllocator_free(rootAllocator, this, sizeof(PincSdl2WindowBackend));
}


void pincSdl2step(struct WindowBackend* obj) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;

    // The offset between SDL2's getTicks() and platform's pCurrentTimeMillis()
    // so that getTicks + timeOffset == pCurrentTimeMillis() (with some margin of error)
    int64_t timeOffset = pincCurrentTimeMillis() - ((int64_t)this->libsdl2.getTicks64());

    SDL_Event event;
    // NEXTLOOP:
    while(this->libsdl2.pollEvent(&event)) {
        switch (event.type) {
            case SDL_WINDOWEVENT: {
                SDL_Window* sdlWin = this->libsdl2.getWindowFromId(event.window.windowID);
                // External -> caused by SDL2 giving us events for nonexistent windows
                PErrorExternal(sdlWin, "SDL2 window from WindowEvent is NULL!");
                PincSdl2Window* windowObj = (PincSdl2Window*)this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
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
            case SDL_MOUSEBUTTONUP: 
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
                break;
            }
            case SDL_MOUSEMOTION: {
                SDL_Window* sdlWin = this->libsdl2.getWindowFromId(event.window.windowID);
                // External -> caused by SDL2 giving us events for nonexistent windows
                PErrorExternal(sdlWin, "SDL2 window from WindowEvent is NULL!");
                PincSdl2Window* windowObj = (PincSdl2Window*)this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
                // Assert -> caused by Pinc not setting the window event data (supposedly)
                PErrorAssert(windowObj, "Pinc SDL2 window object from WindowEvent is NULL!");
                // TODO: make sure the window that has the cursor is actually the window that SDL2 gave us
                int32_t x = event.motion.x;
                int32_t y = event.motion.y;
                int32_t oldX = event.motion.x - event.motion.xrel;
                int32_t oldY = event.motion.y - event.motion.yrel;
                // SDL (On X11 anyways) reports cursor coordinates beyond the window.
                // This happens when a mouse button is held down (so the window keeps cursor focus) and is moved outside the window.
                // From my understanding, this is technically valid but rather strange behavior.
                // If someone actually needs this to behave the exact same as SDL2, they can make an issue about it.
                if(x < 0) x = 0;
                if(y < 0) y = 0;
                if(oldX < 0) oldX = 0;
                if(oldY < 0) oldY = 0;
                if((uint32_t)x > windowObj->width) x = (int32_t)windowObj->width;
                if((uint32_t)y > windowObj->height) y = (int32_t)windowObj->height;
                if((uint32_t)oldX > windowObj->width) oldX = (int32_t)windowObj->width;
                if((uint32_t)oldY > windowObj->height) oldY = (int32_t)windowObj->height;
                
                // TODO: is it even worth handling the case where the window's width is greater than the maximum value of int32_t? Because it seems every OS / desktop environment / compositor breaks way before that anyways
                PincEventCursorMove((int64_t)event.button.timestamp + timeOffset, windowObj->frontHandle, (uint32_t)oldX, (uint32_t)oldY, (uint32_t)x, (uint32_t)y);
                break;
            }
            case SDL_MOUSEWHEEL: {
                float xMovement = (float)event.wheel.x;
                float yMovement = (float)event.wheel.y;
                if(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    // Seriously though, what in the heck is this for
                    yMovement *= -1;
                    xMovement *= -1;
                }
                // Gotta love backwards compatibility
                SDL_version sdlVersion;
                this->libsdl2.getVersion(&sdlVersion);
                if(sdlVersion.minor > 2 || (sdlVersion.minor == 2 && sdlVersion.patch >= 18)) {
                    xMovement += event.wheel.preciseX;
                    yMovement += event.wheel.preciseY;
                }
                PincEventScroll((int64_t)event.wheel.timestamp + timeOffset, yMovement, xMovement);
                break;
            }
            case SDL_CLIPBOARDUPDATE: {
                // There is some weirdness with this - in particular, duplicates tend to get sent often.
                // When VSCode is open, as much as selecting some text will cause it to spam this function with the current clipboard
                // Or more accurately, system events are rather chaotic so often things get doubled along the way
                // It's an absolute non-issue, but something worth noting here.
                if(this->libsdl2.hasClipboardText()) {
                    char* clipboardText = this->libsdl2.getClipboardText();
                    PErrorExternal(clipboardText, "SDL2 clipboard is NULL");
                    if(!clipboardText) break;
                    size_t clipboardTextLen = pincStringLen(clipboardText);
                    char* clipboardTextCopy = pincAllocator_allocate(tempAllocator, clipboardTextLen + 1);
                    pincMemCopy(clipboardText, clipboardTextCopy, clipboardTextLen);
                    clipboardTextCopy[clipboardTextLen] = 0;
                    PincEventClipboardChanged((int64_t)event.common.timestamp + timeOffset, PincMediaType_text, clipboardTextCopy, clipboardTextLen);
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

WindowHandle pincSdl2completeWindow(struct WindowBackend* obj, IncompleteWindow const * incomplete, PincWindowHandle frontHandle) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
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
        // TODO: do we want to use fullscreen or fullscreen_desktop?
        // Currently, we use "real" fullscreen,
        // But it may be a good option to "fake" fullscreen via fullscreen_desktop.
        // Really, we should add an option so the user gets to decide.
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
        PincSdl2Window* dummyWindow = this->dummyWindow;
        uint32_t realFlags = this->libsdl2.getWindowFlags(dummyWindow->sdlWindow);

        // If we need opengl but the dummy window doesn't have it,
        // Then, as long as Pinc doesn't start supporting a different graphics api for each window,
        // the dummy window is fully useless and it should be replaced.
        // SDL does not support changing a window to have opengl after it was created without it.
        if((windowFlags&SDL_WINDOW_OPENGL) && !(realFlags&SDL_WINDOW_OPENGL)) {
            this->libsdl2.destroyWindow(dummyWindow->sdlWindow);
            pincAllocator_free(rootAllocator, dummyWindow, sizeof(PincSdl2Window));
            goto SDL_MAKE_NEW_WINDOW;
        }

        // All of the other flags can be changed
        if((windowFlags&SDL_WINDOW_RESIZABLE) != (realFlags&SDL_WINDOW_RESIZABLE)) {
            pincSdl2setWindowResizable(obj, dummyWindow, (windowFlags&SDL_WINDOW_RESIZABLE) != 0);
        }
        if((windowFlags&SDL_WINDOW_MINIMIZED) != (realFlags&SDL_WINDOW_MINIMIZED)) {
            pincSdl2setWindowMinimized(obj, dummyWindow, (windowFlags&SDL_WINDOW_MINIMIZED) != 0);
        }
        if((windowFlags&SDL_WINDOW_MAXIMIZED) != (realFlags&SDL_WINDOW_MAXIMIZED)) {
            pincSdl2setWindowMaximized(obj, dummyWindow, (windowFlags&SDL_WINDOW_MAXIMIZED) != 0);
        }
        if((windowFlags&SDL_WINDOW_FULLSCREEN) != (realFlags&SDL_WINDOW_FULLSCREEN)) {
            pincSdl2setWindowFullscreen(obj, dummyWindow, (windowFlags&SDL_WINDOW_FULLSCREEN) != 0);
        }
        if((windowFlags&SDL_WINDOW_INPUT_FOCUS) != (realFlags&SDL_WINDOW_INPUT_FOCUS)) {
            pincSdl2setWindowFocused(obj, dummyWindow, (windowFlags&SDL_WINDOW_INPUT_FOCUS) != 0);
        }
        if((windowFlags&SDL_WINDOW_HIDDEN) != (realFlags&SDL_WINDOW_HIDDEN)) {
            pincSdl2setWindowHidden(obj, dummyWindow, (windowFlags&SDL_WINDOW_HIDDEN) != 0);
        }

        this->dummyWindowInUse = true;
        pincSdl2setWindowWidth(obj, this->dummyWindow, realWidth);
        pincSdl2setWindowHeight(obj, this->dummyWindow, realHeight);

        char* titleNullTerm = pincString_marshalAlloc(incomplete->title, tempAllocator);
        this->libsdl2.setWindowTitle(dummyWindow->sdlWindow, titleNullTerm);
        pincAllocator_free(tempAllocator, titleNullTerm, incomplete->title.len+1);
        dummyWindow->frontHandle = frontHandle;
        // They gave us ownership
        // Sooner or later I'm going to change that
        pincString_free((pincString*)&incomplete->title, rootAllocator);
        return dummyWindow;
    }
    SDL_MAKE_NEW_WINDOW:
    // Seriously, why did it take until C23 for us lowly C programmers to declare variables after a label
    // Now I have to put this in a scope
    {
        // SDL expects a null-terminated title, while our actual title is not null terminated
        // (How come nobody ever makes options for those using non null-terminated strings?)
        // Reminder: SDL2 uses UTF8 encoding for pretty much all strings
        char* titleNullTerm = pincString_marshalAlloc(incomplete->title, tempAllocator);
        SDL_Window* win = this->libsdl2.createWindow(titleNullTerm, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)realWidth, (int)realHeight, windowFlags);
        // I'm so paranoid, I actually went through the SDL2 source code to make sure it actually duplicates the window title to avoid a use-after-free
        // Better too worried than not enough I guess
        pincAllocator_free(tempAllocator, titleNullTerm, incomplete->title.len+1);

        PincSdl2Window* windowObj = pincAllocator_allocate(rootAllocator, sizeof(PincSdl2Window));
        *windowObj = (PincSdl2Window){
            .sdlWindow = win,
            .frontHandle = frontHandle,
            .width = realWidth,
            .height = realHeight,
        };
        
        // They gave us ownership
        // Sooner or later I'm going to change that
        pincString_free((pincString*)&incomplete->title, rootAllocator);

        // So we can easily get one of our windows out of the SDL2 window handle
        this->libsdl2.setWindowData(win, "pincSdl2Window", windowObj);

        // Add it to the list of windows
        pincSdl2AddWindow(this, windowObj);

        // If the dummy window is not set, make this the dummy window
        if(!this->dummyWindow) {
            this->dummyWindow = windowObj;
            this->dummyWindowInUse = true;
        }
        return (WindowHandle)windowObj;
    }
}

void pincSdl2deinitWindow(struct WindowBackend* obj, WindowHandle windowHandle) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* window = (PincSdl2Window*)windowHandle;
    #if PINC_ENABLE_ERROR_VALIDATE
    if(1) {
        bool dummyWindowActuallyInUse = false;
        for(size_t i=0; i<this->windowsNum; ++i) {
            PincSdl2Window* windowToCheck = this->windows[i];
            if(windowToCheck == this->dummyWindow) {
                dummyWindowActuallyInUse = true;
            }
        }
        PErrorValidate(dummyWindowActuallyInUse == this->dummyWindowInUse, "Dummy window in use does not match reality");
    }
    #endif
    pincSdl2RemoveWindow(this, window);
    if(window == this->dummyWindow) {
        // Don't want to accidentally delete the dummy window
        this->dummyWindowInUse = false;
        return;
    }
    this->libsdl2.destroyWindow(window->sdlWindow);
    pincAllocator_free(rootAllocator, window, sizeof(PincSdl2Window));
}

void pincSdl2setWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, uint8_t* title, size_t titleLen) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* window = (PincSdl2Window*)windowHandle;
    
    // It needs to be null terminated because reasons
    char* titleNullTerm = pincString_marshalAlloc((pincString){.str = title, .len = titleLen}, tempAllocator);
    this->libsdl2.setWindowTitle(window->sdlWindow, titleNullTerm);
    pincAllocator_free(tempAllocator, titleNullTerm, titleLen+1);
    // We take ownership of the title
    pincAllocator_free(rootAllocator, title, titleLen);
}

uint8_t const * pincSdl2getWindowTitle(struct WindowBackend* obj, WindowHandle windowHandle, size_t* outTitleLen) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* window = (PincSdl2Window*)windowHandle;
    char const* title = this->libsdl2.getWindowTitle(window->sdlWindow);
    *outTitleLen = pincStringLen(title);
    return (uint8_t const*)title;
}

void pincSdl2setWindowWidth(struct WindowBackend* obj, WindowHandle windowHandle, uint32_t width) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* window = (PincSdl2Window*)windowHandle;
    window->width = width;
    PErrorAssert(window->width < INT32_MAX, "Integer Overflow");
    PErrorAssert(window->height < INT32_MAX, "Integer Overflow");
    // TODO: what about the hacky HiDPI / scaling support in SDL2?
    // This function's documentation somehow manages to make the situation more confusing.
    // Really, I think we'll just have to abandon the idea of supporting scaling for the SDL2 backend
    // and work on implementing 'native' backends that deal with it properly.
    this->libsdl2.setWindowSize(window->sdlWindow, (int)window->width, (int)window->height);
    
}

uint32_t pincSdl2getWindowWidth(struct WindowBackend* obj, WindowHandle window) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* windowObj = (PincSdl2Window*)window;
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
    PErrorSanitize(width <= INT32_MAX && width > 0, "Integer overflow");
    PErrorAssert((uint32_t)width == windowObj->width, "Window width and \"real\" width do not match!");
    return (uint32_t)width;
}

void pincSdl2setWindowHeight(struct WindowBackend* obj, WindowHandle windowHandle, uint32_t height) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* window = (PincSdl2Window*)windowHandle;
    window->height = height;
    PErrorAssert(window->width < INT32_MAX, "Integer Overflow");
    PErrorAssert(window->height < INT32_MAX, "Integer Overflow");
    // TODO: what about the hacky HiDPI / scaling support in SDL2?
    // This function's documentation somehow manages to make the situation more confusing.
    // Really, I think we'll just have to abandon the idea of supporting scaling for the SDL2 backend
    // and work on implementing 'native' backends that deal with it properly.
    this->libsdl2.setWindowSize(window->sdlWindow, (int)window->width, (int)window->height);
}

uint32_t pincSdl2getWindowHeight(struct WindowBackend* obj, WindowHandle window) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* windowObj = (PincSdl2Window*)window;
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
    PErrorSanitize(height <= INT32_MAX && height > 0, "Integer overflow");
    return (uint32_t)height;
}

float pincSdl2getWindowScaleFactor(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return 0;
}

void pincSdl2setWindowResizable(struct WindowBackend* obj, WindowHandle window, bool resizable) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(resizable);
    // TODO
}

bool pincSdl2getWindowResizable(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void pincSdl2setWindowMinimized(struct WindowBackend* obj, WindowHandle window, bool minimized) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(minimized);
    // TODO
}

bool pincSdl2getWindowMinimized(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void pincSdl2setWindowMaximized(struct WindowBackend* obj, WindowHandle window, bool maximized) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(maximized);
    // TODO
}

bool pincSdl2getWindowMaximized(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void pincSdl2setWindowFullscreen(struct WindowBackend* obj, WindowHandle window, bool fullscreen) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(fullscreen);
    // TODO
}

bool pincSdl2getWindowFullscreen(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void pincSdl2setWindowFocused(struct WindowBackend* obj, WindowHandle window, bool focused) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(focused);
    // TODO
}

bool pincSdl2getWindowFocused(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

void pincSdl2setWindowHidden(struct WindowBackend* obj, WindowHandle window, bool hidden) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(hidden);
    // TODO
}

bool pincSdl2getWindowHidden(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO
    return false;
}

PincReturnCode pincSdl2setVsync(struct WindowBackend* obj, bool vsync) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
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

bool pincSdl2getVsync(struct WindowBackend* obj) {
    // TODO: only graphics backend is OpenGL, shortcuts are taken
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    return this->libsdl2.glGetSwapInterval() != 0;
}

void pincSdl2windowPresentFramebuffer(struct WindowBackend* obj, WindowHandle window) {
    PincSdl2Window* windowObj = (PincSdl2Window*)window;
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    // TODO: only graphics backend is OpenGL, shortcuts are taken
    this->libsdl2.glSwapWindow(windowObj->sdlWindow);
}

PincOpenglSupportStatus pincSdl2queryGlVersionSupported(struct WindowBackend* obj, uint32_t major, uint32_t minor, PincOpenglContextProfile profile) {
    P_UNUSED(obj);
    P_UNUSED(major);
    P_UNUSED(minor);
    P_UNUSED(profile);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlAccumulatorBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t channel, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(channel);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlAlphaBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlDepthBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlStencilBits(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t bits) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(bits);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlSamples(struct WindowBackend* obj, FramebufferFormat framebuffer, uint32_t samples) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    P_UNUSED(samples);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlStereoBuffer(struct WindowBackend* obj, FramebufferFormat framebuffer) {
    P_UNUSED(obj);
    P_UNUSED(framebuffer);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlContextDebug(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlRobustAccess(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

PincOpenglSupportStatus pincSdl2queryGlResetIsolation(struct WindowBackend* obj) {
    P_UNUSED(obj);
    // SDL2 has no clean way to query OpenGL support before attempting to make a context.
    return PincOpenglSupportStatus_maybe;
}

RawOpenglContextHandle pincSdl2glCompleteContext(struct WindowBackend* obj, IncompleteGlContext incompleteContext) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
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
    PincSdl2Window* dummyWindow = _dummyWindow(obj);
    SDL_GLContext sdlGlContext = this->libsdl2.glCreateContext(dummyWindow->sdlWindow);
    if(!sdlGlContext) {
        #if PINC_ENABLE_ERROR_EXTERNAL
        pincString errorMsg = pincString_concat(2, (pincString[]){
            pincString_makeDirect((char*)"SDL2 backend: Could not create OpenGl context: "),
            // const is not an issue, this string will not be modified
            pincString_makeDirect((char*)this->libsdl2.getError()),
        },tempAllocator);
        PErrorExternalStr(false, errorMsg);
        pincString_free(&errorMsg, tempAllocator);
        #endif
        return 0;
    }
    // This is to stop users from assuming the context will be current after completion, like what SDL2 does.
    this->libsdl2.glMakeCurrent(0, 0);
    // Unlike a window, an OpenGl context contains no other information than just the opaque pointer
    // So no need to wrap it in a struct or anything
    return sdlGlContext;
}

void pincSdl2glDeinitContext(struct WindowBackend* obj, RawOpenglContextObject context) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    SDL_GLContext sdlGlContext = (SDL_GLContext)context.handle;
    this->libsdl2.glDeleteContext(sdlGlContext);
}

uint32_t pincSdl2glGetContextAccumulatorBits(struct WindowBackend* obj, RawOpenglContextObject context, uint32_t channel){
    P_UNUSED(obj);
    P_UNUSED(context);
    P_UNUSED(channel);
    // TODO
    return 0;
}

uint32_t pincSdl2glGetContextAlphaBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t pincSdl2glGetContextDepthBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t pincSdl2glGetContextStencilBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t pincSdl2glGetContextSamples(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

bool pincSdl2glGetContextStereoBuffer(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool pincSdl2glGetContextDebug(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool pincSdl2glGetContextRobustAccess(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool pincSdl2glGetContextResetIsolation(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}


PincReturnCode pincSdl2glMakeCurrent(struct WindowBackend* obj, WindowHandle window, RawOpenglContextHandle context) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* windowObj = (PincSdl2Window*)window;
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
    if(result != 0) {
        #if PINC_ENABLE_ERROR_EXTERNAL
        pincString errorMsg = pincString_concat(2, (pincString[]){
            pincString_makeDirect((char*)"SDL2 backend: Could not make context current: "),
            pincString_makeDirect((char*)this->libsdl2.getError()),
        },tempAllocator);
        PErrorExternalStr(false, errorMsg);
        pincString_free(&errorMsg, tempAllocator);
        #endif
        return PincReturnCode_error;
    }
    return PincReturnCode_pass;
}

PincWindowHandle pincSdl2glGetCurrentWindow(struct WindowBackend* obj) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    SDL_Window* sdlWin = this->libsdl2.glGetCurrentWindow();
    if(!sdlWin) {
        return 0;
    }
    PincSdl2Window* thisWin = this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
    if(!thisWin) {
        return 0;
    }
    return thisWin->frontHandle;
}

PincOpenglContextHandle pincSdl2glGetCurrentContext(struct WindowBackend* obj) {
    // TODO: I'm not too confident about this, it should be tested properly
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
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

PincPfn pincSdl2glGetProc(struct WindowBackend* obj, char const* procname) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PErrorUser(this->libsdl2.glGetCurrentContext(), "Cannot get proc address of an OpenGL function without a current context");
    return this->libsdl2.glGetProcAddress(procname);
}

#endif
