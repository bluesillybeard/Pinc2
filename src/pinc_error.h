#ifndef PINC_ERROR_H
#define PINC_ERROR_H

#include "pinc_options.h"
#include "libs/pinc_string.h"
#include <pinc.h>
// TODO(bluesillybeard): should we make errors use __file__ and __line__ macros?

// NOTE: This function is implemented in pinc_main.c - there is no pinc_error.c
// Call the error function for a non-fatal error
void pinc_intern_callError(pincString message, PincErrorType type);

// Call the error function for a fatal / assertion error
P_NORETURN void pinc_intern_callFatalError(pincString message, PincErrorType type);

#if PINC_ENABLE_ERROR_EXTERNAL == 1
# define PErrorExternal(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callError(pincString_makeDirect((char*)(messageNulltermStr)), PincErrorType_external)
# define PErrorExternalStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callError(messagePstring, PincErrorType_external)
#else
# define PErrorExternal(assertExpression, messageNulltermStr)
# define PErrorExternalStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_ASSERT == 1
# define PErrorAssert(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callFatalError(pincString_makeDirect((char*)(messageNulltermStr)), PincErrorType_assert)
# define PErrorAssertStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callFatalError(messagePstring, PincErrorType_assert)
#else
# define PErrorAssert(assertExpression, messageNulltermStr)
# define PErrorAssertStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_USER == 1
# define PErrorUser(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callFatalError(pincString_makeDirect((char*)(messageNulltermStr)), PincErrorType_user)
# define PErrorUserStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callFatalError(messagePstring, PincErrorType_user)
#else
# define PErrorUser(assertExpression, messageNulltermStr)
# define PErrorUserStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_SANITIZE == 1
# define PErrorSanitize(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callFatalError(pincString_makeDirect((char*)(messageNulltermStr)), PincErrorType_sanitize)
# define PErrorSanitizeStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callFatalError(messagePstring, PincErrorType_sanitize)
#else
# define PErrorSanitize(assertExpression, messageNulltermStr)
# define PErrorSanitizeStr(assertExpression, messagePstring)
#endif

#if PINC_ENABLE_ERROR_VALIDATE == 1
# define PErrorValidate(assertExpression, messageNulltermStr) if(!(assertExpression)) pinc_intern_callFatalError(pincString_makeDirect((char*)(messageNulltermStr)), PincErrorType_validate)
# define PErrorValidateStr(assertExpression, messagePstring) if(!(assertExpression)) pinc_intern_callFatalError(messagePstring, PincErrorType_validate)
#else
# define PErrorValidate(assertExpression, messageNulltermStr)
# define PErrorValidateStr(assertExpression, messagePstring)
#endif

// Super quick panic function
#define PPANIC(messageNulltermStr) pinc_intern_callFatalError(pincString_makeDirect((char *)(messageNulltermStr)), PincErrorType_unknown)
#define PPANICSTR(messagePstring) pinc_intern_callFatalError(messagePstring, PincErrorType_unknown)

#endif
