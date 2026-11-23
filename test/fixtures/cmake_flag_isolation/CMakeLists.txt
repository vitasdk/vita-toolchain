cmake_minimum_required(VERSION 3.16)
project(cmake_flag_isolation_test C)

set(VITA_ELF_CREATE "echo")
set(VITA_ELF_EXPORT "echo")
set(VITA_LIBS_GEN "echo")
set(VITA_MAKE_FSELF "echo")
set(VITA_MKSFOEX "echo")
set(VITA_PACK_VPK "echo")

set(VITA_ELF_CREATE_FLAGS "--elf-base" CACHE STRING "")
set(VITA_MAKE_FSELF_FLAGS "--fself-base" CACHE STRING "")
set(VITA_ELF_EXPORT_FLAGS "--export-base" CACHE STRING "")
set(VITA_LIBS_GEN_FLAGS "--libs-base" CACHE STRING "")
set(VITA_MKSFOEX_FLAGS "--sfo-base" CACHE STRING "")
set(VITA_PACK_VPK_FLAGS "--pack-base" CACHE STRING "")

include("${VITA_CMAKE}")

add_executable(self_a dummy.c)
add_executable(self_b dummy.c)
vita_create_self(first.self self_a STRIPPED MEMSIZE 42)
vita_create_self(second.self self_b UNSAFE UNCOMPRESSED)

add_executable(stub_a dummy.c)
add_executable(stub_b dummy.c)
vita_create_stubs(stubs_a stub_a "${CMAKE_CURRENT_LIST_DIR}/export.yml" GEN_EXPORTS)
vita_create_stubs(stubs_b stub_b "${CMAKE_CURRENT_LIST_DIR}/export.yml" KERNEL GEN_EXPORTS)

vita_create_vpk(first.vpk FIRST0001 first.self
  VERSION 01.23
  NAME "First package"
  FILE "${CMAKE_CURRENT_LIST_DIR}/resource.txt" "data/resource.txt"
)
vita_create_vpk(second.vpk SECOND002 second.self
  NAME "Second package"
)

file(WRITE "${CMAKE_BINARY_DIR}/public-flags.txt"
  "VITA_ELF_CREATE_FLAGS=${VITA_ELF_CREATE_FLAGS}\n"
  "VITA_MAKE_FSELF_FLAGS=${VITA_MAKE_FSELF_FLAGS}\n"
  "VITA_ELF_EXPORT_FLAGS=${VITA_ELF_EXPORT_FLAGS}\n"
  "VITA_LIBS_GEN_FLAGS=${VITA_LIBS_GEN_FLAGS}\n"
  "VITA_MKSFOEX_FLAGS=${VITA_MKSFOEX_FLAGS}\n"
  "VITA_PACK_VPK_FLAGS=${VITA_PACK_VPK_FLAGS}\n"
)
