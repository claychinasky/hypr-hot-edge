cmake_minimum_required(VERSION 3.19)
project(hypr-hot-edge
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PkgConfig
find_package(PkgConfig REQUIRED)

# Find Hyprland headers
pkg_check_modules(HYPRLAND REQUIRED hyprland)

# Find other dependencies
pkg_check_modules(DEPS REQUIRED
    pixman-1
    libdrm
)

# Lua headers for the hl.plugin.hot_edge.* bindings (Hyprland links Lua 5.5)
pkg_search_module(LUA REQUIRED lua5.5 lua-5.5 lua55 lua>=5.5)

# Create the shared library
add_library(hypr-hot-edge SHARED
    src/main.cpp
    src/HotEdge.cpp
)

target_include_directories(hypr-hot-edge PRIVATE
    ${HYPRLAND_INCLUDE_DIRS}
    ${DEPS_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIRS}
)

target_compile_options(hypr-hot-edge PRIVATE
    ${HYPRLAND_CFLAGS_OTHER}
)

# No prefix for the .so file
set_target_properties(hypr-hot-edge PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Install target
install(TARGETS hypr-hot-edge
    LIBRARY DESTINATION lib/hyprland/plugins
)
