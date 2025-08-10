#ifndef PINC_ERROR_H
#define PINC_ERROR_H

#include "pinc_options.h"
#include "libs/pinc_string.h"
#include <pinc.h>
// TODO(bluesillybeard): should we make errors use __file__ and __line__ macros?

// NOTE: This function is implemented in pinc_main.c - there is no pinc_error.c
// Call the error function for a non-fatal error
void pinc_intern_callError(pincString message, PincErrorCode type, bool recoverable);

// These macros an error if an expression expands to false
// Usage example: PincAssertExternal(value<=max, "value is too big!", false, {return error_value;});
// the *Str variant takes a pincString instead of a char*

#if PINC_ENABLE_ERROR_EXTERNAL == 1
# define PincAssertExternal(assertExpression, messageNullterm, recoverable, ...) \
    if(!(assertExpression)) { \
        __VA_ARGS__\
        pinc_intern_callError(pincString_makeDirect((char*)(messageNullterm)), PincErrorCode_external, (recoverable)); \
    }

# define PincAssertExternalStr(assertExpression, messageStr, recoverable, ...) \
    if(!(assertExpression)) { \
        __VA_ARGS__\
        pinc_intern_callError(messageStr, PincErrorCode_external, (recoverable)); \
    }
#else
# define PincAssertExternal(assertExpression, messageNullterm, recoverable, ...)
# define PincAssertExternalStr(assertExpression, messageStr, recoverable, ...)
#endif

#if PINC_ENABLE_ERROR_ASSERT == 1
# define PincAssertAssert(assertExpression, messageNullterm, recoverable, ...) \
    if(!(assertExpression)) { \
        __VA_ARGS__\
        pinc_intern_callError(pincString_makeDirect((char*)(messageNullterm)), PincErrorCode_assert, (recoverable)); \
    }

# define PincAssertAssertStr(assertExpression, messageStr, recoverable, ...) \
    if(!(assertExpression)) { \
        __VA_ARGS__\
        pinc_intern_callError(messageStr, PincErrorCode_assert, (recoverable)); \
    }
#else
# define PincAssertAssert(assertExpression, messageNullterm, recoverable, ...)
# define PincAssertAssertStr(assertExpression, messageStr, recoverable, ...)
#endif

#if PINC_ENABLE_ERROR_USER == 1
# define PincAssertUser(assertExpression, messageNullterm, recoverable, ...) \
    if(!(assertExpression)) { \
        __VA_ARGS__\
        pinc_intern_callError(pincString_makeDirect((char*)(messageNullterm)), PincErrorCode_user, (recoverable)); \
    }

# define PincAssertUserStr(assertExpression, messageStr, recoverable, ...) \
    if(!(assertExpression)) { \
        __VA_ARGS__\
        pinc_intern_callError(messageStr, PincErrorCode_user, (recoverable)); \
    }
#else
# define PincAssertUser(assertExpression, messageNullterm, recoverable, ...)
# define PincAssertUserStr(assertExpression, messageStr, recoverable, ...)
#endif

#define PincForwardErrorVoid() if(pincLastErrorCode() != PincErrorCode_pass) { return; }
#define PincForwardErrorVoidNoRecovery() if(pincLastErrorCode() != PincErrorCode_pass) { staticState.recoverable = false; return; }

#define PincForwardError(...) if(pincLastErrorCode != PincErrorCode_pass) { __VA_ARGS__ }

#endif
