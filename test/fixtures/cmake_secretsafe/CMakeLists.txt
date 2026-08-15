cmake_minimum_required(VERSION 3.10)
project(cmake_secretsafe_test C)

# Stub tool paths: this project only configures (never builds), so the
# custom-command lines just need to reference the right flags, not run.
set(VITA_ELF_CREATE "echo")
set(VITA_ELF_EXPORT "echo")
set(VITA_LIBS_GEN "echo")
set(VITA_LIBS_GEN_2 "echo")
set(VITA_MAKE_FSELF "echo")
set(VITA_PACK_VPK "echo")

include(${VITA_CMAKE})

# SELF_FLAG picks which vita_create_self option to exercise: this fixture is
# configured fresh (one macro call per configure) once per flag combination,
# since calling the macro more than once in the same project leaks flags
# between calls (vitasdk/vita-toolchain#289) — unrelated to what's tested here.
set(SELF_FLAG "" CACHE STRING "vita_create_self option under test")

add_executable(mytarget dummy.c)

if(SELF_FLAG STREQUAL "")
  vita_create_self(mytarget.self mytarget)
else()
  vita_create_self(mytarget.self mytarget ${SELF_FLAG})
endif()
