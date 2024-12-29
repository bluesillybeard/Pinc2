// pinc.h - Pinc's entire API

// This header has no includes.

// In general, here is what you can expect from the API:
// - we assume floats are in the single precision IEE754 32 bit format.
// - no structs, those are bad for ABI compatibility
// - typedefs for ID handles

// Usage error policy:
// Usage errors occur when invalid inputs are used in a function. For example, entering 0 for an object handle when it must be defined.
// - Depends on build system settings and application structure
//     - Errors can be set to disabled, rigorous, or light.
//     - If errors are disabled, pinc does not validate any inputs and will crash or behave strangely when used incorrectly
//     - Usage errors will trigger an error function defined by the user.
//     - Rigorous is only meant to be used in debug mode when testing API usage, as it may impact performance significantly. It's meant to be treated like valgrind.
//     - Light has pretty decent performance, and is meant to be potentially shipped to production if performance is not a top concern

// Other error policy:
// - Pinc will trigger a function set by pinc_set_error_callback when something goes wrong outside of the code's direct control. Ex: no GPU available.
//     - These can be disabled, but that is a bad idea unless you really know what you are doing
// - Issues that occur due to pinc itself trigger panics. If this happens to you, please submit an issue with a reproduction test case.
//     If you can't make a minimal reproduction, at least report the error and the context in which it appeared.
// - functions that may trigger an error will return a pinc_error_code

// Memory policy: Ownership is never transferred between Pinc an the application. Pinc has its own allocation management system, and it should never mix with the applications.
// This means that the application will always free its own allocations, and Pinc will never return a pointer unless it was created by the user.

// see settings.md in the root of Pinc's repo for 

// The flow of your code should be roughly like this:
// - Set up preinit things
//     - pinc_preinit_*
// - call pinc_incomplete_init()
// - (optional) Query information and decide settings
//     - This is where the program decides which window backend, framebuffer format, graphics backend, etc to use.
//     - If this step is skipped, Pinc will use default settings for the system it is running on.
//     - pinc_init_query_*
//     - pinc_init_set_*
// - call pinc_complete_init()
// - create objects
//     - windows, graphics objects, whatever else.
//     - these can be created in the main loop as well
// - main loop
//     - call pinc_step()
//     - get user inputs and other events
//     - draw stuff
//     - present window framebuffers

#ifndef PINC_H
#define PINC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// @section options
// @brief build system options and stuff

// API prefix for loading (dllimport or whatever)
#ifndef PINC_EXTERN
#define PINC_EXTERN extern
#endif

// API prefix for exposing (dllexport or whatever)

#ifndef PINC_EXPORT
#define PINC_EXPORT
#endif

// calling convention for pinc functions

#ifndef PINC_CALL
#define PINC_CALL
#endif

// calling conventions for user-supplied functions
// TODO: how does this actually get applied? Apparently it depends on the compiler?

#ifndef PINC_PROC_CALL
#define PINC_PROC_CALL 
#endif

// @section types

/// @brief Window backend
typedef enum {
    pinc_window_backend_any = -1,
    /// @brief No window backend, for doing offline / headless rendering
    pinc_window_backend_none,
    pinc_window_backend_sdl2,
} pinc_window_backend;

// evil enum int trick. This may seem stupid, but remember that C is a volatile language
// where 'enum' means anything from 8 bits to 64 bits depending on all kings of things.
// It isn't even necessarily consistent across different versions or configurations of the same compiler on the same platform.
// Considering one of Pinc's primary goals is to have a predicable ABI for bindings to other languages,
// making all enums a predictable size is paramount.
#undef pinc_window_backend
#define pinc_window_backend uint32_t

typedef enum {
    /// @brief Represents any backend, or an unknown backend. Generally only works for calling pinc_complete_init.
    pinc_graphics_backend_any = 0,
    /// @brief raw OpenGL backend, for using Pinc with OpenGL.
    ///            Requires pinc_opengl.h (or manually declaring the extern functions if you're weird like that.)
    pinc_graphics_backend_raw_opengl,
} pinc_graphics_backend;

#undef pinc_graphics_backend
#define pinc_graphics_backend uint32_t

typedef enum {
    pinc_return_code_pass = 0,
    pinc_return_code_error = 1,
} pinc_return_code;

#undef pinc_return_code
#define pinc_return_code uint32_t

