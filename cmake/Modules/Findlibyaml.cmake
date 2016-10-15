find_package(PkgConfig)
pkg_check_modules(PC_libyaml libyaml>=2.7)

find_path(libyaml_INCLUDE_DIR yaml.h
          HINTS ${PC_libyaml_INCLUDEDIR} ${PC_libyaml_INCLUDE_DIRS})

find_library(libyaml_LIBRARY NAMES libyaml
             HINTS ${PC_libyaml_LIBDIR} ${PC_libyaml_LIBRARY_DIRS} )

set(libyaml_LIBRARIES ${libyaml_LIBRARY})
set(libyaml_INCLUDE_DIRS ${libyaml_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libyaml DEFAULT_MSG
                                  libyaml_LIBRARY libyaml_INCLUDE_DIR)

mark_as_advanced(libyaml_INCLUDE_DIR libyaml_LIBRARY )
