find_package(libelf REQUIRED)
find_package(zlib REQUIRED)
find_package(libzip REQUIRED)
find_package(libyaml REQUIRED)

include_directories(${libelf_INCLUDE_DIRS})
include_directories(${zlib_INCLUDE_DIRS})
include_directories(${libzip_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "-fvisibility=hidden -g -std=gnu99")

if(USE_BUNDLED_ENDIAN_H)
	add_definitions(-DUSE_BUNDLED_ENDIAN_H)
endif()

if(DEFINED ZIP_STATIC)
	add_definitions(-DZIP_STATIC)
endif()

if (WIN32)
	# See "Dynamic-Link Library Search Order (Windows)
	# Standard Search Order for Desktop Applications"
	# https://msdn.microsoft.com/en-us/library/windows/desktop/ms682586(v=vs.85).aspx#standard_search_order_for_desktop_applications
	set(runtime_destination bin)
	add_definitions(-DYAML_DECLARE_STATIC)
else()
	set(runtime_destination lib)

	if(APPLE)
		set(install_rpath @rpath:@loader_path/lib)
	else()
		set(install_rpath $ORIGIN/../lib)
	endif()
endif()

add_library(vita-yaml yamltree.c yamltreeutil.c)
add_library(vita-export vita-export-parse.c sha256.c)
add_library(vita-import vita-import.c vita-import-parse.c)

set_target_properties(vita-yaml PROPERTIES
	INCLUDE_DIRECTORIES "${libyaml_INCLUDE_DIRS};${CMAKE_CURRENT_SOURCE_DIR}/build")
set_target_properties(vita-export PROPERTIES
	INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/build
	PUBLIC_HEADER "vita-toolchain-public.h;vita-export.h")
set_target_properties(vita-import PROPERTIES
	INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/build
	PUBLIC_HEADER "vita-toolchain-public.h;vita-import.h")

target_include_directories(vita-yaml PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>)

target_include_directories(vita-export PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>)

target_include_directories(vita-import PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>)

add_executable(vita-libs-gen vita-libs-gen.c)
add_executable(vita-elf-create vita-elf-create.c elf-create-argp.c vita-elf.c elf-defs.c sce-elf.c varray.c elf-utils.c)
add_executable(vita-mksfoex vita-mksfoex.c getopt_long.c)
add_executable(vita-make-fself vita-make-fself.c sha256.c)
add_executable(vita-pack-vpk vita-pack-vpk.c)
add_executable(vita-elf-export vita-elf-export.c yamlemitter.c)

if(install_rpath)
	set_target_properties(vita-libs-gen PROPERTIES INSTALL_RPATH ${install_rpath})
	set_target_properties(vita-elf-create PROPERTIES INSTALL_RPATH ${install_rpath})
	set_target_properties(vita-elf-export PROPERTIES INSTALL_RPATH ${install_rpath})
	set_target_properties(vita-make-fself PROPERTIES INSTALL_RPATH ${install_rpath})
endif()

target_link_libraries(vita-yaml ${libyaml_LIBRARIES})
target_link_libraries(vita-import vita-yaml)
target_link_libraries(vita-export vita-yaml)
target_link_libraries(vita-libs-gen vita-import)
target_link_libraries(vita-elf-create vita-export vita-import ${libelf_LIBRARIES})
target_link_libraries(vita-pack-vpk ${libzip_LIBRARIES} ${zlib_LIBRARIES})
target_link_libraries(vita-elf-export vita-yaml vita-export)
target_link_libraries(vita-make-fself ${zlib_LIBRARIES} vita-export vita-import)

if(BUILD_SHARED_LIBS)
	target_compile_definitions(vita-yaml PUBLIC VITA_TOOLCHAIN_SHARED)
	target_compile_definitions(vita-import PUBLIC VITA_TOOLCHAIN_SHARED)
	target_compile_definitions(vita-export PUBLIC VITA_TOOLCHAIN_SHARED)
	install(TARGETS vita-yaml
		DESTINATION lib
		RUNTIME DESTINATION ${runtime_destination})
endif()

install(TARGETS vita-export
	DESTINATION lib
	PUBLIC_HEADER DESTINATION include
	RUNTIME DESTINATION ${runtime_destination})
install(TARGETS vita-import
	DESTINATION lib
	PUBLIC_HEADER DESTINATION include
	RUNTIME DESTINATION ${runtime_destination})
install(TARGETS vita-libs-gen DESTINATION bin)
install(TARGETS vita-elf-create DESTINATION bin)
install(TARGETS vita-mksfoex DESTINATION bin)
install(TARGETS vita-make-fself DESTINATION bin)
install(TARGETS vita-pack-vpk DESTINATION bin)
install(TARGETS vita-elf-export DESTINATION bin)
