#ifndef PINC_OPENGL_H
#define PINC_OPENGL_H

#include "pinc.h"

/// @section init
/// @brief Functions to be called after pinc_incomplete_init

// Function pointer because ISO C99 does not like casting regular pointers to function pointers.
typedef void(*PINC_PFN)(void);

typedef enum {
    // definitely not supported
    pinc_opengl_support_status_none,
    // may or may not be supported - unsure. Opengl is famous for being difficult and platform-dependent to query before attempting to initialize state.
    pinc_opengl_support_status_maybe,
    // definitely supported
    pinc_opengl_support_status_definitely,
} pinc_opengl_support_status;

/// @brief OpenGL context handle.
typedef pinc_object pinc_opengl_context;

// evil enum trickery
#undef pinc_opengl_support_status
#define pinc_opengl_support_status uint32_t

PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_version_supported(pinc_window_backend backend, uint32_t major, uint32_t minor, bool es);

// srgb is already handled by base Pinc framebuffer format.
// Unfortunately, due to certain reasons, the opengl backend
// still has to adhere to Pinc's requirement of only one framebuffer format for all windows (and by proxy all opengl contexts).

// The returned value is more or less an educated guess, depending on the window backend, platform, and graphics drivers.
// For example, the glX extension for the X window system supports querying many of these before creating a context.
// SDL2, on the other hand, does not provide an API for querying context support before creating one.
// For now, these are all treated as exact. That is, a context with the exact value queried / set must be created.
// In the future, there may be a flag to specify if the value is an exact requirement or a minimum requirement.
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_accumulator_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t channel, uint32_t bits);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_alpha_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t bits);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_depth_bits(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t bits);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_samples(pinc_window_backend backend, pinc_framebuffer_format framebuffer, uint32_t samples);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_stereo_buffer(pinc_window_backend backend, pinc_framebuffer_format framebuffer);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_context_debug(pinc_window_backend backend);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_forward_compatible(pinc_window_backend backend);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_robust_access(pinc_window_backend backend);
PINC_EXTERN pinc_opengl_support_status PINC_CALL pinc_query_opengl_reset_isolation(pinc_window_backend backend);

// Returns 0 on failure, which should never happen.
PINC_EXTERN pinc_opengl_context PINC_CALL pinc_opengl_create_context_incomplete(void);

// These fail when the exact configuration is definitely not supported. When it fails, the property is not set, and an error is not triggered.
// If it does not fail, then 
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_accumulator_bits(pinc_opengl_context incomplete_context, uint32_t channel, uint32_t bits);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_alpha_bits(pinc_opengl_context incomplete_context, uint32_t bits);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_depth_bits(pinc_opengl_context incomplete_context, uint32_t bits);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_samples(pinc_opengl_context incomplete_context, uint32_t samples);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_stereo_buffer(pinc_opengl_context incomplete_context, bool stereo);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_context_debug(pinc_opengl_context incomplete_context, bool debug);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_forward_compatible(pinc_opengl_context incomplete_context, bool compatible);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_robust_access(pinc_opengl_context incomplete_context, bool robust);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_reset_isolation(pinc_opengl_context incomplete_context, bool isolation);
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_set_context_version(pinc_opengl_context incomplete_context, uint32_t major, uint32_t minor, bool es, bool core);

// Pinc will try its best to apply the given settings for the context, but they may be different from the requested ones.
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_context_complete(pinc_opengl_context incomplete_context);
PINC_EXTERN void PINC_CALL pinc_opengl_context_deinit(pinc_opengl_context context);

PINC_EXTERN uint32_t PINC_CALL pinc_opengl_get_context_accumulator_bits(pinc_opengl_context incomplete_context, uint32_t channel);
PINC_EXTERN uint32_t PINC_CALL pinc_opengl_get_context_alpha_bits(pinc_opengl_context incomplete_context);
PINC_EXTERN uint32_t PINC_CALL pinc_opengl_get_context_depth_bits(pinc_opengl_context incomplete_context);
PINC_EXTERN uint32_t PINC_CALL pinc_opengl_get_context_samples(pinc_opengl_context incomplete_context);
PINC_EXTERN bool PINC_CALL pinc_opengl_get_context_stereo_buffer(pinc_opengl_context incomplete_context);
PINC_EXTERN bool PINC_CALL pinc_opengl_get_context_context_debug(pinc_opengl_context incomplete_context);
PINC_EXTERN bool PINC_CALL pinc_opengl_get_context_forward_compatible(pinc_opengl_context incomplete_context);
PINC_EXTERN bool PINC_CALL pinc_opengl_get_context_robust_access(pinc_opengl_context incomplete_context);
PINC_EXTERN bool PINC_CALL pinc_opengl_get_context_reset_isolation(pinc_opengl_context incomplete_context);

/// @brief Make pinc's OpenGL context current. Asserts that the current backend is an OpenGL backend.
/// @param window the window whose framebuffer to bind to the opengl context, or 0 if it doesn't matter.
/// @param context the opengl context to bind with. Must be a complete context.
/// @return a pinc return code indicating the success of this function
PINC_EXTERN pinc_return_code PINC_CALL pinc_opengl_make_current(pinc_window window, pinc_opengl_context context);

PINC_EXTERN pinc_window PINC_CALL pinc_opengl_get_current_window(void);
PINC_EXTERN pinc_opengl_context PINC_CALL pinc_opengl_get_current_context(void);

/// @brief Get the function pointer to an OpenGL function. On most platforms, this requires a current OpenGL context.
///     Note that every OpenGL implementation has its own ideas about what function pointers end up with which contexts.
///     In general, it's safest to assume every context has its own set of function pointers.
///     Do not assume a function is available if this returns non-null.
/// @param procname A null terminated string with the name of an OpenGL function.
/// @return the function pointer, or null if it could not be found.
PINC_EXTERN PINC_PFN PINC_CALL pinc_opengl_get_proc(char const * procname);

// TODO: query extensions
// TODO: get current window and context

#endif
