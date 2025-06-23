# Build Settings

Build settings are set through C macros.

## Settings that only apply to the header
Header settings are done in pinc.h by defining the setting if it is not already defined. These do not need to be defined for building the library, and in fact they may even be defined before every instance of `#include pinc.h` in your project.

There aren't actually any of these yet.

## Settings that must be applied to both header and library build
Header / Build settings are done in pinc.h by defining the setting if it is not already defined. These need to be defined for both building the Pinc library, as well as in your own project where Pinc headers are included. These are mostly related to the ABI, and should generally be left as default.

- `PINC_EXTERN` for the value before functions. Default value is `extern`. All functions in pinc.h are prefixed with this.
- `PINC_EXPORT` for the value before function declarations. Default value is empty. This should be compatible with PINC_EXTERN in order for the ABI to match and linking to work.
- `PINC_CALL` for the calling convention of pinc functions. Default value is empty (default C calling convention.)
- `PINC_PROC_CALL` for the calling convention of pinc callbacks, such as the error and allocation callbacks. Default value is empty (default C calling convention)

## Settings that only apply to the library build
Build settings are done in the pinc library source code in `src/pinc_main.h` by defining the setting if it is not already defined. These only need to be defined for building pinc, and do not need to be defined when including the header. To be clear, these settings are set by the build system, but their defaults are managed by pinc_src.h. Due to how build systems handle these settings (for example, cmake), these defaults may be duplicated over multiple files.

Boolean options can use "1", and "ON" for enable. Nearly any other value is treated as disabled. This is done by defining "ON" as "1" with a macro in the source code. In general, please only use "1", "ON", "0", and "OFF" to avoid edge cases involving collisions and other issues.

- `PINC_HAVE_WINDOW_SDL2`
    - whether pinc with support for SDL2 window backend. 1 for enabled, 0 for disabled. Defaults to 1.
    - this is currently the *only* window backend, so disabling it is not a good idea
- `PINC_ENABLE_ERROR_EXTERNAL`
    - Compile with external error checking. Defaults to 1.
    - See pinc.h for error policy
- `PINC_ENABLE_ERROR_ASSERT`
    - Compile with assert error checking. Defaults to 1.
    - See pinc.h for error policy
- `PINC_ENABLE_ERROR_USER`
    - Compile with user error checking. Defaults to 1.
    - See pinc.h for error policy
- `PINC_ENABLE_ERROR_SANITIZE`
    - Compile with sanitize error checking. Defaults to 0.
    - See pinc.h for error policy
- `PINC_ENABLE_ERROR_VALIDATE`
    - Compile with validate error checking. Defaults to 0.
    - See pinc.h for error policy
- `PINC_USE_CUSTOM_PLATFORM_IMPLEMENTATION`
    - Disables Pinc's internal platform implementation and expects the user to define it. Defaults to 0.
    - See src/platform/platform.h and src/platform/platform.c for what functions need to be implemented and how we implemented them.
