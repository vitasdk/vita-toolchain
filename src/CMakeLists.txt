find_package(Jansson REQUIRED)
find_package(libelf REQUIRED)
find_package(zlib REQUIRED)
find_package(libzip REQUIRED)
find_package(libyaml REQUIRED)

include_directories(${Jansson_INCLUDE_DIRS})
include_directories(${libelf_INCLUDE_DIRS})
include_directories(${zlib_INCLUDE_DIRS})
include_directories(${libzip_INCLUDE_DIRS})
include_directories(${libyaml_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "-std=gnu99")

if(USE_BUNDLED_ENDIAN_H)
	add_definitions(-DUSE_BUNDLED_ENDIAN_H)
endif()

if(DEFINED ZIP_STATIC)
	add_definitions(-DZIP_STATIC)
endif()

if(DEFINED DEFAULT_JSON)
	add_definitions(-DDEFAULT_JSON="${DEFAULT_JSON}")
endif()

add_executable(vita-libs-gen vita-libs-gen.c vita-import.c vita-import-parse.c)
add_executable(vita-elf-create vita-elf-create.c vita-elf.c vita-import.c vita-import-parse.c elf-defs.c sce-elf.c varray.c elf-utils.c)
add_executable(vita-mksfoex vita-mksfoex.c getopt_long.c)
add_executable(vita-make-fself vita-make-fself.c)
add_executable(vita-pack-vpk vita-pack-vpk.c)

target_link_libraries(vita-libs-gen ${Jansson_LIBRARIES})
target_link_libraries(vita-elf-create ${Jansson_LIBRARIES} ${libelf_LIBRARIES})
target_link_libraries(vita-pack-vpk ${libzip_LIBRARIES} ${zlib_LIBRARIES})

install(TARGETS vita-libs-gen DESTINATION bin)
install(TARGETS vita-elf-create DESTINATION bin)
install(TARGETS vita-mksfoex DESTINATION bin)
install(TARGETS vita-make-fself DESTINATION bin)
install(TARGETS vita-pack-vpk DESTINATION bin)
