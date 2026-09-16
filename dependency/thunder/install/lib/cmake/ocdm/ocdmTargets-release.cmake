#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ocdm::ocdm" for configuration "Release"
set_property(TARGET ocdm::ocdm APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ocdm::ocdm PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore;WPEFrameworkCOM::WPEFrameworkCOM;WPEFrameworkMessaging::WPEFrameworkMessaging"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libocdm.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libocdm.so.1"
  )

list(APPEND _cmake_import_check_targets ocdm::ocdm )
list(APPEND _cmake_import_check_files_for_ocdm::ocdm "${_IMPORT_PREFIX}/lib/libocdm.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
