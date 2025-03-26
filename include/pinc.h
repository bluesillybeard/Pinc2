// pinc.h - Pinc's entire API

// This header has no includes.

// In general, here is what you can expect from the API:
// - we assume floats are in the single precision IEE754 32 bit format.
// - no structs, at least for now. Structs can cause ABI issues, and can be harder to deal with when making bindings.
// - typedefs for ID handles

// Error Policy:
// Pinc has 5 types of errors, all of which can be enabled, disabled, and configured to call user callbacks.
// Ordered from lowest to biggest performance impact:
// - External error: an error that occurred from an external library. This either indicates an issue with the system running the program, or within Pinc.
// - Assert error: an internal assert failed, this is an issue within Pinc.
// - User error: an error in the program's usage of Pinc.
// - Sanitize error: similar to adding "-fsanitize=address,undefined" to Pinc's flags - general validation checks for integer overflows and such
// - Validate error: an error that is tough to check, such as memory allocation tracking, global state validation, etc.

// In general, here are some useful configurations:
// sanitize: External, Assert, User, Sanitize, and Validation are enabled - worst performance possible for maximum validation
// debug: External, Assert, User, and Sanitize - mediocre performance for debugging purposes
// test: External, Assert, User - decent performance for general testing, this is the default!
// release: External, Assert - pretty good performance for distribution
// speed: (all disabled) - maximize performance over everything else, absolutely zero error checking of any kind

// Assert, and User errors are often not recoverable, and even if your callback doesn't, Pinc will call the platform's assert/panic/crash function.
// Sanitize and Validate errors are theoretically recoverable, however Pinc (currently) will call the platform's assert function if you don't.
// External errors are usually recoverable, as long as your program has some way to gracefully handle them.

// Memory policy: Ownership is never transferred between Pinc an the application. Pinc has its own allocation management system, and it should never mix with the applications.
// This means that the application will always free its own allocations, and Pinc will never return a pointer unless it was created by the user.

// see settings.md in the root of Pinc's repo for 

// The flow of your code should be roughly like this:
// - Set up preinit things
//     - pinc_preinit_*
// - call pinc_incomplete_init()
// - (optional) Query information and decide settings
//     - This is where the program decides which window backend, framebuffer format, graphics api, etc to use.
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
    pinc_window_backend_any = 0,
    /// @brief No window backend, for doing offline / headless rendering
    pinc_window_backend_none,
    pinc_window_backend_sdl2,
} pinc_window_backend_enum;

// This may seem stupid, but remember that C is a volatile language
// where 'enum' means anything from 8 bits to 64 bits depending on all kings of things.
// It isn't even necessarily consistent across different versions or configurations of the same compiler on the same platform.
// Considering one of Pinc's primary goals is to have a predicable ABI for bindings to other languages,
// making all enums a predictable size is paramount.
typedef uint32_t pinc_window_backend;

typedef enum {
    /// @brief Represents any backend, or an unknown backend. Generally only works for calling pinc_complete_init.
    pinc_graphics_api_any = 0,
    pinc_graphics_api_opengl,
} pinc_graphics_api_enum;

typedef uint32_t pinc_graphics_api;

typedef enum {
    pinc_return_code_pass = 0,
    pinc_return_code_error = 1,
} pinc_return_code_enum;

typedef uint32_t pinc_return_code;

typedef enum {
    pinc_object_type_none = 0,
    pinc_object_type_window,
    pinc_object_type_framebufferFormat,
    pinc_object_type_incompleteGlContext,
    pinc_object_type_glContext,
} pinc_object_type_enum;

typedef uint32_t pinc_object_type;

// TODO: this needs better docs and clarification on semantics and exactly when these events are triggered.
typedef enum {
    // A window was signalled to close.
    pinc_event_type_close_signal,
    // Mouse button state changed.
    pinc_event_type_mouse_button,
    // A window was resized.
    pinc_event_type_resize,
    // The current window with focus changed. For now, only one window can be focused at a time. This event carries the new focused window as wel las the old one.
    pinc_event_type_focus,
    // The WM / Compositor is explicitly requesting a screen refresh. DO NOT rely on this for all screen refreshes.
    pinc_event_type_exposure,
    // Keyboard button state change or repeat.
    pinc_event_type_keyboard_button,
    // Mouse cursor moved.
    pinc_event_type_cursor_move,
    // Mouse cursor moved from one window to another
    pinc_event_type_cursor_transition,
    // Text was typed
    pinc_event_type_text_input,
    // Scroll wheel / pad
    pinc_event_type_scroll,
} pinc_event_type_enum;

