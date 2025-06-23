// Platform implementation detection thingy

#include "../pinc_options.h"

#if PINC_USE_CUSTOM_PLATFORM_IMPLEMENTATION

// Nothing here, we expect the user to implement platform.h somewhere

#elif defined (__unix) || (__linux)

#include "platform_posix.c.inc"

#elif defined (__WIN32__) || (_WIN32)

#include "platform_win32.c.inc"

#else

#error "No implementation for this platform!"

#endif
