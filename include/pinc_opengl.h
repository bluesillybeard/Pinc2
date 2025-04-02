#ifndef PINC_OPENGL_H
#define PINC_OPENGL_H

#include "pinc.h"

/// @section init
/// @brief Functions to be called after pinc_incomplete_init

// Function pointer because ISO C99 does not like casting regular pointers to function pointers.
typedef void(*PincPfn)(void);

typedef enum {
    // definitely not supported
    PincOpenglSupportStatus_none,
    // may or may not be supported - unsure. Opengl is famous for being difficult and platform-dependent to query before attempting to initialize state.
    PincOpenglSupportStatus_maybe,
    // definitely supported
    PincOpenglSupportStatus_definitely,
} PincOpenglSupportStatusEnum;

typedef uint32_t PincOpenglSupportStatus;

/// @brief OpenGL context handle.
typedef PincObjectHandle PincOpenglContextHandle;

PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglVersionSupported(PincWindowBackend backend, uint32_t major, uint32_t minor, bool es);

// srgb is already handled by base Pinc framebuffer format.
// Unfortunately, due to certain reasons, the opengl backend
// still has to adhere to Pinc's requirement of only one framebuffer format for all windows (and by proxy all opengl contexts).

// The returned value is more or less an educated guess, depending on the window backend, platform, and graphics drivers.
// For example, the glX extension for the X window system supports querying many of these before creating a context.
// SDL2, on the other hand, does not provide an API for querying context support before creating one.
// For now, these are all treated as exact. That is, a context with the exact value queried / set must be created.
// In the future, there may be a flag to specify if the value is an exact requirement or a minimum requirement.
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglAccumulatorBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer, uint32_t channel, uint32_t bits);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglAlphaBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer, uint32_t bits);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglDepthBits(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer, uint32_t bits);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglSamples(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer, uint32_t samples);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglStereoBuffer(PincWindowBackend backend, PincFramebufferFormatHandle framebuffer);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglContextDebug(PincWindowBackend backend);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglForwardCompatible(PincWindowBackend backend);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglRobustAccess(PincWindowBackend backend);
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglResetIsolation(PincWindowBackend backend);

// Returns 0 on failure, which should never happen.
PINC_EXTERN PincOpenglContextHandle PINC_CALL pincOpenglCreateContextIncomplete(void);

// These fail when the exact configuration is definitely not supported. When it fails, the property is not set, and an error is not triggered.
// If it does not fail, then 
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextAccumulatorBits(PincOpenglContextHandle incomplete_context_handle, uint32_t channel, uint32_t bits);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextAlphaBits(PincOpenglContextHandle incomplete_context_handle, uint32_t bits);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextDepthBits(PincOpenglContextHandle incomplete_context_handle, uint32_t bits);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextSamples(PincOpenglContextHandle incomplete_context_handle, uint32_t samples);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextStereoBuffer(PincOpenglContextHandle incomplete_context_handle, bool stereo);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextDebug(PincOpenglContextHandle incomplete_context_handle, bool debug);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextForwardCompatible(PincOpenglContextHandle incomplete_context_handle, bool compatible);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextRobustAccess(PincOpenglContextHandle incomplete_context_handle, bool robust);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextResetIsolation(PincOpenglContextHandle incomplete_context_handle, bool isolation);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextVersion(PincOpenglContextHandle incomplete_context_handle, uint32_t major, uint32_t minor, bool es, bool core);

// Pinc will try its best to apply the given settings for the context, but they may be different from the requested ones.
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextComplete(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN void PINC_CALL pincOpenglSetContextDeinit(PincOpenglContextHandle context_handle);

PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextAccumulatorBits(PincOpenglContextHandle incomplete_context_handle, uint32_t channel);
PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextAlphaBits(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextDepthBits(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextSamples(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextStereoBuffer(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextDebug(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextForwardCompatible(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextRobustAccess(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextResetIsolation(PincOpenglContextHandle incomplete_context_handle);

/// @brief Make pinc's OpenGL context current. Asserts that the current backend is an OpenGL backend.
/// @param window the window whose framebuffer to bind to the opengl context, or 0 if it doesn't matter.
/// @param context the opengl context to bind with. Must be a complete context.
/// @return a pinc return code indicating the success of this function
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglMakeCurrent(PincWindowHandle window, PincOpenglContextHandle complete_context_handle);

PINC_EXTERN PincWindowHandle PINC_CALL pincOpenglGetCurrentWindow(void);
PINC_EXTERN PincOpenglContextHandle PINC_CALL pincOpenglGetCurrentContext(void);

/// @brief Get the function pointer to an OpenGL function. On most platforms, this requires a current OpenGL context.
///     Note that every OpenGL implementation has its own ideas about what function pointers end up with which contexts.
///     In general, it's safest to assume every context has its own set of function pointers.
///     Do not assume a function is available if this returns non-null.
/// @param procname A null terminated string with the name of an OpenGL function.
/// @return the function pointer, or null if it could not be found.
PINC_EXTERN PincPfn PINC_CALL pincOpenglGetProc(char const * procname);

// TODO: query extensions
// TODO: get current window and context

#endif