typedef enum {
    pinc_object_type_none = -1,
    pinc_object_type_window = 0,
    pinc_object_type_vertex_attributes,
    pinc_object_type_uniforms,
    pinc_object_type_shaders,
    pinc_object_type_pipeline,
    pinc_object_type_vertex_array,
    pinc_object_type_texture,
} pinc_object_type;

#undef pinc_object_type
#define pinc_object_type uint32_t

/// @brief enumeration of pinc keyboard codes
///     These are not physical, but logical - when the user presses the button labeled 'q' on their keyboard, that's the key reported here.
///     In order words, this ignores the idea of a keyboard layout, and reports based on what the user is typing, not what actual buttons are being pressed.
typedef enum {
    pinc_keyboard_key_unknown = -1,
    pinc_keyboard_key_space = 0,
    pinc_keyboard_key_apostrophe,
    pinc_keyboard_key_comma,
    pinc_keyboard_key_dash,
    pinc_keyboard_key_dot,
    pinc_keyboard_key_slash,
    pinc_keyboard_key_0,
    pinc_keyboard_key_1,
    pinc_keyboard_key_2,
    pinc_keyboard_key_3,
    pinc_keyboard_key_4,
    pinc_keyboard_key_5,
    pinc_keyboard_key_6,
    pinc_keyboard_key_7,
    pinc_keyboard_key_8,
    pinc_keyboard_key_9,
    pinc_keyboard_key_semicolon,
    pinc_keyboard_key_equals,
    pinc_keyboard_key_a,
    pinc_keyboard_key_b,
    pinc_keyboard_key_c,
    pinc_keyboard_key_d,
    pinc_keyboard_key_e,
    pinc_keyboard_key_f,
    pinc_keyboard_key_g,
    pinc_keyboard_key_h,
    pinc_keyboard_key_i,
    pinc_keyboard_key_j,
    pinc_keyboard_key_k,
    pinc_keyboard_key_l,
    pinc_keyboard_key_m,
    pinc_keyboard_key_n,
    pinc_keyboard_key_o,
    pinc_keyboard_key_p,
    pinc_keyboard_key_q,
    pinc_keyboard_key_r,
    pinc_keyboard_key_s,
    pinc_keyboard_key_t,
    pinc_keyboard_key_u,
    pinc_keyboard_key_v,
    pinc_keyboard_key_w,
    pinc_keyboard_key_x,
    pinc_keyboard_key_y,
    pinc_keyboard_key_z,
    pinc_keyboard_key_left_bracket,
    pinc_keyboard_key_backslash,
    pinc_keyboard_key_right_bracket,
    /// @brief The "\`" character. The "~" button on US keyboards.
    pinc_keyboard_key_backtick,
    pinc_keyboard_key_escape,
    pinc_keyboard_key_enter,
    pinc_keyboard_key_tab,
    pinc_keyboard_key_backspace,
    pinc_keyboard_key_insert,
    pinc_keyboard_key_delete,
    pinc_keyboard_key_right,
    pinc_keyboard_key_left,
    pinc_keyboard_key_down,
    pinc_keyboard_key_up,
    pinc_keyboard_key_page_up,
    pinc_keyboard_key_page_down,
    pinc_keyboard_key_home,
    pinc_keyboard_key_end,
    pinc_keyboard_key_caps_lock,
    pinc_keyboard_key_scroll_lock,
    pinc_keyboard_key_num_lock,
    pinc_keyboard_key_print_screen,
    pinc_keyboard_key_pause,
    pinc_keyboard_key_f1,
    pinc_keyboard_key_f2,
    pinc_keyboard_key_f3,
    pinc_keyboard_key_f4,
    pinc_keyboard_key_f5,
    pinc_keyboard_key_f6,
    pinc_keyboard_key_f7,
    pinc_keyboard_key_f8,
    pinc_keyboard_key_f9,
    pinc_keyboard_key_f10,
    pinc_keyboard_key_f11,
    pinc_keyboard_key_f12,
    pinc_keyboard_key_f13,
    pinc_keyboard_key_f14,
    pinc_keyboard_key_f15,
    pinc_keyboard_key_f16,
    pinc_keyboard_key_f17,
    pinc_keyboard_key_f18,
    pinc_keyboard_key_f19,
    pinc_keyboard_key_f20,
    pinc_keyboard_key_f21,
    pinc_keyboard_key_f22,
    pinc_keyboard_key_f23,
    pinc_keyboard_key_f24,
    pinc_keyboard_key_numpad_0,
    pinc_keyboard_key_numpad_1,
    pinc_keyboard_key_numpad_2,
    pinc_keyboard_key_numpad_3,
    pinc_keyboard_key_numpad_4,
    pinc_keyboard_key_numpad_5,
    pinc_keyboard_key_numpad_6,
    pinc_keyboard_key_numpad_7,
    pinc_keyboard_key_numpad_8,
    pinc_keyboard_key_numpad_9,
    pinc_keyboard_key_numpad_dot,
    pinc_keyboard_key_numpad_slash,
    pinc_keyboard_key_numpad_asterisk,
    pinc_keyboard_key_numpad_dash,
    pinc_keyboard_key_numpad_plus,
    pinc_keyboard_key_numpad_enter,
    pinc_keyboard_key_numpad_equal,
    pinc_keyboard_key_left_shift,
    pinc_keyboard_key_left_control,
    pinc_keyboard_key_left_alt,
    /// @brief On many keyboards, this is a windows icon and is generally called "the windows button"
    pinc_keyboard_key_left_super,
    pinc_keyboard_key_right_shift,
    pinc_keyboard_key_right_control,
    pinc_keyboard_key_right_alt,
    /// @brief On many keyboards, this is a windows icon and is generally called "the windows button". Most keyboards only have the one on the left, not this one.
    pinc_keyboard_key_right_super,
    /// @brief On many keyboards, this is the button next to right control
    pinc_keyboard_key_menu,
    /// @brief This is for convenience
    pinc_keyboard_key_count,
} pinc_keyboard_key;

