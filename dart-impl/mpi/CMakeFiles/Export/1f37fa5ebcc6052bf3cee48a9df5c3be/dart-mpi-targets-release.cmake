#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dart-mpi" for configuration "Release"
set_property(TARGET dart-mpi APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dart-mpi PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE "/nix/store/lqibl7p1hy1l6imy2v7zmxsvsy5hlqkg-openmpi-5.0.6/lib/libmpi.so;dart-base;rt"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdart-mpi.a"
  )

list(APPEND _cmake_import_check_targets dart-mpi )
list(APPEND _cmake_import_check_files_for_dart-mpi "${_IMPORT_PREFIX}/lib/libdart-mpi.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
