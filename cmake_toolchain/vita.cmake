## Copyright (C) 2016 Yifan Lu
##
## This software may be modified and distributed under the terms
## of the MIT license.  See the LICENSE file for details.

## Advanced users may be interested in setting the following
##   - VITA_ELF_CREATE_FLAGS
##   - VITA_MAKE_FSELF_FLAGS
##   - VITA_ELF_EXPORT_FLAGS
##   - VITA_LIBS_GEN_FLAGS
##   - VITA_MKSFOEX_FLAGS
##   - VITA_PACK_VPK_FLAGS

## add_include_guard() has been added in 3.10, but it's too recent so we don't use it
if(__VITA_CMAKE_INCLUDED__)
  return()
endif()
set(__VITA_CMAKE_INCLUDED__ TRUE)

include(CMakeParseArguments)

##################################################
## MACRO: vita_create_self
##
## Generate a SELF from an ARM EABI ELF
##   vita_create_self(target source
##                    [CONFIG file]
##                    [UNCOMPRESSED]
##                    [UNSAFE]
##                    [STRIPPED])
##
## @param target
##   A CMake custom target of this given name
## @param source
##   The ARM EABI ELF target (from add_executable for example)
##   or path to a provided ELF file
## @param[opt] UNCOMPRESSED
##   Do NOT compress the result SELF (compression is default)
## @param[opt] UNSAFE
##   The homebrew uses private/system APIs and requires extended permissions
## @param[opt] CONFIG file
##   Path to a YAML config file defining exports and other optional information
##
macro(vita_create_self target source)
  set(VITA_ELF_CREATE_FLAGS "${VITA_ELF_CREATE_FLAGS}" CACHE STRING "vita-elf-create flags")
  set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS}" CACHE STRING "vita-make-fself flags")

  set(options UNCOMPRESSED UNSAFE STRIPPED)
  set(oneValueArgs CONFIG)
  cmake_parse_arguments(vita_create_self "${options}" "${oneValueArgs}" "" ${ARGN})

  if(vita_create_self_STRIPPED)
    set(VITA_ELF_CREATE_FLAGS "${VITA_ELF_CREATE_FLAGS} -s")
  endif()
  if(vita_create_self_CONFIG)
    get_filename_component(fconfig ${vita_create_self_CONFIG} ABSOLUTE)
    set(VITA_ELF_CREATE_FLAGS "${VITA_ELF_CREATE_FLAGS} -e ${fconfig}")
  endif()
  if(NOT vita_create_self_UNCOMPRESSED)
    set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -c")
  endif()
  if(NOT vita_create_self_UNSAFE)
    set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -s")
  endif()

  ## check source for being a target, otherwise it is a file path
  if(TARGET ${source})
    set(sourcepath ${CMAKE_CURRENT_BINARY_DIR}/${source})
  else()
    set(sourcepath ${source})
  endif()
  get_filename_component(sourcefile ${sourcepath} NAME)

  ## VELF command
  separate_arguments(VITA_ELF_CREATE_FLAGS)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.velf
    COMMAND ${VITA_ELF_CREATE} ${VITA_ELF_CREATE_FLAGS} ${sourcepath} ${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.velf
    DEPENDS ${sourcepath}
    COMMENT "Converting to Sony ELF ${sourcefile}.velf" VERBATIM
  )

  set(self_outfile ${CMAKE_CURRENT_BINARY_DIR}/${target}.out)

  ## SELF command
  separate_arguments(VITA_MAKE_FSELF_FLAGS)
  add_custom_command(OUTPUT ${self_outfile}
    COMMAND ${VITA_MAKE_FSELF} ${VITA_MAKE_FSELF_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.velf ${self_outfile}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${sourcefile}.velf
    COMMENT "Creating SELF ${target}"
  )

  ## SELF target
  add_custom_target(${target}
    ALL
    DEPENDS ${self_outfile}
    COMMAND ${CMAKE_COMMAND} -E copy ${self_outfile} ${target}
  )

  if(TARGET ${source})
    add_dependencies(${target} ${source})
  endif()
endmacro(vita_create_self)
##################################################

##################################################
## MACRO: vita_create_stubs
##
## Generate stub libraries from a Sony ELF and config file
##   vita_create_stubs(target-dir source config
##                     [KERNEL])
##
## @param target-dir
##   A CMake custom target of this given name (will be a directory containing
##   the stubs)
## @param source
##   The ARM EABI ELF target (from add_executable for example)
## @param config
##   Path to a YAML config file defining exports
## @param[opt] KERNEL
##   Specifies that this module makes kernel exports
##
macro(vita_create_stubs target-dir source config)
  set(VITA_ELF_EXPORT_FLAGS "${VITA_ELF_EXPORT_FLAGS}" CACHE STRING "vita-elf-export flags")
  set(VITA_LIBS_GEN_FLAGS "${VITA_LIBS_GEN_FLAGS}" CACHE STRING "vita-libs-gen flags")

  set(options KERNEL)
  cmake_parse_arguments(vita_create_stubs "${options}" "" "" ${ARGN})

  if(vita_create_stubs_KERNEL)
    set(kind kernel)
  else()
    set(kind user)
  endif()

  ## check source for being a target, otherwise it is a file path
  if(TARGET ${source})
    set(sourcepath ${CMAKE_CURRENT_BINARY_DIR}/${source})
  else()
    set(sourcepath ${source})
  endif()
  get_filename_component(sourcefile ${sourcepath} NAME)

  set(target_yml ${CMAKE_CURRENT_BINARY_DIR}/${target-dir}.yml)

  ## ELF EXPORT command
  separate_arguments(VITA_ELF_EXPORT_FLAGS)
  get_filename_component(fconfig ${config} ABSOLUTE)
  file(READ ${fconfig} fconfig_content)
  string(REGEX REPLACE ":.+" "" target-lib ${fconfig_content})
  add_custom_command(OUTPUT ${target_yml}
    COMMAND ${VITA_ELF_EXPORT} ${kind} ${VITA_ELF_EXPORT_FLAGS} ${sourcepath} ${fconfig} ${target_yml}
    DEPENDS ${sourcepath}
    DEPENDS ${fconfig}
    COMMENT "Generating imports YAML for ${sourcefile}"
  )

  ## ELF EXPORT target
  separate_arguments(VITA_LIBS_GEN_FLAGS)

  set(stub_lib lib${target-lib}_stub.a)
  set(stub_weak_lib lib${target-lib}_stub_weak.a)

  add_custom_target(${target-dir}
    ALL
    COMMAND ${VITA_LIBS_GEN} ${VITA_LIBS_GEN_FLAGS} ${target_yml} ${CMAKE_CURRENT_BINARY_DIR}/${target-dir}
    COMMAND make -C ${CMAKE_CURRENT_BINARY_DIR}/${target-dir}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_BINARY_DIR}/${target-dir}/${stub_lib}
      ${CMAKE_CURRENT_BINARY_DIR}/${stub_lib}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_BINARY_DIR}/${target-dir}/${stub_weak_lib}
      ${CMAKE_CURRENT_BINARY_DIR}/${stub_weak_lib}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${target-dir}.yml
    COMMENT "Building stubs ${target-dir}"
  )

  add_custom_target(${stub_lib}
    ALL
    DEPENDS ${target-dir}
  )

  add_custom_target(${stub_weak_lib}
    ALL
    DEPENDS ${target-dir}
  )

  if(TARGET ${source})
    add_dependencies(${target-dir} ${source})
  endif()
