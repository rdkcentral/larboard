Cov-build dependency staging folder
===================================

The CMake build (larboard/build_x86_cmake.sh) compiles ONLY the Cobalt /
Starboard RDK source (larboard/src) into a static library. It builds no
Thunder plugin widget and no binary.

Layout
------
  dependency/
    cobalt-src/ -> A full Cobalt source checkout providing the in-tree headers
                   the RDK port includes (starboard/..., third_party/...).
                   Must contain starboard/event.h. Used as COBALT_SRC_ROOT.

    thunder/    -> Thunder / WPEFramework SDK. Either drop a prebuilt SDK here
                   (with lib/pkgconfig) or let the build create it via
                   `build_x86_cmake.sh --fetch-thunder` (src/ = clones of
                   ThunderTools/Thunder/ThunderInterfaces/ThunderClientLibraries,
                   install/ = built prefix providing WPEFrameworkCore,
                   WPEFrameworkDefinitions, WPEFrameworkWebSocket,
                   WPEFrameworkCryptography, ocdm, securityagent). The build
                   auto-adds its lib/*/pkgconfig to PKG_CONFIG_PATH.

    gstreamer/  -> Local GStreamer clone + build, created by
                   `build_x86_cmake.sh --fetch-gstreamer` (src/ = clone,
                   install/ = built prefix). The build auto-adds its
                   lib/*/pkgconfig to PKG_CONFIG_PATH, avoiding the distro
                   libgstreamer*-dev packages.

System libraries (glib, EGL, GLESv2, etc.) are installed via the distro
package manager (run: larboard/build_x86_cmake.sh --install-deps) and do NOT
need to be copied here.
