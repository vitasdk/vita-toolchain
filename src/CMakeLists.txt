find_package(libelf REQUIRED)
find_package(zlib REQUIRED)
find_package(libzip REQUIRED)
find_package(libyaml REQUIRED)

include_directories(${libelf_INCLUDE_DIRS})
include_directories(${zlib_INCLUDE_DIRS})
include_directories(${libzip_INCLUDE_DIRS})
include_directories(${libyaml_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "-g -std=gnu99")

if(USE_BUNDLED_ENDIAN_H)
	add_definitions(-DUSE_BUNDLED_ENDIAN_H)
endif()

if(DEFINED ZIP_STATIC)
	add_definitions(-DZIP_STATIC)
endif()

if (WIN32)
	add_definitions(-DYAML_DECLARE_STATIC)
endif()

add_executable(vita-libs-gen vita-libs-gen.c vita-import.c vita-import-parse.c yamltree.c yamltreeutil.c)
add_executable(vita-elf-create vita-elf-create.c elf-create-argp.c vita-elf.c vita-import.c vita-import-parse.c vita-export-parse.c elf-defs.c sce-elf.c varray.c elf-utils.c sha256.c yamltree.c yamltreeutil.c)
add_executable(vita-mksfoex vita-mksfoex.c getopt_long.c)
add_executable(vita-make-fself vita-make-fself.c)
add_executable(vita-pack-vpk vita-pack-vpk.c)
add_executable(vita-elf-export vita-elf-export.c yamltree.c yamltreeutil.c sha256.c yamlemitter.c vita-export-parse.c)

target_link_libraries(vita-libs-gen ${libyaml_LIBRARIES})
target_link_libraries(vita-elf-create ${libelf_LIBRARIES} ${libyaml_LIBRARIES})
target_link_libraries(vita-pack-vpk ${libzip_LIBRARIES} ${zlib_LIBRARIES})
target_link_libraries(vita-elf-export ${libyaml_LIBRARIES})
target_link_libraries(vita-make-fself ${zlib_LIBRARIES})

install(TARGETS vita-libs-gen DESTINATION bin)
install(TARGETS vita-elf-create DESTINATION bin)
install(TARGETS vita-mksfoex DESTINATION bin)
install(TARGETS vita-make-fself DESTINATION bin)
install(TARGETS vita-pack-vpk DESTINATION bin)
install(TARGETS vita-elf-export DESTINATION bin)
