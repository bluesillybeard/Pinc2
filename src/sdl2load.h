#include "platform/platform.h"

// This takes the SDL2 functions, and enumerates them so that they can be loaded at runtime.
// SDL2's headers are open source and free to use and distribute, thus they are located within the local SDL2 directory.

#include "SDL2/SDL.h"

// Classic macro trick. This tends to break LSP. Rather annoying, but better than having to repeat every function declaration 5 times.
// Reminder: you can use the -E flag in gcc or clang to preprocess the file without compiling it

#define SDL_FUNC(type, name, realName, args)
#define SDL_FUNCTIONS \
    SDL_FUNC(int, init, SDL_Init, (uint32_t flags)) \
    SDL_FUNC(void, getVersion, SDL_GetVersion, (SDL_version* ver)) \
    SDL_FUNC(int, getNumVideoDisplays, SDL_GetNumVideoDisplays, (void)) \
    SDL_FUNC(int, getNumDisplayModes, SDL_GetNumDisplayModes, (int displayIndex)) \
    SDL_FUNC(int, getDisplayMode, SDL_GetDisplayMode, (int displayIndex, int modeIndex, SDL_DisplayMode* outMode)) \
    SDL_FUNC(SDL_PixelFormat*, allocFormat, SDL_AllocFormat, (uint32_t pixelFormat)) \
    SDL_FUNC(void, freeFormat, SDL_FreeFormat, (SDL_PixelFormat* format)) \
    SDL_FUNC(SDL_Window*, createWindow, SDL_CreateWindow, (char const* title, int x, int y, int w, int h, uint32_t flags)) \
    SDL_FUNC(int, glSetSwapInterval, SDL_GL_SetSwapInterval, (int interval)) \
    SDL_FUNC(char const*, getError, SDL_GetError, (void)) \
    SDL_FUNC(void, setWindowTitle, SDL_SetWindowTitle, (SDL_Window* window, char const* title)) \
    SDL_FUNC(int, pollEvent, SDL_PollEvent, (SDL_Event* event)) \
    SDL_FUNC(void*, setWindowData, SDL_SetWindowData, (SDL_Window* window, char const* name, void* userdata)) \
    SDL_FUNC(void*, getWindowData, SDL_GetWindowData, (SDL_Window* window, char const* name)) \
    SDL_FUNC(SDL_Window*, getWindowFromId, SDL_GetWindowFromID, (uint32_t id)) \

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