#undef pinc_keyboard_key
#define pinc_keyboard_key uint32_t

typedef enum {
    // A basic color space with generally vague semantics. In general, bugger number = brighter output.
    pinc_color_space_basic,
    // Linear color space where the output magnitude is directly correlated with pixel values 
    pinc_color_space_linear,
    // Some kind of perceptual color space, usually with a gamma value of 2.2 but it often may not be.
    pinc_color_space_perceptual,
    // srgb color space, or a grayscale equivalent. This is similar to perceptual with a gamma value of 2.2
    pinc_color_space_srgb
} pinc_color_space;

#undef pinc_color_space
#define pinc_color_space uint32_t

/// @section IDs

// framebuffer formats are transferrable between window and graphics backends, but not between different runs of the application.
typedef int32_t pinc_framebuffer_format;

// objects are non-transferrable between runs.
typedef uint32_t pinc_object;

typedef pinc_object pinc_window;

/// @section initialization functions

/// @subsection preinit functions

/// @brief Error callback. message_buf is null terminated for convenience. message_buf is temporary and a reference to it should not be kept.
typedef void ( PINC_PROC_CALL * pinc_error_callback) (uint8_t const * message_buf, uintptr_t message_len);

/// @brief Set a function called when external issues occur. This should be set for ALL pinc applications. It is technically optional.
PINC_EXTERN void PINC_CALL pinc_preinit_set_error_callback(pinc_error_callback callback);

/// @brief Set a function called when issues withing Pinc occur. This is completely optional.
///     It exists primarily for language and engines with their own panic reporting / debugging system. 
///     It is also used for testing that panics ARE caught.
///     Too often, testing cases that are supposed to panic is not simple, so this function makes that a possibility.
///     After a panic, pinc_deinit() should be called and Pinc should be completely re-initialized from scratch in order to restore any broken state.
PINC_EXTERN void PINC_CALL pinc_preinit_set_panic_callback(pinc_error_callback callback);

/// @brief Set a function called when pinc usage errors occur. This is completely optional.
///     It exists primarily for language and engines with their own panic reporting / debugging system. 
PINC_EXTERN void PINC_CALL pinc_preinit_set_user_error_callback(pinc_error_callback callback);

/// @brief Callback to allocate some memory. Aligned depending on platform such that any structure can be placed into this memory.
///     Identical to libc's malloc function.
/// @param alloc_size_bytes Number of bytes to allocate.
/// @return A pointer to the memory.
typedef void* ( PINC_PROC_CALL * pinc_alloc_callback) (void* userPtr, size_t alloc_size_bytes);

/// @brief Callback to allocate some memory with explicit alignment.
/// @param alloc_size_bytes Number of bytes to allocate. Must be a multiple of alignment.
/// @param alignment Alignment requirement. Must be a power of 2.
/// @return A pointer to the allocated memory.
typedef void* ( PINC_PROC_CALL * pinc_alloc_aligned_callback) (void* userPtr, size_t alloc_size_bytes, size_t alignment);

