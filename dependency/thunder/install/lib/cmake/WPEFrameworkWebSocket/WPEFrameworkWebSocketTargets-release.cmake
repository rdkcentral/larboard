#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkWebSocket::WPEFrameworkWebSocket" for configuration "Release"
set_property(TARGET WPEFrameworkWebSocket::WPEFrameworkWebSocket APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkWebSocket::WPEFrameworkWebSocket PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore;WPEFrameworkCryptalgo::WPEFrameworkCryptalgo"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libWPEFrameworkWebSocket.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkWebSocket.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkWebSocket::WPEFrameworkWebSocket )
list(APPEND _cmake_import_check_files_for_WPEFrameworkWebSocket::WPEFrameworkWebSocket "${_IMPORT_PREFIX}/lib/libWPEFrameworkWebSocket.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
