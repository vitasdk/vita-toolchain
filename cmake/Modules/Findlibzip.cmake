find_package(PkgConfig)
pkg_check_modules(PC_libzip REQUIRED libzip>=1.1.3)

find_path(libzip_INCLUDE_DIR zip.h
          HINTS ${PC_libzip_INCLUDEDIR} ${PC_libzip_INCLUDE_DIRS})

find_path(libzip_CONFIG_INCLUDE_DIR zipconf.h
          HINTS ${PC_libzip_INCLUDEDIR} ${PC_libzip_INCLUDE_DIRS})

find_library(libzip_LIBRARY NAMES zip
             HINTS ${PC_libzip_LIBDIR} ${PC_libzip_LIBRARY_DIRS} )

set(libzip_LIBRARIES ${libzip_LIBRARY})
set(libzip_INCLUDE_DIRS ${libzip_INCLUDE_DIR} ${libzip_CONFIG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libzip DEFAULT_MSG
                                  libzip_LIBRARY libzip_INCLUDE_DIR)

mark_as_advanced(libzip_INCLUDE_DIR libzip_LIBRARY )
