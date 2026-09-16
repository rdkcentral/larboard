#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkCryptography::WPEFrameworkCryptography" for configuration "Release"
set_property(TARGET WPEFrameworkCryptography::WPEFrameworkCryptography APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkCryptography::WPEFrameworkCryptography PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore;WPEFrameworkCOM::WPEFrameworkCOM"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libWPEFrameworkCryptography.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkCryptography.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkCryptography::WPEFrameworkCryptography )
list(APPEND _cmake_import_check_files_for_WPEFrameworkCryptography::WPEFrameworkCryptography "${_IMPORT_PREFIX}/lib/libWPEFrameworkCryptography.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
