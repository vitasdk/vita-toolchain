cmake_minimum_required(VERSION 3.0)

project(psp2rela C)

set(CMAKE_C_COMPILE_FEATURES "${CMAKE_C_FLAGS} -Wunused-result -Wl,-q -Wall -O3 -fno-inline -fno-builtin")

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
  z
)

# install(TARGETS psp2rela DESTINATION "$ENV{VITASDK}/bin")
install(TARGETS psp2rela DESTINATION bin)
