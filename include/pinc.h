// pinc.h - Pinc's entire API

// This header has no includes.

// In general, here is what you can expect from the API:
// - we assume floats are in the single precsision IEE754 format.
// - no structs, those probably aren't nessesary anyway
// - typedefs for ID handles

// Usage error policy:
// Usage errors occur when invalid inputs are used in a function. For example, entering 0 for an object handle when it must be defined.
// - Depends on build system settings and application structure
//     - Errors can be set to disabled, rigorous-panic, rigorous-except, light-panic, or light-except. 
//     - If errors are disabled, pinc does not validate any inputs and will crash or behave strangely when used incorrectly
//     - If errors are enabled as panic, then pinc will trigger a debug breakpoint or application exit upon detection of a usage error
//     - If errors are enabled as exceptions, pinc will trigger a function set by the user by pinc_set_error_callback.
//     - Rigorous is only meant to be used in debug mode when testing API usage, as it may impact performance significantly. It's meant to be treated like valgrind.
//     - Light has pretty decent performance, and is meant to be potentially shipped to production if performance is not a top concern

// Other error policy:
// - Pinc will trigger a function set by pinc_set_error_callback when something goes wrong outside of the code's direct control. Ex: no GPU avalable.
//     - These can be disabled, but that is a bad idea unless you really know what you are doing
// - Issues that occur due to pinc itself trigger panics. If this happens to you, please submit an issue with a reproduction test case ASAP.
// - functions that may trigger an error will return a pinc_error_code

// This header expects to be preprocessed and compiled with the same settings as Pinc itself.
// If that will not be the case, it will likely work, but don't expect it to.

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

// @section options
// @brief build system options and stuff

// API prefix (dllexport or whatever)
#ifndef PINC_API
#define PINC_API extern
#endif

// calling convention

#ifndef PINC_CALL
#define PINC_CALL
#endif

// custom ABI settings. This is for when you need ABI interop.

#ifndef Pint
#define Pint int
#endif

#ifndef Pchar
#define Pchar char
#endif

#ifndef Pbool
#define Pbool int
#define Ptrue 1
#define Pfalse 0
#endif

// @section types

/// @brief Window backend
typedef enum {
    pinc_window_backend_any = -1,
    /// @brief No window backend, for doing offline / headless rendering
    pinc_window_backend_none,
    pinc_window_backend_sdl2,
} pinc_window_backend;

// trick to make the enum use the int type defined earlier
#define pinc_window_backend Pint

typedef enum {
    pinc_graphics_backend_any = 0,
    pinc_graphics_backend_openg_2_1,
} pinc_graphics_backend;

// trick to make the enum use the int type defined earlier
#define pinc_graphics_backend Pint

/// @brief Type of generated error
typedef enum {
    /// @brief Something wrong happened internally, meaning Pinc has something wrong with it. Only triggered when PINC_HANDLE_PANIC is set.
    pinc_error_type_panic_redirect = -1,
    /// @brief The error is caused by invalid user inputs. See error policy explained at the start of pinc.h.
    pinc_error_type_invalid_use = 0,
    /// @brief Something outside of Pinc's control went wrong. Failed memory allocation, no GPU available, etc.
    pinc_error_type_external = 1,
} pinc_error_type;

// trick to make the enum use the int type defined earlier
#define pinc_error_type Pint

typedef enum {
    pinc_return_code_pass = 0,
    pinc_return_code_error = 1,
} pinc_return_code;

// trick to make the enum use the int type defined earlier
#define pinc_return_code Pint

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


// trick to make the enum use the int type defined earlier
#define pinc_object_type Pint

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
    /// @brief The ` character. The ~` button on US keyboards.
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

// trick to make the enum use the int type defined earlier
#define pinc_object_type Pint

/// @section IDs

typedef Pint pinc_framebuffer_format;

typedef Pint pinc_object;

typedef Pint pinc_window;

