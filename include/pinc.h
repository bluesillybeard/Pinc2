#ifndef PINC_H
#define PINC_H

// pinc.h - Pinc's entire API

// This header has ZERO includes, and is fully self-sufficient assuming a valid C compiler.

// In general, here is what you can expect from the API:
// - we assume floats are in the single precision IEE754 32 bit format.
// - no structs, at least for now. Structs can cause ABI issues, and can be harder to deal with when making bindings.
// - typedefs for ID handles

// Error Policy:
// Pinc has 5 types of errors, all of which can be enabled, disabled, and configured to call user callbacks.
// Ordered from least to most performance impact:
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
//     - pincPreinit*
// - call pincIncompleteInit()
// - (optional) Query information and decide settings
//     - This is where the program decides which window backend, framebuffer format, graphics api, etc to use.
//     - If this step is skipped, Pinc will use default settings for the system it is running on.
//     - pincInitQuery*
//     - pincInitSet*
// - call pincCompleteInit()
// - create objects
//     - windows, graphics objects, whatever else.
//     - these can be created in the main loop as well
// - main loop
//     - call pincStep()
//     - get user inputs and other events
//     - draw stuff
//     - present window framebuffers

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
    PincWindowBackend_any = 0,
    /// @brief No window backend, for doing offline / headless rendering
    PincWindowBackend_none,
    PincWindowBackend_sdl2,
} PincWindowBackendEnum;

// This may seem stupid, but remember that C is a volatile language
// where 'enum' means anything from 8 bits to 64 bits depending on all kings of things.
// It isn't even necessarily consistent across different versions or configurations of the same compiler on the same platform.
// Considering one of Pinc's primary goals is to have a predicable ABI for bindings to other languages,
// making all enums a predictable size is paramount.
typedef uint32_t PincWindowBackend;

typedef enum {
    /// @brief Represents any backend, or an unknown backend. Generally only works for calling pinc_complete_init.
    PincGraphicsApi_any = 0,
    PincGraphicsApi_opengl,
} PincGraphicsApiEnum;

typedef uint32_t PincGraphicsApi;

typedef enum {
    PincReturnCode_pass = 0,
    PincReturnCode_error = 1,
} PincReturnCode_enum;

typedef uint32_t PincReturnCode;

typedef enum {
    PincObjectType_none = 0,
    PincObjectType_window,
    PincObjectType_framebufferFormat,
    PincObjectType_incompleteGlContext,
    PincObjectType_glContext,
} PincObjectType_enum;

typedef uint32_t PincObjectType;

// TODO: this needs better docs and clarification on semantics and exactly when these events are triggered.
typedef enum {
    // A window was signalled to close.
    PincEventType_closeSignal,
    // Mouse button state changed.
    PincEventType_mouseButton,
    // A window was resized.
    PincEventType_resize,
    // The current window with focus changed. For now, only one window can be focused at a time. This event carries the new focused window as wel las the old one.
    PincEventType_focus,
    // The WM / Compositor is explicitly requesting a screen refresh. DO NOT rely on this for all screen refreshes.
    PincEventType_exposure,
    // Keyboard button state change or repeat.
    PincEventType_keyboardButton,
    // Mouse cursor moved.
    PincEventType_cursorMove,
    // Mouse cursor moved from one window to another
    PincEventType_cursorTransition,
    // Text was typed
    PincEventType_textInput,
    // Scroll wheel / pad
    PincEventType_scroll,
} pinc_event_type_enum;

typedef uint32_t PincEventType;

