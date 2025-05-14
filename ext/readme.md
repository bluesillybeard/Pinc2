# ext directory

This directory is added to the include path of the Pinc library. It contains headers for external libraries - the headers are here so they don't have to be installed or managed externally.

This ext folder is added as an include path when building Pinc.

## SDL2

SDL2 headers - SDL2 is ABI stable and is actually heading towards deprecation, so these should never need to be updated.

Modifications from base SDL2 headers:
- in SDL_thread.h line 38, replaced reference to process.h to pinc_win32.h

## WIN32

win32 headers because some compilers (notably zig cc without libc) do not have it.

They are binary compatible with the actual windows.h, but generally not API compatible.
