// To make the build system as simple as possible, backend source files must remove themselves rather than rely on the build system
#include "pinc_options.h"
#if PINC_HAVE_WINDOW_SDL2

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_scancode.h>

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>

#include <pinc.h>
#include <pinc_opengl.h>

#include "pinc_error.h"
#include "pinc_main.h"
#include "pinc_sdl2load.h"
#include "pinc_types.h"
#include "pinc_window.h"
#include "platform/pinc_platform.h"
#include <libs/pinc_allocator.h>
#include <libs/pinc_string.h>
#include <libs/pinc_utf8.h>

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
    // TODO(bluesillybeard): instead of a list of pointers, just use a list of the struct itself
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
    uintptr_t index = 0;
    for(uintptr_t i=0; i<this->windowsNum; i++) {
        if(this->windows[i] == window) {
            index = i;
            indexFound = true;
            break;
        }
    }
    if(!indexFound) { return; }
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

static PincKeyboardKey pincSdl2ConvertSdlKeycode(SDL_Scancode code) {
    PincKeyboardKey key = 0;
    // NOLINTBEGIN(bugprone-branch-clone)
    switch(code){
        case SDL_SCANCODE_UNKNOWN: key = PincKeyboardKey_unknown; break;
        case SDL_SCANCODE_A: key = PincKeyboardKey_a; break;
        case SDL_SCANCODE_B: key = PincKeyboardKey_b; break;
        case SDL_SCANCODE_C: key = PincKeyboardKey_c; break;
        case SDL_SCANCODE_D: key = PincKeyboardKey_d; break;
        case SDL_SCANCODE_E: key = PincKeyboardKey_e; break;
        case SDL_SCANCODE_F: key = PincKeyboardKey_f; break;
        case SDL_SCANCODE_G: key = PincKeyboardKey_g; break;
        case SDL_SCANCODE_H: key = PincKeyboardKey_h; break;
        case SDL_SCANCODE_I: key = PincKeyboardKey_i; break;
        case SDL_SCANCODE_J: key = PincKeyboardKey_j; break;
        case SDL_SCANCODE_K: key = PincKeyboardKey_k; break;
        case SDL_SCANCODE_L: key = PincKeyboardKey_l; break;
        case SDL_SCANCODE_M: key = PincKeyboardKey_m; break;
        case SDL_SCANCODE_N: key = PincKeyboardKey_n; break;
        case SDL_SCANCODE_O: key = PincKeyboardKey_o; break;
        case SDL_SCANCODE_P: key = PincKeyboardKey_p; break;
        case SDL_SCANCODE_Q: key = PincKeyboardKey_q; break;
        case SDL_SCANCODE_R: key = PincKeyboardKey_r; break;
        case SDL_SCANCODE_S: key = PincKeyboardKey_s; break;
        case SDL_SCANCODE_T: key = PincKeyboardKey_t; break;
        case SDL_SCANCODE_U: key = PincKeyboardKey_u; break;
        case SDL_SCANCODE_V: key = PincKeyboardKey_v; break;
        case SDL_SCANCODE_W: key = PincKeyboardKey_w; break;
        case SDL_SCANCODE_X: key = PincKeyboardKey_x; break;
        case SDL_SCANCODE_Y: key = PincKeyboardKey_y; break;
        case SDL_SCANCODE_Z: key = PincKeyboardKey_z; break;
        case SDL_SCANCODE_1: key = PincKeyboardKey_1; break;
        case SDL_SCANCODE_2: key = PincKeyboardKey_2; break;
        case SDL_SCANCODE_3: key = PincKeyboardKey_3; break;
        case SDL_SCANCODE_4: key = PincKeyboardKey_4; break;
        case SDL_SCANCODE_5: key = PincKeyboardKey_5; break;
        case SDL_SCANCODE_6: key = PincKeyboardKey_6; break;
        case SDL_SCANCODE_7: key = PincKeyboardKey_7; break;
        case SDL_SCANCODE_8: key = PincKeyboardKey_8; break;
        case SDL_SCANCODE_9: key = PincKeyboardKey_9; break;
        case SDL_SCANCODE_0: key = PincKeyboardKey_0; break;
        case SDL_SCANCODE_RETURN: key = PincKeyboardKey_enter; break;
        case SDL_SCANCODE_ESCAPE: key = PincKeyboardKey_escape; break;
        case SDL_SCANCODE_BACKSPACE: key = PincKeyboardKey_backspace; break;
        case SDL_SCANCODE_TAB: key = PincKeyboardKey_tab; break;
        case SDL_SCANCODE_SPACE: key = PincKeyboardKey_space; break;
        case SDL_SCANCODE_MINUS: key = PincKeyboardKey_dash; break;
        case SDL_SCANCODE_EQUALS: key = PincKeyboardKey_equals; break;
        case SDL_SCANCODE_LEFTBRACKET: key = PincKeyboardKey_leftBracket; break;
        case SDL_SCANCODE_RIGHTBRACKET: key = PincKeyboardKey_rightBracket; break;
        case SDL_SCANCODE_BACKSLASH: key = PincKeyboardKey_backspace; break;
        case SDL_SCANCODE_NONUSHASH: key = PincKeyboardKey_unknown; break;// TODO(bluesillybeard): what is this?
        case SDL_SCANCODE_SEMICOLON: key = PincKeyboardKey_semicolon; break;
        case SDL_SCANCODE_APOSTROPHE: key = PincKeyboardKey_apostrophe; break;
        case SDL_SCANCODE_GRAVE: key = PincKeyboardKey_backtick; break;
        case SDL_SCANCODE_COMMA: key = PincKeyboardKey_comma; break;
        case SDL_SCANCODE_PERIOD: key = PincKeyboardKey_dot; break;
        case SDL_SCANCODE_SLASH: key = PincKeyboardKey_slash; break;
        case SDL_SCANCODE_CAPSLOCK: key = PincKeyboardKey_capsLock; break;
        case SDL_SCANCODE_F1: key = PincKeyboardKey_f1; break;
        case SDL_SCANCODE_F2: key = PincKeyboardKey_f2; break;
        case SDL_SCANCODE_F3: key = PincKeyboardKey_f3; break;
        case SDL_SCANCODE_F4: key = PincKeyboardKey_f4; break;
        case SDL_SCANCODE_F5: key = PincKeyboardKey_f5; break;
        case SDL_SCANCODE_F6: key = PincKeyboardKey_f6; break;
        case SDL_SCANCODE_F7: key = PincKeyboardKey_f7; break;
        case SDL_SCANCODE_F8: key = PincKeyboardKey_f8; break;
        case SDL_SCANCODE_F9: key = PincKeyboardKey_f9; break;
        case SDL_SCANCODE_F10: key = PincKeyboardKey_f10; break;
        case SDL_SCANCODE_F11: key = PincKeyboardKey_f11; break;
        case SDL_SCANCODE_F12: key = PincKeyboardKey_f12; break;
        case SDL_SCANCODE_PRINTSCREEN: key = PincKeyboardKey_printScreen; break;
        case SDL_SCANCODE_SCROLLLOCK: key = PincKeyboardKey_scrollLock; break;
        case SDL_SCANCODE_PAUSE: key = PincKeyboardKey_pause; break;
        case SDL_SCANCODE_INSERT: key = PincKeyboardKey_insert; break;
        case SDL_SCANCODE_HOME: key = PincKeyboardKey_home; break;
        case SDL_SCANCODE_PAGEUP: key = PincKeyboardKey_pageUp; break;
        case SDL_SCANCODE_DELETE: key = PincKeyboardKey_delete; break;
        case SDL_SCANCODE_END: key = PincKeyboardKey_end; break;
        case SDL_SCANCODE_PAGEDOWN: key = PincKeyboardKey_pageDown; break;
        case SDL_SCANCODE_RIGHT: key = PincKeyboardKey_right; break;
        case SDL_SCANCODE_LEFT: key = PincKeyboardKey_left; break;
        case SDL_SCANCODE_DOWN: key = PincKeyboardKey_down; break;
        case SDL_SCANCODE_UP: key = PincKeyboardKey_up; break;
        case SDL_SCANCODE_NUMLOCKCLEAR: key = PincKeyboardKey_numLock; break;
        case SDL_SCANCODE_KP_DIVIDE: key = PincKeyboardKey_numpadSlash; break;
        case SDL_SCANCODE_KP_MULTIPLY: key = PincKeyboardKey_numpadAsterisk; break;
        case SDL_SCANCODE_KP_MINUS: key = PincKeyboardKey_numpadDash; break;
        case SDL_SCANCODE_KP_PLUS: key = PincKeyboardKey_numpadPlus; break;
        case SDL_SCANCODE_KP_ENTER: key = PincKeyboardKey_numpadEnter; break;
        case SDL_SCANCODE_KP_1: key = PincKeyboardKey_numpad1; break;
        case SDL_SCANCODE_KP_2: key = PincKeyboardKey_numpad2; break;
        case SDL_SCANCODE_KP_3: key = PincKeyboardKey_numpad3; break;
        case SDL_SCANCODE_KP_4: key = PincKeyboardKey_numpad4; break;
        case SDL_SCANCODE_KP_5: key = PincKeyboardKey_numpad5; break;
        case SDL_SCANCODE_KP_6: key = PincKeyboardKey_numpad6; break;
        case SDL_SCANCODE_KP_7: key = PincKeyboardKey_numpad7; break;
        case SDL_SCANCODE_KP_8: key = PincKeyboardKey_numpad8; break;
        case SDL_SCANCODE_KP_9: key = PincKeyboardKey_numpad9; break;
        case SDL_SCANCODE_KP_0: key = PincKeyboardKey_numpad0; break;
        case SDL_SCANCODE_KP_PERIOD: key = PincKeyboardKey_numpadDot; break;
        case SDL_SCANCODE_NONUSBACKSLASH: key = PincKeyboardKey_unknown; break; // TODO(bluesillybeard): what is this
        case SDL_SCANCODE_APPLICATION: key = PincKeyboardKey_menu; break; // If SDL's APPLICATION key is menu, then what is SDL's MENU?
        case SDL_SCANCODE_POWER: key = PincKeyboardKey_unknown; break; // TODO(bluesillybeard): what is this
        case SDL_SCANCODE_KP_EQUALS: key = PincKeyboardKey_numpadEqual; break;
        case SDL_SCANCODE_F13: key = PincKeyboardKey_f13; break;
        case SDL_SCANCODE_F14: key = PincKeyboardKey_f14; break;
        case SDL_SCANCODE_F15: key = PincKeyboardKey_f15; break;
        case SDL_SCANCODE_F16: key = PincKeyboardKey_f16; break;
        case SDL_SCANCODE_F17: key = PincKeyboardKey_f17; break;
        case SDL_SCANCODE_F18: key = PincKeyboardKey_f18; break;
        case SDL_SCANCODE_F19: key = PincKeyboardKey_f19; break;
        case SDL_SCANCODE_F20: key = PincKeyboardKey_f20; break;
        case SDL_SCANCODE_F21: key = PincKeyboardKey_f21; break;
        case SDL_SCANCODE_F22: key = PincKeyboardKey_f22; break;
        case SDL_SCANCODE_F23: key = PincKeyboardKey_f23; break;
        case SDL_SCANCODE_F24: key = PincKeyboardKey_f24; break;
        case SDL_SCANCODE_EXECUTE: key = PincKeyboardKey_unknown; break; // TODO(bluesillybeard): what is this
        case SDL_SCANCODE_HELP: key = PincKeyboardKey_unknown; break; // TODO(bluesillybeard): what is this
        case SDL_SCANCODE_MENU: key = PincKeyboardKey_menu; break;
        case SDL_SCANCODE_SELECT: key = PincKeyboardKey_unknown; break; // TODO(bluesillybeard): what is this
        case SDL_SCANCODE_STOP: key = PincKeyboardKey_unknown; break; // TODO(bluesillybeard): what is this
        case SDL_SCANCODE_AGAIN: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_UNDO: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_CUT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_COPY: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_PASTE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_FIND: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_MUTE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_VOLUMEUP: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_VOLUMEDOWN: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_COMMA: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_EQUALSAS400: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL1: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL2: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL3: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL4: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL5: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL6: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL7: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL8: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_INTERNATIONAL9: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG1: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG2: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG3: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG4: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG5: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG6: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG7: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG8: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LANG9: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_ALTERASE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_SYSREQ: key = PincKeyboardKey_printScreen; break;  // TODO(bluesillybeard): is this really the same as print screen?
        case SDL_SCANCODE_CANCEL: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_CLEAR: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_PRIOR: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_RETURN2: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_SEPARATOR: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_OUT: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_OPER: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_CLEARAGAIN: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_CRSEL: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_EXSEL: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_KP_00: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_KP_000: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_THOUSANDSSEPARATOR: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_DECIMALSEPARATOR: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_CURRENCYUNIT: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_CURRENCYSUBUNIT: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_KP_LEFTPAREN: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_KP_RIGHTPAREN: key = PincKeyboardKey_unknown; break;  // what
        case SDL_SCANCODE_KP_LEFTBRACE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_RIGHTBRACE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_TAB: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_BACKSPACE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_A: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_B: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_C: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_D: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_E: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_F: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_XOR: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_POWER: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_PERCENT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_LESS: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_GREATER: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_AMPERSAND: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_DBLAMPERSAND: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_VERTICALBAR: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_DBLVERTICALBAR: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_COLON: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_HASH: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_SPACE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_AT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_EXCLAM: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMSTORE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMRECALL: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMCLEAR: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMADD: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMSUBTRACT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMMULTIPLY: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_MEMDIVIDE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_PLUSMINUS: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_CLEAR: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_CLEARENTRY: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_BINARY: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_OCTAL: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_DECIMAL: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KP_HEXADECIMAL: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_LCTRL: key = PincKeyboardKey_leftControl; break;
        case SDL_SCANCODE_LSHIFT: key = PincKeyboardKey_leftShift; break;
        case SDL_SCANCODE_LALT: key = PincKeyboardKey_leftAlt; break;
        case SDL_SCANCODE_LGUI: key = PincKeyboardKey_leftSuper; break; // TODO(bluesillybeard): is this right?
        case SDL_SCANCODE_RCTRL: key = PincKeyboardKey_rightControl; break;
        case SDL_SCANCODE_RSHIFT: key = PincKeyboardKey_rightShift; break;
        case SDL_SCANCODE_RALT: key = PincKeyboardKey_rightAlt; break;
        case SDL_SCANCODE_RGUI: key = PincKeyboardKey_rightSuper; break; // TODO(bluesillybeard): is this right?
        case SDL_SCANCODE_MODE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIONEXT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIOPREV: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIOSTOP: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIOPLAY: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIOMUTE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_MEDIASELECT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_WWW: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_MAIL: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_CALCULATOR: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_COMPUTER: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_SEARCH: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_HOME: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_BACK: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_FORWARD: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_STOP: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_REFRESH: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AC_BOOKMARKS: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_BRIGHTNESSDOWN: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_BRIGHTNESSUP: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_DISPLAYSWITCH: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KBDILLUMTOGGLE: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KBDILLUMDOWN: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_KBDILLUMUP: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_EJECT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_SLEEP: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_APP1: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_APP2: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIOREWIND: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_AUDIOFASTFORWARD: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_SOFTLEFT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_SOFTRIGHT: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_CALL: key = PincKeyboardKey_unknown; break; // what
        case SDL_SCANCODE_ENDCALL: key = PincKeyboardKey_unknown; break; // what
        default: PincAssertExternal(false, "Received invalid keyboard scancode", true, ;); key = PincKeyboardKey_unknown; break; // Maybe better as a simple log in case SDL2 adds new scancodes. With that said, SDL2 is ABI stable so this really shouldn't ever trigger.
    }
    // NOLINTEND(bugprone-branch-clone)
    return key;
}