endmacro(vita_create_stubs)
##################################################

##################################################
## MACRO: vita_create_vpk
##
## Creates a homebrew VPK from a SELF
##   vita_create_vpk(target titleid eboot
##                   [VERSION version]
##                   [NAME name]
##                   [FILE path dest])
##
## @param target
##   A CMake custom target of this given name
## @param titleid
##   A nine character identifier for this homebrew. The recommended format is
##   XXXXYYYYY where XXXX is an author unique identifier and YYYYY is a number.
## @param eboot
##   The SELF target (from vita_create_self for example)
##   or path to a provided SELF file
## @param[opt] VERSION
##   A version string
## @param[opt] NAME
##   The display name under the bubble in LiveArea
## @param[opt] FILE
##   Add an additional file at path to dest in the vpk (there can be multiple
##   of this parameter).
##
macro(vita_create_vpk target titleid eboot)
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS}" CACHE STRING "vita-mksfoex flags")
  set(VITA_PACK_VPK_FLAGS "${VITA_PACK_VPK_FLAGS}" CACHE STRING "vita-pack-vpk flags")

  set(oneValueArgs VERSION NAME)
  set(multiValueArgs FILE)
  cmake_parse_arguments(vita_create_vpk "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  list(LENGTH vita_create_vpk_FILE left)
  while(left GREATER 0)
    if(left EQUAL 1)
      message(FATAL_ERROR "Invalid number of arguments")
    endif()
    list(GET vita_create_vpk_FILE 0 fname)
    list(GET vita_create_vpk_FILE 1 fdest)
    get_filename_component(fpath ${fname} ABSOLUTE)
    list(APPEND resources "${fpath}")
    list(REMOVE_AT vita_create_vpk_FILE 0 1)
    set(VITA_PACK_VPK_FLAGS "${VITA_PACK_VPK_FLAGS} -a ${fpath}=${fdest}")
    list(LENGTH vita_create_vpk_FILE left)
  endwhile()

  if(vita_create_vpk_VERSION)
    set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s APP_VER=${vita_create_vpk_VERSION}")
  endif()
  set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s TITLE_ID=${titleid}")
  if(NOT vita_create_vpk_NAME)
    set(vita_create_vpk_NAME "${PROJECT_NAME}")
  endif()

  ## check eboot for being a target, otherwise it is a file path
  if(TARGET ${eboot})
    set(sourcepath ${CMAKE_CURRENT_BINARY_DIR}/${eboot})
  else()
    set(sourcepath ${eboot})
  endif()
  get_filename_component(sourcefile ${sourcepath} NAME)

  ## PARAM.SFO command
  separate_arguments(VITA_MKSFOEX_FLAGS)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target}_param.sfo
    COMMAND ${VITA_MKSFOEX} ${VITA_MKSFOEX_FLAGS} ${vita_create_vpk_NAME} ${CMAKE_CURRENT_BINARY_DIR}/${target}_param.sfo
    DEPENDS ${sourcepath}
    COMMENT "Generating param.sfo for ${target}"
  )

  set(vpk_outfile ${CMAKE_CURRENT_BINARY_DIR}/${target}.out)

  ## VPK command
  separate_arguments(VITA_PACK_VPK_FLAGS)
  add_custom_command(OUTPUT ${vpk_outfile}
    COMMAND ${VITA_PACK_VPK} ${VITA_PACK_VPK_FLAGS} -s ${CMAKE_CURRENT_BINARY_DIR}/${target}_param.sfo -b ${sourcepath} ${vpk_outfile}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${target}_param.sfo
    DEPENDS ${sourcepath}
    DEPENDS ${resources}
    COMMENT "Building vpk ${target}"
  )

  ## VPK target
  add_custom_target(${target}
    ALL
    DEPENDS ${vpk_outfile}
    COMMAND ${CMAKE_COMMAND} -E copy ${vpk_outfile} ${target}
  )

  if(TARGET ${eboot})
    add_dependencies(${target} ${eboot})
  endif()
endmacro(vita_create_vpk)
##################################################
