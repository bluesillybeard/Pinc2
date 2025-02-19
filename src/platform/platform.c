#include "platform.h"

// Platform implementation detection thingy

#if defined (__unix) || (__linux)

#include "platform_posix.c.inc"

#endif
