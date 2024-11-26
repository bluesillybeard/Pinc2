# We use cmake because it's convenient, not because it's good!
# If there is a different build system that you think is better or is just worth supporting / using,
# feel free to make an issue or poll request to add it.
# Cmake will be continued to be supported, for those who want to use it with pinc.

# We don't have a pkg-config because Pinc is meant to be linked fully statically.
# Pinc can be linked dynamically, but that option only exists for making bindings to other languages.
# In the future, Pinc will be ABI stable and dynamic libraries will be a more viable option.

# If this is too new and an older version works, make an issue or poll request
cmake_minimum_required(VERSION 3.28)

project(pinc
    VERSION 2.0.0
    DESCRIPTION "Windowing / graphics library written in C"
    HOMEPAGE_URL "TODO"
    LANGUAGES C
)

include(GNUInstallDirs)

# For now, use Pinc unity build
# TODO: Make option to use split build for debug and rebuilding
add_library(pinc
    src/pinc_main.c
)

set_target_properties(pinc PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(pinc PROPERTIES PUBLIC_HEADER include/pinc.h)
target_include_directories(pinc PRIVATE include)
target_include_directories(pinc PRIVATE src)

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
