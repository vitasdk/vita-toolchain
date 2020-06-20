cmake_minimum_required(VERSION 2.6)
project(vita-toolchain)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SOURCE_DIR}/builds/deps_build/lib/pkgconfig")
include_directories("${CMAKE_SOURCE_DIR}/builds/deps_build/include")

add_subdirectory(src)
add_subdirectory(cmake_toolchain)
