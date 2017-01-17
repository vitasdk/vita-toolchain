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

include(CMakeParseArguments)

##################################################
## MACRO: vita_create_self
## 
## Generate a SELF from an ARM EABI ELF
##   vita_create_self(target source
##                    [CONFIG file]
##                    [UNCOMPRESSED]
##                    [UNSAFE])
## 
## @param target
##   A CMake custom target of this given name
## @param source
##   The ARM EABI ELF target (from add_executable for example)
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

  set(options UNCOMPRESSED UNSAFE)
  set(oneValueArgs CONFIG)
  cmake_parse_arguments(vita_create_self "${options}" "${oneValueArgs}" "" ${ARGN})

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
  separate_arguments(VITA_ELF_CREATE_FLAGS)
  add_custom_command(OUTPUT ${source}.velf
    COMMAND ${VITA_ELF_CREATE} ${VITA_ELF_CREATE_FLAGS} ${source} ${source}.velf
    DEPENDS ${source}
    COMMENT "Converting to Sony ELF ${source}.velf" VERBATIM
  )
  separate_arguments(VITA_MAKE_FSELF_FLAGS)
  add_custom_target(${target} ALL
    COMMAND ${VITA_MAKE_FSELF} ${VITA_MAKE_FSELF_FLAGS} ${source}.velf ${target}
    DEPENDS ${source}.velf
    COMMENT "Creating SELF ${target}"
  )
  add_dependencies(${target} ${source})
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
  separate_arguments(VITA_ELF_EXPORT_FLAGS)
  get_filename_component(fconfig ${config} ABSOLUTE)
  add_custom_command(OUTPUT ${target-dir}.yml
    COMMAND ${VITA_ELF_EXPORT} ${kind} ${VITA_ELF_EXPORT_FLAGS} ${source} ${fconfig} ${target-dir}.yml
    DEPENDS ${source} ${fconfig}
    COMMENT "Generating imports YAML for ${source}"
  )
  separate_arguments(VITA_LIBS_GEN_FLAGS)
  add_custom_target(${target-dir} ALL
    COMMAND ${VITA_LIBS_GEN} ${VITA_LIBS_GEN_FLAGS} ${target-dir}.yml ${target-dir}
    COMMAND make -C ${target-dir}
    DEPENDS ${target-dir}.yml
    COMMENT "Building stubs ${target-dir}"
  )
  add_dependencies(${target-dir} ${source})
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
##   The main SELF to package (from vita_create_self for example).
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
  separate_arguments(VITA_MKSFOEX_FLAGS)
  add_custom_command(OUTPUT ${target}_param.sfo
    COMMAND ${VITA_MKSFOEX} ${VITA_MKSFOEX_FLAGS} ${vita_create_vpk_NAME} ${target}_param.sfo
    COMMENT "Generating param.sfo for ${target}"
  )
  separate_arguments(VITA_PACK_VPK_FLAGS)
  add_custom_target(${target} ALL
    COMMAND ${VITA_PACK_VPK} ${VITA_PACK_VPK_FLAGS} -s ${target}_param.sfo -b ${eboot} ${target}
    DEPENDS ${target}_param.sfo
    COMMENT "Building vpk ${target}"
  )
  add_dependencies(${target} ${eboot})
endmacro(vita_create_vpk)
##################################################
