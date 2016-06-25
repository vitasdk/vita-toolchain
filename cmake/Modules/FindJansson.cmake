find_package(PkgConfig)
pkg_check_modules(PC_Jansson jansson>=2.7)

find_path(Jansson_INCLUDE_DIR jansson.h
          HINTS ${PC_Jansson_INCLUDEDIR} ${PC_Jansson_INCLUDE_DIRS})

find_library(Jansson_LIBRARY NAMES jansson
             HINTS ${PC_Jansson_LIBDIR} ${PC_Jansson_LIBRARY_DIRS} )

set(Jansson_LIBRARIES ${Jansson_LIBRARY})
set(Jansson_INCLUDE_DIRS ${Jansson_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jansson DEFAULT_MSG
                                  Jansson_LIBRARY Jansson_INCLUDE_DIR)

mark_as_advanced(Jansson_INCLUDE_DIR Jansson_LIBRARY )