/// @section initialization functions

/// @subsection preinit functions

/// @brief Error callback. Message_buf is null terminated for convenience.
typedef void (* error_callback) (pinc_error_type error, Pchar const * message_buf, Pint message_len);

PINC_API void PINC_CALL pinc_preinit_set_error_callback(error_callback callback);

/// @brief Memory allocation callback. Alignment will usually be 1 except for certain circumstances.
typedef void* (* alloc_callback) (Pint alloc_size_bytes, Pint alignment);

/// @brief Memory free callback. alloc_size_bytes is the size of the allocation being freed, for convenience and potential optimizations.
typedef void (* free_callback) (void* ptr, Pint alloc_size_bytes);

PINC_API void PINC_CALL pinc_preinit_set_alloc_callbacks(alloc_callback alloc, free_callback free);

/// @brief Begin the initialization process
/// @return the success or failure of this function call. Failures are likely caused by external factors (ex: no window backends) or a failed allocation.
PINC_API pinc_return_code PINC_CALL pinc_incomplete_init(void);

/// @subsection full initialization functions
/// @brief The query functions work after initialization, although most of them are useless after the fact

/// @brief Query the window backends available for this system
/// @param backend_dest a pointer to a memory buffer to write to, or null
/// @param capacity the amount of space available for window backends to be written. ignored if backend_dest is null
/// @return the number of window backends that are available
PINC_API Pint PINC_CALL pinc_query_window_backends(pinc_window_backend* backend_dest, Pint capacity);

/// @brief Query the graphics backends available for a given window backend.
/// @param window_backend The window backend to query. Must be a backend from query_window_backends.
/// @param backend_dest a pointer to a memory buffer to write to, or null
/// @param capacity the amount of space available for graphics backends to be written. Ignored if backend_dest is null
/// @return the number of graphics backends that are available
PINC_API Pint PINC_CALL pinc_query_graphics_backends(pinc_window_backend window_backend, pinc_graphics_backend* backend_dest, Pint capacity);

/// @brief Query the frame buffer format ids supported by a window backend and one of its supported graphics backend
/// @param window_backend the window backend to query from. Must be a supported window backend
/// @param graphics_backend the graphics backend to query from. Must be a supported graphics backend of the window backend
/// @param ids_dest a buffer to output the framebuffer format ids, or null
/// @param capacity the amount of space available for framebuffer format ids to be written. Ignored if dest is null 
/// @return the number of framebuffer format ids supported.
PINC_API Pint PINC_CALL pinc_query_framebuffer_format_ids(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format* ids_dest, Pint capacity);

/// @brief Query the number of channels that a frame buffer format supports
/// @param format_id the id of the framebuffer format. Must be from query_framebuffer_format_ids
/// @return the number of channels. 1 for greyscale, 2 for grayscale+alpha, 3 for RGB, 4 for RGBA.
PINC_API Pint PINC_CALL pinc_query_framebuffer_format_channels(pinc_framebuffer_format format_id);

/// @brief Query the bit depth of a channel of a frame buffer format.
/// @param format_id the frame buffer format to query.
/// @param channel the channel index to query. Must be between 0 and channels-1 inclusively.
/// @return the number of bits per pixel in this channel.
PINC_API Pint PINC_CALL pinc_query_framebuffer_format_channel_bits(pinc_framebuffer_format format_id, Pint channel);

PINC_API Pint PINC_CALL pinc_query_framebuffer_format_depth_buffer_bits(pinc_framebuffer_format format_id);

// This requires the graphics backend for things like GLFW where only one OpenGL window may exist, but the Vulkan backend supports many windows.
PINC_API Pint PINC_CALL pinc_query_max_open_windows(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend);

// use -1 to use a default framebuffer format
PINC_API pinc_return_code PINC_CALL pinc_complete_init(pinc_window_backend window_backend, pinc_graphics_backend graphics_backend, pinc_framebuffer_format framebuffer_format_id);

