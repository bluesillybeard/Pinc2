# Platform support

"Is it supported" Key:
- A -> tested before merging to main branch and actively supported
- B -> tested occasionally / rarely but actively supported
- C -> untested but may work anyways
- D -> untested but porting is not probably too far off
- F -> unsupported
- ? -> we don't know of this existing / unsure

## Operating systems & architectures (Platform Backend)

Os / Arch    |x86_64|x86 (32 bit)|x86 (16 bit)|aarch64|arm (32 bit)|riscv|WASM
-------------|------|------------|------------|-------|------------|-----|----
Windows 10   |A     |A           |?           |C      |C           |?    |?
Windows 7    |C     |C           |?           |C      |C           |?    |?
Windows NT 4 |C     |C           |?           |C      |C           |?    |?
Windows 9x   |C     |C           |C           |?      |?           |?    |?
GNU/Linux 6.x|A     |A           |?           |C      |C           |C    |?
GNU/Linux 5.x|C     |C           |?           |C      |C           |C    |?
GNU/Linux 4.x|C     |C           |?           |C      |C           |C    |?
MacOS X      |C     |?           |?           |C      |?           |?    |?
BSD          |C     |C           |?           |C      |C           |C    |?
Haiku        |D     |D           |?           |?      |?           |?    |?
Android      |D     |D           |?           |D      |D           |?    |?
IOS          |?     |?           |?           |D      |D           |?    |?

Random notes:
- Yes, windows 10 for 32 bit arm DOES exist, it's just extremely difficult to find and only support specific devices

Others:
- Emscripten: F
- WASI: F
- Classic Macos: F
- Posix without libc: F

TODO: Questions that need to be answered:
- Did windows 9x ever support ARM?
- Do we need to split BSD into flavors?
- What about non-GNU Linux?
- Do we need to split MacOS into flavors?
- What about libc versions and ABI compatibility? (Particularly for Linux)

## Window backends

- SDL2: A
    - \>= 2.30: A
    - \< 2.30: D
- win32 API: F
- Xlib: F
- Cocoa: F
- Wayland: F
- Android API: F
- SDL1: F
- SDL3: F
- Playstation 4: F
- Playstation 5: F
- Nintendo DS: F
- Nintendo Switch: F
- Nintendo 64: F
- MSDOS: F
- Terminal text output: F
- Xcb: F
- X11 via custom packets: F

## Graphics APIs

- OpenGL: A
- SDL3 GPU: F
- Vulkan: F
- Raw framebuffer: F
- Metal: F
- DirectX F
- SDL1: F
- SDL2: F
- SDL3 Renderer: F
- Glide: F

## Compilers

- Clang: A
- Gcc: A
- MSVC: A
- clang-cl: D
- Cosmopolitan C: D
- Emscripten: See operating systems
- Tcc: F

- mingw: This is a windows SDK, not a C compiler, but A (Notice the MinGW folder in this repo, it has a readme)
- cygwin: This is an implementation of libc + extras, not a C compiler, but D
- musl: This is an implementation of libc, not a C compiler, but F
    - Unsupported because Pinc (currently) mandates dynamic loading

## Build systems

See build.md