typedef uint32_t pinc_event_type;

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

// framebuffer formats are transferrable between window and graphics apis, but not between different runs of the application.
// A framebuffer format simply describes the pixel output values on the window itself. It does not include things like a depth buffer or MSAA.
typedef uint32_t pinc_framebuffer_format;

// null framebuffer format ID
#define PINC_FRAMEBUFFER_FORMAT_NULL 0


// objects are non-transferrable between runs.
typedef uint32_t pinc_object;

typedef pinc_object pinc_window;

/// @section initialization functions

/// @subsection preinit functions

/// @brief See error policy at the top of pinc.h
typedef enum {
    /// @brief Unknown error type, this is something should almost never happen. Usually this is called when Pinc devs get lazy and forget to implement something.
    pinc_error_type_unknown = -1,
    pinc_error_type_external = 0,
    pinc_error_type_assert,
    pinc_error_type_user,
    pinc_error_type_sanitize,
    pinc_error_type_validate,
} pinc_error_type;

#undef pinc_error_type
#define pinc_error_type int32_t

/// @brief Error callback. message_buf will be null terminated. message_buf is temporary and a reference to it should not be kept.
typedef void ( PINC_PROC_CALL * pinc_error_callback) (uint8_t const * message_buf, uintptr_t message_len, pinc_error_type error_type);

/// @brief Set the function for handling errors. This is optional, however Pinc just calls assert(0) if this is not set.
PINC_EXTERN void PINC_CALL pinc_preinit_set_error_callback(pinc_error_callback callback);

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

PINC_EXTERN bool PINC_CALL pinc_query_window_backend_support(pinc_window_backend window_backend);

/// @brief Query the default window backend for this system.
/// @return The default window backend.
PINC_EXTERN pinc_window_backend PINC_CALL pinc_query_window_backend_default(void);

PINC_EXTERN bool PINC_CALL pinc_query_graphics_api_support(pinc_window_backend window_backend, pinc_graphics_api graphics_api);

/// @brief Query the default graphics api for this system, given a window backend.
///        The window backend is required because it's used to determine the best graphics api for the system.
///        use pinc_window_backend_any to query the default window backend.
/// @return the default graphics api.
PINC_EXTERN pinc_graphics_api PINC_CALL pinc_query_graphics_api_default(pinc_window_backend window_backend);

/// @brief Query the default framebuffer format that would be chosen for a given window and graphics api.
/// @param window_backend the window backend to query. Must be a supported graphics api of the window backend.
/// @param graphics_api the graphics api to query.
/// @return the default framebuffer format.
PINC_EXTERN pinc_framebuffer_format PINC_CALL pinc_query_framebuffer_format_default(pinc_window_backend window_backend, pinc_graphics_api graphics_api);

/// @brief Query the frame buffer format ids supported by a window backend and one of its supported graphics api.
/// @param window_backend the window backend to query from. Must be a supported window backend.
/// @param graphics_api the graphics api to query from. Must be a supported graphics api of the window backend, or pinc_graphics_api_none to use the default one.
/// @param ids_dest a buffer to output the framebuffer format ids, or null to just query the number of formats.
/// @param capacity the amount of space available for framebuffer format ids to be written. Ignored if dest is null.
/// @return the number of framebuffer format ids supported.
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_ids(pinc_window_backend window_backend, pinc_graphics_api graphics_api, pinc_framebuffer_format* ids_dest, uint32_t capacity);

/// @brief Query the number of channels that a frame buffer format supports
/// @param format_id the id of the framebuffer format.
/// @return the number of channels. 1 for grayscale, 2 for grayscale+alpha, 3 for RGB. Transparent windows (RGBA) is not yet supported.
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id);

/// @brief Query the bit depth of a channel of a frame buffer format.
/// @param format_id the frame buffer format to query.
/// @param channel the channel index to query. Must be between 0 and channels-1 inclusively.
/// @return the number of bits per pixel in this channel.
PINC_EXTERN uint32_t PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, uint32_t channel);

PINC_EXTERN pinc_color_space PINC_CALL pinc_query_framebuffer_format_color_space(pinc_framebuffer_format format_id);

// Many platforms (web & most consoles) can only have a certain number of windows open at a time. Most of the time this is one,
// but it's possible to set up a web template with multiple canvases and there are consoles with 2 screens (like the nintendo DS) where it makes sense to treat them as separate windows.
// Returns 0 if there is no reasonable limit (the limit is not a specific number, and you'll probably never encounter related issues in this case)
PINC_EXTERN uint32_t PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend);