/// @subsection post initialization related functions

PINC_API void PINC_CALL pinc_deinit(void);

PINC_API pinc_window_backend PINC_CALL pinc_query_set_window_backend(void);

PINC_API pinc_graphics_backend PINC_CALL pinc_query_set_graphics_backend(void);

PINC_API Pint PINC_CALL pinc_query_set_framebuffer_format(void);

PINC_API pinc_object_type PINC_CALL pinc_get_object_type(pinc_object obj);

PINC_API Pbool PINC_CALL pinc_get_object_complete(pinc_object obj);

/// @section windows

PINC_API pinc_window PINC_CALL pinc_window_create_incomplete(void);

PINC_API pinc_return_code PINC_CALL pinc_window_complete(pinc_window window);

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
//     - This can be very dificult to obtain before a window is open on the desktop, and even then the system scale may not be set.
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

PINC_API void pinc_window_set_title(pinc_window window, const char* title_buf, Pint title_len);

PINC_API Pint pinc_window_get_title(pinc_window window, char* title_buf, Pint title_capacity);

/// @brief set the width of a window, in pixels.
/// @param window the window whose width to set. Asserts the object is valid, and is a window.
/// @param width the width to set.
PINC_API void PINC_CALL pinc_window_set_width(pinc_window window, Pint width);

/// @brief get the width of a window, in pixels
/// @param window the window whose width to get. Asserts the object is valid, is a window, and has its width set (see pinc_window_has_width)
/// @return the width of the window
PINC_API Pint PINC_CALL pinc_window_get_width(pinc_window window);

/// @brief get if a window has its width defined. A windows width will become defined either when completed, or when set using pinc_window_set_width
/// @param window the window. Asserts the object is valid, and is a window
/// @return 1 if the windows width is set, 0 if not.
PINC_API Pint PINC_CALL pinc_window_has_width(pinc_window window);

/// @brief set the height of a window, in pixels
/// @param window the window whose height to set. Asserts the object is valid, and is a window
/// @param height the heignt to set.
PINC_API void PINC_CALL pinc_window_set_height(pinc_window window, Pint height);

/// @brief get the height of a window, in pixels
/// @param window the window whose height to get. Asserts the object is valid, is a window, and has its height set (see pinc_window_has_height)
/// @return the height of the window
PINC_API Pint PINC_CALL pinc_window_get_height(pinc_window window);

/// @brief get if a window has its height defined. A windows height will become defined either when completed, or when set using pinc_window_set_height
/// @param window the window. Asserts the object is valid, and is a window
/// @return 1 if the windows height is set, 0 if not.
PINC_API Pbool PINC_CALL pinc_window_has_height(pinc_window window);

/// @brief get the scale factor of a window. This is set by the user when they want to "zoom in" - a value of 1.5 should make everything appear 1.5x larger.
/// @param window the window. Asserts the object is valid, is a window, and has its scale factor set (see pinc_window_has_scale_factor)
/// @return the scale factor of this window.
PINC_API float PINC_CALL pinc_window_get_scale_factor(pinc_window window);

/// @brief get if a window has its scale factor defined. Whether this is true depends on the backend, whether the scale is set, and if the window is complete.
///        In general, it is safe to assume 1 unless it is set otherwise.
/// @param window the window. Asserts the object is valid, and is a window
/// @return 1 if the windows scale factor is set, 0 if not.
PINC_API int PINC_CALL pinc_window_has_scale_factor(pinc_window window);

/// @brief set if a window is resizable or not
/// @param window the window. Asserts the object is valid, is a window, has its width defined (see pinc_window_has_width), and has its height defined (see pinc_window_has_height)
/// @param resizable 1 if the window is resizable, 0 if not.
PINC_API void PINC_CALL pinc_window_set_resizable(pinc_window window, Pbool resizable);