/// @brief Reallocate some memory with a different size
/// @param ptr Pointer to the memory to reallocate. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param old_alloc_size_bytes Old size of the allocation. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
/// @param alloc_size_bytes The new size of the allocation
/// @return A pointer to this memory. May be the same or different from pointer.
typedef void* ( PINC_PROC_CALL * pinc_realloc_callback) (void* userPtr, void* ptr, size_t old_alloc_size_bytes, size_t alloc_size_bytes);

/// @brief Free some memory
/// @param ptr Pointer to free. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param alloc_size_bytes Number of bytes to free. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
typedef void ( PINC_PROC_CALL * pinc_free_callback) (void* userPtr, void* ptr, size_t alloc_size_bytes);

/// @brief Set allocation callbacks. Must be called before incomplete_init, or never. The type of each proc has more information. They either must all be set, or all null.
PINC_EXTERN void PINC_CALL pinc_preinit_set_alloc_callbacks(void* user_ptr, pinc_alloc_callback alloc, pinc_alloc_aligned_callback alloc_aligned, pinc_realloc_callback realloc, pinc_free_callback free);

/// @brief Begin the initialization process
/// @return the success or failure of this function call. Failures are likely caused by external factors (ex: no window backends) or a failed allocation.
PINC_EXTERN pinc_return_code PINC_CALL pinc_incomplete_init(void);

/// @subsection full initialization functions
/// @brief The query functions work after initialization, although most of them are useless after the fact

/// @brief Query the window backends available for this system
/// @param backend_dest a pointer to a memory buffer to write to, or null
/// @param capacity the amount of space available for window backends to be written. ignored if backend_dest is null
/// @return the number of window backends that are available
PINC_EXTERN uint32_t PINC_CALL pinc_query_window_backends(pinc_window_backend* backend_dest, uint32_t capacity);

/// @brief Query the graphics backends available for a given window backend.
/// @param window_backend The window backend to query. Must be a backend from query_window_backends.
/// @param backend_dest a pointer to a memory buffer to write to, or null
/// @param capacity the amount of space available for graphics backends to be written. Ignored if backend_dest is null
/// @return the number of graphics backends that are available
PINC_EXTERN uint32_t PINC_CALL pinc_query_graphics_backends(pinc_window_backend window_backend, pinc_graphics_backend* backend_dest, uint32_t capacity);

/// @brief Query the frame buffer format ids supported by a window backend and one of its supported graphics backend
/// @param window_backend the window backend to query from. Must be a supported window backend
/// @param graphics_backend the graphics backend to query from. Must be a supported graphics backend of the window backend
/// @param ids_dest a buffer to output the framebuffer format ids, or null
/// @param capacity the amount of space available for framebuffer format ids to be written. Ignored if dest is null 
/// @return the number of framebuffer format ids supported.
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_ids(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format* ids_dest, uint32_t capacity);

/// @brief Query the number of channels that a frame buffer format supports
/// @param format_id the id of the framebuffer format. Must be from query_framebuffer_format_ids
/// @return the number of channels. 1 for grayscale, 2 for grayscale+alpha, 3 for RGB, 4 for RGBA.
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id);

/// @brief Query the bit depth of a channel of a frame buffer format.
/// @param format_id the frame buffer format to query.
/// @param channel the channel index to query. Must be between 0 and channels-1 inclusively.
/// @return the number of bits per pixel in this channel.
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, uint32_t channel);

PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_depth_buffer_bits(pinc_framebuffer_format format_id);

PINC_EXTERN pinc_color_space PINC_CALL pinc_query_framebuffer_format_color_space(pinc_framebuffer_format format_id);

// maximum number of samples for MSAA
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_max_samples(pinc_framebuffer_format format_id);

// This requires the graphics backend for things like GLFW where only one OpenGL window may exist, but the Vulkan backend supports many windows.
// the function itself is also useful for many console platforms where the entire idea of a window separate from the display itself doesn't make sense.
PINC_EXTERN uint32_t PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend);

// use -1 to use a default framebuffer format
// samples is for MSAA.
PINC_EXTERN pinc_return_code PINC_CALL pinc_complete_init(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format framebuffer_format_id, uint32_t samples);

/// @subsection post initialization related functions

PINC_EXTERN void PINC_CALL pinc_deinit(void);

