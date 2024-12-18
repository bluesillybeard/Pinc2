# Pinc 2

This is the second iteration of a project called Pinc.

Pinc aims to build a new-age window / graphics toolkit for game and application development, with a focus on cross-compilation, dependency minimization, and ease of integration.

Pinc is written entirely in C, making it (theoretically) portable to any system with a C compiler. Pinc currently uses cmake, however its design hopefully makes it simple to port to other build systems. See [build.md](build.md) for how the build system is configured.

## Pinc goals

- Language agnostic - the API should avoid things that are difficult to port to other languages
    - The header is written in plain C, so any language with C FFI can be used
    - The external API only uses `int`, `char`, `void`, and basic pointers to arrays (`char*` and `len` for strings, `int*` and `len` for element arrays, etc)

- ZERO compile-time dependencies other than a functional C compiler / linker
    - Required headers are included in this repository. Their license is marked at the top.
    - proprietary headers are rebuilt with the minimal types and functions required (ex: windows.h).
    - all required shared libraries are loaded at runtime, unless they are guaranteed to exist as-is for the target platform without development libraries installed
        - example: the windows API can be linked normally, although it remains to be seen if import libraries will be strictly needed.
    - libc is still required on unix systems (linux, macos, bsd, etc) because doing unix development without libc is literal hell
        - musl will be supported eventually, but for now dlopen is a required function which musl does not implement due to technical constraints.
        - Note: pinc does not need the PLT or related tables updated with the symbols from loaded objects, as it loads them into its own proc tables. So pinc support for unix without libc is absolutely possible, but would require a complete ELF loader outside of libc.

- Easy to use
    - comparable to something like SDL or SFML
    - Admittedly the API is a bit verbose

- Flexible
    - Bring your own build system (we just use cmake for convenience)
        - unity build with a single compiler command is an option
    - determines the API to use at runtime, reducing the number of required compile targets
        - Note: we do not yet support cosmopolitan C, although it is a potential consideration

## Other things
- Pinc does not take hold of the entry point
- Pinc does not provide a main loop

## This library is barely even started. Here's what's left to do:
- (DONE) figure out stdint.h and stdbool.h situation
    - stdint.h is part of C99 and is guaranteed to exist for pretty much any compiler / build system imaginable
    - Note that some niche C compilers do not implement stdint nicely due to various reasons. For all desktop and modern console platforms, this is not an issue. Pinc will not support embedded platforms (at least not any time soon). Vintage platforms are also problematic, but for now Pinc does not intend on supporting most of those.
    - VERDICT: use stdint.h and stdbool.h in spares. It can be easily refactored later anyway.
- (DONE) finish the pinc header (just the base header)
- (DONE) finish the pinc opengl header
    - the graphics api will come later!
- set up common macros
    - asserts and user error reporting
        - The Zig implementation just has "unreachable" everywhere, which is unacceptable from a UX point of view.
    - errors and calling the error callback easily
    - Are macros for zig-like error types possible?
- set up libc wrappers
    - memory allocation
    - string printing
    - unicode conversions (which libc absolutely sucks crap at anyway so yeah)
- set up interfaces for window and graphics backends
- set up other fundamental components
    - arrays and slices (will require heavy macro use but whatever)
    - build system and settings documentation
        - the build system itself must remain as minimal as possible. IE: The C code should do as much as possible to make adding new build systems simpler.
- Would it be worth setting up header injection?
    - this is where pinc includes a user-defined header as part of its compilation process, rather than having a million separate defines that need to be managed.
    - SDL does something like this actually, but only for new platforms and not on a project-per-project basis.
- implement SDL2 & raw opengl backend
    - add tests as things are implemented
- finish pinc graphics header
    - Just base it off the original one.
    - One change to make: move the texture sampling properties from the uniform definition to the pipeline definition
- implement opengl 2.1 backend
- Celebrate! we've made it back to where we left off in the original Zig version of Pinc.
    - And in fact, with some new things that the original prototype-like thing did not have

## Implemented platforms
- Posix / Unix

## Implemented windowing backends
- SDL2

## Implemented graphics backends
- OpenGL 2.1

## Implemented build systems
- cmake

## Implemented IDE setups:
- Visual Studio Code

## Tested Compilers
- GNU C Compiler (gcc)

## Important notes

Pinc is a very new library, and is MASSIVELY out of scope for a single developer like myself. As such:

- Expect bugs / issues
    - Many functions may not be implemented
- the API is highly variable for the foreseeable future
    - Give me your suggestions - the API is VERY incomplete and we don't know what's missing!
- the project desperately needs contributors - see contribution guide

Pinc's current API is fundamentally incompatible with multithreading. Sorry.

## How to get started
- Get familiar with cmake a bit
    - Follow a tutorial if you do not know C or cmake
    - If you want to use another programming language than C, you're on your own for now
- Compile the project with cmake
    - We recommend Clang + cmake since that is the system we use for testing and development
    - use the main branch, it's tested regularly. We don't have CI set up yet though.
- test it by running one of the examples (see build system documentation)
- add it to your project
    - Again, see build system documentation

I suggest reading through the header a bit as well as looking at the examples to get a feel for the API. 

## Q&A
- Why make this when other libraries already exist?
    - for fun
    - To make a low-level windowing / graphics library that can be built and used, with only the requirement being a C compiler and some time.
    - Additionally, a library with an insanely wide net of supported backends is very useful. Admittedly, the only backend implemented at the moment is based on SDL2, but take a look at the [Planned Backends](#planned-window-backends-not-final),
- Why support OpenGL 2.1. It's so old! (and deprecated)
    - I thought it would be cool to be able to run this on extremely ancient hardware and OS, for no other reason than to see it run. Partially inspired by [MattKC porting .NET framework 2 to Windows 95.](https://www.youtube.com/watch?v=CTUMNtKQLl8)
    - If a platform is capable of running OpenGL 2.1, someone has probably made an opengl driver for it
        - Even the Nintendo 64 has an OpenGL 1 implementation - it doesn't have reprogrammable shading though, so no opengl 2.

## Planned supported platforms
- Windows NT
- Windows NT without libc
- Windows 9x
- Windows 9x without libc
- Macos X
- Haiku

## Maybe planned supported platforms (if someone wants to contribute)
- Posix without libc
    - Will require some inline assembly for all architectures to make syscalls
    - Will require a custom shared object loader

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
    - not sure which 3.x version yet
- OpenGL 4.x
    - note sure which 4.x version(s) yet
- OpenGL 4.6 (last OpenGL release)
- Vulkan 1.0 (first vulkan release)
- Vulkan 1.2 (last Vulkan release that is very widely supported on older hardware)
- Vulkan 1.3 (last Vulkan release)
- Raw Vulkan

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
- Android
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
    - a Microsoft DOS backend doesn't even make sense, as I don't think it has a universal way to draw pixels on the screen.
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
        - Budgie
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
- add debug print callback
