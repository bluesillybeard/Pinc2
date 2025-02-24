#ifndef PINC_ERROR_H
#define PINC_ERROR_H

#include "pinc_options.h"
// TODO: should we make errors use __file__ and __line__ macros?

// NOTE: This function is implemented in pinc_main.c - there is no pinc_error.c
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
