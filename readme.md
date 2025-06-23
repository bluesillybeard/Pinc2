# Pinc 2

Pinc is a cross platform window library for game and application development, with a focus on cross-compilation, dependency minimization, and ease of integration.

Pinc is written entirely in C, making it (theoretically) portable to any system with a C compiler. Pinc currently uses cmake, however its design hopefully makes it simple to port to other build systems. See [build.md](build.md) for how the build system is configured.

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
    - Note: this is a goal. Reality Check: every new platform requires consistent valuable development time.

## Other things
- Pinc does not take hold of the entry point
- Pinc does not provide a main loop
- Pinc's API has a complete memory barrier from your application. Pinc does not give you pointers to keep, and it does not keep yours
    - The only exception is custom user data
- Pinc is written for C99. If your compiler doesn't support C99, you are on your own my friend.

## Implemented platforms
- Posix / Unix
- Windows / Win32
- see platform support page

## Implemented window backends
- SDL2

## Implemented graphics apis
- OpenGL

## Implemented build systems
- cmake
- a single raw compiler command (if you can even call that a build system...)
- zig

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
- the project desperately needs contributors - see [contribution guide](contribute.md)

Pinc's current API is fundamentally incompatible with multithreading. It is suggested that a single thread is selected to interact with Pinc, if such multithreaded operation is desired. In the future, Pinc may support threaded operation.

Pinc includes headers from other projects, see the following files in this repository:
- ext/readme.md
- MinGW/README_IMPORTANT.md

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

