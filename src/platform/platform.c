// Platform implementation detection thingy

#if defined (__unix) || (__linux)

#include "platform_posix.c.inc"

#elif defined (__WIN32__) || (_WIN32)

#include "platform_win32.c.inc"

#else

#error "No implementation for this platform!"

#endif
