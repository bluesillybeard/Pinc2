# We use cmake because it's convenient, not because it's good!
# If there is a different build system that you think is better or is just worth supporting / using,
# feel free to make an issue or poll request to add it.
# Cmake will be continued to be supported, for those who want to use it with pinc.

# We don't have a pkg-config because Pinc is meant to be linked fully statically.
# Pinc can be linked dynamically, but that option only exists for making bindings to other languages.
# In the future, Pinc will be ABI stable and dynamic libraries will be a more viable option.

# If this is too new and an older version works, make an issue or poll request
cmake_minimum_required(VERSION 3.28)

# public
set(PINC_EXTERN "extern" CACHE STRING "extern property before function declarations.")
set(PINC_EXPORT "" CACHE STRING "export property before function definitions")
set(PINC_CALL "" CACHE STRING "calling convention to use")
set(PINC_PROC_CALL "" CACHE STRING "calling convention for user-defined functions")
# private
set(PINC_ERROR_SETTING 2 CACHE STRING "error reporting severity")
option(PINC_HAVE_WINDOW_SDL2 "compile with SDL2 window backend" ON)
option(PINC_HAVE_GRAPHICS_RAW_OPENGL, "compile with raw opengl graphics backend" ON)

project(pinc
    VERSION 2.0.0
    DESCRIPTION "Windowing / graphics library written in C"
    HOMEPAGE_URL "TODO"
    LANGUAGES C
)

include(GNUInstallDirs)

add_library(pinc
    src/pinc_main.c
    src/platform/platform.c
)

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
    PRIVATE PINC_ERROR_SETTING=${PINC_ERROR_SETTING}
    PRIVATE PINC_HAVE_WINDOW_SDL2=${PINC_HAVE_WINDOW_SDL2}
    PRIVATE PINC_HAVE_GRAPHICS_RAW_OPENGL=${PINC_HAVE_GRAPHICS_RAW_OPENGL}
)
target_compile_options(pinc PRIVATE -Wall)

install(TARGETS pinc
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# examples

add_executable(example_window
    examples/0_window.c
)

target_include_directories(example_window PUBLIC include)
target_include_directories(example_window PRIVATE examples)

target_link_libraries(example_window PUBLIC pinc)
