cmake_minimum_required(VERSION 2.6)
project(vita-toolchain)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
if (NOT TOOLCHAIN_DEPS_DIR)
    set(TOOLCHAIN_DEPS_DIR "${CMAKE_SOURCE_DIR}/builds/deps_build")
endif()
set(ENV{PKG_CONFIG_PATH} "${TOOLCHAIN_DEPS_DIR}/lib/pkgconfig:${TOOLCHAIN_DEPS_DIR}/share/pkgconfig")

add_subdirectory(src)
add_subdirectory(cmake_toolchain)
