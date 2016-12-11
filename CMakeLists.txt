cmake_minimum_required(VERSION 2.6)
project(vita-toolchain)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_subdirectory(src)
add_subdirectory(cmake_toolchain)
