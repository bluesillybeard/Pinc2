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

// Oh goodness me, what on earth were the crazy lads at ARB thinking when they created this mess?

// An enum of OpenGL profiles
// Although the API defines forward compatible compatibility context to be possible, it does nothing at the cost of an extra possible setup. So that possibility is not present in Pinc.
// The OpenGL deprecation model is a borderline actual disaster, at least as far as how many arguments are sparked from it.
// We've tried to make sense of it in the comments here, but in short: you should generally use a core context, unless you have a specific reason to use any other profile.
// Which, there are a lot of specific reasons to use other profiles.
// Also, do not trust that the driver will stop you from using a removed feature. Drivers are known to just not care - they do whatever they want as long as applications still work.
typedef enum {
    // Create a legacy context. This uses the (probably deprecated) "give me the opengl" method that was used until ARB created the concept of context profiles.
    // This may fail if the underlying window backend does not have the option to use the old method, such as when using SDL or EGL.
    // Only really valid for 3.0 and older, but it is technically possible to ask for a newer version.
    PincOpenglContextProfile_legacy,
    // Create a compatibility desktop context. Only valid for versions 3.1 and newer.
    // Make note that under the hood, the way this is implemented is actually a bit messy, due to OpenGL's history.
    // On OpenGL 3.1, it makes a core context then checks for the ARB_compatibility extension. So, if you want 3.1 but this fails, try to make a 3.2 compatibility context instead.
    // On 3.2 and newer, this actually does what it says on the tin: create an OpenGL context with deprecated features enabled.
    // Note that Macos wholly does not support compatibility contexts, and in general compatibility contexts should be avoided where possible.
    PincOpenglContextProfile_compatibility,
    // Create a core desktop context. Effectively, this says "give me something compatible with the version I asked for - nothing more or less"
    // It's worth mentioning that this does not work on MacOS at all.
    PincOpenglContextProfile_core,
    // Creates a forward compatible desktop context. What this means depends on the version:
    // For OpenGl 3.0, this removes all of the same functionality that was removed in 3.1
    // For OpenGL 3.1 and newer, this removes all of the deprecated functionality left in core.
    PincOpenglContextProfile_forward,
} PincOpenglContextProfileEnum;

typedef uint32_t PincOpenglContextProfile;

/// @brief OpenGL context handle.
typedef PincObjectHandle PincOpenglContextHandle;

// If this returns PincOpenglSupportStatus_maybe, then you will have to fall back to the tried and true method of just trying to create a context to see if it works.
// It's meant to be a faster but less accurate way to check if a version is supported, compared to attempting to create a context.
PINC_EXTERN PincOpenglSupportStatus PINC_CALL pincQueryOpenglVersionSupported(PincWindowBackend backend, uint32_t major, uint32_t minor, PincOpenglContextProfile profile);

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
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextRobustAccess(PincOpenglContextHandle incomplete_context_handle, bool robust);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextResetIsolation(PincOpenglContextHandle incomplete_context_handle, bool isolation);
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglSetContextVersion(PincOpenglContextHandle incomplete_context_handle, uint32_t major, uint32_t minor, PincOpenglContextProfile profile);

// Pinc will try its best to apply the given settings for the context, but they may be different from the requested ones.
PINC_EXTERN PincReturnCode PINC_CALL pincOpenglCompleteContext(PincOpenglContextHandle incomplete_context_handle);
PINC_EXTERN void PINC_CALL pincOpenglDeinitContext(PincOpenglContextHandle context_handle);

PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextAccumulatorBits(PincOpenglContextHandle context_handle, uint32_t channel);
PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextAlphaBits(PincOpenglContextHandle context_handle);
PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextDepthBits(PincOpenglContextHandle context_handle);
PINC_EXTERN uint32_t PINC_CALL pincOpenglGetContextSamples(PincOpenglContextHandle context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextStereoBuffer(PincOpenglContextHandle context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextDebug(PincOpenglContextHandle context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextRobustAccess(PincOpenglContextHandle context_handle);
PINC_EXTERN bool PINC_CALL pincOpenglGetContextResetIsolation(PincOpenglContextHandle context_handle);

/// @brief Make pinc's OpenGL context current. Asserts that the current backend is an OpenGL backend.
/// @param window the window whose framebuffer to bind to the opengl context, or 0 if it doesn't matter.
/// @param complete_context_handle the opengl context to bind with. Must be a complete context.
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