/// @brief get if a window is resizable or not
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is resizable, 0 if not.
PINC_API Pbool PINC_CALL pinc_window_get_resizable(pinc_window window);

/// @brief set if a window is minimized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param minimized 1 if the window is minimized, 0 if not
PINC_API void PINC_CALL pinc_window_set_minimized(pinc_window window, Pbool minimized);

/// @brief get if a window is minimized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is minimized, 0 if not
PINC_API Pbool PINC_CALL pinc_window_get_minimized(pinc_window window);

/// @brief set if a window is maximized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param maximized 1 if the window is maximized, 0 if not
PINC_API void PINC_CALL pinc_window_set_maximized(pinc_window window, Pbool maximized);

/// @brief get if a window is maximized or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is maximized, 0 if not
PINC_API Pbool PINC_CALL pinc_window_get_maximized(pinc_window window);

/// @brief set if a window is fullscreen or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param fullscreen 1 if the window is fullscreen, 0 if not
PINC_API void PINC_CALL pinc_window_set_fullscreen(pinc_window window, Pbool fullscreen);

/// @brief get if a window is fullscreen or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is fullscreen, 0 if not
PINC_API Pbool PINC_CALL pinc_window_get_fullscreen(pinc_window window);

/// @brief set if a window is focused or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param focused 1 if the window is focused, 0 if not
PINC_API void PINC_CALL pinc_window_set_focused(pinc_window window, Pbool focused);

/// @brief get if a window is focused or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is focused, 0 if not
PINC_API Pbool PINC_CALL pinc_window_get_focused(pinc_window window);

/// @brief set if a window is hidden or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @param hidden 1 if the window is hidden, 0 if not
PINC_API void PINC_CALL pinc_window_set_hidden(pinc_window window, Pbool hidden);

/// @brief get if a window is hidden or not.
/// @param window the window. Asserts the object is valid, and is a window.
/// @return 1 if the window is hidden, 0 if not
PINC_API Pbool PINC_CALL pinc_window_get_hidden(pinc_window window);

// It is best practice to set vsync before completing the window
// as some systems do not support changing vsync after a window is completed.
// vsync is true by defualt on systems that support it.
PINC_API pinc_return_code PINC_CALL pinc_window_set_vsync(pinc_window window, Pbool sync);

PINC_API Pbool PINC_CALL pinc_get_vsync(pinc_window window);

/// @brief Present the framebuffer of a given window and prepares a backbuffer to draw on.
///        The number of backbuffers depends on the graphics backend, but it's generally 2 or 3.
/// @param window the window whose framebuffer to present.
PINC_API void PINC_CALL pinc_window_present_framebuffer(pinc_window window);

/// @section user IO

// TODO: Clipboard

/// @brief Get the state of a mouse button
/// @param button the button to check. Generally, 0 is the left button, 1 is the right, and 2 is the middle
/// @return 1 if the button is pressed, 0 if it is not pressed OR if this application has no focused windows.
PINC_API Pbool PINC_CALL pinc_mouse_button_get(int button);

/// @brief Get the state of a keyboard key.
/// @param button A value of pinc_keyboard_key to check
/// @return 1 if the key is pressed, 0 if it is not.
PINC_API Pbool PINC_CALL pinc_keyboard_key_get(pinc_keyboard_key button);

/// @brief Get the cursor X position relative to the window the cursor is currently in
/// @return the X position. 0 on the left to width-1 on the right.
PINC_API Pint PINC_CALL pinc_get_cursor_x(void);

/// @brief get the cursor Y position relative to the window the cursor is currently in
/// @return the X position. 0 on the top to height-1 on the bottom.
PINC_API Pint PINC_CALL pinc_get_cursor_y(void);

// Get the window the cursor is currently in, or 0 if the cursor is not over a window in this application.
PINC_API pinc_window PINC_CALL pinc_get_cursor_window(void);

/// @section main loop & events

/// @brief Flushes internal buffers and collects user input
PINC_API void PINC_CALL pinc_step(void);

