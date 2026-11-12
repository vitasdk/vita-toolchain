cmake_minimum_required(VERSION 3.16)
project(pkgconfig-windows-test LANGUAGES C)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
enable_testing()

if(NOT DEFINED VITA_TOOLCHAIN_SOURCE)
  message(FATAL_ERROR "VITA_TOOLCHAIN_SOURCE is required")
endif()

add_subdirectory("${VITA_TOOLCHAIN_SOURCE}/pkgconfig" pkgconfig)
if(NOT TARGET arm-vita-eabi-pkg-config)
  message(FATAL_ERROR "the Windows pkg-config frontend target was not created")
endif()

add_executable(pkgconf-capture ../pkgconf_capture.c)
if(MINGW)
  target_link_options(pkgconf-capture PRIVATE -municode)
endif()

add_test(NAME test_pkg_config_windows
  COMMAND ${Python3_EXECUTABLE} ${VITA_TOOLCHAIN_SOURCE}/test/test_pkg_config_windows.py
    $<TARGET_FILE:arm-vita-eabi-pkg-config>
    $<TARGET_FILE:pkgconf-capture>
)

if(CMAKE_HOST_WIN32)
  foreach(test_mode default legacy custom)
    add_test(NAME test_pkg_config_toolchain_${test_mode}
      COMMAND ${CMAKE_COMMAND}
        "-DVITA_TOOLCHAIN_FILE=${VITA_TOOLCHAIN_SOURCE}/cmake_toolchain/vita.toolchain.cmake"
        "-DTEST_SDK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/Vita SDK toolchain test/${test_mode}"
        -DTEST_MODE=${test_mode}
        -P ${VITA_TOOLCHAIN_SOURCE}/test/test_pkg_config_toolchain.cmake
    )
  endforeach()
endif()
