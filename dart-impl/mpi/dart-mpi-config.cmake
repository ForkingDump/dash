# - Config file for the dart package
# - provides support for all transitive dependencies
#
# - Automatically locates DART-BASE
# - DART-IMPL is not imported, as the user should be
# - able to select the implementation


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was dart-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set(DART_INSTALL_PREFIX "${PACKAGE_PREFIX_DIR}")

find_package(DART-BASE REQUIRED HINTS "${DASH_INSTALL_PREFIX}/share/cmake")

include("${DASH_INSTALL_PREFIX}/share/cmake/dart-mpi-targets.cmake")

