# Building Pinc

If all you want to do is build Pinc into a library or mess around with the examples, cmake is the best way to do that. Other build systems are supported just so they can be used to (hopefully easily) import Pinc into a project using said build system.

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

These options can be set using `-D[option name]=[value]` in the cmake command. Note that these are cached, in order to change them you should delete `build/CMakeCache.txt`

The cmake build has these targets:
- `pinc` - the actual pinc library object and headers
- `example_[name]` - the examples - new examples are added constantly, so they are not listed here. See the `examples` folder for more information.

## Raw compiler command

Pinc only strictly requires these things in order to compile:
- a decent C99 compiler
- unitybuild.c which `#include`'s all of the Pinc source files
- access to Pinc headers (`-I[pinc_dir]/include`)
- access to internal pinc headers (`-I[pinc_dir]/src`)
- On posix (`#if defined (__unix) || (__linux)`):
    - an implementation of the C standard library, as well as `dlfcn.h` - I have yet to find a posix C compiler without these
- On windows (`#if defined (__WIN32__)`):
    - `windows.h` from at least windows XP / Server Edition 2003 - Clang, GCC, and cl.exe have this built-in
    - `printf` from `stdio.h` because reasons (Will be removed soon, hopefully)

Of course, all of the options in settings.md work just the same as in cmake.