// declare the sdl2 window functions

#define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames, defaultReturn) type pincSdl2##name arguments;
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
    #define PINC_WINDOW_INTERFACE_FUNCTION(type, arguments, name, argumentsNames, defaultReturn) obj->vt.name = pincSdl2##name;
    #define PINC_WINDOW_INTERFACE_PROCEDURE(arguments, name, argumentsNames) obj->vt.name = pincSdl2##name;

    PINC_WINDOW_INTERFACE

    #undef PINC_WINDOW_INTERFACE_FUNCTION
    #undef PINC_WINDOW_INTERFACE_PROCEDURE
    return true;
}

// Clang, GCC, and MSVC optimize this into the most efficient form, when optimization flags are passed.
static uint32_t bitCount32(uint32_t value) {
    uint32_t counter = 0;
    while(value) {
        counter++;
        value &= (value - 1);
    }
    return counter;
}

static PincSdl2Window* pincSdl2GetDummyWindow(struct WindowBackend* obj) {
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
    PincAssertAssert(this->dummyWindow, "SDL2 Backend: Could not create dummy window", false, return 0;);
    return this->dummyWindow;
}

// Quick function for convenience
static void pincSdl2FramebufferFormatAdd(FramebufferFormat** formats, size_t* formatsNum, size_t* formatsCapacity, FramebufferFormat const* fmt) {
    for(size_t formatid=0; formatid<(*formatsNum); formatid++) {
        FramebufferFormat format = (*formats)[formatid];
        if(format.color_space != fmt->color_space) {
            continue;
        }
        if(format.channels != fmt->channels) {
            continue;
        }
        if(format.channel_bits[0] != fmt->channel_bits[0]) {
            continue;
        }
        if(format.channels >= 2 && format.channel_bits[1] != fmt->channel_bits[1]) {
            continue;
        }
        if(format.channels >= 3 && format.channel_bits[2] != fmt->channel_bits[2]) {
            continue;
        }
        if(format.channels >= 4 && format.channel_bits[3] != fmt->channel_bits[3]) {
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
    // TODO(bluesillybeard): A proper set data structure for deduplication is a good idea
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
        *outNumFormats = 0;
        PincAssertExternalStr(numDisplays > 0, err, false, return 0;);
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
            PincAssertExternalStr(numDisplays > 0, err, false, return 0;);
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
            // TODO(bluesillybeard): properly figure out transparent window support
            // There is absolutely no reason for assuming sRGB, other than SDL doesn't let us get what the real color space is.
            // sRGB is a fairly safe bet, and even if it's wrong, 99% of the time if it's not Srgb it's another similar perceptual color space.
            // There is a rare chance that it's a linear color space, but given SDL2's supported platforms, I find that incredibly unlikely.
            bufferFormat.color_space = PincColorSpace_srgb;
            // TODO(bluesillybeard): is this right? I think so
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
            pincSdl2FramebufferFormatAdd(&formats, &formatsNum, &formatsCapacity, &bufferFormat);
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
    switch (api) { //NOLINT: this switch statement will have more cases added to it over time
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

PincErrorCode pincSdl2completeInit(struct WindowBackend* obj, PincGraphicsApi graphicsBackend, FramebufferFormat framebuffer) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    this->libsdl2.startTextInput();
    P_UNUSED(framebuffer);
    switch (graphicsBackend) //NOLINT: more cases will be added over time
    {
        case PincGraphicsApi_opengl:
            break;
        
        default:
            // We don't support this graphics api.
            // Technically this code should never run, because the user API frontend should have caught this
            PincAssertUser(false, "Attempt to use SDL2 backend with an unsupported graphics api", true, return PincErrorCode_user;);
            return PincErrorCode_assert;
    }
    return PincErrorCode_pass;
}

void pincSdl2deinit(struct WindowBackend* obj) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    // Make sure the frontend deleted all of the windows already
    PincAssertAssert(this->windowsNum == 0, "Internal pinc error: the frontend didn't delete the windows before calling backend deinit", false, return;);
    
    this->libsdl2.destroyWindow(this->dummyWindow->sdlWindow);
    pincAllocator_free(rootAllocator, this->dummyWindow, sizeof(PincSdl2Window));

    this->libsdl2.quit();
    pincSdl2UnloadLib(this->sdl2Lib);
    pincAllocator_free(rootAllocator, this->windows, sizeof(PincSdl2Window*) * this->windowsCapacity);
    pincAllocator_free(rootAllocator, this, sizeof(PincSdl2WindowBackend));
}

void pincSdl2step(struct WindowBackend* obj) { //NOLINT: TODO: Fix this abominably massive function. I'm still undecided on the best way to do this.
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;

    // The offset between SDL2's getTicks() and platform's pCurrentTimeMillis()
    // so that getTicks + timeOffset == pCurrentTimeMillis() (with some margin of error)
    int64_t timeOffset = pincCurrentTimeMillis() - ((int64_t)this->libsdl2.getTicks64());

    SDL_Event event;
    while(this->libsdl2.pollEvent(&event)) {
        int64_t timestamp = (int64_t)event.common.timestamp + timeOffset;
        switch (event.type) {
            case SDL_WINDOWEVENT: {
                SDL_Window* sdlWin = this->libsdl2.getWindowFromId(event.window.windowID);
                // External -> caused by SDL2 giving us events for nonexistent windows
                PincAssertExternal(sdlWin, "SDL2 window from WindowEvent is NULL!", true, return;);
                PincSdl2Window* windowObj = (PincSdl2Window*)this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
                // Assert -> caused by Pinc not setting the window event data (supposedly)
                PincAssertAssert(windowObj, "Pinc SDL2 window object from WindowEvent is NULL!", false, return;);
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:{
                        PincEventCloseSignal(timestamp, windowObj->frontHandle);
                        break;
                    }
                    // case SDL_WINDOWEVENT_RESIZED: // This is the version for only external events, where the other one does both
                    // TODO(bluesillybeard): which one makes more sense to do? Pinc still needs to have the edge-case semantics of event triggers determined.
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        // Gotta love using variables named "data1" and "data2" with inappropriate signedness.
                        PincAssertAssert(event.window.data1 > 0, "Integer underflow", true, return;);
                        PincAssertAssert(event.window.data2 > 0, "Integer underflow", true, return;);
                        PincEventResize(timestamp, windowObj->frontHandle, windowObj->width, windowObj->height, (uint32_t)event.window.data1, (uint32_t)event.window.data2);
                        windowObj->width = (uint32_t)event.window.data1;
                        windowObj->height = (uint32_t)event.window.data2;
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_GAINED: {
                        PincEventFocus(timestamp, windowObj->frontHandle);
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_LOST: {
                        PincEventFocus(timestamp, 0);
                        break;
                    }
                    case SDL_WINDOWEVENT_EXPOSED: {
                        // SDL only understands exposure events in terms of redrawing the entire window
                        PincEventExposure(timestamp, windowObj->frontHandle, 0, 0, windowObj->width, windowObj->height);
                        break;
                    }
                    case SDL_WINDOWEVENT_ENTER: {
                        // TODO(bluesillybeard): it seems we need to keep track of the cursor position in order to fill out this event
                        // SDL doesn't reliably place cursor move events before the enter / leave event (in fact, it seems to reliable do the exact opposite)
                        // So some additional event processing may be required here....
                        // With that said, it may be worth entirely switching to the enter/leave paradigm as it seems more generic, if less immediately useful.
                        PincEventCursorTransition(timestamp, 0, 0, 0, windowObj->frontHandle, 0, 0);
                        break;
                    }
                    case SDL_WINDOWEVENT_LEAVE: {
                        // TODO(bluesillybeard): it seems we need to keep track of the cursor position in order to fill out this event
                        PincEventCursorTransition(timestamp, windowObj->frontHandle, 0, 0, 0, 0, 0);
                    }
                    default:{
                        // TODO(bluesillybeard): once all window events are handled, assert.
                        break;
                    }
                }
                break;
            }
            case SDL_MOUSEBUTTONUP: 
            case SDL_MOUSEBUTTONDOWN: {
                // TODO(bluesillybeard): We only care about mouse 0?
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
                    uint32_t buttonBit = 0;
                    switch (event.button.button)
                    {
                        case 1: buttonBit = 0; break;
                        case 2: buttonBit = 2; break;
                        case 3: buttonBit = 1; break;
                        case 4: buttonBit = 3; break;
                        case 5: buttonBit = 4; break;
                        // There are certainly more buttons, but for now I don't think they really exist.
                        default: PincAssertAssert(false, "Invalid button index!", true, return;);
                    }
                    uint32_t buttonBitMask = (uint32_t)1<<buttonBit;
                    // event.button.state is 1 when pressed, 0 when released. SDL2 is ABI stable, so this is safe.
                    PincAssertAssert(event.button.state < 2, "It appears SDL2's ABI has changed. The universe as we know it is broken!", false, return;);
                    // Cast here because for some reason the compiler thinks one of these is signed
                    uint32_t newState = (this->mouseState & ~buttonBitMask) | (((uint32_t)event.button.state)<<buttonBit);
                    PincEventMouseButton(timestamp, this->mouseState, newState);
                    this->mouseState = newState;
                }
                break;
            }
            case SDL_MOUSEMOTION: {
                SDL_Window* sdlWin = this->libsdl2.getWindowFromId(event.window.windowID);
                // External -> caused by SDL2 giving us events for nonexistent windows
                PincAssertExternal(sdlWin, "SDL2 window from WindowEvent is NULL!", true, return;);
                PincSdl2Window* windowObj = (PincSdl2Window*)this->libsdl2.getWindowData(sdlWin, "pincSdl2Window");
                // Assert -> caused by Pinc not setting the window event data (supposedly)
                PincAssertAssert(windowObj, "Pinc SDL2 window object from WindowEvent is NULL!", false, return;);
                // TODO(bluesillybeard): make sure the window that has the cursor is actually the window that SDL2 gave us
                int32_t motion_x = event.motion.x;
                int32_t motion_y = event.motion.y;
                int32_t oldMotionX = event.motion.x - event.motion.xrel;
                int32_t oldMotionY = event.motion.y - event.motion.yrel;
                // SDL (On X11 anyways) reports cursor coordinates beyond the window.
                // This happens when a mouse button is held down (so the window keeps cursor focus) and is moved outside the window.
                // From my understanding, this is technically valid but rather strange behavior.
                // If someone actually needs this to behave the exact same as SDL2, they can make an issue about it.
                if(motion_x < 0) { motion_x = 0; }
                if(motion_y < 0) { motion_y = 0; }
                if(oldMotionX < 0) { oldMotionX = 0; }
                if(oldMotionY < 0) { oldMotionY = 0; }
                if((uint32_t)motion_x > windowObj->width) { motion_x = (int32_t)windowObj->width; }
                if((uint32_t)motion_y > windowObj->height) { motion_y = (int32_t)windowObj->height; }
                if((uint32_t)oldMotionX > windowObj->width) { oldMotionX = (int32_t)windowObj->width; }
                if((uint32_t)oldMotionY > windowObj->height) { oldMotionY = (int32_t)windowObj->height; }
                
                // TODO(bluesillybeard): is it even worth handling the case where the window's width is greater than the maximum value of int32_t? Because it seems every OS / desktop environment / compositor breaks way before that anyways
                PincEventCursorMove(timestamp, windowObj->frontHandle, (uint32_t)oldMotionX, (uint32_t)oldMotionY, (uint32_t)motion_x, (uint32_t)motion_y);
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
                PincEventScroll(timestamp, yMovement, xMovement);
                break;
            }
            case SDL_CLIPBOARDUPDATE: {
                // There is some weirdness with this - in particular, duplicates tend to get sent often.
                // When VSCode is open, as much as selecting some text will cause it to spam this function with the current clipboard
                // Or more accurately, system events are rather chaotic so often things get doubled along the way
                // It's an absolute non-issue, but something worth noting here.
                if(this->libsdl2.hasClipboardText()) {
                    char* clipboardText = this->libsdl2.getClipboardText();
                    PincAssertExternal(clipboardText, "SDL2 clipboard is NULL", true, return;);
                    if(!clipboardText) { break; }
                    size_t clipboardTextLen = pincStringLen(clipboardText);
                    char* clipboardTextCopy = pincAllocator_allocate(tempAllocator, clipboardTextLen + 1);
                    pincMemCopy(clipboardText, clipboardTextCopy, clipboardTextLen);
                    clipboardTextCopy[clipboardTextLen] = 0;
                    PincEventClipboardChanged(timestamp, PincMediaType_text, clipboardTextCopy, clipboardTextLen);
                }
                break;
            }
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                PincKeyboardKey key = pincSdl2ConvertSdlKeycode(event.key.keysym.scancode);
                PincEventKeyboardButton(timestamp, key, event.key.state == SDL_PRESSED, event.key.repeat != 0);
                break;
            }
            case SDL_TEXTINPUT: {
                uint32_t buffer[32];
                size_t text_size = pincStringLen(event.text.text);
                PincAssertAssert(text_size < 32, "32 byte buffer produced >32 bytes", false, return; );
                size_t num_codepoints = pincDecodeUTF8String(event.text.text, text_size, buffer, 32);
                PincAssertAssert(num_codepoints < 32, "32 bytes of utf8 produced >32 unicode codepoints", false, return; );
                for(size_t index = 0; index < num_codepoints; ++index) {
                    // TODO(bluesillybeard) text input should account for which window was typed into?
                    // Or will it always be the focused window under all circumstances?
                    // I can imagine automation macros typing into windows that aren't focused.
                    PincEventTextInput(timestamp, buffer[index]);
                }
                break;
            }
            // TODO(bluesillybeard): text edit event
            // TODO(bluesillybeard): text edit extended event
            // It apparently has a selection cursor...? What is that about?
            default:{
                // TODO(bluesillybeard): Once all SDL events are handled, assert.
                break;
            }
        }
    }
}

