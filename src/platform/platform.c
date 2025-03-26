// Platform implementation detection thingy

#if defined (__unix) || (__linux)

#include "platform_posix.c.inc"

#else

#error "No implementation for this platform!"

#endif
