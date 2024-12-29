#include "platform/platform.h"
#include "window.h"
#include "pinc_main.h"
#include "sdl2load.h"

typedef struct {
    Sdl2Functions libsdl2;
} Sdl2WindowBackend;

static void* sdl2Lib = 0;

static void* sdl2LoadLib(void) {
    // TODO: what are all the library name possibilities?
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

void psdl2NotUsed(void) {
    // This function is called when psdl2IncompleteInit was called, but SDL2 was not chosen as the window backend.
    if(sdl2Lib) {
        pUnloadLibrary(sdl2Lib);
    }
}

bool psdl2Init(WindowBackend* obj) {
    if(sdl2Lib) {
        return true;
    }
    // The only thing required for SDL2 support is for the SDL2 library to be present
    void* lib = sdl2LoadLib();
    if(!lib) {
        return false;
    }
    sdl2Lib = lib;
    obj->obj = Allocator_allocate(rootAllocator, sizeof(Sdl2WindowBackend));
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    loadSdl2Functions(sdl2Lib, &this->libsdl2);
    SdlVersion sdlVersion;
    this->libsdl2.getVersion(&sdlVersion);
    pPrintFormat("Loaded SDL2 version: %i.%i.%i", sdlVersion.major, sdlVersion.minor, sdlVersion.patch);
    return true;
}

void psdl2Deinit(WindowBackend* obj) {
    Sdl2WindowBackend* this = (Sdl2WindowBackend*)obj->obj;
    Allocator_free(rootAllocator, this, sizeof(Sdl2WindowBackend));
    // TODO: quit sdl2
}