WindowHandle pincSdl2completeWindow(struct WindowBackend* obj, IncompleteWindow const * incomplete, PincWindowHandle frontHandle) { //NOLINT: TODO: this function is a mess, rewrite it to be better
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
        windowFlags |= (uint32_t)SDL_WINDOW_RESIZABLE;
    }
    if(incomplete->minimized) {
        windowFlags |= (uint32_t)SDL_WINDOW_MINIMIZED;
    }
    if(incomplete->maximized) {
        windowFlags |= (uint32_t)SDL_WINDOW_MAXIMIZED;
    }
    if(incomplete->fullscreen) {
        // TODO(bluesillybeard): do we want to use fullscreen or fullscreen_desktop?
        // Currently, we use "real" fullscreen,
        // But it may be a good option to "fake" fullscreen via fullscreen_desktop.
        // Really, we should add an option so the user gets to decide.
        windowFlags |= (uint32_t)SDL_WINDOW_FULLSCREEN;
    }
    if(incomplete->focused) {
        // TODO(bluesillybeard): Does keyboard grabbed just steal keyboard input entirely, or does it allow the user to move to other windows normally?
        // What even is keyboard grabbed? the SDL2 documentation is annoyingly not specific.
        windowFlags |= (uint32_t)SDL_WINDOW_INPUT_FOCUS; // | SDL_WINDOW_KEYBOARD_GRABBED;
    }
    if(incomplete->hidden) {
        windowFlags |= (uint32_t)SDL_WINDOW_HIDDEN;
    }
    // Only graphics api is OpenGL, shortcuts are taken
    windowFlags |= (uint32_t)SDL_WINDOW_OPENGL;

    if(!this->dummyWindowInUse && this->dummyWindow) {
        PincSdl2Window* dummyWindow = this->dummyWindow;
        uint32_t realFlags = this->libsdl2.getWindowFlags(dummyWindow->sdlWindow);

        // If we need opengl but the dummy window doesn't have it,
        // Then, as long as Pinc doesn't start supporting a different graphics api for each window,
        // the dummy window is fully useless and it should be replaced.
        // SDL does not support changing a window to have opengl after it was created without it.
        // TODO(bluesillybeard) clang-tidy doesn't complain about this, but this if statement is hard to read
        // In fact, a lof of these if statements are difficult to parse
        if((windowFlags&(uint32_t)SDL_WINDOW_OPENGL) && !(realFlags&(uint32_t)SDL_WINDOW_OPENGL)) {
            this->libsdl2.destroyWindow(dummyWindow->sdlWindow);
            pincAllocator_free(rootAllocator, dummyWindow, sizeof(PincSdl2Window));
            goto SDL_MAKE_NEW_WINDOW;
        }

        // All of the other flags can be changed
        if((windowFlags&(uint32_t)SDL_WINDOW_RESIZABLE) != (realFlags&(uint32_t)SDL_WINDOW_RESIZABLE)) {
            pincSdl2setWindowResizable(obj, dummyWindow, (windowFlags&(uint32_t)SDL_WINDOW_RESIZABLE) != 0);
        }
        if((windowFlags&(uint32_t)SDL_WINDOW_MINIMIZED) != (realFlags&(uint32_t)SDL_WINDOW_MINIMIZED)) {
            pincSdl2setWindowMinimized(obj, dummyWindow, (windowFlags&(uint32_t)SDL_WINDOW_MINIMIZED) != 0);
        }
        if((windowFlags&(uint32_t)SDL_WINDOW_MAXIMIZED) != (realFlags&(uint32_t)SDL_WINDOW_MAXIMIZED)) {
            pincSdl2setWindowMaximized(obj, dummyWindow, (windowFlags&(uint32_t)SDL_WINDOW_MAXIMIZED) != 0);
        }
        if((windowFlags&(uint32_t)SDL_WINDOW_FULLSCREEN) != (realFlags&(uint32_t)SDL_WINDOW_FULLSCREEN)) {
            pincSdl2setWindowFullscreen(obj, dummyWindow, (windowFlags&(uint32_t)SDL_WINDOW_FULLSCREEN) != 0);
        }
        if((windowFlags&(uint32_t)SDL_WINDOW_INPUT_FOCUS) != (realFlags&(uint32_t)SDL_WINDOW_INPUT_FOCUS)) {
            pincSdl2setWindowFocused(obj, dummyWindow, (windowFlags&(uint32_t)SDL_WINDOW_INPUT_FOCUS) != 0);
        }
        if((windowFlags&(uint32_t)SDL_WINDOW_HIDDEN) != (realFlags&(uint32_t)SDL_WINDOW_HIDDEN)) {
            pincSdl2setWindowHidden(obj, dummyWindow, (windowFlags&(uint32_t)SDL_WINDOW_HIDDEN) != 0);
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
        SDL_Window* win = this->libsdl2.createWindow(titleNullTerm, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)realWidth, (int)realHeight, windowFlags); //NOLINT: we do not have control over SDL macros
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
    bool dummyWindowActuallyInUse = false;
    for(size_t i=0; i<this->windowsNum; ++i) {
        PincSdl2Window* windowToCheck = this->windows[i];
        if(windowToCheck == this->dummyWindow) {
            dummyWindowActuallyInUse = true;
        }
    }
    PErrorValidate(dummyWindowActuallyInUse == this->dummyWindowInUse, "Dummy window in use does not match reality");
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
    PincAssertAssert(window->width < INT32_MAX, "Integer Overflow", false, return;);
    PincAssertAssert(window->height < INT32_MAX, "Integer Overflow", false, return;);
    // TODO(bluesillybeard): what about the hacky HiDPI / scaling support in SDL2?
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
    PincAssertAssert(width <= INT32_MAX && width > 0, "Integer overflow", false, return 0;);
    PincAssertAssert((uint32_t)width == windowObj->width, "Window width and \"real\" width do not match!", false, return 0;);
    return (uint32_t)width;
}