/// @brief enumeration of pinc keyboard codes
///     These are not physical, but logical - when the user presses the button labeled 'q' on their keyboard, that's the key reported here.
///     In order words, this ignores the idea of a keyboard layout, and reports based on what the user is typing, not what actual buttons are being pressed.
typedef enum {
    PincKeyboardKey_unknown = -1,
    PincKeyboardKey_space = 0,
    PincKeyboardKey_apostrophe,
    PincKeyboardKey_comma,
    PincKeyboardKey_dash,
    PincKeyboardKey_dot,
    PincKeyboardKey_slash,
    PincKeyboardKey_0,
    PincKeyboardKey_1,
    PincKeyboardKey_2,
    PincKeyboardKey_3,
    PincKeyboardKey_4,
    PincKeyboardKey_5,
    PincKeyboardKey_6,
    PincKeyboardKey_7,
    PincKeyboardKey_8,
    PincKeyboardKey_9,
    PincKeyboardKey_semicolon,
    PincKeyboardKey_equals,
    PincKeyboardKey_a,
    PincKeyboardKey_b,
    PincKeyboardKey_c,
    PincKeyboardKey_d,
    PincKeyboardKey_e,
    PincKeyboardKey_f,
    PincKeyboardKey_g,
    PincKeyboardKey_h,
    PincKeyboardKey_i,
    PincKeyboardKey_j,
    PincKeyboardKey_k,
    PincKeyboardKey_l,
    PincKeyboardKey_m,
    PincKeyboardKey_n,
    PincKeyboardKey_o,
    PincKeyboardKey_p,
    PincKeyboardKey_q,
    PincKeyboardKey_r,
    PincKeyboardKey_s,
    PincKeyboardKey_t,
    PincKeyboardKey_u,
    PincKeyboardKey_v,
    PincKeyboardKey_w,
    PincKeyboardKey_x,
    PincKeyboardKey_y,
    PincKeyboardKey_z,
    PincKeyboardKey_leftBracket,
    PincKeyboardKey_backslash,
    PincKeyboardKey_rightBracket,
    /// @brief The "\`" character. The "~" button on US keyboards.
    PincKeyboardKey_backtick,
    PincKeyboardKey_escape,
    PincKeyboardKey_enter,
    PincKeyboardKey_tab,
    PincKeyboardKey_backspace,
    PincKeyboardKey_insert,
    PincKeyboardKey_delete,
    PincKeyboardKey_right,
    PincKeyboardKey_left,
    PincKeyboardKey_down,
    PincKeyboardKey_up,
    PincKeyboardKey_pageUp,
    PincKeyboardKey_pageDown,
    PincKeyboardKey_home,
    PincKeyboardKey_end,
    PincKeyboardKey_capsLock,
    PincKeyboardKey_scrollLock,
    PincKeyboardKey_numLock,
    PincKeyboardKey_printScreen,
    PincKeyboardKey_pause,
    PincKeyboardKey_f1,
    PincKeyboardKey_f2,
    PincKeyboardKey_f3,
    PincKeyboardKey_f4,
    PincKeyboardKey_f5,
    PincKeyboardKey_f6,
    PincKeyboardKey_f7,
    PincKeyboardKey_f8,
    PincKeyboardKey_f9,
    PincKeyboardKey_f10,
    PincKeyboardKey_f11,
    PincKeyboardKey_f12,
    PincKeyboardKey_f13,
    PincKeyboardKey_f14,
    PincKeyboardKey_f15,
    PincKeyboardKey_f16,
    PincKeyboardKey_f17,
    PincKeyboardKey_f18,
    PincKeyboardKey_f19,
    PincKeyboardKey_f20,
    PincKeyboardKey_f21,
    PincKeyboardKey_f22,
    PincKeyboardKey_f23,
    PincKeyboardKey_f24,
    PincKeyboardKey_numpad0,
    PincKeyboardKey_numpad1,
    PincKeyboardKey_numpad2,
    PincKeyboardKey_numpad3,
    PincKeyboardKey_numpad4,
    PincKeyboardKey_numpad5,
    PincKeyboardKey_numpad6,
    PincKeyboardKey_numpad7,
    PincKeyboardKey_numpad8,
    PincKeyboardKey_numpad9,
    PincKeyboardKey_numpadDot,
    PincKeyboardKey_numpadSlash,
    PincKeyboardKey_numpadAsterisk,
    PincKeyboardKey_numpadDash,
    PincKeyboardKey_numpadPlus,
    PincKeyboardKey_numpadEnter,
    PincKeyboardKey_numpadEqual,
    PincKeyboardKey_leftShift,
    PincKeyboardKey_leftControl,
    PincKeyboardKey_leftAlt,
    /// @brief On many keyboards, this is a windows icon and is generally called "the windows button"
    PincKeyboardKey_leftSuper,
    PincKeyboardKey_rightShift,
    PincKeyboardKey_rightControl,
    PincKeyboardKey_rightAlt,
    /// @brief On many keyboards, this is a windows icon and is generally called "the windows button". Most keyboards only have the one on the left, not this one.
    PincKeyboardKey_rightSuper,
    /// @brief On many keyboards, this is the button next to right control
    PincKeyboardKey_menu,
    /// @brief This is for convenience
    PincKeyboardKey_count,
} PincKeyboardKeyEnum;

