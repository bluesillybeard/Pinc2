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

// Classic macro trick
// Reminder: you can use the -E flag in gcc or clang to preprocess the file without compiling it

#define SDL_FUNC(type, name, realName, args)
#define SDL_FUNCTIONS \
    SDL_FUNC(int, init, SDL_Init, (uint32_t flags)) \
    SDL_FUNC(void, getVersion, SDL_GetVersion, (SdlVersion* ver)) \
    

#undef SDL_FUNC

#define SDL_FUNC(type, name, realName, args) typedef type (* PFN_##realName) args;
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