void pincSdl2setWindowHeight(struct WindowBackend* obj, WindowHandle windowHandle, uint32_t height) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* window = (PincSdl2Window*)windowHandle;
    window->height = height;
    PincAssertAssert(window->width < INT32_MAX, "Integer Overflow", false, return;);
    PincAssertAssert(window->height < INT32_MAX, "Integer Overflow", false, return;);
    // TODO(bluesillybeard): what about the hacky HiDPI / scaling support in SDL2?
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
        // TODO(bluesillybeard): only graphics api is OpenGl, shortcuts are made
        this->libsdl2.glGetDrawableSize(windowObj->sdlWindow, NULL, &height);
    } else {
        // If the previously tried functions don't work, then it means this is a version of SDL from before it became hidpi aware.
        // There's not a lot we can do. If I'm not mistaken, non-dpi aware applications (in windows) are just fed incorrect window size values,
        // which can cause all kinds of issues.
        // Just assume the window size is equal to pixels. It's unlikely anyone is using an SDL2 version this old anyway.
        this->libsdl2.getWindowSize(windowObj->sdlWindow, NULL, &height);
    }
    PincAssertAssert(height <= INT32_MAX && height > 0, "Integer overflow", false, return 0;);
    return (uint32_t)height;
}

float pincSdl2getWindowScaleFactor(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return 0;
}

