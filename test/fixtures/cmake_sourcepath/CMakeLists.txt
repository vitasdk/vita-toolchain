cmake_minimum_required(VERSION 3.10)
project(cmake_sourcepath_test C)

# Stub tool paths: this project only configures (never builds), so the
# custom-command lines just need to reference the right file, not run.
set(VITA_ELF_CREATE "echo")
set(VITA_ELF_EXPORT "echo")
set(VITA_LIBS_GEN "echo")
set(VITA_LIBS_GEN_2 "echo")
set(VITA_MAKE_FSELF "echo")
set(VITA_PACK_VPK "echo")

include(${VITA_CMAKE})

add_executable(mytarget dummy.c)
set_target_properties(mytarget PROPERTIES OUTPUT_NAME othername)

vita_create_self(mytarget.self mytarget UNSAFE)
