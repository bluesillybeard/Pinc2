#include "platform.h"

// Jumping ground for platform implementations

#if defined (__unix) || (__linux)

#include "platform_posix.c.inc"

#endif