PINC_EXTERN pinc_window_backend PINC_CALL pinc_query_set_window_backend(void);

PINC_EXTERN pinc_graphics_backend PINC_CALL pinc_query_set_graphics_backend(void);

PINC_EXTERN uint32_t PINC_CALL pinc_query_set_framebuffer_format(void);

PINC_EXTERN pinc_object_type PINC_CALL pinc_get_object_type(pinc_object obj);

PINC_EXTERN bool PINC_CALL pinc_get_object_complete(pinc_object obj);

/// @section windows

PINC_EXTERN pinc_window PINC_CALL pinc_window_create_incomplete(void);

PINC_EXTERN pinc_return_code PINC_CALL pinc_window_complete(pinc_window window);

// window properties:
// ALL window properties have defaults so users can get up and running ASAP. However, many of those defaults cannot be determined until after some point.
// r -> can be read at any time. It has a default [default is in square brackets]
// rc -> can only be read after the default has been determined, so it needs a has_[property] function
// w -> can be set at any time
// r means it just has a get function, but rc properties have both a get and a has.
// - string title (rw)
//     - default is "Pinc window [window object id]"
// - int width (rcw)
//     - This is the size of the actual pixel buffer
//     - default is determined on completion
// - int height (rcw)
//     - This is the size of the actual pixel buffer
//     - default is determined on completion
// - float scale factor (rc)
//     - this is the system scale. For example, if the user wants everything to be 1.5x larger on screen, this is 1.5
//     - This is on the window so the user can, in theory, set a different scale for each window.
//     - This can be very difficult to obtain before a window is open on the desktop, and even then the system scale may not be set.
//     - If not set, you can probably assume 1.0
// - bool resizable (rw) [true]
// - bool minimized (rw) [false]
//     - when minimized, a window is in the system tray / app switcher / whatever, but is not open on the desktop
// - bool maximized (rw) [false]
//     - when maximized, the window's size is set to the largest it can get without covering elements of the desktop environment
// - bool fullscreen (rw) [false]
// - bool focused (rw) [false]
// - bool hidden (rw) [false]
//     - when hidden, a window cannot be seen anywhere to the user (at least not directly), but is still secretly open.
// - bool vsync (rw) [true]
//     - note: should be set before completion, but may be able to be set after.

// if title_len is zero, title_buf is assumed to be null terminated, or itself null for an empty title.
PINC_EXTERN void PINC_CALL pinc_window_set_title(pinc_window window, const char* title_buf, uint32_t title_len);

PINC_EXTERN uint32_t PINC_CALL pinc_window_get_title(pinc_window window, char* title_buf, uint32_t title_capacity);

/// @brief set the width of a window, in pixels.
/// @param window the window whose width to set. Asserts the object is valid, and is a window.
/// @param width the width to set.
PINC_EXTERN void PINC_CALL pinc_window_set_width(pinc_window window, uint32_t width);

/// @brief get the width of a window, in pixels
/// @param window the window whose width to get. Asserts the object is valid, is a window, and has its width set (see pinc_window_has_width)
/// @return the width of the window
PINC_EXTERN uint32_t PINC_CALL pinc_window_get_width(pinc_window window);

/// @brief get if a window has its width defined. A windows width will become defined either when completed, or when set using pinc_window_set_width
/// @param window the window. Asserts the object is valid, and is a window
/// @return 1 if the windows width is set, 0 if not.
PINC_EXTERN uint32_t PINC_CALL pinc_window_has_width(pinc_window window);

/// @brief set the height of a window, in pixels
/// @param window the window whose height to set. Asserts the object is valid, and is a window
/// @param height the height to set.
PINC_EXTERN void PINC_CALL pinc_window_set_height(pinc_window window, uint32_t height);

/// @brief get the height of a window, in pixels
/// @param window the window whose height to get. Asserts the object is valid, is a window, and has its height set (see pinc_window_has_height)
/// @return the height of the window
PINC_EXTERN uint32_t PINC_CALL pinc_window_get_height(pinc_window window);

/// @brief get if a window has its height defined. A windows height will become defined either when completed, or when set using pinc_window_set_height
/// @param window the window. Asserts the object is valid, and is a window
/// @return 1 if the windows height is set, 0 if not.
PINC_EXTERN bool PINC_CALL pinc_window_has_height(pinc_window window);

