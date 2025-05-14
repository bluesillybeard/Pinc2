# Platform support

"Will it work" Key:
- A -> tested regularly and actively supported
- B -> tested occasionally / rarely but actively supported
- C -> untested but may work anyways
- D -> untested but porting is not too far off
- F -> untested
- ? -> we don't know of this existing / unsure

"Will it be supported in the future" Key:
- -1 -> explicitly not supported
- 0 -> Not on our radar, but not closed to suggestions
- 1 -> May be supported eventually, if someone can work on it or make a good argument for it
- 2 -> Will be supported at some point
- 3 -> Will be supported soon
- 4 -> Will be supported in the foreseeable future

## Operating systems & architectures (Platform Backend)

Os / Arch    |x86_64|x86 (32 bit)|x86 (16 bit)|aarch64|arm (32 bit)|riscv|WASM
-------------|------|------------|------------|-------|------------|-----|----
Windows 10   |A4    |B4          |?           |C4     |C1          |?    |?
Windows 7    |C4    |C4          |?           |C4     |C2          |?    |?
Windows NT 4 |C4    |C4          |?           |C4     |C4          |?    |?
Windows 9x   |C2    |C2          |C2          |?      |?           |?    |?
GNU/Linux 6.x|A4    |B4          |?           |C4     |C4          |C4   |?
GNU/Linux 5.x|B4    |B4          |?           |C4     |C4          |C4   |?
GNU/Linux 4.x|B4    |B4          |?           |C4     |C4          |C4   |?
MacOS X      |C2    |?           |?           |C2     |?           |?    |?
BSD          |C2    |C2          |?           |C2     |C2          |C2   |?
Haiku        |D2    |D2          |?           |?      |?           |?    |?
Android      |D1    |D1          |?           |D2     |D2          |?    |?
IOS          |?     |?           |?           |D2     |D2          |?    |?

Random notes:
- Yes, windows 10 for 32 bit arm DOES exist, it's just extremely difficult to find and only support specific devices

Others:
- Emscripten: F2
- WASI: F2
- Classic Macos: F0
- Posix without libc: F0

TODO: Questions that need to be answered:
- Did windows 9x ever support ARM?
- What architectures does Haiku support?
- Do we need to split BSD into flavors?
- What about non-GNU Linux?
- Do we need to split MacOS into flavors?
- What about libc versions and ABI compatibility? (Particularly for Linux)

## Window backends

- SDL2: A4
    - \> 2.30: A4
    - = 2.0: D4
- win32 API: F3
- Xlib: F3
- Cocoa: F3
- Wayland: F2
- Android API: F2
- SDL1: F2
- SDL3: F2
- Playstation 4: F1
- Playstation 5: F1
- Nintendo DS: F1
- Nintendo Switch: F1
- Nintendo 64: F0
- MSDOS: F0
- Terminal text output: F0
- Xcb: F-1
- X11 via custom packets: F-1

## Graphics APIs

- OpenGL: A4
- SDL3 GPU: F3
- Vulkan: F3
- Raw framebuffer: F3
- Metal: F2
- DirectX F2
- SDL1: F0
- SDL2: F0
- SDL3 Renderer: F0
- Glide: F0

## Compilers

- Clang: A4
- Gcc: B4
- MSVC: C4
- clang-cl: D3
- Cosmopolitan C: D1
- Emscripten: See operating systems
- Tcc: F0

- mingw: This is a windows SDK, not a C compiler, but D4
- cygwin: This is an implementation of libc + extras, not a C compiler, but D4
- musl: This is an implementation of libc, not a C compiler, but F-1
    - Unsupported because Pinc (currently) requires dynamic loading

## Build systems

See build.md
