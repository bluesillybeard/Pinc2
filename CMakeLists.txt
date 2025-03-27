# We use cmake because it's convenient, not because it's good!
# If there is a different build system that you think is better or is just worth supporting / using,
# feel free to make an issue or poll request to add it.
# Cmake will be continued to be supported, for those who want to use it with pinc.

# We don't have a pkg-config because Pinc is meant to be linked fully statically.
# Pinc can be linked dynamically, but that option only exists for making bindings to other languages.
# In the future, Pinc will be ABI stable and dynamic libraries will be a more viable option.

# If this is too new and an older version works, make an issue or poll request
cmake_minimum_required(VERSION 3.28)

# public options - these must also be applied to your header build, or defined to identical values before including pinc headers

set(PINC_EXTERN "extern" CACHE STRING "extern property before function declarations.")
set(PINC_EXPORT "" CACHE STRING "export property before function definitions")
set(PINC_CALL "" CACHE STRING "calling convention to use")
set(PINC_PROC_CALL "" CACHE STRING "calling convention for user-defined functions")

# private options - these only apply to the library build and do not need to be defined in your code

option(PINC_ENABLE_ERROR_EXTERNAL "Enable external error checking" ON)
option(PINC_ENABLE_ERROR_ASSERT "Enable assert error checking" ON)
option(PINC_ENABLE_ERROR_USER "Enable user error checking" ON)
option(PINC_ENABLE_ERROR_SANITIZE "Enable sanitize error checking" OFF)
option(PINC_ENABLE_ERROR_VALIDATE "Enable validate error checking" OFF)
option(PINC_HAVE_WINDOW_SDL2 "compile with SDL2 window backend" ON)

# Options specific to cmake build

# If this is nonstandard or weird, please make an issue on how to do it better
set(PINC_ADDITIONAL_COMPILE_OPTIONS "" CACHE STRING "Additional compiler options")
set(PINC_ADDITIONAL_LINK_OPTIONS "" CACHE STRING "Additional linker options")

# TODO: more compilers than just assuming clang / compatible compiler flags
set(PINC_COMPILE_OPTIONS ${PINC_ADDITIONAL_COMPILE_OPTIONS} "-Werror" "-pedantic-errors" "-Wall" "-Wextra" "-Wconversion" "-Wno-unused-function" CACHE STRING "options used to compile Pinc and the examples")
set(PINC_LINK_OPTIONS ${PINC_ADDITIONAL_LINK_OPTIONS} "" CACHE STRING "linker flags for pinc and examples")

project(pinc
    VERSION 2.0.0
    DESCRIPTION "Windowing / graphics library written in C"
    HOMEPAGE_URL "TODO"
    LANGUAGES C
)

add_library(pinc
    # Actual source files
    src/pinc_main.c
    src/platform/platform.c
    src/sdl2.c
    src/arena.c
    # Internal headers - these are so the LSP works correctly, and have nothing to do with the build itself
    # there are a lot of these because implementations of functions tend to get grouped together more than macros and types
    src/pinc_error.h
    src/pinc_main.h
    src/pinc_options.h
    src/pinc_types.h
    src/sdl2.h
    src/sdl2load.h
    src/window.h
    src/libs/dynamic_allocator.h
    src/libs/pstring.h
    src/platform/platform.h
)
set_target_properties(pinc PROPERTIES C_STANDARD 99)
set_target_properties(pinc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(pinc PROPERTIES PUBLIC_HEADER include/pinc.h)
set_target_properties(pinc PROPERTIES PUBLIC_HEADER include/pinc_opengl.h)
target_include_directories(pinc PUBLIC include)
target_include_directories(pinc PRIVATE src)
target_compile_definitions(pinc
    PUBLIC PINC_EXTERN=${PINC_EXTERN}
    PUBLIC PINC_EXPORT=${PINC_EXPORT}
    PUBLIC PINC_CALL=${PINC_CALL}
    PUBLIC PINC_PROC_CALL=${PINC_PROC_CALL}
    PRIVATE PINC_ENABLE_ERROR_EXTERNAL=${PINC_ENABLE_ERROR_EXTERNAL}
    PRIVATE PINC_ENABLE_ERROR_ASSERT=${PINC_ENABLE_ERROR_ASSERT}
    PRIVATE PINC_ENABLE_ERROR_USER=${PINC_ENABLE_ERROR_USER}
    PRIVATE PINC_ENABLE_ERROR_SANITIZE=${PINC_ENABLE_ERROR_SANITIZE}
    PRIVATE PINC_ENABLE_ERROR_VALIDATE=${PINC_ENABLE_ERROR_VALIDATE}
    PRIVATE PINC_HAVE_WINDOW_SDL2=${PINC_HAVE_WINDOW_SDL2}
)

target_compile_options(pinc PRIVATE ${PINC_COMPILE_OPTIONS})

install(TARGETS pinc
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# examples - Everything beyond this point is OPTIONAL for additional build systems.

# Example 0_window

add_executable(example_window
    examples/0_window.c
)

target_include_directories(example_window PUBLIC include)
target_include_directories(example_window PRIVATE examples)

target_compile_options(example_window PRIVATE ${PINC_COMPILE_OPTIONS})
target_link_options(example_window PRIVATE ${PINC_LINK_OPTIONS})

target_link_libraries(example_window PUBLIC pinc)

# Example 1_multi_init

add_executable(example_multi_init
    examples/1_multi_init.c
)

target_include_directories(example_multi_init PUBLIC include)
target_include_directories(example_multi_init PRIVATE examples)

target_compile_options(example_multi_init PRIVATE ${PINC_COMPILE_OPTIONS})
target_link_options(example_multi_init PRIVATE ${PINC_LINK_OPTIONS})

target_link_libraries(example_multi_init PUBLIC pinc)

# Example 2_opengl

add_executable(example_opengl
    examples/2_opengl.c
)

target_include_directories(example_opengl PUBLIC include)
target_include_directories(example_opengl PRIVATE examples)

target_compile_options(example_opengl PRIVATE ${PINC_COMPILE_OPTIONS})
target_link_options(example_opengl PRIVATE ${PINC_LINK_OPTIONS})

target_link_libraries(example_opengl PUBLIC pinc)