void pincSdl2setWindowResizable(struct WindowBackend* obj, WindowHandle window, bool resizable) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(resizable);
    // TODO(bluesillybeard): 
}

bool pincSdl2getWindowResizable(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return false;
}

void pincSdl2setWindowMinimized(struct WindowBackend* obj, WindowHandle window, bool minimized) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(minimized);
    // TODO(bluesillybeard): 
}

bool pincSdl2getWindowMinimized(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return false;
}

void pincSdl2setWindowMaximized(struct WindowBackend* obj, WindowHandle window, bool maximized) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(maximized);
    // TODO(bluesillybeard): 
}

bool pincSdl2getWindowMaximized(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return false;
}

void pincSdl2setWindowFullscreen(struct WindowBackend* obj, WindowHandle window, bool fullscreen) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(fullscreen);
    // TODO(bluesillybeard): 
}

bool pincSdl2getWindowFullscreen(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return false;
}

void pincSdl2setWindowFocused(struct WindowBackend* obj, WindowHandle window, bool focused) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(focused);
    // TODO(bluesillybeard): 
}

bool pincSdl2getWindowFocused(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return false;
}

void pincSdl2setWindowHidden(struct WindowBackend* obj, WindowHandle window, bool hidden) {
    P_UNUSED(obj);
    P_UNUSED(window);
    P_UNUSED(hidden);
    // TODO(bluesillybeard): 
}

