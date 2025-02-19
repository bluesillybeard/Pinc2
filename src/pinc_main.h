#ifndef PINC_MAIN_H
#define PINC_MAIN_H

// most options are handled by the external handle (as they must be shared between pinc and the user)
#include <pinc.h>
#include "platform/platform.h"
#include "libs/pstring.h"

// options only for library build

// This is so cmake options work correctly when put directly as defines
#define ON 1
#define OFF 0

#ifndef PINC_HAVE_WINDOW_SDL2
# define PINC_HAVE_WINDOW_SDL2 1
#endif

#ifndef PINC_HAVE_GRAPHICS_RAW_OPENGL
# define PINC_HAVE_GRAPHICS_RAW_OPENGL 1
#endif

#ifndef PINC_ENABLE_ERROR_EXTERNAL
# define PINC_ENABLE_ERROR_EXTERNAL 1
#endif

#ifndef PINC_ENABLE_ERROR_ASSERT
# define PINC_ENABLE_ERROR_ASSERT 1
#endif

#ifndef PINC_ENABLE_ERROR_USER
# define PINC_ENABLE_ERROR_USER 1
#endif

#ifndef PINC_ENABLE_ERROR_SANITIZE
# define PINC_ENABLE_ERROR_SANITIZE 0
#endif

#ifndef PINC_ENABLE_ERROR_VALIDATE
# define PINC_ENABLE_ERROR_VALIDATE 0
#endif

#include "libs/dynamic_allocator.h"

/// @brief Pinc primary "root" allocator
/// @remarks 
///     This is either a wrapper around the platform.h functions, or user-defined allocator functions (which themselves might just be wrappers of platform.h)
///     It is undefined until pinc_incomplete_init is called by the user.
extern Allocator pinc_intern_rootAllocator;

/// @brief Pinc temporary allocator. This is an arena/bump allocator that is cleared at the start of pinc_step. undefined until pinc_incomplete_init is called by the user.
extern Allocator pinc_intern_tempAllocator;

// To make it easier to reference while avoiding linker name conflicts
#define rootAllocator pinc_intern_rootAllocator
#define tempAllocator pinc_intern_tempAllocator

// TODO: should we make errors use __file__ and __line__ macros?

void pinc_intern_callError(PString message, pinc_error_type type);

#if PINC_ENABLE_ERROR_EXTERNAL == 1
# define PErrorExternal(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callError(PString_makeDirect((char*)(messageNulltermStr)), pinc_error_type_external)
# define PErrorExternalStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callError(messagePstring, pinc_error_type_external)
#else
# define PErrorExternal(assertExpression, messageNulltermStr)
# define PErrorExternalStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_ASSERT == 1
# define PErrorAssert(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callError(PString_makeDirect((char*)(messageNulltermStr)), pinc_error_type_assert)
# define PErrorAssertStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callError(messagePstring, pinc_error_type_assert)
#else
# define PErrorAssert(assertExpression, messageNulltermStr)
# define PErrorAssertStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_USER == 1
# define PErrorUser(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callError(PString_makeDirect((char*)(messageNulltermStr)), pinc_error_type_user)
# define PErrorUserStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callError(messagePstring, pinc_error_type_user)
#else
# define PErrorUser(assertExpression, messageNulltermStr)
# define PErrorUserStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_SANITIZE == 1
# define PErrorSanitize(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callError(PString_makeDirect((char*)(messageNulltermStr)), pinc_error_type_sanitize)
# define PErrorSanitizeStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callError(messagePstring, pinc_error_type_sanitize)
#else
# define PErrorSanitize(assertExpression, messageNulltermStr)
# define PErrorSanitizeStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_VALIDATE == 1
# define PErrorValidate(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callError(PString_makeDirect((char*)(messageNulltermStr)), pinc_error_type_validate)
# define PErrorValidateStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callError(messagePstring, pinc_error_type_validate)
#else
# define PErrorValidate(assertExpression, messageNulltermStr)
# define PErrorValidateStr(assertExpression, messagePstring)
#endif

// Super quick panic function
#define PPANIC(messageNulltermStr) pinc_intern_callError(PString_makeDirect((char *)(messageNulltermStr)), pinc_error_type_unknown)
#define PPANICSTR(messagePstring) pinc_intern_callError(messagePstring, pinc_error_type_unknown)

#endif