/// @brief get the scale factor of a window. This is set by the user when they want to "zoom in" - a value of 1.5 should make everything appear 1.5x larger.
/// @param window the window. Asserts the object is valid, is a window, and has its scale factor set (see pinc_window_has_scale_factor)
/// @return the scale factor of this window.
PINC_EXTERN float PINC_CALL pinc_window_get_scale_factor(pinc_window window);

/// @brief get if a window has its scale factor defined. Whether this is true depends on the backend, whether the scale is set, and if the window is complete.
///        In general, it is safe to assume 1 unless it is set otherwise.
/// @param window the window. Asserts the object is valid, and is a window
/// @return 1 if the windows scale factor is set, 0 if not.
PINC_EXTERN int PINC_CALL pinc_window_has_scale_factor(pinc_window window);

/// @brief set if a window is resizable or not
/// @param window the window. Asserts the object is valid, is a window, has its width defined (see pinc_window_has_width), and has its height defined (see pinc_window_has_height)
/// @param resizable 1 if the window is resizable, 0 if not.
PINC_EXTERN void PINC_CALL pinc_window_set_resizable(pinc_window window, bool resizable);

/// @brief get if a window is resizable or not
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is resizable, 0 if not.
PINC_EXTERN bool PINC_CALL pinc_window_get_resizable(pinc_window window);

/// @brief set if a window is minimized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param minimized 1 if the window is minimized, 0 if not
PINC_EXTERN void PINC_CALL pinc_window_set_minimized(pinc_window window, bool minimized);

/// @brief get if a window is minimized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is minimized, 0 if not
PINC_EXTERN bool PINC_CALL pinc_window_get_minimized(pinc_window window);

/// @brief set if a window is maximized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param maximized 1 if the window is maximized, 0 if not
PINC_EXTERN void PINC_CALL pinc_window_set_maximized(pinc_window window, bool maximized);

/// @brief get if a window is maximized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is maximized, 0 if not
PINC_EXTERN bool PINC_CALL pinc_window_get_maximized(pinc_window window);

/// @brief set if a window is fullscreen or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param fullscreen 1 if the window is fullscreen, 0 if not
PINC_EXTERN void PINC_CALL pinc_window_set_fullscreen(pinc_window window, bool fullscreen);

/// @brief get if a window is fullscreen or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is fullscreen, 0 if not
PINC_EXTERN bool PINC_CALL pinc_window_get_fullscreen(pinc_window window);

/// @brief set if a window is focused or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param focused 1 if the window is focused, 0 if not
PINC_EXTERN void PINC_CALL pinc_window_set_focused(pinc_window window, bool focused);

/// @brief get if a window is focused or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is focused, 0 if not
PINC_EXTERN bool PINC_CALL pinc_window_get_focused(pinc_window window);

/// @brief set if a window is hidden or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param hidden 1 if the window is hidden, 0 if not
PINC_EXTERN void PINC_CALL pinc_window_set_hidden(pinc_window window, bool hidden);

/// @brief get if a window is hidden or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is hidden, 0 if not
PINC_EXTERN bool PINC_CALL pinc_window_get_hidden(pinc_window window);

// It is best practice to set vsync before completing the window
// as some systems do not support changing vsync after a window is completed.
// vsync is true by default on systems that support it.
PINC_EXTERN pinc_return_code PINC_CALL pinc_window_set_vsync(pinc_window window, bool sync);

PINC_EXTERN bool PINC_CALL pinc_get_vsync(pinc_window window);

/// @brief Present the framebuffer of a given window and prepares a backbuffer to draw on.
///        The number of backbuffers depends on the graphics backend, but it's generally 2 or 3.
/// @param window the window whose framebuffer to present.
PINC_EXTERN void PINC_CALL pinc_window_present_framebuffer(pinc_window window);

/// @section user IO

// TODO: Clipboard

/// @brief Get the state of a mouse button
/// @param button the button to check. Generally, 0 is the left button, 1 is the right, and 2 is the middle
/// @return 1 if the button is pressed, 0 if it is not pressed OR if this application has no focused windows.
PINC_EXTERN bool PINC_CALL pinc_mouse_button_get(int button);

/// @brief Get the state of a keyboard key.
/// @param button A value of pinc_keyboard_key to check
/// @return 1 if the key is pressed, 0 if it is not.
PINC_EXTERN bool PINC_CALL pinc_keyboard_key_get(pinc_keyboard_key button);