typedef uint32_t PincKeyboardKey;

typedef enum {
    // A basic color space with generally vague semantics. In general, bugger number = brighter output.
    PincColorSpace_basic,
    // Linear color space where the output magnitude is directly correlated with pixel values 
    PincColorSpace_linear,
    // Some kind of perceptual color space, usually with a gamma value of 2.2 but it often may not be.
    PincColorSpace_perceptual,
    // srgb color space, or a grayscale equivalent. This is similar to perceptual with a gamma value of 2.2
    PincColorSpace_srgb
} PincColorSpaceEnum;

typedef uint32_t PincColorSpace;

/// @section IDs


// objects are non-transferrable between runs.
typedef uint32_t PincObjectHandle;

typedef PincObjectHandle PincFramebufferFormatHandle;

typedef PincObjectHandle PincWindowHandle;

/// @section initialization functions

/// @subsection preinit functions

/// @brief See error policy at the top of pinc.h
typedef enum {
    /// @brief Unknown error type, this is something should almost never happen. Usually this is called when Pinc devs get lazy and forget to implement something.
    PincErrorType_unknown = -1,
    PincErrorType_external = 0,
    PincErrorType_assert,
    PincErrorType_user,
    PincErrorType_sanitize,
    PincErrorType_validate,
} PincErrorTypeEnum;

typedef int32_t PincErrorType;

/// @brief Error callback. message_buf will be null terminated. message_buf is temporary and a reference to it should not be kept.
typedef void ( PINC_PROC_CALL * PincErrorCallback) (uint8_t const * message_buf, uintptr_t message_len, PincErrorType error_type);

/// @brief Set the function for handling errors. This is optional, however Pinc just calls assert(0) if this is not set.
PINC_EXTERN void PINC_CALL pincPreinitSetErrorCallback(PincErrorCallback callback);

/// @brief Callback to allocate some memory. Aligned depending on platform such that any structure can be placed into this memory.
///     Identical to libc's malloc function.
/// @param alloc_size_bytes Number of bytes to allocate.
/// @return A pointer to the memory.
typedef void* ( PINC_PROC_CALL * PincAllocCallback) (void* user_ptr, size_t alloc_size_bytes);

/// @brief Callback to allocate some memory with explicit alignment.
/// @param alloc_size_bytes Number of bytes to allocate. Must be a multiple of alignment.
/// @param alignment Alignment requirement. Must be a power of 2.
/// @return A pointer to the allocated memory.
typedef void* ( PINC_PROC_CALL * PincAllocAlignedCallback) (void* user_ptr, size_t alloc_size_bytes, size_t alignment);

/// @brief Reallocate some memory with a different size
/// @param ptr Pointer to the memory to reallocate. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param old_alloc_size_bytes Old size of the allocation. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
/// @param alloc_size_bytes The new size of the allocation
/// @return A pointer to this memory. May be the same or different from pointer.
typedef void* ( PINC_PROC_CALL * PincReallocCallback) (void* userPtr, void* ptr, size_t old_alloc_size_bytes, size_t alloc_size_bytes);

/// @brief Free some memory
/// @param ptr Pointer to free. Must exactly be a pointer returned by pAlloc, pAllocAligned, or pRealloc.
/// @param alloc_size_bytes Number of bytes to free. Must be the exact size given to pAlloc, pAllocAligned, or pRealloc for the respective pointer.
typedef void ( PINC_PROC_CALL * PincFreeCallback) (void* userPtr, void* ptr, size_t alloc_size_bytes);

/// @brief Set allocation callbacks. Must be called before incomplete_init, or never. The type of each proc has more information. They either must all be set, or all null.
PINC_EXTERN void PINC_CALL pincPreinitSetAllocCallbacks(void* user_ptr, PincAllocCallback alloc, PincAllocAlignedCallback alloc_aligned, PincReallocCallback realloc, PincFreeCallback free);

