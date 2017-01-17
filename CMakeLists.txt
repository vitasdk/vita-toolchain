cmake_minimum_required(VERSION 2.6)
project(vita-toolchain)
option(COVERAGE "Build with coverage support" OFF)
add_definitions(-std=gnu99)
set(LIB ${CMAKE_SOURCE_DIR}/deps/lib/lib)

enable_testing()

if(NOT EXISTS ${LIB}elf.a)
	execute_process(COMMAND bash ${CMAKE_SOURCE_DIR}/deps.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

function(vita_tool)
	FOREACH(arg ${ARGN})
		LIST(APPEND src ${CMAKE_SOURCE_DIR}/src/${arg}.c)
	ENDFOREACH(arg)
	add_executable(${ARGV0} ${src})
	include_directories(${CMAKE_SOURCE_DIR}/deps/include ${CMAKE_SOURCE_DIR}/deps/lib/libzip/include)
	target_link_libraries(${ARGV0} ${LIB}elf.a ${LIB}zip.a ${LIB}z.a ${LIB}yaml.a)
	install(TARGETS ${ARGV0} DESTINATION bin)
	
	# generate some basic functionals tests
	add_test(${ARGV0}_usage ${CMAKE_BINARY_DIR}/${ARGV0})# no arguments should bring the USAGE screen
	set_tests_properties(${ARGV0}_usage PROPERTIES PASS_REGULAR_EXPRESSION [Uu]sage)
	add_test(${ARGV0}_retcode ${CMAKE_BINARY_DIR}/${ARGV0})# no arguments should have a failed (non-zero) return code
	set_tests_properties(${ARGV0}_retcode PROPERTIES WILL_FAIL TRUE)
endfunction()

vita_tool(vita-elf-create vita-elf vita-import vita-import-parse vita-export-parse elf-defs sce-elf varray elf-utils sha256 yamltree yamltreeutil elf-create-argp)
vita_tool(vita-libs-gen vita-import vita-import-parse yamltree yamltreeutil)
vita_tool(vita-mksfoex)
vita_tool(vita-make-fself sha256)
vita_tool(vita-pack-vpk)
vita_tool(vita-elf-export yamltree yamltreeutil sha256 yamlemitter vita-export-parse)

add_test(build-bin   bash -c "arm-vita-eabi-gcc -Wl,-q -o test.elf -xc - <<<'int main(){return 0;}'")
add_test(build-velf  ./vita-elf-create test.elf  test.velf)
add_test(build-fself ./vita-make-fself test.velf test.fself)
add_test(build-sfoex ./vita-mksfoex    sample -s STITLE=sample -d ID=42 test.sfo)
add_test(build-vpk   ./vita-pack-vpk   -s test.sfo -b test.fself -a Makefile test.vpk)

if(COVERAGE)
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -ftest-coverage -fprofile-arcs")
	find_program (GCOV_EXECUTABLE gcov)
	find_program (LCOV_EXECUTABLE lcov)
	if (NOT GCOV_EXECUTABLE)
		message(FATAL_ERROR "gcov was not found, can't do coverage")
	endif ()
	if (NOT LCOV_EXECUTABLE)
		message("lcov was not found, skip html report generation")
	else()
		add_test(lcov lcov --capture --directory ./ --output-file coverage.info)
		add_test(genhtml genhtml coverage.info --output-directory coverage)
	endif ()
endif()

install(FILES
	contrib/usr/share/*
	DESTINATION share)
