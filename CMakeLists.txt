cmake_minimum_required(VERSION 3.4)
project(vita-toolchain LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(TOOLCHAIN_DEPS_DIR "${CMAKE_SOURCE_DIR}/builds/deps_build" CACHE PATH "Prefix to search for dependencies in.")
list(APPEND CMAKE_PREFIX_PATH "${TOOLCHAIN_DEPS_DIR}")

add_subdirectory(src)
add_subdirectory(psp2rela)
add_subdirectory(cmake_toolchain)
add_subdirectory(pkgconfig)