/// @brief Begin the initialization process
/// @return the success or failure of this function call. Failures are likely caused by external factors (ex: no window backends) or a failed allocation.
PINC_EXTERN PincReturnCode PINC_CALL pincInitIncomplete(void);

/// @subsection full initialization functions
/// @brief The query functions work after initialization, although most of them are useless after the fact

PINC_EXTERN bool PINC_CALL pincQueryWindowBackendSupport(PincWindowBackend window_backend);

/// @brief Query the default window backend for this system.
/// @return The default window backend.
PINC_EXTERN PincWindowBackend PINC_CALL pincQueryWindowBackendDefault(void);

PINC_EXTERN bool PINC_CALL pincQueryGraphicsApiSupport(PincWindowBackend window_backend, PincGraphicsApi graphics_api);

/// @brief Query the default graphics api for this system, given a window backend.
///        The window backend is required because it's used to determine the best graphics api for the system.
///        use pinc_window_backend_any to query the default window backend.
/// @return the default graphics api.
PINC_EXTERN PincGraphicsApi PINC_CALL pincQueryGraphicsApiDefault(PincWindowBackend window_backend);

/// @brief Query the default framebuffer format that would be chosen for a given window and graphics api.
/// @param window_backend the window backend to query. Must be a supported graphics api of the window backend.
/// @param graphics_api the graphics api to query.
/// @return the default framebuffer format.
PINC_EXTERN PincFramebufferFormatHandle PINC_CALL pincQueryFramebufferFormatDefault(PincWindowBackend window_backend, PincGraphicsApi graphics_api);

/// @brief Query the frame buffer formats supported by a window backend and one of its supported graphics api.
/// @param window_backend the window backend to query from. Must be a supported window backend.
/// @param graphics_api the graphics api to query from. Must be a supported graphics api of the window backend, or pinc_graphics_api_none to use the default one.
/// @param ids_dest a buffer to output the framebuffer format handles, or null to just query the number of formats.
/// @param capacity the amount of space available for framebuffer formats to be written. Ignored if dest is null.
/// @return the number of framebuffer formats supported.
PINC_EXTERN uint32_t PINC_CALL pincQueryFramebufferFormats(PincWindowBackend window_backend, PincGraphicsApi graphics_api, PincFramebufferFormatHandle* handles_dest, uint32_t capacity);

/// @brief Query the number of channels that a frame buffer format supports
/// @param format_handle the id of the framebuffer format.
/// @return the number of channels. 1 for grayscale, 2 for grayscale+alpha, 3 for RGB. Transparent windows (RGBA) is not yet supported.
PINC_EXTERN uint32_t PINC_CALL pincQueryFramebufferFormatChannels(PincFramebufferFormatHandle format_handle);

/// @brief Query the bit depth of a channel of a frame buffer format.
/// @param format_handle the frame buffer format to query.
/// @param channel the channel index to query. Must be between 0 and channels-1 inclusively.
/// @return the number of bits per pixel in this channel.
PINC_EXTERN uint32_t PINC_CALL pincQueryFramebufferFormatChannelBits(PincFramebufferFormatHandle format_handle, uint32_t channel);

PINC_EXTERN PincColorSpace PINC_CALL pincQueryFramebufferFormatColorSpace(PincFramebufferFormatHandle format_handle);

// Many platforms (web & most consoles) can only have a certain number of windows open at a time. Most of the time this is one,
// but it's possible to set up a web template with multiple canvases and there are consoles with 2 screens (like the nintendo DS) where it makes sense to treat them as separate windows.
// Returns 0 if there is no reasonable limit (the limit is not a specific number, and you'll probably never encounter related issues in this case)
PINC_EXTERN uint32_t PINC_CALL pincQueryMaxOpenWindows(PincWindowBackend window_backend);

// Null framebuffer format is a shortcut to use the default framebuffer format.
PINC_EXTERN PincReturnCode PINC_CALL pincInitComplete(PincWindowBackend window_backend, PincGraphicsApi graphics_api, PincFramebufferFormatHandle framebuffer_format_id);

/// @subsection post initialization related functions

PINC_EXTERN void PINC_CALL pincDeinit(void);

PINC_EXTERN PincWindowBackend PINC_CALL pincQuerySetWindowBackend(void);

PINC_EXTERN PincGraphicsApi PINC_CALL pincQuerySetGraphicsApi(void);

