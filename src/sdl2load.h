#include "platform/platform.h"

// Figure out SDL2 calling convention
// Stolen from SDL2's source code: include/begin_code.h

/* By default SDL uses the C calling convention */
#ifndef SDLCALL
#if (defined(__WIN32__) || defined(__WINRT__) || defined(__GDK__)) && !defined(__GNUC__)
#define SDLCALL __cdecl
#elif defined(__OS2__) || defined(__EMX__)
#define SDLCALL _System
# if defined (__GNUC__) && !defined(_System)
#  define _System /* for old EMX/GCC compat.  */
# endif
#else
#define SDLCALL
#endif
#endif /* SDLCALL */

// SDL types

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} SdlVersion;

typedef struct {
    uint32_t format;
    int width;
    int height;
    void* driverData;
} SdlDisplayMode;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} SdlColor;

typedef struct {
    int ncolors;
    SdlColor *colors;
    uint32_t version;
    int refcount;
} SdlPalette;

typedef struct SdlPixelFormat {
    uint32_t format;
    SdlPalette *palette;
    uint8_t BitsPerPixel;
    uint8_t BytesPerPixel;
    uint8_t padding[2];
    uint32_t Rmask;
    uint32_t Gmask;
    uint32_t Bmask;
    uint32_t Amask;
    uint8_t Rloss;
    uint8_t Gloss;
    uint8_t Bloss;
    uint8_t Aloss;
    uint8_t Rshift;
    uint8_t Gshift;
    uint8_t Bshift;
    uint8_t Ashift;
    int refcount;
    struct SdlPixelFormat *next;
} SdlPixelFormat;

typedef void* SdlWindowHandle;

typedef void* SdlOpenglContext;

// Enums (these are just copied directly from SDL2 source code for now)

#define SDL_INIT_TIMER          0x00000001u
#define SDL_INIT_AUDIO          0x00000010u
#define SDL_INIT_VIDEO          0x00000020u  /**< SDL_INIT_VIDEO implies SDL_INIT_EVENTS */
#define SDL_INIT_JOYSTICK       0x00000200u  /**< SDL_INIT_JOYSTICK implies SDL_INIT_EVENTS */
#define SDL_INIT_HAPTIC         0x00001000u
#define SDL_INIT_GAMECONTROLLER 0x00002000u  /**< SDL_INIT_GAMECONTROLLER implies SDL_INIT_JOYSTICK */
#define SDL_INIT_EVENTS         0x00004000u
#define SDL_INIT_SENSOR         0x00008000u
#define SDL_INIT_NOPARACHUTE    0x00100000u  /**< compatibility; this flag is ignored. */
#define SDL_INIT_EVERYTHING ( \
                SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | \
                SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR \
            )

/**
 *  \brief The flags on a window
 *
 *  \sa SDL_GetWindowFlags()
 */
typedef enum
{
    SDL_WINDOW_FULLSCREEN = 0x00000001,         /**< fullscreen window */
    SDL_WINDOW_OPENGL = 0x00000002,             /**< window usable with OpenGL context */
    SDL_WINDOW_SHOWN = 0x00000004,              /**< window is visible */
    SDL_WINDOW_HIDDEN = 0x00000008,             /**< window is not visible */
    SDL_WINDOW_BORDERLESS = 0x00000010,         /**< no window decoration */
    SDL_WINDOW_RESIZABLE = 0x00000020,          /**< window can be resized */
    SDL_WINDOW_MINIMIZED = 0x00000040,          /**< window is minimized */
    SDL_WINDOW_MAXIMIZED = 0x00000080,          /**< window is maximized */
    SDL_WINDOW_MOUSE_GRABBED = 0x00000100,      /**< window has grabbed mouse input */
    SDL_WINDOW_INPUT_FOCUS = 0x00000200,        /**< window has input focus */
    SDL_WINDOW_MOUSE_FOCUS = 0x00000400,        /**< window has mouse focus */
    SDL_WINDOW_FULLSCREEN_DESKTOP = ( SDL_WINDOW_FULLSCREEN | 0x00001000 ),
    SDL_WINDOW_FOREIGN = 0x00000800,            /**< window not created by SDL */
    SDL_WINDOW_ALLOW_HIGHDPI = 0x00002000,      /**< window should be created in high-DPI mode if supported.
                                                     On macOS NSHighResolutionCapable must be set true in the
                                                     application's Info.plist for this to have any effect. */
    SDL_WINDOW_MOUSE_CAPTURE    = 0x00004000,   /**< window has mouse captured (unrelated to MOUSE_GRABBED) */
    SDL_WINDOW_ALWAYS_ON_TOP    = 0x00008000,   /**< window should always be above others */
    SDL_WINDOW_SKIP_TASKBAR     = 0x00010000,   /**< window should not be added to the taskbar */
    SDL_WINDOW_UTILITY          = 0x00020000,   /**< window should be treated as a utility window */
    SDL_WINDOW_TOOLTIP          = 0x00040000,   /**< window should be treated as a tooltip */
    SDL_WINDOW_POPUP_MENU       = 0x00080000,   /**< window should be treated as a popup menu */
    SDL_WINDOW_KEYBOARD_GRABBED = 0x00100000,   /**< window has grabbed keyboard input */
    SDL_WINDOW_VULKAN           = 0x10000000,   /**< window usable for Vulkan surface */
    SDL_WINDOW_METAL            = 0x20000000,   /**< window usable for Metal view */

    SDL_WINDOW_INPUT_GRABBED = SDL_WINDOW_MOUSE_GRABBED /**< equivalent to SDL_WINDOW_MOUSE_GRABBED for compatibility */
} SDL_WindowFlags;

