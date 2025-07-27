# Building Pinc

If all you want to do is build Pinc into a library or mess around with the examples, cmake is the best way to do that. Other build systems are supported just so they can be used to (hopefully easily) import Pinc into a project using said build system. To be absolutely clear: building the examples with any build system other than cmake is not supported. The other build systems are available to import or build Pinc itself.

Super Quick Disclaimer: this documentation is frequently out of date, read the actual build system scripts for more up to date and detailed information

## All build systems - Dependencies

The only real build dependency is a decent compiler. Exactly what that actually means depends on the build system and platform.

At runtime, however, Pinc does require some libraries depending on the platform:
- On Windows: SDL2.dll in the path or next to the executable
- On Posix (Linux, unix, bsd, whatever): libSDL2.so or libSDL2-2.0.so in the path or in LD_PRELOAD

## Cmake

The CMakeLists.txt in the root of this project should be entirely self explanatory for anyone well versed with cmake. So, if you are comfortable, skip this document and go straight to just reading the CMakeLists.txt file, whose comments should guide you. This will still be available for reference if you get confused.

### Cmake usage - building Pinc

Setting up cmake is fairly simple. First, make a directory called `build` in the Pinc root directory, then cd into it:
```
mkdir build
cd build
```

Then, invoke cmake:
```
cmake .. [options]
```

There are many options available. The ones common for all build systems are documented in settings.md in the same folder as this file.
However, there are many options specific to the cmake build. Those are:
- `PINC_ADDITIONAL_COMPILE_OPTIONS` - this will be added to the list of compiler options used to build Pinc and the examples
- `PINC_ADDITIONAL_LINK_OPTIONS` - this will be added to the list of linker options used to build Pinc and the examples
- `PINC_COMPILE_OPTIONS` - Setting this will completely override the compiler options used to build Pinc
- `PINC_LINK_OPTIONS` - Setting this will completely override the compiler options used to build Pinc
- `PINC_USE_CLANG_TIDY` - Setting this to true will create an extra target to run clang-tidy on the entire project

These options can be set using `-D[option name]=[value]` in the cmake command. Note that these are cached, in order to change them you should delete `build/CMakeCache.txt` before re-running the cmake command.

The cmake build has these targets:
- `pinc` - the actual pinc library object and headers
- `example_[name]` - the examples - new examples are added constantly, so they are not listed here. See the `examples` folder for more information.

## Raw compiler command

Pinc only strictly requires these things in order to compile:
- a decent C99 compiler
- unitybuild.c which `#include`'s all of the Pinc source files
- access to Pinc headers (`-I[pinc_dir]/include`)
- access to internal pinc headers (`-I[pinc_dir]/src` and `-I[pinc_dir]/ext`)
- On posix (`#if defined (__unix) || (__linux)`):
    - an implementation of the C standard library, as well as `dlfcn.h` - I have yet to find a decent posix C compiler without these
- On windows (`#if defined (_WIN32)`):
    - Linkage to kernel32.dll (which is a guarantee by default for any valid compiler that supports windows)

Of course, all of the options in settings.md work just the same as in cmake.

Example: `gcc -Iinclude -Isrc -Iext src/unitybuild.c -c -o build/libpinc.o`

Example that disables errors: `gcc -DPINC_ENABLE_ERROR_USER=0 -DPINC_ENABLE_ERROR_ASSERT=0 -Iinclude -Isrc -Iext src/unitybuild.c -c -o build/libpinc.o`

In case you missed it in the readme, anything that isn't baked into the compiler is either loaded dynamically or baked into Pinc itself. So you do not need any additional libraries and headers to build. You may still need libraries (such as SDL2) based on what window backends you have enabled.

## Zig

All of the standard zig options work: `-Dtarget` to change the compile target, `-Doptimize` to change the optimize level, etc.

All of the options in settings.md are exposed through options with the PINC_ prefix removed and the case lowered. ex: `PINC_HAVE_WINDOW_SDL2` -> `have_window_sdl2`

Additional options:
- `-Dshared=[true/false]`: whether to build a shared library, defaults to false

