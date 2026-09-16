#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkDefinitions::WPEFrameworkDefinitions" for configuration "Release"
set_property(TARGET WPEFrameworkDefinitions::WPEFrameworkDefinitions APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkDefinitions::WPEFrameworkDefinitions PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libWPEFrameworkDefinitions.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkDefinitions.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkDefinitions::WPEFrameworkDefinitions )
list(APPEND _cmake_import_check_files_for_WPEFrameworkDefinitions::WPEFrameworkDefinitions "${_IMPORT_PREFIX}/lib/libWPEFrameworkDefinitions.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
