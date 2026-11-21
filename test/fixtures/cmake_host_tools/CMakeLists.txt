cmake_minimum_required(VERSION 3.16)
project(cmake_host_tools_test C)

set(VITA_ELF_CREATE "echo")
set(VITA_ELF_EXPORT "echo")
set(VITA_LIBS_GEN "echo")
set(VITA_MAKE_FSELF "echo")
set(VITA_MKSFOEX "echo")
set(VITA_PACK_VPK "echo")

include(${VITA_CMAKE})

add_executable(mytarget dummy.c)
vita_create_self(mytarget.self mytarget UNSAFE REL_OPTIMIZE)