/// @brief Get the cursor X position relative to the window the cursor is currently in
/// @return the X position. 0 on the left to width-1 on the right.
PINC_EXTERN uint32_t PINC_CALL pinc_get_cursor_x(void);

/// @brief get the cursor Y position relative to the window the cursor is currently in
/// @return the X position. 0 on the top to height-1 on the bottom.
PINC_EXTERN uint32_t PINC_CALL pinc_get_cursor_y(void);

// Get the window the cursor is currently in, or 0 if the cursor is not over a window in this application.
PINC_EXTERN pinc_window PINC_CALL pinc_get_cursor_window(void);

/// @section main loop & events

/// @brief Flushes internal buffers and collects user input
PINC_EXTERN void PINC_CALL pinc_step(void);

/// @brief Gets if a window was signalled to close during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if a window was signalled to close in the last step, 0 otherwise.
PINC_EXTERN bool PINC_CALL pinc_event_window_closed(pinc_window window);

/// @brief Get if there was a mouse press/release from the last step
/// @param window The window to check - although all windows share the same cursor,
///     it is possible for multiple windows to be clicked in the same step.
/// @return 1 if there any mouse buttons were pressed or released, 0 otherwise.
PINC_EXTERN bool PINC_CALL pinc_event_window_mouse_button(pinc_window window);

/// @brief Gets if a window was resized during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return if the window was resized during the last step
PINC_EXTERN bool PINC_CALL pinc_event_window_resized(pinc_window window);

/// @brief Gets if a window received input focus in the last step.
///        If a window lost and gained focus in the same step, it is safe to assume that window is not focused.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the window was focused in the last step, 0 otherwise.
PINC_EXTERN bool PINC_CALL pinc_event_window_focused(pinc_window window);

/// @brief gets if a window lost input focus in the last step, unless it regained focus in that same step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the window was focused in the last step, 0 otherwise.
PINC_EXTERN bool PINC_CALL pinc_event_window_unfocused(pinc_window window);

/// @brief gets if a window should be redrawn from last step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the window should be redrawn this step, 0 otherwise.
PINC_EXTERN bool PINC_CALL pinc_event_window_exposed(pinc_window window);

// Keyboard events require a window because it's possible for multiple windows to have key events in a single step

/// @brief Get key events in the last step. Key events are generateted for presses, releases, and repeats. release v.s press or repeat is detected by getting that key's state after the event.
/// @param window the window to get events from. Only accepts complete windows
/// @param key_buffer a buffer to place the key events into. Events happened from lowest index to highest index. accepts null.
/// @param capacity the number of keys the buffer can hold. Ignored if key_buffer is null
/// @return the number of key events
PINC_EXTERN uint32_t PINC_CALL pinc_event_window_keyboard_button_get(pinc_window window, pinc_keyboard_key* key_buffer, uint32_t capacity);

/// @brief Get whether key events from the last step were repeats or not. Index-matched with the output of event_window_keyboard_button_get.
/// @param window the window to get events from. Only accepts complete windows
/// @param repeat_buffer a buffer to place the repeat values into. Accepts null.
/// @param capacity the number of values repeat_buffer can hold
/// @return the number of key events
PINC_EXTERN uint32_t PINC_CALL pinc_event_window_keyboard_button_get_repeat(pinc_window window, bool* repeat_buffer, uint32_t capacity);

/// @brief Get if the cursor moved within a window during the last step.
///        Requires a window because it's possible for the cursor to move from one window to another window in a single step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the cursor moved in this window, 0 if not.
PINC_EXTERN bool PINC_CALL pinc_event_window_cursor_move(pinc_window window);

/// @brief Get if the cursor has left a window during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the cursor left the window, 0 if not.
PINC_EXTERN bool PINC_CALL pinc_event_window_cursor_exit(pinc_window window);

/// @brief Get if the cursor has entered a window during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the cursor entered the window, 0 if not.
PINC_EXTERN bool PINC_CALL pinc_event_window_cursor_enter(pinc_window window);

// returns text that was typed into a window since the last step. Encoded in UTF 8.
PINC_EXTERN uint32_t PINC_CALL pinc_event_window_text_get(pinc_window window, uint8_t* return_buf, uint32_t capacity);

// TODO: doc
PINC_EXTERN float PINC_CALL pinc_window_scroll_vertical(pinc_window window);

// TODO: doc
PINC_EXTERN float PINC_CALL pinc_window_scroll_horizontal(pinc_window window);


#endif