// Null framebuffer format is a shortcut to use the default framebuffer format.
// samples is for MSAA. 1 is guaranteed to be supported
// A depth bits of 0 means no depth buffer.
PINC_EXTERN pinc_return_code PINC_CALL pinc_complete_init(pinc_window_backend window_backend, pinc_graphics_api graphics_api, pinc_framebuffer_format framebuffer_format_id, uint32_t samples, uint32_t depth_buffer_bits);

/// @subsection post initialization related functions

PINC_EXTERN void PINC_CALL pinc_deinit(void);

PINC_EXTERN pinc_window_backend PINC_CALL pinc_query_set_window_backend(void);

PINC_EXTERN pinc_graphics_api PINC_CALL pinc_query_set_graphics_api(void);

PINC_EXTERN pinc_framebuffer_format PINC_CALL pinc_query_set_framebuffer_format(void);

PINC_EXTERN pinc_object_type PINC_CALL pinc_get_object_type(pinc_object obj);

PINC_EXTERN bool PINC_CALL pinc_get_object_complete(pinc_object id);

PINC_EXTERN void PINC_CALL pinc_set_object_user_data(pinc_object obj, void* user_data);

PINC_EXTERN void* PINC_CALL pinc_get_object_user_data(pinc_object obj);

/// @section windows

PINC_EXTERN pinc_window PINC_CALL pinc_window_create_incomplete(void);

PINC_EXTERN pinc_return_code PINC_CALL pinc_window_complete(pinc_window window);

// Deinit / close / destroy a window object.
PINC_EXTERN void PINC_CALL pinc_window_deinit(pinc_window window);

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

// if title_len is zero, title_buf is assumed to be null terminated, or itself null for an empty title.
PINC_EXTERN void PINC_CALL pinc_window_set_title(pinc_window window, const char* title_buf, uint32_t title_len);

PINC_EXTERN uint32_t PINC_CALL pinc_window_get_title(pinc_window window, char* title_buf, uint32_t title_capacity);

/// @brief set the width of a window, in pixels.
/// @param window the window whose width to set. Asserts the object is valid, and is a window.
/// @param width the width to set.
PINC_EXTERN void PINC_CALL pinc_window_set_width(pinc_window window, uint32_t width);

/// @brief get the width of a window's drawable area, in pixels
/// @param window the window whose width to get. Asserts the object is valid, is a window, and has its width set (see pinc_window_has_width)
/// @return the width of the window
PINC_EXTERN uint32_t PINC_CALL pinc_window_get_width(pinc_window window);

/// @brief get if a window has its width defined. A windows width will become defined either when completed, or when set using pinc_window_set_width
/// @param window the window. Asserts the object is valid, and is a window
/// @return true if the windows width is set, false if not.
PINC_EXTERN bool PINC_CALL pinc_window_has_width(pinc_window window);

/// @brief set the height of a window's drawable area, in pixels
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
/// @return true if the windows scale factor is set, false if not.
PINC_EXTERN bool PINC_CALL pinc_window_has_scale_factor(pinc_window window);

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

// vsync is true by default on systems that support it.
// Technically, vsync is generally either bound to a window or a graphics context.
// Any half-decently modern graphics API supports setting vsync arbitrary at any time,
// However some of them (*cough cough* OpenGL *cough cough*) are a bit more picky in how vsync is handled.
// In general, call this function right after pinc_complete_init to be safe,
// and call it again before present_framebuffer in hopes that the underlying API supports modifying vsync at runtime.
// An error from this function just means vsync couldn't be changed, and is otherwise harmless.
PINC_EXTERN pinc_return_code PINC_CALL pinc_set_vsync(bool sync);

PINC_EXTERN bool PINC_CALL pinc_get_vsync(void);

/// @brief Present the framebuffer of a given window and prepares a backbuffer to draw on.
///        The number of backbuffers depends on the graphics api, but it's generally 2 or 3.
/// @param window the window whose framebuffer to present.
PINC_EXTERN void PINC_CALL pinc_window_present_framebuffer(pinc_window window);

/// @section user IO

// TODO: Clipboard

/// @brief Get the state of a mouse button
/// @param button the button to check. 0 is the left button, 1 is the right, 2 is the middle, 3 is back and 4 is forward.
/// @return 1 if the button is pressed, 0 if it is not pressed OR if this application has no focused windows.
PINC_EXTERN bool PINC_CALL pinc_mouse_button_get(uint32_t button);

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