PINC_EXTERN PincFramebufferFormatHandle PINC_CALL pincQuerySetFramebufferFormat(void);

PINC_EXTERN PincObjectType PINC_CALL pincGetObjectType(PincObjectHandle handle);

PINC_EXTERN bool PINC_CALL pincGetObjectComplete(PincObjectHandle handle);

PINC_EXTERN void PINC_CALL pincSetObjectUserData(PincObjectHandle handle, void* user_data);

PINC_EXTERN void* PINC_CALL pincGetObjectUserData(PincObjectHandle handle);

/// @section windows

PINC_EXTERN PincWindowHandle PINC_CALL pincWindowCreateIncomplete(void);

PINC_EXTERN PincReturnCode PINC_CALL pincWindowComplete(PincWindowHandle incomplete_window_handle);

// Deinit / close / destroy a window object.
PINC_EXTERN void PINC_CALL pincWindowDeinit(PincWindowHandle window_handle);

// window properties:
// ALL window properties have defaults so users can get up and running ASAP. However, many of those defaults cannot be determined until after some point.
// r -> can be read at any time. It has a default [default is in square brackets]
// rc -> can only be read after the default has been determined, so it needs a has[property] function
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
PINC_EXTERN void PINC_CALL pincWindowSetTitle(PincWindowHandle window_handle, const char* title_buf, uint32_t title_len);

PINC_EXTERN uint32_t PINC_CALL pincWindowGetTitle(PincWindowHandle window_handle, char* title_buf, uint32_t title_capacity);

/// @brief set the width of a window, in pixels.
/// @param window_handle the window whose width to set. Asserts the object is valid, and is a window.
/// @param width the width to set.
PINC_EXTERN void PINC_CALL pincWindowSetWidth(PincWindowHandle window_handle, uint32_t width);

/// @brief get the width of a window's drawable area, in pixels
/// @param window_handle the window whose width to get. Asserts the object is valid, is a window, and has its width set (see pinc_window_has_width)
/// @return the width of the window
PINC_EXTERN uint32_t PINC_CALL pincWindowGetWidth(PincWindowHandle window_handle);

/// @brief get if a window has its width defined. A windows width will become defined either when completed, or when set using pinc_window_set_width
/// @param window_handle the window. Asserts the object is valid, and is a window
/// @return true if the windows width is set, false if not.
PINC_EXTERN bool PINC_CALL pincWindowHasWidth(PincWindowHandle window_handle);

/// @brief set the height of a window's drawable area, in pixels
/// @param window_handle the window whose height to set. Asserts the object is valid, and is a window
/// @param height the height to set.
PINC_EXTERN void PINC_CALL pincWindowSetHeight(PincWindowHandle window_handle, uint32_t height);

/// @brief get the height of a window, in pixels
/// @param window_handle the window whose height to get. Asserts the object is valid, is a window, and has its height set (see pinc_window_has_height)
/// @return the height of the window
PINC_EXTERN uint32_t PINC_CALL pincWindowGetHeight(PincWindowHandle window_handle);

/// @brief get if a window has its height defined. A windows height will become defined either when completed, or when set using pinc_window_set_height
/// @param window_handle the window. Asserts the object is valid, and is a window
/// @return 1 if the windows height is set, false if not.
PINC_EXTERN bool PINC_CALL pincWindowHasHeight(PincWindowHandle window_handle);

/// @brief get the scale factor of a window. This is set by the user when they want to "zoom in" - a value of 1.5 should make everything appear 1.5x larger.
/// @param window_handle the window. Asserts the object is valid, is a window, and has its scale factor set (see pinc_window_has_scale_factor)
/// @return the scale factor of this window.
PINC_EXTERN float PINC_CALL pincWindowGetScaleFactor(PincWindowHandle window_handle);

/// @brief get if a window has its scale factor defined. Whether this is true depends on the backend, whether the scale is set, and if the window is complete.
///        In general, it is safe to assume 1 unless it is set otherwise.
/// @param window_handle the window. Asserts the object is valid, and is a window
/// @return true if the windows scale factor is set, false if not.
PINC_EXTERN bool PINC_CALL pincWindowHasScaleFactor(PincWindowHandle window_handle);

