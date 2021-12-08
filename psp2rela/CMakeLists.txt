find_package(zlib REQUIRED)

include_directories(${zlib_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "-fvisibility=hidden -g -std=gnu99")

add_executable(psp2rela
  src/main.c
  src/debug.c
  src/module_loader.c
  src/module_relocation.c
  src/rela/convert.c
  src/rela/core.c
  src/rela/register.c
  src/rela/data_register.c
)

target_link_libraries(psp2rela
  ${zlib_LIBRARIES}
)

install(TARGETS psp2rela DESTINATION bin)