// Get the window that currently has focus
PINC_EXTERN pinc_window PINC_CALL pinc_get_focus_window(void);

/// @section main loop & events

/// @brief Flushes internal buffers and collects user input
PINC_EXTERN void PINC_CALL pinc_step(void);

PINC_EXTERN uint32_t PINC_CALL pinc_event_get_num(void);

PINC_EXTERN pinc_event_type PINC_CALL pinc_event_get_type(uint32_t event_index);

/// @brief Returns the window that had focus at the time of the event. This is not necessarily the currently focused window.
/// @return The window that was focused when the event happened, or 0 if no window was focused.
PINC_EXTERN pinc_window PINC_CALL pinc_event_get_window(uint32_t event_index);

PINC_EXTERN int64_t PINC_CALL pinc_event_get_timestamp_unix_millis(uint32_t event_index);

// the window that received the close signal
PINC_EXTERN pinc_window PINC_CALL pinc_event_close_signal_window(uint32_t event_index);

/// @brief Get the old state of the mouse buttons before the event ocurred. Only defined for mouse button events.
/// @return The state of the mouse buttons in a bitfield. The 5 bits, from least to most significant, are left, right, middle, back, and forward respectively.
PINC_EXTERN uint32_t PINC_CALL pinc_event_mouse_button_old_state(uint32_t event_index);

/// @brief Get the new state of the mouse buttons after the event ocurred. Only defined for mouse button events.
/// @return The state of the mouse buttons in a bitfield. The 5 bits, from least to most significant, are left, right, middle, back, and forward respectively.
PINC_EXTERN uint32_t PINC_CALL pinc_event_mouse_button_state(uint32_t event_index);

// The size of the window before it was resized
PINC_EXTERN uint32_t PINC_CALL pinc_event_resize_old_width(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pinc_event_resize_old_height(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pinc_event_resize_width(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pinc_event_resize_height(uint32_t event_index);

PINC_EXTERN pinc_window PINC_CALL pinc_event_resize_window(uint32_t event_index);

// The window that was focused before the event - will always return the same thing as pinc_event_get_window, assuming a valid focus event.
PINC_EXTERN pinc_window PINC_CALL pinc_event_focus_old_window(uint32_t event_index);

// The window that gained focus
PINC_EXTERN pinc_window PINC_CALL pinc_event_focus_window(uint32_t event_index);

// The top left of the exposed area
PINC_EXTERN uint32_t PINC_CALL pinc_event_exposure_x(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pinc_event_exposure_y(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pinc_event_exposure_width(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pinc_event_exposure_height(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pinc_event_exposure_window(uint32_t event_index);

// The button whose state changed
PINC_EXTERN pinc_keyboard_key PINC_CALL pinc_event_keyboard_button(uint32_t event_index);

// The state of the button after the event
PINC_EXTERN bool PINC_CALL pinc_event_keyboard_button_state(uint32_t event_index);

// Whether this keyboard button event is a repeat or not
PINC_EXTERN bool PINC_CALL pinc_event_keyboard_button_repeat(uint32_t event_index);

// Where the cursor was before it moved in the window
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_move_old_x(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_move_old_y(uint32_t event_index);

// Where the cursor moved to within the window
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_move_x(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_move_y(uint32_t event_index);

// The window that the cursor moved within
PINC_EXTERN pinc_window PINC_CALL pinc_event_cursor_move_window(uint32_t event_index);

// Where the cursor was before it moved from the previous window, relative to the previous window. Will return 0 if there is no source window / if the source window is foreign.
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_transition_old_x(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_transition_old_y(uint32_t event_index);
// Will return 0 if there is no source window / if the source window is foreign.
PINC_EXTERN pinc_window PINC_CALL pinc_event_cursor_transition_old_window(uint32_t event_index);

// Where the cursor moved to within the new window
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_transition_x(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pinc_event_cursor_transition_y(uint32_t event_index);

// Will not necessarily return the same thing as pinc_event_get_window
PINC_EXTERN pinc_window PINC_CALL pinc_event_cursor_transition_window(uint32_t event_index);

// The unicode codepoint that was typed
PINC_EXTERN uint32_t PINC_CALL pinc_event_text_input_codepoint(uint32_t event_index);

// The amount of vertical scroll
PINC_EXTERN float PINC_CALL pinc_event_scroll_vertical(uint32_t event_index);

PINC_EXTERN float PINC_CALL pinc_event_scroll_horizontal(uint32_t event_index);

#endif