/// @brief Gets if a window was signalled to close during the last step.
/// @param window the window to query. Only accepts comple windows.
/// @return 1 if a window was signalled to close in the last step, 0 otherwise.
PINC_API Pbool PINC_CALL pinc_event_window_closed(pinc_window window);

/// @brief Get if there was a mouse press/release from the last step
/// @param window The window to check - although all windows share the same cursor,
///     it is possible for multiple windows to be clicked in the same step.
/// @return 1 if there any mouse buttons were pressed or released, 0 otherwise.
PINC_API Pbool PINC_CALL pinc_event_window_mouse_button(pinc_window window);

/// @brief Gets if a window was resized during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return if the window was resized during the last step
PINC_API Pbool PINC_CALL pinc_event_window_resized(pinc_window window);

/// @brief Gets if a window recieved input focus in the last step.
///        If a window lost and gained focus in the same step, it is safe to assume that window is not focused.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the window was focused in the last step, 0 otherwise.
PINC_API Pbool PINC_CALL pinc_event_window_focused(pinc_window window);

/// @brief gets if a window lost input focus in the last step, unless it regained focus in that same step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the window was focused in the last step, 0 otherwise.
PINC_API Pbool PINC_CALL pinc_event_window_unfocused(pinc_window window);

/// @brief gets if a window should be redrawn from last step.
/// @param window the window to query. Only accepts complere windows.
/// @return 1 if the window should be redrawn this step, 0 otherwise.
PINC_API Pbool PINC_CALL pinc_event_window_exposed(pinc_window window);

// Keyboard events require a window because it's possible for multiple windows to have key events in a single step

/// @brief Get key events in the last step. Key events are generatetd for presses, releases, and repeats. release v.s press or repeat is detected by getting that key's state after the event.
/// @param window the window to get events from. Only accepts complete windows
/// @param key_buffer a buffer to place the key events into. Events happened from lowest index to highest index. accepts null.
/// @param capacity the number of keys the buffer can hold. Ignored if key_buffer is null
/// @return the number of key events
PINC_API Pint PINC_CALL pinc_event_window_keyboard_button_get(pinc_window window, pinc_keyboard_key* key_buffer, Pint capacity);

/// @brief Get whether key events from the last step were repeats or not. Index-matched with the output of event_window_keyboard_button_get.
/// @param window the window to get events from. Only accecpts complete windows
/// @param repeat_buffer a buffer to place the repeat values into. Accepts null.
/// @param capacity the number of values repeat_buffer can hold
/// @return the number of key events
PINC_API Pint PINC_CALL pinc_event_window_keyboard_button_get_repeat(pinc_window window, Pbool* repeat_buffer, Pint capacity);

/// @brief Get if the cursor moved within a window during the last step.
///        Requires a window because it's possible for the cursor to move from one window to another window in a single step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the cursor moved in this window, 0 if not.
PINC_API Pbool PINC_CALL pinc_event_window_cursor_move(pinc_window window);

/// @brief Get if the cursor has left a window during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the cursor left the window, 0 if not.
PINC_API Pbool PINC_CALL pinc_event_window_cursor_exit(pinc_window window);

/// @brief Get if the cursor has entered a window during the last step.
/// @param window the window to query. Only accepts complete windows.
/// @return 1 if the cursor entered the window, 0 if not.
PINC_API Pbool PINC_CALL pinc_event_window_cursor_enter(pinc_window window);

// returns text that was typed into a window since the last step. Encoded in UTF 8.
PINC_API Pint PINC_CALL pinc_event_window_text_get(pinc_window window, Pchar* return_buf, Pint capacity);

// TODO: doc
PINC_API float PINC_CALL pinc_window_scroll_vertical(pinc_window window);

// TODO: doc
PINC_API float PINC_CALL pinc_window_scroll_horizontal(pinc_window window);


#endif
