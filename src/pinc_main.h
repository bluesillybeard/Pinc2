#ifndef PINC_MAIN_H
#define PINC_MAIN_H

// most options are handled by the external handle (as they must be shared between pinc and the user)
#include <pinc.h>
#include "platform/platform.h"
#include "libs/pstring.h"
#include "libs/dynamic_allocator.h"

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

// Internal versions of external Pinc api pieces

typedef struct {
    uint32_t channels;
    uint32_t channel_bits[4];
    pinc_color_space color_space;
} FramebufferFormat;

typedef struct {
    // Allocated on the root allocator
    PString title;
    bool hasWidth;
    uint32_t width;
    bool hasHeight;
    uint32_t height;
    bool resizable;
    bool minimized;
    bool maximized;
    bool fullscreen;
    bool focused;
    bool hidden;
} IncompleteWindow;

typedef void* WindowHandle;

typedef struct {
    uint32_t accumulatorBits[4];
    bool stereo;
    bool debug;
    bool forwardCompatible;
    bool robustAccess;
    uint32_t versionMajor;
    uint32_t versionMinor;
    bool versionEs;
    bool core;
} IncompleteRawGlContext;

typedef void* RawOpenglContextHandle;

// pinc objects are a user-facing map from what the user sees, and the actual internal objects / handles.
// The real internal objects are usually done with opaque pointers managed by the backend itself.
typedef enum {
    // This needs to be zero so null-initialized objects are valid
    PincObjectDiscriminator_none = 0,
    PincObjectDiscriminator_incompleteWindow,
    PincObjectDiscriminator_window,
    PincObjectDiscriminator_incompleteRawGlContext,
    PincObjectDiscriminator_rawGlContext,

} PincObjectDiscriminator;

typedef struct {
    PincObjectDiscriminator discriminator;
    union PincObjectData {
        IncompleteWindow incompleteWindow;
        WindowHandle window;
        IncompleteRawGlContext incompleteRawGlContext;
        RawOpenglContextHandle rawGlContext;
    } data;
} PincObject;

// Pinc static state

// window.h is down here because it needs the error macros
// TODO: Arguably the error macros should go in their own header
#include "window.h"

typedef enum {
    PincState_preinit,
    PincState_incomplete,
    PincState_init,
} PincState;

typedef struct {
    // Keep track of what stage of initialization we're in
    PincState initState;
    // See doc for rootAllocator macro. Live for incomplete and init
    Allocator alloc;
    // See doc for tempAllocator macro. Live for incomplete and init.
    Allocator tempAlloc;
    // Nullable, Lifetime separate from initState
    pinc_error_callback userCallError;
    // Live for incomplete and init
    WindowBackend sdl2WindowBackend;
    // On the root allocator, Live for incomplete and init
    FramebufferFormat* framebufferFormats;
    size_t framebufferFormatNum;
    // On the root allocator, Live for init
    PincObject* objects;
    size_t objectsNum;
    size_t objectsCapacity;

    // TODO: name these nicer
    void* userAllocObj;
    pinc_alloc_callback userAllocFn;
    pinc_alloc_aligned_callback userAllocAlignedFn;
    pinc_realloc_callback userReallocFn;
    pinc_free_callback userFreeFn;
} PincStaticState;

#define PINC_PREINIT_STATE (PincStaticState) { \
    .initState = PincState_preinit, \
    .alloc = {.allocatorObjectPtr = 0, .vtable = 0}, \
    .tempAlloc = {.allocatorObjectPtr = 0, .vtable = 0}, \
    .userCallError = 0, \
    .sdl2WindowBackend = {.obj = 0, .vt = {0}}, \
    .framebufferFormats = 0, \
    .framebufferFormatNum = 0, \
    .objects = 0, \
    .objectsNum = 0, \
    .objectsCapacity = 0, \
}

extern PincStaticState pinc_intern_staticState;

// shortcuts
// shortcut to the Pinc global static state struct.
#define staticState pinc_intern_staticState
// The primary allocator. This is either a wrapper of libs/platform.h, or the user-defined allocation callbacks
#define rootAllocator staticState.alloc
// A temporary allocator that is cleared at the end of pinc_step().
#define tempAllocator staticState.tempAlloc

#endif
