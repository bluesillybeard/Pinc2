#ifndef PINC_MAIN_H
#define PINC_MAIN_H

// most options are handled by the external handle (as they must be shared between pinc and the user)
#include <pinc.h>
#include "platform/platform.h"

// options only for library build

// This is so cmake options work correctly when put directly as defines
#define ON 1
#define OFF 0

#ifndef PINC_ERROR_SETTING
// 0 -> disabled
// 1 -> light
// 2 -> rigorous
#define PINC_ERROR_SETTING 2
#endif

#ifndef PINC_HAVE_WINDOW_SDL2
#define PINC_HAVE_WINDOW_SDL2 1
#endif

#ifndef PINC_HAVE_GRAPHICS_RAW_OPENGL
#define PINC_HAVE_GRAPHICS_RAW_OPENGL 1
#endif

// These are "implemented" in pinc_main.c
extern pinc_error_callback pfn_pinc_error_callback;
extern pinc_error_callback pfn_pinc_panic_callback;
extern pinc_error_callback pfn_pinc_user_error_callback;

// Call when some kind of external issue occurs
#define PERROR(message, len) \
    { \
        if(pfn_pinc_error_callback) { \
            pfn_pinc_error_callback((uint8_t*) (message), (len)); \
        } else { \
            pPrintError((uint8_t*) (message), (len)); \
        } \
    }

#define PERROR_NL(message) PERROR((message), pStringLen(message))

#define PPANIC(message, len) \
    { \
        if(pfn_pinc_panic_callback) { \
            pfn_pinc_panic_callback((uint8_t*) (message), (len)); \
        } else { \
            pPrintError((uint8_t*) (message), (len)); \
        } \
        pAssertFail(); \
    }

#define PPANIC_NL(message) PPANIC(message, pStringLen((char*)message))

#define PUSEERROR(message, len) \
    { \
        if(pfn_pinc_user_error_callback) { \
            pfn_pinc_user_error_callback((uint8_t*) (message), (len)); \
        } else { \
            pPrintError((uint8_t*)(message), (len)); \
        } \
        pAssertFail(); \
    }

#if PINC_ERROR_SETTING == 2

#define PERROR_RIGOROUS(expr, message, len) if(!(expr)) PERROR(message, len)

#define PERROR_LIGHT(expr, message, len) if(!(expr)) PERROR(message, len)

#define PERROR_RIGOROUS_NL(expr, message) if(!(expr)) PERROR(message, pStringLen((char*)(message)))

#define PERROR_LIGHT_NL(expr, message) if(!(expr)) PERROR(message, pStringLen((char*)(message)))

#define PUSEERROR_RIGOROUS(expr, message, len) if(!(expr)) PUSEERROR(message, len)

#define PUSEERROR_LIGHT(expr, message, len) if(!(expr)) PUSEERROR(message, len)

#define PUSEERROR_RIGOROUS_NL(expr, message) if(!(expr)) PUSEERROR(message, pStringLen((char*)(message)))

#define PUSEERROR_LIGHT_NL(expr, message) if(!(expr)) PUSEERROR(message, pStringLen((char*)(message)))

#elif PINC_ERROR_SETTING == 1

#define PERROR_RIGOROUS(expr, message, len) 

#define PERROR_LIGHT(expr, message, len) if(!(expr)) PERROR(message, len)

#define PERROR_RIGOROUS_NL(expr, message)

#define PERROR_LIGHT_NL(expr, message) if(!(expr)) PERROR(message, pStringLen((char*)(message)))

#define PUSEERROR_RIGOROUS(expr, message, len)

#define PUSEERROR_LIGHT(expr, message, len) if(!(expr)) PUSEERROR(message, len)

#define PUSEERROR_RIGOROUS_NL(expr, message)

#define PUSEERROR_LIGHT_NL(expr, message) if(!(expr)) PUSEERROR(message, pStringLen((char*)(message)))

#else

#define PERROR_RIGOROUS(expr, message, len) 

#define PERROR_LIGHT(expr, message, len)

#define PERROR_RIGOROUS_NL(expr, message)

#define PERROR_LIGHT_NL(expr, message)

#define PUSEERROR_RIGOROUS(expr, message, len)

#define PUSEERROR_LIGHT(expr, message, len)

#define PUSEERROR_RIGOROUS_NL(expr, message)

#define PUSEERROR_LIGHT_NL(expr, message)

#endif

#include "libs/dynamic_allocator.h"

/// @brief Pinc primary "root" allocator
/// @remarks 
///     This is either a wrapper around the platform.h functions, or user-defined allocator functions (which themselves might just be wrappers of platform.h)
///     It is undefined until pinc_incomplete_init is called by the user.
extern Allocator rootAllocator;

/// @brief Pinc temporary allocator. This is an arena/bump allocator that is cleared at the start of pinc_step. undefined until pinc_incomplete_init is called by the user.
extern Allocator tempAllocator;

#endif
