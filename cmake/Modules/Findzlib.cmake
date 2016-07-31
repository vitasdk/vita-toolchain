find_package(PkgConfig)
pkg_check_modules(PC_zlib zlib>=1.2.8)

find_path(zlib_INCLUDE_DIR zlib.h
          HINTS ${PC_zlib_INCLUDEDIR} ${PC_zlib_INCLUDE_DIRS})

find_library(zlib_LIBRARY NAMES z
             HINTS ${PC_zlib_LIBDIR} ${PC_zlib_LIBRARY_DIRS} )

set(zlib_LIBRARIES ${zlib_LIBRARY})
set(zlib_INCLUDE_DIRS ${zlib_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(zlib DEFAULT_MSG
                                  zlib_LIBRARY zlib_INCLUDE_DIR)

mark_as_advanced(zlib_INCLUDE_DIR zlib_LIBRARY )
