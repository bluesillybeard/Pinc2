# Build Settings

Build settings are set through C macros.

## Settings that only apply to the header
Header settings are done in pinc.h by defining the setting if it is not already defined. These do not need to be defined for building the library, and in fact they may even be defined before every instance of `#include pinc.h` in your project.

There aren't actually any of these yet.

## Settings that must be applied to both header and library build
Header / Build settings are done in pinc.h by defining the setting if it is not already defined. These need to be defined for both building the Pinc library, as well as in your own project. These are mostly related to the ABI, and should generally be left as default.

- `PINC_EXTERN` for the value before functions. Default value is `extern`. All functions in pinc.h are prefixed with this.
- `PINC_EXPORT` for the value before function declarations. Default value is empty. This should be compatible with PINC_EXTERN in order for the ABI to match and linking to work.
- `PINC_CALL` for the calling convention of pinc functions. Default value is empty (default C calling convention.)
- `PINC_PROC_CALL` for the calling convention of pinc callbacks, such as the error and allocation callbacks. Default value is empty (default C calling convention)

## Settings that only apply to the library build
Header settings are done in the pinc library source code in src/pinc_src.h by defining the setting if it is not already defined. These only need to be defined for building pinc, and do not need to be defined when including the header. To be clear, these settings are set by the build system, but their defaults are managed by pinc_src.h.

- `PINC_ERROR_SETTING` for what kind of error reporting should be present.
    - options are 0, 1, 2
    - 0 outright disables all error reporting. Only errors caused by external issues (such as no GPU available) are reported. Other errors cause undefined behavior. This is only recommended when performance is an absolute priority.
    - 1 Provides good performance at the cost of some error reporting. This is the recommended option for distribution.
    - 2 rigorously checks for programmer / user errors, and calls a panic function when they occur. This is the recommended option for debugging and development. It is also the default option.
- `PINC_HAVE_WINDOW_SDL2`
    - whether pinc with support for SDL2 window backend. 1 for enabled, 0 for disabled. Defaults to 1.
    - this is currently the *only* window backend, so disabling it is not a good idea
- `PINC_HAVE_GRAPHICS_RAW_OPENGL`
    - Compile pinc with support for raw opengl graphics backend.
    - this is currently the *only* graphics backend, so disabling it is not a good idea
