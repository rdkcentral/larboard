#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkSecurityAgent::WPEFrameworkSecurityAgent" for configuration "Release"
set_property(TARGET WPEFrameworkSecurityAgent::WPEFrameworkSecurityAgent APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkSecurityAgent::WPEFrameworkSecurityAgent PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore;WPEFrameworkCOM::WPEFrameworkCOM"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libWPEFrameworkSecurityAgent.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkSecurityAgent.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkSecurityAgent::WPEFrameworkSecurityAgent )
list(APPEND _cmake_import_check_files_for_WPEFrameworkSecurityAgent::WPEFrameworkSecurityAgent "${_IMPORT_PREFIX}/lib/libWPEFrameworkSecurityAgent.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
