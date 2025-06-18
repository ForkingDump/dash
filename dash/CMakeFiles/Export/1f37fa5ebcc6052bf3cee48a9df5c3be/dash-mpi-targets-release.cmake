#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dash-mpi" for configuration "Release"
set_property(TARGET dash-mpi APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dash-mpi PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE "rt;atomic;pthread"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdash-mpi.a"
  )

list(APPEND _cmake_import_check_targets dash-mpi )
list(APPEND _cmake_import_check_files_for_dash-mpi "${_IMPORT_PREFIX}/lib/libdash-mpi.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