bool pincSdl2getWindowHidden(struct WindowBackend* obj, WindowHandle window) {
    P_UNUSED(obj);
    P_UNUSED(window);
    // TODO(bluesillybeard): 
    return false;
}

PincErrorCode pincSdl2setVsync(struct WindowBackend* obj, bool vsync) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    // TODO(bluesillybeard): only graphics backend is OpenGL, shortcuts are taken
    if(vsync) {
        if(this->libsdl2.glSetSwapInterval(-1) == -1) {
            // Try again with non-adaptive vsync
            if(this->libsdl2.glSetSwapInterval(1) == -1) {
                // big sad
                return PincErrorCode_assert;
            }
        }
    } else {
        if(this->libsdl2.glSetSwapInterval(0) == -1) {
            return PincErrorCode_assert;
        }
    }
    return PincErrorCode_pass;
}

bool pincSdl2getVsync(struct WindowBackend* obj) {
    // TODO(bluesillybeard): only graphics backend is OpenGL, shortcuts are taken
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    return this->libsdl2.glGetSwapInterval() != 0;
}

void pincSdl2windowPresentFramebuffer(struct WindowBackend* obj, WindowHandle window) {
    PincSdl2Window* windowObj = (PincSdl2Window*)window;
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    // TODO(bluesillybeard): only graphics backend is OpenGL, shortcuts are taken
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
    // TODO(bluesillybeard): is this actually valid?
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
            PincAssertAssert(false, "Invalid number of channels in framebuffer format", false, return 0;);
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
    // TODO(bluesillybeard): As far as I can tell, MULTISAMPLEBUFFERS is only 1 or 0. Nobody explains a use case with more than 1.
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
            PincAssertUser(false, "SDL2 does not support creating a legacy context", true, return 0;);
            return 0;
        case PincOpenglContextProfile_compatibility:
            this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            break;
        case PincOpenglContextProfile_core:
            this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            break;
        case PincOpenglContextProfile_forward:
            this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            glFlags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG; //NOLINT: SDL wants an int
            break;
    }
    if(incompleteContext.robustAccess) {
        glFlags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG; //NOLINT: SDL wants an int
    }
    if(incompleteContext.debug) {
        glFlags |= SDL_GL_CONTEXT_DEBUG_FLAG; //NOLINT: SDL wants an int
    }
    this->libsdl2.glSetAttribute(SDL_GL_CONTEXT_FLAGS, glFlags);
    int share = 0;
    if(incompleteContext.shareWithCurrent) {
        share = 1;
    }
    this->libsdl2.glSetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, share);

    // TODO(bluesillybeard): SDL says it needs the attributes set before creating the window,
    // But is that actually true beyond just the framebuffer format?
    PincSdl2Window* dummyWindow = pincSdl2GetDummyWindow(obj);
    SDL_GLContext sdlGlContext = this->libsdl2.glCreateContext(dummyWindow->sdlWindow);
    if(!sdlGlContext) {
        #if PINC_ENABLE_ERROR_EXTERNAL
        pincString errorMsg = pincString_concat(2, (pincString[]){
            pincString_makeDirect((char*)"SDL2 backend: Could not create OpenGl context: "),
            // const is not an issue, this string will not be modified
            pincString_makeDirect((char*)this->libsdl2.getError()),
        },tempAllocator);
        PincAssertExternalStr(false, errorMsg, true, return 0;);// Probably recoverable? Look into it.
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
    // TODO (bluesillybeard): 
    return 0;
}