/// @brief set if a window is resizable or not
/// @param window_handle the window. Asserts the object is valid, is a window, has its width defined (see pinc_window_has_width), and has its height defined (see pinc_window_has_height)
/// @param resizable true if the window is resizable, false if not.
PINC_EXTERN void PINC_CALL pincWindowSetResizable(PincWindowHandle window_handle, bool resizable);

/// @brief get if a window is resizable or not
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @return true if the window is resizable, false if not.
PINC_EXTERN bool PINC_CALL pincWindowGetResizable(PincWindowHandle window_handle);

/// @brief set if a window is minimized or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @param minimized true if the window is minimized, false if not
PINC_EXTERN void PINC_CALL pincWindowSetMinimized(PincWindowHandle window_handle, bool minimized);

/// @brief get if a window is minimized or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @return true if the window is minimized, false if not
PINC_EXTERN bool PINC_CALL pincWindowGetMinimized(PincWindowHandle window_handle);

/// @brief set if a window is maximized or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @param maximized true if the window is maximized, false if not
PINC_EXTERN void PINC_CALL pincWindowSetMaximized(PincWindowHandle window_handle, bool maximized);

/// @brief get if a window is maximized or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @return true if the window is maximized, false if not
PINC_EXTERN bool PINC_CALL pincWindowGetMaximized(PincWindowHandle window_handle);

/// @brief set if a window is fullscreen or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @param fullscreen true if the window is fullscreen, false if not
PINC_EXTERN void PINC_CALL pincWindowSetFullscreen(PincWindowHandle window_handle, bool fullscreen);

/// @brief get if a window is fullscreen or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @return true if the window is fullscreen, false if not
PINC_EXTERN bool PINC_CALL pincWindowGetFullscreen(PincWindowHandle window_handle);

/// @brief set if a window is focused or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @param focused true if the window is focused, false if not
PINC_EXTERN void PINC_CALL pincWindowSetFocused(PincWindowHandle window_handle, bool focused);

/// @brief get if a window is focused or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @return true if the window is focused, false if not
PINC_EXTERN bool PINC_CALL pincWindowGetFocused(PincWindowHandle window_handle);

/// @brief set if a window is hidden or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @param hidden true if the window is hidden, false if not
PINC_EXTERN void PINC_CALL pincWindowSetHidden(PincWindowHandle window_handle, bool hidden);

/// @brief get if a window is hidden or not.
/// @param window_handle the window. Asserts the object is valid, and is a window.
/// @return true if the window is hidden, false if not
PINC_EXTERN bool PINC_CALL pincWindowGetHidden(PincWindowHandle window_handle);

// vsync is true by default on systems that support it.
// Technically, vsync is generally either bound to a window or a graphics context.
// Any half-decently modern graphics API supports setting vsync arbitrary at any time,
// However some of them (*cough cough* OpenGL *cough cough*) are a bit more picky in how vsync is handled.
// In general, call this function right after pinc_complete_init to be safe,
// and call it again before present_framebuffer in hopes that the underlying API supports modifying vsync at runtime.
// An error from this function just means vsync couldn't be changed, and is otherwise harmless.
PINC_EXTERN PincReturnCode PINC_CALL pincSetVsync(bool sync);

PINC_EXTERN bool PINC_CALL pincGetVsync(void);

/// @brief Present the framebuffer of a given window and prepares a backbuffer to draw on.
///        The number of backbuffers depends on the graphics api, but it's generally 2 or 3.
/// @param complete_window_handle the window whose framebuffer to present.
PINC_EXTERN void PINC_CALL pincWindowPresentFramebuffer(PincWindowHandle complete_window_handle);

/// @section user IO, TODO: [IMPORTANT] this needs to be redone entirely!

// Clipboard, the general results of events (cursor position, current window, keyboard state, etc), other window / application IO

/// @section main loop & events

/// @brief Flushes internal buffers and collects user input
PINC_EXTERN void PINC_CALL pincStep(void);

PINC_EXTERN uint32_t PINC_CALL pincEventGetNum(void);

PINC_EXTERN PincEventType PINC_CALL pincEventGetType(uint32_t event_index);

/// @brief Returns the window that had focus at the time of the event. This is not necessarily the currently focused window.
/// @return The window that was focused when the event happened, or 0 if no window was focused.
PINC_EXTERN PincWindowHandle PINC_CALL pincEventGetWindow(uint32_t event_index);

