#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dart-base" for configuration "Release"
set_property(TARGET dart-base APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dart-base PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE "rt"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdart-base.a"
  )

list(APPEND _cmake_import_check_targets dart-base )
list(APPEND _cmake_import_check_files_for_dart-base "${_IMPORT_PREFIX}/lib/libdart-base.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
