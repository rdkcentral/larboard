#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkMessaging::WPEFrameworkMessaging" for configuration "Release"
set_property(TARGET WPEFrameworkMessaging::WPEFrameworkMessaging APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkMessaging::WPEFrameworkMessaging PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libWPEFrameworkMessaging.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkMessaging.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkMessaging::WPEFrameworkMessaging )
list(APPEND _cmake_import_check_files_for_WPEFrameworkMessaging::WPEFrameworkMessaging "${_IMPORT_PREFIX}/lib/libWPEFrameworkMessaging.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
