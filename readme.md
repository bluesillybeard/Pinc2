# Pinc 2

Pinc is a cross platform window library for game and application development, with a focus on cross-compilation, dependency minimization, and ease of integration.

Pinc is written entirely in C, making it (theoretically) portable to any system with a C compiler. Pinc currently uses cmake, however its design hopefully makes it simple to port to other build systems. See [build.md](build.md) for how the build system is configured.

The original version written in Zig can be found [here](https://github.com/bluesillybeard/Pinc). Using it is an absolutely bad idea.

## Pinc goals

- Language agnostic - the API should avoid things that are difficult to port to other languages
    - The header is written in plain C, so any language with C FFI can be used
    - The external API only uses `int`, `char`, `void`, and basic pointers to arrays (`char*` and `len` for strings, `int*` and `len` for element arrays, etc)
    - Callbacks are rarely used

- ZERO compile-time dependencies other than a supported C compiler / linker
    - Headers for external libraries are baked into the library itself (ex: sdl2), with care taken to avoid licensing issues

- Easy to use
    - comparable to something like SDL or SFML
    - Admittedly the API is a bit verbose

- Flexible
    - Bring your own build system (we just use cmake for convenience)
        - unity build with a simple compiler command is an option.

- Wide Support
    - Something along the lines of what ClassiCube supports: https://github.com/ClassiCube/ClassiCube/tree/master?tab=readme-ov-file
    - Note: this is a goal, the reality is that every new platform requires consistent valuable development time...

## Other things
- Pinc does not take hold of the entry point
- Pinc does not provide a main loop
- Pinc's API has a complete memory barrier from your application. Pinc does not give you pointers to keep, and it does not keep yours.
- Pinc is written for C99. If your compiler doesn't support C99, you are on your own my friend.

## Implemented platforms
- Posix / Unix
- Windows / Win32

## Implemented window backends
- SDL2

## Implemented graphics apis
- OpenGL

## Implemented build systems
- cmake
- a single raw compiler command (if you can even call that a build system...)

## Implemented IDE setups:
- Visual Studio Code

## Tested Compilers
- Clang (the LLVM C compiler)
- Gcc
- MSVC / cl - note: not tested frequently, as that requires me to spin up my windows development workstation.

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
    - If you want to use another programming language than C, you're on your own for now
- Compile the project with cmake
    - We recommend Clang + cmake since that is the system we use for testing and development
    - use the main branch, it's meant to be the one that always works.
- test it by running one of the examples (see build system documentation)
- add it to your project
    - Again, see build system documentation

It is a good idea to read through the header a bit as well as look at the examples to get a feel for the API. 

## Planned supported platforms
- Macos X
- emscripten
- Haiku

## Maybe planned supported platforms (if someone wants to contribute)
- Cosmopolitan C
- WASI
- IOS
- "old" macos
- Posix without libc
    - Who on earth thinks this is a good idea. This is almost like doing windows without windows.h
    - Will require some assembly for all architectures to make syscalls
    - Will require a custom shared object loader
- Windows 9x
- Windows 9x without libc

## Planned graphics apis (NOT FINAL)
- plain
    - Just a grid of pixels with your direct control
- SDL 1 maybe
- SDL 2 maybe
- SDL 3
    - SDL3 GPU API is pretty cool
- Vulkan
- Metal

## Planned window backends (NOT FINAL)
- SDL 1
- SDL 3
- X11 (Xlib)
- Cocoa
- Wayland
- GLFW
- Haiku
- Android
- IOS
- Web canvas
    - Pinc is compiled to WASM, and a bunch of JS boilerplate crap is used to create WebGL contexts and such.

## Planned backends / platforms in the VERY FAR future
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

## Next Steps / Roadmap
- implement the rest of SDL2 + OpenGl backend
    - add tests as things are implemented
- Make sure all of the important TODOs are handled
- Not yet at where the Zig version is, but the library is usable.
- create pinc graphics
    - Just base it off the original one.
    - Changes I want to make:
        - move the texture sampling properties from the uniform to the pipeline
        - More dynamic pipeline state
        - Remove GLSL and use a custom shader definition compatible with fixed function rendering (aim for minimal features like OpenGL 1.0 or the N64)
        - Add GLSL as an optional feature that a graphics backend may or may not have
        - Add indexed framebuffer / texture (where colors are an enum instead of brightness values)
            - This may be as simple as adding a new color space and some query functions.
            - Note: Modern APIs don't support this directly (lol I wonder why), so this capability needs to be queryable
            - SDL2 supports this! I'm genuinely not quite sure why they would, seeing as they only target platforms of the last ~20 years, but still neat.
        - scissor rectangle
        - add framebuffer objects / arbitrary draw surfaces
            - many APIs do not support these so they must be optional to implement
            - With that said, on the OpenGL 1.1 - 2.1 side, ARB_framebuffer_object or EXT_framebuffer_object are apparently widely supported, and almost guaranteed on 2.x hardware.
        - queue based rendering - the user makes a queue, submits all of it at once, and can query if it's finished or not.
            - Not so nice in OpenGL land, which does all of the sync explicitly, but this is a nice abstraction that many other APIs can benefit from.
                - Note: Look at OpenGL sync objects, ARB_sync, NV_fence.
                - Note: Look into NV_command_list, also ARB_shader_draw_parameters may be useful
    - set up interface for graphics backend
    - implement opengl 2.1 backend
- implement the rest of the examples from the original project
    - Remove the link to the old Zig version
- Celebrate! we've made it back to where we left off in the original Zig version of Pinc.
    - And in fact, with some new things that the original prototype-like thing did not have
- implement many examples, inspired from the likes of SDL and GLFW
- proper documentation to guide users on how to use this library

## Absolutely Important TODOs for before the library goes anywhere
- Make things consistent...
    - Change all object handle types to include handle in the name
    - Rename object handle parameters to 'handle' instead of using 'obj' or 'id', as well as include their completeness
        - example: complete_window_handle, incomplete_window_handle, window_handle (for either)
    - Commit to a naming convention, such as:
        - TypesAreNamedLikeThis
        - functionsAreaNamedLikeThis
        - EnumValueType_enumValue
        - This is just an example for this TODO

## TODO for API features / changes
- better text selection input. ex: SDL_TextSelectionEvent
- window position
    - wayland be like:
    - In fact, there are many potential platforms that don't support window positioning:
        - Pretty much any console
        - Emscripten / web canvas
        - applets
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
- Add option to capture mouse and lock mouse
    - Capture mouse just forces it to be in the window. As much as I hate this as a user, may as well just add it for completeness.
    - Lock mouse forces it to be in the window, and is used for first-person games for camera movement, and sometimes by applications (like Blender) to have infinite mouse movement for certain operations (like changing unbounded sliders or panning the camera).
- header option to only include types and not functions
- Potentially allow a different framebuffer format for each window
- binding policy
    - how bindings to new languages should be handled, maintained, etc.
    - building the library and shipping the .so for dynamic languages, integrating with the build system of native languages, etc.
- expose the Pinc temp allocator for user convenience
- Allow a different graphics API per window
    - Maybe even completely separate the idea of a graphics API and a window, like what SDL2 somewhat does
    - Probably wait until more than just OpenGL is supported
- Change window states to an enum instead of a bunch of sorta-implicitly-exclusive bools
- add window state change events
- Split this readme into multiple files, it's getting too long

## TODO for internal library
- test and get working on different compilers / implementations of libc
    - gcc
        - Going back to GCC 8 seems reasonable enough
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
- add debug print system with proper formatting (that isn't just a copy of libc's formatting)
- expose (most of) platform.h so users can write code that is just as portable as Pinc itself while not giving up features that libc doesn't have
- Add options / code / auto detection for where libraries come from
    - This is a requirement for supporting platforms without dynamic linking (such as the web)
- set up clang-tidy or another linter
- set up testing with valgrind or other runtime analysis tools
- allocation tracking
- More build systems
    - scons
    - premake
    - zig
    - GNU make
    - nix
    - meson?
    - autoconf?
- linking / using ANGLE for better macos+opengl support, among other improvements to OpenGL
