find_package(PkgConfig)
pkg_check_modules(PC_libelf REQUIRED libelf=0.8.13)

find_path(libelf_INCLUDE_DIR libelf.h
          HINTS ${PC_libelf_INCLUDEDIR} ${PC_libelf_INCLUDE_DIRS})

find_library(libelf_LIBRARY NAMES elf
             HINTS ${PC_libelf_LIBDIR} ${PC_libelf_LIBRARY_DIRS} )

set(libelf_LIBRARIES ${libelf_LIBRARY})
set(libelf_INCLUDE_DIRS ${libelf_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libelf DEFAULT_MSG
                                  libelf_LIBRARY libelf_INCLUDE_DIR)

mark_as_advanced(libelf_INCLUDE_DIR libelf_LIBRARY )
