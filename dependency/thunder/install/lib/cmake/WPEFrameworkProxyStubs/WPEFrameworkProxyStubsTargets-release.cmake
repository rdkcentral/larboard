#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WPEFrameworkProxyStubs::WPEFrameworkProxyStubs" for configuration "Release"
set_property(TARGET WPEFrameworkProxyStubs::WPEFrameworkProxyStubs APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(WPEFrameworkProxyStubs::WPEFrameworkProxyStubs PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "WPEFrameworkCore::WPEFrameworkCore;WPEFrameworkCOM::WPEFrameworkCOM"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/wpeframework/proxystubs/libWPEFrameworkProxyStubs.so.1.0.0"
  IMPORTED_SONAME_RELEASE "libWPEFrameworkProxyStubs.so.1"
  )

list(APPEND _cmake_import_check_targets WPEFrameworkProxyStubs::WPEFrameworkProxyStubs )
list(APPEND _cmake_import_check_files_for_WPEFrameworkProxyStubs::WPEFrameworkProxyStubs "${_IMPORT_PREFIX}/lib/wpeframework/proxystubs/libWPEFrameworkProxyStubs.so.1.0.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