PINC_EXTERN int64_t PINC_CALL pincEventGetTimestampUnixMillis(uint32_t event_index);

// the window that received the close signal
PINC_EXTERN PincWindowHandle PINC_CALL pincEventCloseSignalWindow(uint32_t event_index);

/// @brief Get the old state of the mouse buttons before the event ocurred. Only defined for mouse button events.
/// @return The state of the mouse buttons in a bitfield. The 5 bits, from least to most significant, are left, right, middle, back, and forward respectively.
PINC_EXTERN uint32_t PINC_CALL pincEventMouseButtonOldState(uint32_t event_index);

/// @brief Get the new state of the mouse buttons after the event ocurred. Only defined for mouse button events.
/// @return The state of the mouse buttons in a bitfield. The 5 bits, from least to most significant, are left, right, middle, back, and forward respectively.
PINC_EXTERN uint32_t PINC_CALL pincEventMouseButtonState(uint32_t event_index);

// The size of the window before it was resized
PINC_EXTERN uint32_t PINC_CALL pincEventResizeOldWidth(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pincEventResizeOldHeight(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pincEventResizeWidth(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pincEventResizeHeight(uint32_t event_index);

PINC_EXTERN PincWindowHandle PINC_CALL pincEventResizeWindow(uint32_t event_index);

// The window that was focused before the event - will always return the same thing as pinc_event_get_window, assuming a valid focus event.
PINC_EXTERN PincWindowHandle PINC_CALL pincEventFocusOldWindow(uint32_t event_index);

// The window that gained focus
PINC_EXTERN PincWindowHandle PINC_CALL pincEventFocusWindow(uint32_t event_index);

// The top left of the exposed area
PINC_EXTERN uint32_t PINC_CALL pincEventExposureX(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pincEventExposureY(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pincEventExposureWidth(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pincEventExposureHeight(uint32_t event_index);

PINC_EXTERN uint32_t PINC_CALL pincEventExposureWindow(uint32_t event_index);

// The button whose state changed
PINC_EXTERN PincKeyboardKey PINC_CALL pincEventKeyboardButtonKey(uint32_t event_index);

// The state of the button after the event
PINC_EXTERN bool PINC_CALL pincEventKeyboardButtonState(uint32_t event_index);

// Whether this keyboard button event is a repeat or not
PINC_EXTERN bool PINC_CALL pincEventKeyboardButtonRepeat(uint32_t event_index);

// Where the cursor was before it moved in the window
PINC_EXTERN uint32_t PINC_CALL pincEventCursorMoveOldX(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pincEventCursorMoveOldY(uint32_t event_index);

// Where the cursor moved to within the window
PINC_EXTERN uint32_t PINC_CALL pincEventCursorMoveX(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pincEventCursorMoveY(uint32_t event_index);

// The window that the cursor moved within
PINC_EXTERN PincWindowHandle PINC_CALL pincEventCursorMoveWindow(uint32_t event_index);

// Where the cursor was before it moved from the previous window, relative to the previous window. Will return 0 if there is no source window / if the source window is foreign.
PINC_EXTERN uint32_t PINC_CALL pincEventCursorTransitionOldX(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pincEventCursorTransitionOldY(uint32_t event_index);
// Will return 0 if there is no source window / if the source window is foreign.
PINC_EXTERN PincWindowHandle PINC_CALL pincEventCursorTransitionOldWindow(uint32_t event_index);

// Where the cursor moved to within the new window
PINC_EXTERN uint32_t PINC_CALL pincEventCursorTransitionX(uint32_t event_index);
PINC_EXTERN uint32_t PINC_CALL pincEventCursorTransitionY(uint32_t event_index);

// Will not necessarily return the same thing as pinc_event_get_window
PINC_EXTERN PincWindowHandle PINC_CALL pincEventCursorTransitionWindow(uint32_t event_index);

// The unicode codepoint that was typed
PINC_EXTERN uint32_t PINC_CALL pincEventTextInputCodepoint(uint32_t event_index);

// The amount of vertical scroll
PINC_EXTERN float PINC_CALL pincEventScrollVertical(uint32_t event_index);

PINC_EXTERN float PINC_CALL pincEventScrollHorizontal(uint32_t event_index);

#endif
