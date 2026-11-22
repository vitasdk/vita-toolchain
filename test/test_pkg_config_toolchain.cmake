cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED VITA_TOOLCHAIN_FILE OR NOT DEFINED TEST_SDK_ROOT OR NOT DEFINED TEST_MODE)
  message(FATAL_ERROR "VITA_TOOLCHAIN_FILE, TEST_SDK_ROOT and TEST_MODE are required")
endif()

file(MAKE_DIRECTORY "${TEST_SDK_ROOT}")
set(ENV{VITASDK} "${TEST_SDK_ROOT}")
unset(VITASDK CACHE)
unset(PKG_CONFIG_EXECUTABLE CACHE)
unset(CMAKE_CROSSCOMPILING)

set(expected "${TEST_SDK_ROOT}/bin/arm-vita-eabi-pkg-config.exe")
if(TEST_MODE STREQUAL "legacy")
  set(PKG_CONFIG_EXECUTABLE
    "${TEST_SDK_ROOT}/bin/arm-vita-eabi-pkg-config"
    CACHE FILEPATH "legacy wrapper path")
elseif(TEST_MODE STREQUAL "custom")
  set(expected "${TEST_SDK_ROOT}/custom/pkgconf.exe")
  set(PKG_CONFIG_EXECUTABLE "${expected}" CACHE FILEPATH "custom wrapper path")
elseif(NOT TEST_MODE STREQUAL "default")
  message(FATAL_ERROR "unknown TEST_MODE: ${TEST_MODE}")
endif()

include("${VITA_TOOLCHAIN_FILE}")

if(NOT "${PKG_CONFIG_EXECUTABLE}" STREQUAL "${expected}")
  message(FATAL_ERROR
    "Windows toolchain ${TEST_MODE} case selected '${PKG_CONFIG_EXECUTABLE}', expected '${expected}'")
endif()

file(REMOVE_RECURSE "${TEST_SDK_ROOT}")
message(STATUS "Windows pkg-config toolchain ${TEST_MODE} case passed")