/**
 *  \brief Used to indicate that you don't care what the window position is.
 */
#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000u
#define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X)  (SDL_WINDOWPOS_UNDEFINED_MASK|(X))
#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)
#define SDL_WINDOWPOS_ISUNDEFINED(X)    \
            (((X)&0xFFFF0000) == SDL_WINDOWPOS_UNDEFINED_MASK)

/**
 *  \brief Used to indicate that the window position should be centered.
 */
#define SDL_WINDOWPOS_CENTERED_MASK    0x2FFF0000u
#define SDL_WINDOWPOS_CENTERED_DISPLAY(X)  (SDL_WINDOWPOS_CENTERED_MASK|(X))
#define SDL_WINDOWPOS_CENTERED         SDL_WINDOWPOS_CENTERED_DISPLAY(0)
#define SDL_WINDOWPOS_ISCENTERED(X)    \
            (((X)&0xFFFF0000) == SDL_WINDOWPOS_CENTERED_MASK)

// Classic macro trick
// Reminder: you can use the -E flag in gcc or clang to preprocess the file without compiling it

#define SDL_FUNC(type, name, realName, args)
#define SDL_FUNCTIONS \
    SDL_FUNC(int, init, SDL_Init, (uint32_t flags)) \
    SDL_FUNC(void, getVersion, SDL_GetVersion, (SdlVersion* ver)) \
    SDL_FUNC(int, getNumVideoDisplays, SDL_GetNumVideoDisplays, (void)) \
    SDL_FUNC(int, getNumDisplayModes, SDL_GetNumDisplayModes, (int displayIndex)) \
    SDL_FUNC(int, getDisplayMode, SDL_GetDisplayMode, (int displayIndex, int modeIndex, SdlDisplayMode* outMode)) \
    SDL_FUNC(SdlPixelFormat*, allocFormat, SDL_AllocFormat, (uint32_t pixelFormat)) \
    SDL_FUNC(void, freeFormat, SDL_FreeFormat, (SdlPixelFormat* format)) \
    SDL_FUNC(SdlWindowHandle, createWindow, SDL_CreateWindow, (char const* title, int x, int y, int w, int h, uint32_t flags)) \
    SDL_FUNC(int, glSetSwapInterval, SDL_GL_SetSwapInterval, (int interval)) \
    SDL_FUNC(char const*, getError, SDL_GetError, (void)) \
    SDL_FUNC(void, setWindowTitle, SDL_SetWindowTitle, (SdlWindowHandle window, char const* title)) \

#undef SDL_FUNC

#define SDL_FUNC(type, name, realName, args) typedef type (SDLCALL * PFN_##realName) args;
SDL_FUNCTIONS
#undef SDL_FUNC

#define SDL_FUNC(type, name, realName, args) PFN_##realName name;
typedef struct {
    SDL_FUNCTIONS
} Sdl2Functions;
#undef SDL_FUNC

#define SDL_FUNC(type, name, realName, args) functions->name = pLibrarySymbol(lib, (uint8_t*)#realName, pStringLen(#realName));
void loadSdl2Functions(void* lib, Sdl2Functions* functions) {
    SDL_FUNCTIONS
}
#undef SDL_FUNC
