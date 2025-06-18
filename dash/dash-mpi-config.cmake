# - Config file for the dash package
# - provides compiler flags of dash installation
# - as well as all transitive dependencies
#
# - Automatically locates DART-BASE
# - DART-IMPL is not imported, as the user should be
# - able to select the implementation


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was dash-config.cmake.in                            ########

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

set(DASH_VERSION_MAJOR "0")
set(DASH_VERSION_MINOR "4")
set(DASH_VERSION_PATCH "0")

set(DASH_LIBRARY "dash-mpi")
set(DASH_LIBRARIES ${DASH_LIBRARY} "dart-mpi")
set(DASH_CXX_FLAGS " -DDASH_EXAMPLES_TASKSUPPORT -fopenmp -Wno-sign-compare -DDASH_MPI_IMPL_ID='openmpi' -DOMPI_SKIP_MPICXX  -DDASH -DDASH_ENABLE_DEFAULT_INDEX_TYPE_LONG -DDASH_ENABLE_THREADSUPPORT -DDASH_ENABLE_TEST_LOGGING -DDASH_HAVE_STD_TRIVIALLY_COPYABLE -DDASH_ENV_HOST_SYSTEM_ID='default' -fopenmp -O3 -DNDEBUG  -Ofast -DDASH_RELEASE")
set(DASH_INSTALL_PREFIX "${PACKAGE_PREFIX_DIR}")

find_package(DART-MPI REQUIRED HINTS "${DASH_INSTALL_PREFIX}/share/cmake")

include("${DASH_INSTALL_PREFIX}/share/cmake/${DASH_LIBRARY}-targets.cmake")
