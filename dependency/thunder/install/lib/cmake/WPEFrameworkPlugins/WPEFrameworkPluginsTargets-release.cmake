#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkPlugins::WPEFrameworkPlugins" for configuration "Release"
set_property(TARGET WPEFrameworkPlugins::WPEFrameworkPlugins APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkPlugins::WPEFrameworkPlugins PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libWPEFrameworkPlugins.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkPlugins.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkPlugins::WPEFrameworkPlugins )
list(APPEND _cmake_import_check_files_for_WPEFrameworkPlugins::WPEFrameworkPlugins "${_IMPORT_PREFIX}/lib/libWPEFrameworkPlugins.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