uint32_t pincSdl2glGetContextAlphaBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t pincSdl2glGetContextDepthBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t pincSdl2glGetContextStencilBits(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

uint32_t pincSdl2glGetContextSamples(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return 0;
}

bool pincSdl2glGetContextStereoBuffer(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool pincSdl2glGetContextDebug(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool pincSdl2glGetContextRobustAccess(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}

bool pincSdl2glGetContextResetIsolation(struct WindowBackend* obj, RawOpenglContextObject context) {
    // TODO (bluesillybeard): 
    P_UNUSED(obj);
    P_UNUSED(context);
    return false;
}


PincErrorCode pincSdl2glMakeCurrent(struct WindowBackend* obj, WindowHandle window, RawOpenglContextHandle context) {
    PincSdl2WindowBackend* this = (PincSdl2WindowBackend*)obj->obj;
    PincSdl2Window* windowObj = (PincSdl2Window*)window;
    // Window may be null to indicate any window.
    // SDL2 does not state it needs a window, but it also does not state that no window is an option
    // So, in the event of a null window, get the dummy window
    if(!windowObj) {
        windowObj = pincSdl2GetDummyWindow(obj);
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
        PincAssertExternalStr(false, errorMsg, true, return PincErrorCode_assert;);
        pincString_free(&errorMsg, tempAllocator);
        #endif
        return PincErrorCode_assert;
    }
    return PincErrorCode_pass;
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
    // TODO(bluesillybeard): I'm not too confident about this, it should be tested properly
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
    PincAssertUser(this->libsdl2.glGetCurrentContext(), "Cannot get proc address of an OpenGL function without a current context", true, return 0;);
    return this->libsdl2.glGetProcAddress(procname);
}

#endif
