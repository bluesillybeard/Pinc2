# Pinc 2

This is the second iteration of a project called Pinc.

Pinc aims to build a new-age window / graphics toolkit for game and application development, with a focus on cross-compilation, dependency minimalization, and ease of integration.

Pinc is written entirely in C, making it (theoretically) portable to any system with a C compiler. Pinc currently uses cmake, however its design hopefulyl makes it simple to port to other build systems. See [build.md](build.md) for how the build system is configured.

## Pinc goals

- Language agnostic - the API should avoid things that are dificult to port to other languages
    - The header is written in plain C, so any language with C FFI can be used
    - The external API only uses `int`, `char`, `void`, and basic pointers to arrays (`char*` and `len` for strings, `int*` and `len` for element arrays, etc)

- ZERO compile-time dependencies other than a functional C compiler / linker
    - Required headers are included in this repository. Their licence is marked at the top.
    - The only exception is the windows.h header, as it appears to be copyrighted. In the future, a windows.h replacement may be used instead.
    - all required shared libraries are loaded at runtime
    - libc is still required on unix systems (linux, macos, bsd, etc) because doing unix development without libc is literal hell

- Easy to use
    - comparable to something like SDK or SFML
    - Admittedly the API is a bit verbose

- Flexible
    - Bring your own build system (we just use cmake for convenience)
    - determines the API to use at runtime, reducing the number of required compile targets
        - Note: we do not yet support cosmopilitan C, although it is a potential consideration

## Other things
- Pinc does not take hold of the entry point
- Pinc does not provide a main loop

## Implemented windowing backends
- SDL2

## Implemented graphics backends
- OpenGL 2.1

## Important notes

Pinc is a very new library, and is MASSIVELY out of scope for a single developer like myself. As such:

- Expect bugs / issues
    - Many functions may not be implemented
- the API is highly variable for the forseeable future
    - Give me your suggestions - the API is VERY incomplete and we don't know what's missing!
- the project desparately needs contributors - see contribution guide

Pinc's current API is fundamentally incompatible with multithreading. Sorry.

## How to get started
- Get familiar with cmake a bit
    - Follow a tutorial if you do not know C or cmake
    - If you want to use another programming language than C, you're on your own for now
- Compile the project with cmake
    - We reccomend Clang + cmake since that is the system we use for testing and development
    - use the main branch, it's tested regularly. We don't have CI set up yet though.
- test it by running one of the examples (see build system documentation)
- add it to your project
    - Again, see build system documentation

I suggest reading through the header a bit as well as looking at the examples to get a feel for the API. 

## Q&A
- Why make this when other libraries already exist?
    - for fun
    - the state of low-level windowing / graphics libraries is not ideal. Kinc's build system is a mess, Raylib is too basic, V-EZ hasn't been updated in many years, bgfx is written in C++, SDL doesn't cross-compile particularly easily, nicegraf and llgl don't provide a way to create a window, GLFW has no way to have multiple windows on a single OpenGL context, Jai is a programming language instead of a library, and the list goes on and on and on. They are all great, but they all suck in specific ways that are conveniently very bad for a certain group of programmers
    - Additionally, a library with an insanely wide net of supported backends is very useful. Admittedly, the only backend implemented at the moment is based on SDL2, but take a look at the [Planned Backends](#planned-window-backends-not-final),
- Why support OpenGL 2.1. It's so old! (and deprecated)
    - I thought it would be cool to be able to run this on extremely ancient hardware and OS, for no other reason than to see it run. Partially inspired by [MattKC porting .NET framework 2 to Windows 95.](https://www.youtube.com/watch?v=CTUMNtKQLl8)
    - If a platform is capible of running OpenGL 2.1, someone has probably made an opengl driver for it

## Planned graphics backends (NOT FINAL)
- Raw / framebuffer on the CPU / software rasterizer
- SDL 1
    - Is this even worth implementing despite the raw and opengl backends?
- SDL 2
    - Is this even worth implementing despite the raw and opengl backends?
- SDL 3
    - Is this even worth implementing despite the raw and opengl backends?
- OpenGL 1.x
    - not sure which 1.x version(s) yet
- OpenGL 3.x
    - not sure which 3.x verison yet
- OpenGL 4.x
    - note sure which 4.x version(s) yet
- OpenGL 4.6 (last OpenGL release)
- Vulkan 1.0 (first vulkan release)
- Vulkan 1.2 (last Vulkan release that is very widely supported on older hardware)
- Vulkan 1.3 (last Vulkan release)

## Planned window backends (NOT FINAL)
- SDL 1
- SDL 3
- X11 (Xlib)
- win32
- windows 9x (TODO: figure out what the API is called for this)
- Cocoa
- Wayland
- GLFW
- Haiku
- Andriod
- IOS

## Planned backends / platforms in the VERY FAR future
None of these are going to be implemented any time soon - if ever.
- Playstation 4/5
- Nintendo DS
- Xbox
    - It's basically just Windows (I think), should be pretty easy actually
- Nintendo switch
    - There seems to be a lack of info on how this could be done.
- N64 would be funny
- Playstation would also be funny
- Microsoft DOS
    - an msdos backend doesn't even make sense, as I don't think it has a universal way to draw pixels on the screen
- terminal text output
    - haha ascii art go BRRR
- 3dfx's Glide graphics api
    - The funny thing is, there are actual Glide drivers available for modern GPUS. I believe it's an implementation that uses Vulkan to emulate the original API.

## backends that will NEVER be implemented
- Raw X11 network packets
    - are you crazy!? Also, good luck getting OpenGL or Vulkan to work.
- Xcb
    - Not worth the effort. Xlib works fine for X11.

## Missing features (LOOKING FOR CONTRIBUTORS / API DESIGN IDEAS)
- window position
    - wayland be like:
- lots of events aren't implemented yet
- ability get data from specific backends
    - X display, X windows, SDL2 opengl context, Win32 window handle, etc etc etc
- backend-specific settings
- ability to use backend objects to create Pinc objects
    - example: init pinc's X backend with an X Display, or a window with an X window handle, etc.
- general input methods
    - controller / gamepad
    - touch screen
    - drawing tablet
    - VR headsets
        - every headset seems to have its own position / velocity system...
- HDR support
    - Admittedly, HDR support is still in the early stages on anything other than Windows...
- Ability to get system theme colors and name
    - Probably pretty easy on Windows
    - Does MacOS even have themes?
    - good luck doing this on Linux lol
        - GNOME
        - KDE Plasma
        - Cosmic
        - XFCE
        - AwesomeWM
        - Sway
        - Cinnamon
        - Budgeee
        - Mate
        - ... and a billion more, although most of the current smaller ones will likely die along with X11
        - could probably just read the GTK and QT system themes and get 99% coverage
        - I think recently a portal and standard for cross-toolkit theming was created
- Audio support
    - audio playback to a default or specific device
    - software audio, or something like OpenAL which supports hardware acceleration
    - directly output samples
    - audio recording
    - arbitrary inputs and outputs for something like DAW software
        - ability to check the system's ability to handle multiple inputs / multiple outputs
        - notify the application when the user edits the audio output through system settings or something like qpwgraph
    - native implementations for OpenAL, ALSA, pipewire, and whatever else?
        - SDL backend probably

## TODO
- test on different compilers
    - Clang
    - GCC
    - MSVC, as much as their C compiler sucks
    - Cosmopolitan (Not yet though)
    - emscripten (once web support is added)
