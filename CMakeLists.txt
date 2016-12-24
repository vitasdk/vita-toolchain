cmake_minimum_required(VERSION 2.6)
project(vita-toolchain)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(SRC_DIR src)
option (COVERAGE "Build with coverage support" OFF)

find_package(libelf REQUIRED)
find_package(zlib REQUIRED)
find_package(libzip REQUIRED)
find_package(libyaml REQUIRED)

include_directories(${libelf_INCLUDE_DIRS})
include_directories(${zlib_INCLUDE_DIRS})
include_directories(${libzip_INCLUDE_DIRS})
include_directories(${libyaml_INCLUDE_DIRS})

add_definitions(-std=gnu99) # -Wall -Wextra

if(USE_BUNDLED_ENDIAN_H)
	add_definitions(-DUSE_BUNDLED_ENDIAN_H)
endif()

if(DEFINED ZIP_STATIC)
	add_definitions(-DZIP_STATIC)
endif()

if (WIN32)
	add_definitions(-DYAML_DECLARE_STATIC)
endif()

enable_testing ()
macro(vita_tool name libs)
	SET(tool "vita-${name}")
	SET(CMAKE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
	SET(src "${SRC_DIR}/${tool}")
	FOREACH(f ${ARGN})
		LIST(APPEND src "${SRC_DIR}/${f}")
	ENDFOREACH(f)
	add_executable(${tool} ${src})
	target_link_libraries(${tool} ${libs})
	install(TARGETS ${tool} DESTINATION bin)
	#not arguments should bring the USAGE screen
	add_test(${name}_usage ${CMAKE_BINARY_DIR}/${tool})
	set_tests_properties(${name}_usage PROPERTIES PASS_REGULAR_EXPRESSION Usage)
	#not arguments should have a non-zero return code
	add_test(${name}_retcode ${CMAKE_BINARY_DIR}/${tool})
	set_tests_properties(${name}_retcode PROPERTIES WILL_FAIL TRUE)
endmacro()

vita_tool(libs-gen   "${libyaml_LIBRARIES}" import import-parse yaml-tree yaml-treeutil)
vita_tool(elf-create "${libyaml_LIBRARIES};${libelf_LIBRARIES}" velf import import-parse export-parse elf-defs sce-elf varray elf-utils sha256 yaml-tree yaml-treeutil)
vita_tool(mksfoex    "")
vita_tool(make-sfo   "")
vita_tool(make-fself "${zlib_LIBRARIES}" sha256)
vita_tool(make-vpk   "${zlib_LIBRARIES};${libzip_LIBRARIES}")
vita_tool(pack-vpk   "${zlib_LIBRARIES};${libzip_LIBRARIES}")
vita_tool(elf-export "${libyaml_LIBRARIES}" yaml-tree yaml-treeutil sha256 yaml-emitter export-parse)


if(COVERAGE)
	find_program (GCOV_EXECUTABLE gcov)
	find_program (LCOV_EXECUTABLE lcov)
	if (NOT GCOV_EXECUTABLE OR NOT LCOV_EXECUTABLE)
		message(FATAL_ERROR "gcov or lcov was not found")
	endif ()
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -ftest-coverage -fprofile-arcs")
	add_test(gencov lcov --capture --directory ./ --output-file coverage.info)
	add_test(genhtml genhtml coverage.info --output-directory coverage)
endif()

install(FILES
	contrib/usr/share/*
	DESTINATION share)



