find_package(PkgConfig)
pkg_check_modules(PC_zlib zlib>=1.2.8)

find_path(libz_INCLUDE_DIR zlib.h
          HINTS ${PC_libz_INCLUDEDIR} ${PC_libz_INCLUDE_DIRS})

find_library(libz_LIBRARY NAMES z
             HINTS ${PC_libz_LIBDIR} ${PC_libz_LIBRARY_DIRS} )

set(libz_LIBRARIES ${libz_LIBRARY})
set(libz_INCLUDE_DIRS ${libz_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(zlib DEFAULT_MSG
                                  libz_LIBRARY libz_INCLUDE_DIR)

mark_as_advanced(libz_INCLUDE_DIR libz_LIBRARY )
