# Pinc 2

Pinc is a cross platform window / graphics library for game and application development, with a focus on cross-compilation, dependency minimization, and ease of integration.

Pinc is written entirely in C, making it (theoretically) portable to any system with a C compiler. Pinc currently uses cmake, however its design hopefully makes it simple to port to other build systems. See [build.md](build.md) for how the build system is configured.

The original version written in Zig can be found [here](https://github.com/bluesillybeard/Pinc). Using it is an absolutely bad idea.

## Pinc goals

- Language agnostic - the API should avoid things that are difficult to port to other languages
    - The header is written in plain C, so any language with C FFI can be used
    - The external API only uses `int`, `char`, `void`, and basic pointers to arrays (`char*` and `len` for strings, `int*` and `len` for element arrays, etc)

- ZERO compile-time dependencies other than a functional C compiler / linker
    - Headers for external libraries are rebuilt and baked into the library itself
    - libraries can be linked statically, dynamically, or at runtime
        - Note: this part hasn't been implemented yet!
    - Note: certain base platform libraries are required (ex: libc on Posix / Unix, kernel32 on Windows), but those are usually built into the compiler anyway

- Easy to use
    - comparable to something like SDL or SFML
    - Admittedly the API is a bit verbose

- Flexible
    - Bring your own build system (we just use cmake for convenience)
        - unity build with a simple compiler command is an option. Note: not implemented yet!
    - determines the API to use at runtime, reducing the number of required compile targets
        - Note: we do not yet support cosmopolitan C, although it is a potential consideration

- Wide Support
    - Something along the lines of what ClassiCube supports: https://github.com/ClassiCube/ClassiCube/tree/master?tab=readme-ov-file
    - this is why we use C: you would be hard pressed to find a platform that does not support C
        - note: there may be some C++, objective-c, or Java code in this codebase. This is for BeOS/Haiku, Macos, and Android respectively, who decided to defy language-agnostic APIs in favor of using a more advanced programming language.
    - Note: The library has barely been started, currently very few platforms are actually supported.

## Other things
- Pinc does not take hold of the entry point
    - This means on certain platforms (Android for example), some shenanigans may be required. Android without Java is possible, but realistically it's best to make a JNI entry point.
- Pinc does not provide a main loop
    - For applet based platforms (ex: emscripten, WASI), this means potentially making your code either reentrant or just running it on another thread
- Pinc's API has a complete memory barrier from your application. Pinc will not give you pointers to accidentally store, and it will not store any pointers you give it. (other than some callbacks and related pointers)

## This library is barely even started. Here's what's left to do:
- (PARTIALLY DONE) set up interface for window backend
- implement SDL2 + raw OpenGl backend
    - add tests as things are implemented
- Make sure all of the important TODOs are handled
- Make the library public
    - Not yet at where the Zig version is, but the library is usable.
- finish pinc graphics header
    - Just base it off the original one.
    - Changes I want to make:
        - move the texture sampling properties from the uniform to the pipeline
        - Remove GLSL and use a custom shader definition compatible with fixed function rendering (aim for minimal features like OpenGL 1.0 or the N64)
        - Add GLSL as an optional feature that a graphics backend may or may not have
        - Add indexed framebuffer / texture (where colors are an enum instead of brightness values)
            - This may be as simple as adding a new color space and some query functions.
            - Note: Modern APIs don't support this directly (lol I wonder why), so this capability needs to be queryable
            - SDL2 supports this! I'm genuinely not quite sure why they would, seeing as they only target platforms of the last ~20 years, but still neat.
        - scissor rectangle
- set up interface for graphics backend
- implement opengl 2.1 backend
- Add platform implementations for at least windows (probably macos if it needs to be separate from posix)
- implement the rest of the examples from the original project
- Celebrate! we've made it back to where we left off in the original Zig version of Pinc.
    - And in fact, with some new things that the original prototype-like thing did not have

## Absolutely Important TODOs for before the library goes anywhere
- warning print system
- (proper) debug print system
- Make sure all functions that take pinc_window_backend or pinc_graphics_backend can take 'any' to reference the default one.

## Implemented platforms
- Posix / Unix

## Implemented window backends
- SDL2

## Implemented graphics backends
- OpenGL 2.1

## Implemented build systems
- cmake

## Implemented IDE setups:
- Visual Studio Code

## Tested Compilers
- Clang (the LLVM C compiler)
- GNU C Compiler (gcc)

## Important notes

Pinc is a very new library, and is MASSIVELY out of scope for a single developer like myself. As such:

- Expect bugs / issues
    - Many functions may not be implemented
- the API is highly variable for the foreseeable future
    - Report any suggestions - the API is VERY incomplete and we don't know what's missing!
- the project desperately needs contributors - see contribution guide
    - TODO: actually make the contribution guide

Pinc's current API is fundamentally incompatible with multithreading. It is suggested that a single thread is selected to interact with Pinc, if such multithreaded operation is desired. In the future, Pinc may support threaded operation.

## How to get started
- Get familiar with cmake a bit
    - Follow a tutorial if you do not know C or cmake
    - If you want to use another programming language than C or C++, you're on your own for now
- Compile the project with cmake
    - We recommend Clang + cmake since that is the system we use for testing and development
    - use the main branch, it's tested regularly. We don't have CI set up yet though.
- test it by running one of the examples (see build system documentation)
- add it to your project
    - Again, see build system documentation

It is a good idea to read through the header a bit as well as look at the examples to get a feel for the API. 

## Q&A
- Why make this when other libraries already exist?
    - for fun
    - To make a low-level windowing / graphics library that can be built and used, with only the requirement being a C compiler and some time.
    - Additionally, a library with an insanely wide net of supported backends is very useful. Admittedly, the only backend implemented at the moment is based on SDL2, but take a look at the [Planned Backends](#planned-window-backends-not-final),
- Why support fixed-function rendering It's so old! (and deprecated)
    - I thought it would be cool to be able to run this on extremely ancient hardware and OS, for no other reason than to see it run. Partially inspired by [MattKC porting .NET framework 2 to Windows 95.](https://www.youtube.com/watch?v=CTUMNtKQLl8)
    - Remember the wide support goal: we want this project to work on pretty much any platform that can render textured polygons
    - It's easier (for now) than trying to get the same shader code to work across backends

## Planned supported platforms
- Windows NT
- Windows NT without libc
- Windows 9x
- Windows 9x without libc
- Macos X
- Haiku
- emscripten

## Maybe planned supported platforms (if someone wants to contribute)
- Cosmopolitan C
- WASI
- IOS
- "old" macos
- Posix without libc
    - Who on earth thinks this is a good idea. This is almost like doing windows without windows.h
    - Will require some assembly for all architectures to make syscalls
    - Will require a custom shared object loader

## Planned graphics backends (NOT FINAL)
- Raw software rasterizer
- SDL 1
    - Is this even worth implementing despite the raw and opengl backends?
- SDL 2
    - Is this even worth implementing despite the raw and opengl backends?
- SDL 3
    - Is this even worth implementing despite the raw and opengl backends?
- OpenGL 1.2
    - Oldest version of OpenGL worth supporting
- OpenGL 1.5
    - Latest fixed-function version of OpenGL
- OpenGL 3.2
    - The last widest supported version of OpenGL. For context, this is the version that Minecraft (Java edition) uses.
- OpenGL 3.3
    - Backported a bunch of 4.x features. TODO: what features are these? Is it worth an OpenGL 3.3 backend when the 3.2 one exists?
- OpenGL 4.3
    - Last version of OpenGL supported on Macos, other than using some kind of weird cursed ANGLE->MoltenVK->Metal setup, or a pure software implementation
- OpenGl 4.6
    - Last version of OpenGL
- Vulkan 1.0
- Vulkan 1.2
    - last Vulkan release that is widely supported on older hardware
- TODO: What do Vulkan 1.3 and 1.4 bring to the table?
- Raw Vulkan

## Planned window backends (NOT FINAL)
- SDL 1
- SDL 3
- X11 (Xlib)
- win32
- windows 9x (more or less just win32 with an extremely limited set of features)
- Cocoa
- Wayland
- GLFW
- Haiku
- Android
- IOS
- Web canvas
    - Pinc is compiled to WASM, and a bunch of JS boilerplate crap is used to create WebGL contexts and such. This is what Emscripten does to get GLFW to work.

## Planned backends / platforms in the VERY FAR future
None of these are going to be implemented any time soon - if ever.
- Playstation 4/5
- Nintendo DS
- Xbox
    - It's basically just Windows (I think), should be pretty easy actually
- Nintendo switch
    - This actually seems pretty simple. Nintendo seems to be OK with letting Switch code exist in open source code, although whether the SDK itself is freely available is another issue.
- N64 would be funny
- Playstation would also be funny
- Microsoft DOS
    - a Microsoft DOS backend doesn't even make sense, as I don't think it has a universal way to draw pixels on the screen.
- terminal text output
    - haha ascii art go BRRR
- 3dfx's Glide graphics api
    - The funny thing is, there are actual Glide drivers available for modern GPUS. I believe it's an implementation that uses Vulkan to emulate the original API.

## backends that will NEVER be implemented (unless for good reason)
- Raw X11 network packets
    - are you crazy!? Also, good luck getting OpenGL or Vulkan to work.
- Xcb
    - Not worth the effort. Xlib works fine for X11.

## TODO and missing features (LOOKING FOR CONTRIBUTORS / API DESIGN IDEAS)
- better text selection input. ex: SDL_TextSelectionEvent
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
    - Admittedly, it seems HDR support is still in the early stages on anything other than Windows...
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
    - Most consoles don't even have user-defined coloring, so those can be hard-coded
- Audio support
    - audio playback to a default or specific device
    - software audio, or something like (the ancient and probably deprecated) OpenAL which supports hardware acceleration
    - directly output samples
    - audio recording
    - arbitrary inputs and outputs for something like DAW software
        - ability to check the system's ability to handle multiple inputs / multiple outputs
        - notify the application when the user edits the audio output through system settings or something like qpwgraph
    - native implementations for OpenAL, ALSA, pipewire, and whatever else?
        - SDL backend probably
- built-in text / font rendering
    - This is an extremely common thing to do, so it may make sense to just add this into Pinc directly - maybe an official (but separate) module?
    - Would we want to do only basic text (ASCII), a partial implementation of unicode (ex: FreeType), or a full text shaping engine (ex: HarfBuz)?
- test and get working on different compilers / implementations of libc
    - MSVC, as much as their C compiler sucks
        - how far back do we want to support? MSVC from 2010?
    - emscripten (once web support is added)
    - tcc
    - clang-cl
    - mingw
    - cygwin
    - Zig's cross-compiler thing for clang
        - This is ultimately clang, however it does some extra work to get libc and platform libraries/headers working nicely
    - musl maybe??
- add debug print callback with proper formatting (that isn't just a copy of libc's formatting)
- expose (most of) platform.h so users can write code that is just as portable as Pinc itself while not giving up features that libc doesn't have
- Add options / code / auto detection for where libraries come from
    - This is a requirement for supporting platforms without dynamic linking (such as the web)
- move state to structs or add prefixes to avoid strange linker weirdness
- set up clang-tidy or another linter
- Add option to capture mouse and lock mouse
    - Capture mouse just forces it to be in the window. As much as I hate this as a user, may as well just add it for completeness.
    - Lock mouse forces it to be in the window, and is used for first-person games for camera movement, and sometimes by applications (like Blender) to have infinite mouse movement for certain operations (like changing unbounded sliders or panning the camera).
