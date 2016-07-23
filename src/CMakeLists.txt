find_package(Jansson REQUIRED)
find_package(libelf REQUIRED)

include_directories(${Jansson_INCLUDE_DIRS})
include_directories(${libelf_INCLUDE_DIRS})

if(USE_BUNDLED_ENDIAN_H)
	add_definitions(-DUSE_BUNDLED_ENDIAN_H)
endif()

if(DEFINED DEFAULT_JSON)
	add_definitions(-DDEFAULT_JSON="${DEFAULT_JSON}")
endif()

add_executable(vita-libs-gen vita-libs-gen.c vita-import.c vita-import-parse.c)
add_executable(vita-elf-create vita-elf-create.c vita-elf.c vita-import.c vita-import-parse.c elf-defs.c sce-elf.c varray.c elf-utils.c)
add_executable(vita-mksfoex vita-mksfoex.c getopt_long.c)
add_executable(vita-make-fself vita-make-fself.c)

target_link_libraries(vita-libs-gen ${Jansson_LIBRARIES})
target_link_libraries(vita-elf-create ${Jansson_LIBRARIES} ${libelf_LIBRARIES})

install(TARGETS vita-libs-gen DESTINATION bin)
install(TARGETS vita-elf-create DESTINATION bin)
install(TARGETS vita-mksfoex DESTINATION bin)
install(TARGETS vita-make-fself DESTINATION bin)
