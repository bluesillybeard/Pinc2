#include "platform/pinc_platform.h"
#include "pinc_error.h"

// This takes the SDL2 functions, and enumerates them so that they can be loaded at runtime.
// SDL2's headers are open source and free to use and distribute, thus they are located within the local SDL2 directory.

// Avoid references to libc
#define SDL_DISABLE_IMMINTRIN_H 1
#define SDL_DISABLE_XMMINTRIN_H 1
#define SDL_DISABLE_EMMINTRIN_H 1
#include <SDL2/SDL.h>

// Classic macro trick. This tends to break LSP. Rather annoying, but better than having to repeat every function declaration 5 times.
// Reminder: you can use the -E flag in gcc or clang to preprocess the file without compiling it
// Even clangd gets confused from this mega death macro, if you have a better way that isn't just writing every function 3 times, send it in.

#define SDL_FUNC(type, name, realName, args)
#define SDL_FUNC_OPTIONAL(type, name, realName, args)
#define SDL_FUNCTIONS \
    SDL_FUNC(int, init, SDL_Init, (uint32_t flags)) \
    SDL_FUNC(void, quit, SDL_Quit, (void)) \
    SDL_FUNC(void, getVersion, SDL_GetVersion, (SDL_version* ver)) \
    SDL_FUNC(int, getNumVideoDisplays, SDL_GetNumVideoDisplays, (void)) \
    SDL_FUNC(int, getNumDisplayModes, SDL_GetNumDisplayModes, (int displayIndex)) \
    SDL_FUNC(int, getDisplayMode, SDL_GetDisplayMode, (int displayIndex, int modeIndex, SDL_DisplayMode* outMode)) \
    SDL_FUNC(SDL_PixelFormat*, allocFormat, SDL_AllocFormat, (uint32_t pixelFormat)) \
    SDL_FUNC(void, freeFormat, SDL_FreeFormat, (SDL_PixelFormat* format)) \
    SDL_FUNC(SDL_Window*, createWindow, SDL_CreateWindow, (char const* title, int x, int y, int w, int h, uint32_t flags)) \
    SDL_FUNC(int, glSetSwapInterval, SDL_GL_SetSwapInterval, (int interval)) \
    SDL_FUNC(int, glGetSwapInterval, SDL_GL_GetSwapInterval, (void)) \
    SDL_FUNC(char const*, getError, SDL_GetError, (void)) \
    SDL_FUNC(void, setWindowTitle, SDL_SetWindowTitle, (SDL_Window* window, char const* title)) \
    SDL_FUNC(int, pollEvent, SDL_PollEvent, (SDL_Event* event)) \
    SDL_FUNC(void*, setWindowData, SDL_SetWindowData, (SDL_Window* window, char const* name, void* userdata)) \
    SDL_FUNC(void*, getWindowData, SDL_GetWindowData, (SDL_Window* window, char const* name)) \
    SDL_FUNC(SDL_Window*, getWindowFromId, SDL_GetWindowFromID, (uint32_t id)) \
    SDL_FUNC(void, glSwapWindow, SDL_GL_SwapWindow, (SDL_Window* window)) \
    SDL_FUNC(SDL_GLContext, glCreateContext, SDL_GL_CreateContext, (SDL_Window* window)) \
    SDL_FUNC(void, glDeleteContext, SDL_GL_DeleteContext, (SDL_GLContext context)) \
    SDL_FUNC(int, glMakeCurrent, SDL_GL_MakeCurrent, (SDL_Window* window, SDL_GLContext context)) \
    SDL_FUNC(pincPFN, glGetProcAddress, SDL_GL_GetProcAddress, (const char* proc)) \
    SDL_FUNC(SDL_GLContext, glGetCurrentContext, SDL_GL_GetCurrentContext, (void)) \
    SDL_FUNC(SDL_Window*, glGetCurrentWindow, SDL_GL_GetCurrentWindow, (void)) \
    SDL_FUNC(int, glSetAttribute, SDL_GL_SetAttribute, (SDL_GLattr attr, int value)) \
    SDL_FUNC(void, getWindowSize, SDL_GetWindowSize, (SDL_Window* window, int* width, int* height)) \
    /* added in 2.0.1 */ SDL_FUNC_OPTIONAL(void, glGetDrawableSize, SDL_GL_GetDrawableSize, (SDL_Window* window, int* width, int* height)) \
    /* added in 2.26.0 */ SDL_FUNC_OPTIONAL(void, getWindowSizeInPixels, SDL_GetWindowSizeInPixels, (SDL_Window* window, int* width, int* height)) \
    SDL_FUNC(void, setWindowSize, SDL_SetWindowSize, (SDL_Window* window, int width, int height)) \
    SDL_FUNC(void, destroyWindow, SDL_DestroyWindow, (SDL_Window* window)) \
    SDL_FUNC(uint32_t, getWindowFlags, SDL_GetWindowFlags, (SDL_Window* window)) \
    SDL_FUNC(uint32_t, getTicks, SDL_GetTicks, (void)) \
    SDL_FUNC(uint64_t, getTicks64, SDL_GetTicks64, (void)) \
    SDL_FUNC(void, resetHints, SDL_ResetHints, (void)) \
    SDL_FUNC(char const*, getWindowTitle, SDL_GetWindowTitle, (SDL_Window* window)) \
    SDL_FUNC(SDL_bool, pixelFormatEnumToMasks, SDL_PixelFormatEnumToMasks, (uint32_t format, int* bpp, uint32_t* Rmask, uint32_t* Gmask, uint32_t* Bmask, uint32_t* Amask)) \
    SDL_FUNC(char*, getClipboardText, SDL_GetClipboardText, (void)) \
    SDL_FUNC(SDL_bool, hasClipboardText, SDL_HasClipboardText, (void)) \
    SDL_FUNC(void, SDL_free, free, (void *mem)) \


#undef SDL_FUNC
#undef SDL_FUNC_OPTIONAL

#define SDL_FUNC(_type, _name, _realName, _args) typedef _type (SDLCALL * PFN_##_realName) _args;
#define SDL_FUNC_OPTIONAL(_type, _name, _realName, _args) typedef _type (SDLCALL * PFN_##_realName) _args;

SDL_FUNCTIONS //NOLINT: TODO: fix short stupid names for function arguments

#undef SDL_FUNC
#undef SDL_FUNC_OPTIONAL

#define SDL_FUNC(type, name, realName, args) PFN_##realName name;
#define SDL_FUNC_OPTIONAL(type, name, realName, args) PFN_##realName name;

typedef struct {
    SDL_FUNCTIONS
} Sdl2Functions;

#undef SDL_FUNC
#undef SDL_FUNC_OPTIONAL

#define SDL_FUNC(type, name, realName, args)\
    functions->name = (PFN_##realName) pincLibrarySymbol(lib, (uint8_t*)#realName, pincStringLen(#realName));\
    PErrorExternal(functions->name, "Unable to load SDL2 function " #realName);

#define SDL_FUNC_OPTIONAL(type, name, realName, args)\
    functions->name = (PFN_##realName) pincLibrarySymbol(lib, (uint8_t*)#realName, pincStringLen(#realName));\

void pincLoadSdl2Functions(void* lib, Sdl2Functions* functions) { //NOLINT: this function is auto-generated by the macros. One could argue that is just as bad, but here we are.
    SDL_FUNCTIONS
}

#undef SDL_FUNC
#undef SDL_FUNC_OPTIONAL
