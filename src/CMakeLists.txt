# Copyright 2022 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
#
# CMake build for the Cobalt / Starboard RDK platform source ONLY.
# Produces a single static library (libstarboard_platform.a).
# It intentionally builds NO executable/binary and NO Thunder plugin widget.

cmake_minimum_required(VERSION 3.10)

project(cobalt_starboard_rdk LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options (mirror the GN declare_args in rdk/shared/BUILD.gn)
# ---------------------------------------------------------------------------
option(RDK_ENABLE_OCDM            "Enable OCDM DRM support"          ON)
option(RDK_ENABLE_SECURITYAGENT   "Enable Thunder SecurityAgent"     ON)
option(RDK_ENABLE_WPECRYPTOGRAPHY "Enable WPE Cryptography"          ON)
option(RDK_ENABLE_RDKSERVICES_API "Enable RDKServices integration"   ON)
option(RDK_ENABLE_RFC_API         "Enable RFC API"                   OFF)
option(RDK_ENABLE_FIREBOLT        "Enable Firebolt API integration"  OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Cobalt source root.
# The RDK port sources include full-tree headers (starboard/..., third_party/...)
# that are NOT vendored in this repo. Point COBALT_SRC_ROOT at a full Cobalt
# checkout that contains the "starboard/" and "third_party/" directories.
# ---------------------------------------------------------------------------
set(COBALT_SRC_ROOT "" CACHE PATH "Path to a full Cobalt source checkout")

# This repo's src/ dir (contains third_party/starboard/rdk/...).
set(LARBOARD_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(RDK_SHARED_DIR "${LARBOARD_SRC_ROOT}/third_party/starboard/rdk/shared")

if(NOT COBALT_SRC_ROOT OR NOT EXISTS "${COBALT_SRC_ROOT}/starboard/event.h")
  message(FATAL_ERROR
    "COBALT_SRC_ROOT is not set or does not point to a Cobalt checkout.\n"
    "The RDK port needs full-tree headers (e.g. starboard/event.h).\n"
    "Configure with: -DCOBALT_SRC_ROOT=/path/to/cobalt/src")
endif()

# ---------------------------------------------------------------------------
# Local, path-independent dependency wiring.
# The GStreamer (meson) and Thunder (cmake) installs under dependency/ bake an
# ABSOLUTE prefix= into every .pc at build time. If this tree is moved/copied to
# a different path, those prefixes go stale and pkg-config emits -I flags to
# non-existent dirs (fatal: gst/gst.h / core/JSON.h not found). Resolve this
# entirely inside CMake so it works from ANY path and without the wrapper
# script: stage healed copies of every .pc (prefix re-pointed at the real
# on-disk location, derived relative to this source tree) into the build dir and
# put that dir first on PKG_CONFIG_PATH. The source dependency/ tree is left
# untouched.
# ---------------------------------------------------------------------------
get_filename_component(DEP_ROOT "${LARBOARD_SRC_ROOT}/../dependency" ABSOLUTE)
set(STAGED_PC_DIR "${CMAKE_BINARY_DIR}/pkgconfig")
file(MAKE_DIRECTORY "${STAGED_PC_DIR}")

function(_stage_healed_pc install_root)
  if(NOT IS_DIRECTORY "${install_root}")
    return()
  endif()
  file(GLOB_RECURSE _pc_files "${install_root}/*.pc")
  foreach(_pc IN LISTS _pc_files)
    file(STRINGS "${_pc}" _lines)
    set(_out "")
    foreach(_line IN LISTS _lines)
      if(_line MATCHES "^prefix=/")
        set(_line "prefix=${install_root}")
      endif()
      string(APPEND _out "${_line}\n")
    endforeach()
    get_filename_component(_name "${_pc}" NAME)
    file(WRITE "${STAGED_PC_DIR}/${_name}" "${_out}")
  endforeach()
endfunction()

_stage_healed_pc("${DEP_ROOT}/gstreamer/install")
_stage_healed_pc("${DEP_ROOT}/thunder/install")

if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
  set(ENV{PKG_CONFIG_PATH} "${STAGED_PC_DIR}:$ENV{PKG_CONFIG_PATH}")
else()
  set(ENV{PKG_CONFIG_PATH} "${STAGED_PC_DIR}")
endif()

# ---------------------------------------------------------------------------
# System dependencies (pkg-config)
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0 gthread-2.0)
pkg_check_modules(GSTREAMER REQUIRED
  gstreamer-1.0
  gstreamer-app-1.0
  gstreamer-base-1.0
  gstreamer-video-1.0
  gstreamer-audio-1.0
  gstreamer-pbutils-1.0)
pkg_check_modules(WPEFRAMEWORK REQUIRED WPEFrameworkCore)

if(RDK_ENABLE_RDKSERVICES_API)
  pkg_check_modules(WPEFRAMEWORK_WS REQUIRED
    WPEFrameworkDefinitions WPEFrameworkWebSocket)
endif()
if(RDK_ENABLE_OCDM)
  pkg_check_modules(OCDM REQUIRED ocdm)
endif()
if(RDK_ENABLE_SECURITYAGENT)
  pkg_check_modules(SECURITYAGENT REQUIRED securityagent)
endif()
if(RDK_ENABLE_WPECRYPTOGRAPHY)
  pkg_check_modules(WPECRYPTOGRAPHY REQUIRED WPEFrameworkCryptography)
endif()

# ---------------------------------------------------------------------------
# Sources: the local RDK port .cc files, excluding the binary entry point.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE RDK_SOURCES CONFIGURE_DEPENDS "${RDK_SHARED_DIR}/*.cc")

# main_rdk.cc defines int main() -> that is a binary, not part of the library.
list(FILTER RDK_SOURCES EXCLUDE REGEX "/main_rdk\\.cc$")

if(NOT RDK_ENABLE_RDKSERVICES_API)
  list(FILTER RDK_SOURCES EXCLUDE REGEX "/(rdkservices|accessibility_data)\\.cc$")
endif()
if(NOT RDK_ENABLE_OCDM)
  list(FILTER RDK_SOURCES EXCLUDE REGEX "/drm/.*ocdm.*\\.cc$")
endif()
# Firebolt is off by default in the GN build (rdk_enable_firebolt_api) and needs
# the external Firebolt SDK (<firebolt/firebolt.h>), which is not vendored here.
if(NOT RDK_ENABLE_FIREBOLT)
  list(FILTER RDK_SOURCES EXCLUDE REGEX "/platform/firebolt_interface\\.cc$")
endif()

# ---------------------------------------------------------------------------
# The starboard_platform static library (the only build product).
# ---------------------------------------------------------------------------
add_library(starboard_platform STATIC ${RDK_SOURCES})

# Essos (RDK EGL/input abstraction) provides essos-app.h. No pkg-config here,
# so accept an explicit header dir; try pkg-config as a fallback.
set(ESSOS_INCLUDE_DIR "" CACHE PATH "Directory containing essos-app.h")
if(NOT ESSOS_INCLUDE_DIR)
  pkg_check_modules(ESSOS essos)
endif()

# essos-app.h pulls in <EGL/egl.h> and <GLES2/gl2.h>. These normally live on the
# default system include path (libegl1-mesa-dev / libgles2-mesa-dev), but on some
# targets/sysroots they are only discoverable via pkg-config. Detect optionally
# and add their include dirs so the transitive include resolves everywhere.
pkg_check_modules(EGL egl)
pkg_check_modules(GLES glesv2)

# Vendored Khronos EGL/GLES/KHR API headers, used as a fallback when the DEV VM
# has no mesa -dev packages and no egl.pc (self-contained, no sudo/apt needed).
set(KHRONOS_INCLUDE_DIR "${LARBOARD_SRC_ROOT}/../dependency/khronos/include"
    CACHE PATH "Directory containing vendored EGL/GLES/KHR headers")

target_include_directories(starboard_platform PRIVATE
  ${LARBOARD_SRC_ROOT}
  ${COBALT_SRC_ROOT}
  ${ESSOS_INCLUDE_DIR}
  ${ESSOS_INCLUDE_DIRS}
  ${EGL_INCLUDE_DIRS}
  ${GLES_INCLUDE_DIRS}
  ${KHRONOS_INCLUDE_DIR}
  ${GLIB_INCLUDE_DIRS}
  ${GSTREAMER_INCLUDE_DIRS}
  ${WPEFRAMEWORK_INCLUDE_DIRS}
  ${WPEFRAMEWORK_WS_INCLUDE_DIRS}
  ${OCDM_INCLUDE_DIRS}
  ${SECURITYAGENT_INCLUDE_DIRS}
  ${WPECRYPTOGRAPHY_INCLUDE_DIRS})

target_compile_definitions(starboard_platform PRIVATE
  STARBOARD
  STARBOARD_IMPLEMENTATION
  __STDC_FORMAT_MACROS
  _FILE_OFFSET_BITS=64
  # --- Cobalt / Starboard platform configuration (x86_64 host / Coverity build)
  # BUILDFLAG(IS_COBALT) is gated on this in build/build_config.h.
  ENABLE_BUILDFLAG_IS_COBALT
  # dependency/cobalt (25.lts) allows SB_API_VERSION in [14, 16]; the RDK
  # platform targets 16 (see rdk/shared/platform_configuration/configuration.gni).
  SB_API_VERSION=16
  # Platform-specific headers pulled in by starboard/configuration.h and atomic.h.
  STARBOARD_CONFIGURATION_INCLUDE="starboard/linux/x64x11/configuration_public.h"
  STARBOARD_ATOMIC_INCLUDE="starboard/linux/x64x11/atomic_public.h"
  # RDK platform identity macros (normally set by the rdk/<arch>/configuration_public.h).
  SB_PLATFORM_OPERATOR_NAME="Unknown"
  SB_PLATFORM_MODEL_NAME="RDKReference"
  # Thunder version of the built WPEFramework (R4.4.1); selects the >=4.4
  # cryptography interface namespace (WPEFramework::Exchange).
  THUNDER_VERSION_MAJOR=4
  THUNDER_VERSION_MINOR=4
  # Starboard ABI (SABI) values for x64 / little-endian / 64-bit.
  # Source: starboard/sabi/x64/sysv/sabi-v16.json + starboard/build/config/sabi/BUILD.gn
  SB_SABI_TARGET_ARCH="x64"
  SB_SABI_WORD_SIZE="64"
  SB_IS_ARCH_X64=1
  SB_IS_64_BIT=1
  SB_IS_BIG_ENDIAN=0
  SB_IS_LITTLE_ENDIAN=1
  SB_HAS_SYSV_CALLING=1
  SB_HAS_SIGNED_CHAR=1
  SB_HAS_SIGNED_ENUM=1
  SB_ALIGNMENT_OF_CHAR=1
  SB_ALIGNMENT_OF_DOUBLE=8
  SB_ALIGNMENT_OF_FLOAT=4
  SB_ALIGNMENT_OF_INT=4
  SB_ALIGNMENT_OF_LLONG=8
  SB_ALIGNMENT_OF_LONG=8
  SB_ALIGNMENT_OF_POINTER=8
  SB_ALIGNMENT_OF_SHORT=2
  SB_SIZE_OF_CHAR=1
  SB_SIZE_OF_DOUBLE=8
  SB_SIZE_OF_ENUM=4
  SB_SIZE_OF_FLOAT=4
  SB_SIZE_OF_INT=4
  SB_SIZE_OF_LONG=8
  SB_SIZE_OF_LLONG=8
  SB_SIZE_OF_POINTER=8
  SB_SIZE_OF_SHORT=2)

if(RDK_ENABLE_RDKSERVICES_API)
  target_compile_definitions(starboard_platform PRIVATE ENABLE_RDKSERVICES_API=1)
endif()
if(RDK_ENABLE_OCDM)
  target_compile_definitions(starboard_platform PRIVATE HAS_OCDM=1)
endif()
if(RDK_ENABLE_SECURITYAGENT)
  target_compile_definitions(starboard_platform PRIVATE HAS_SECURITY_AGENT=1)
endif()
if(RDK_ENABLE_WPECRYPTOGRAPHY)
  target_compile_definitions(starboard_platform PRIVATE HAS_WPECRYPTOGRAPHY=1)
endif()
if(RDK_ENABLE_RFC_API)
  target_compile_definitions(starboard_platform PRIVATE HAS_RFC_API=1)
endif()
if(RDK_ENABLE_FIREBOLT)
  target_compile_definitions(starboard_platform PRIVATE ENABLE_FIREBOLT_API=1)
endif()

target_compile_options(starboard_platform PRIVATE
  -fsigned-char
  -fno-strict-aliasing
  # Some RDK port headers use fixed-width ints without including <stdint.h>
  # directly (they rely on a transitive include present in the GN build).
  -include stdint.h
  -Wno-literal-suffix
  -Wno-reorder
  -Wno-unused-parameter)

# Header-only linkage here: this target compiles source into an archive.
# Final linking against EGL/GLESv2/essos/gstreamer/glib/ocdm/etc. is done by
# whatever consumes libstarboard_platform.a (out of scope: no binary built).

install(TARGETS starboard_platform ARCHIVE DESTINATION lib)
