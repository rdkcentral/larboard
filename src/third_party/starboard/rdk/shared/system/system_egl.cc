//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "starboard/egl.h"

#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/log_override.h"

#include <cstring>
#include <mutex>
#include <vector>
#include <essos-app.h>

#ifdef EGL_PLATFORM_WAYLAND_EXT
extern "C" EssAppPlatformDisplayType EssContextGetAppPlatformDisplayType( EssCtx *ctx ) __attribute__((weak));
#endif

#if !defined(EGL_VERSION_1_0) || !defined(EGL_VERSION_1_1) || \
    !defined(EGL_VERSION_1_2) || !defined(EGL_VERSION_1_3) || \
    !defined(EGL_VERSION_1_4)
#error "EGL version must be >= 1.4"
#endif

namespace {

// Global state for EGL capabilities
// Assume supported until proven otherwise to avoid unnecessary overhead on platforms with support.
static bool gPbufferSupported = true;
static bool gSurfacelessContextSupported = false;
static bool gCapabilitiesChecked = false;

#ifdef EGL_PLATFORM_WAYLAND_EXT
static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC gEglCreatePlatformWindowSurfaceEXT;
static PFNEGLGETPLATFORMDISPLAYEXTPROC gEglGetPlatformDisplayEXT;
#endif

bool isExtensionSupported(const char* extension_list, const char* extension) {
  if (!extension_list || !extension) {
    return false;
  }
  int len = strlen(extension);
  const char* ptr = extension_list;
  while ((ptr = strstr(ptr, extension))) {
    if (ptr[len] == ' ' || ptr[len] == '\0')
      return true;
    ptr += len;
  }
  return false;
}

void checkEglCapabilities(EGLDisplay display) {
  if (gCapabilitiesChecked) {
    return;
  }
  gCapabilitiesChecked = true;

  // Check for surfaceless context support
  const char* egl_extensions = eglQueryString(display, EGL_EXTENSIONS);
  if (egl_extensions) {
    gSurfacelessContextSupported =
        isExtensionSupported(egl_extensions, "EGL_KHR_surfaceless_context") ||
        isExtensionSupported(egl_extensions, "EGL_KHR_no_config_context");

    if (gSurfacelessContextSupported) {
      SB_LOG(INFO) << "Surfaceless context support detected, pbuffers optional";
    }
  }

  // Test if pbuffers are actually supported by attempting to query configs
  EGLint pbuffer_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE
  };
  EGLint num_configs = 0;
  if (eglChooseConfig(display, pbuffer_attribs, nullptr, 0, &num_configs)) {
    if (num_configs == 0) {
      gPbufferSupported = false;
      SB_LOG(WARNING) << "No EGL configs with EGL_PBUFFER_BIT support found";
    }
  }
}

#ifdef EGL_PLATFORM_WAYLAND_EXT
bool resolveEglPlatfromExtFns() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!isExtensionSupported(extensions, "EGL_EXT_platform_wayland")) {
      SB_LOG(INFO) << "Wayland EGL platform extension is not supported. Supported extensions: " << extensions;
    }
    else {
      gEglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
      gEglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
      if (!gEglGetPlatformDisplayEXT || !gEglCreatePlatformWindowSurfaceEXT) {
        if (!gEglGetPlatformDisplayEXT)
          SB_LOG(INFO) << "eglGetPlatformDisplayEXT is not available";
        if (!gEglCreatePlatformWindowSurfaceEXT)
          SB_LOG(INFO) << "eglCreatePlatformWindowSurfaceEXT is not available";
        gEglGetPlatformDisplayEXT = nullptr;
        gEglCreatePlatformWindowSurfaceEXT = nullptr;
      } else {
        SB_LOG(INFO) << "Successfully resolved EGL platform display extension functions.";
      }
    }
  });
  return gEglGetPlatformDisplayEXT && gEglCreatePlatformWindowSurfaceEXT;
}
#endif

// Wrapper for eglChooseConfig that filters pbuffer requirements if unsupported
// For platforms with pbuffer support, this is just a direct pass-through with zero overhead
SbEglBoolean SbEglChooseConfigWrapper(SbEglDisplay dpy,
                                      const SbEglInt32* attrib_list,
                                      SbEglConfig* configs,
                                      SbEglInt32 config_size,
                                      SbEglInt32* num_config) {
  // Fast path: Direct pass-through for platforms with pbuffer or surfaceless support
  if (gPbufferSupported || gSurfacelessContextSupported) {
    return eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);
  }

  // Platform doesn't support pbuffers - filter the config attributes
  SB_LOG(WARNING) << "Filtering EGL_PBUFFER_BIT from config request (pbuffers unsupported)";

  // Count attributes and copy to new list, filtering EGL_SURFACE_TYPE
  std::vector<SbEglInt32> filtered_attribs;
  if (attrib_list) {
    for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
      if (attrib_list[i] == EGL_SURFACE_TYPE) {
        // Remove EGL_PBUFFER_BIT from surface type
        SbEglInt32 surface_type = attrib_list[i + 1];
        surface_type &= ~EGL_PBUFFER_BIT;
        if (surface_type != 0) {
          filtered_attribs.push_back(EGL_SURFACE_TYPE);
          filtered_attribs.push_back(surface_type);
        }
      } else {
        filtered_attribs.push_back(attrib_list[i]);
        filtered_attribs.push_back(attrib_list[i + 1]);
      }
    }
  }
  filtered_attribs.push_back(EGL_NONE);

  return eglChooseConfig(dpy, filtered_attribs.data(), configs, config_size, num_config);
}

// Wrapper for eglCreatePbufferSurface that provides graceful fallback when unsupported
// For platforms with pbuffer support, this is a direct pass-through with zero overhead
SbEglSurface SbEglCreatePbufferSurfaceStub(SbEglDisplay dpy,
                                           SbEglConfig config,
                                           const SbEglInt32* attrib_list) {
  // Fast path: Direct pass-through for platforms with pbuffer support
  if (gPbufferSupported) {
    return eglCreatePbufferSurface(dpy, config, attrib_list);
  }

  SB_LOG(ERROR) << "eglCreatePbufferSurface called but pbuffers not supported on this platform";
  SB_LOG(ERROR) << "Consider using EGL_KHR_surfaceless_context or window-based surfaces";
  return EGL_NO_SURFACE;
}

// Convenience functions that redirect to the intended function but "cast" the
// type of the SbEglNative*Type parameter into the desired type. Depending on
// the platform, the type of cast to use is different so either C-style casts or
// constructor-style casts are needed to work across platforms (or provide
// implementations for these functions for each platform).

SbEglBoolean SbEglCopyBuffers(SbEglDisplay dpy,
                              SbEglSurface surface,
                              SbEglNativePixmapType target) {
  return eglCopyBuffers(dpy, surface, (EGLNativePixmapType)target);
}

SbEglSurface SbEglCreatePixmapSurface(SbEglDisplay dpy,
                                      SbEglConfig config,
                                      SbEglNativePixmapType pixmap,
                                      const SbEglInt32* attrib_list) {
  return eglCreatePixmapSurface(dpy, config, (EGLNativePixmapType)pixmap,
                                attrib_list);
}

SbEglSurface SbEglCreateWindowSurface(SbEglDisplay dpy,
                                      SbEglConfig config,
                                      SbEglNativeWindowType win,
                                      const SbEglInt32* attrib_list) {
  SbEglSurface result = EGL_NO_SURFACE;

#ifdef EGL_PLATFORM_WAYLAND_EXT
  if (gEglCreatePlatformWindowSurfaceEXT) {
    result = gEglCreatePlatformWindowSurfaceEXT(dpy, config, (void*)win,
                                                attrib_list);
    if (result == EGL_NO_SURFACE)
      SB_LOG(WARNING) << "eglCreatePlatformWindowSurfaceEXT failed, err: " << eglGetError();
  }
#endif

  if (result == EGL_NO_SURFACE) {
    result = eglCreateWindowSurface(dpy, config, (EGLNativeWindowType)win,
                                    attrib_list);
    if (result == EGL_NO_SURFACE)
      SB_LOG(ERROR) << "eglCreateWindowSurface failed, err: " << eglGetError();
  }

  return result;
}

SbEglDisplay SbEglGetDisplay(SbEglNativeDisplayType display_id) {
  NativeDisplayType display_type;
  EssCtx *ctx = third_party::starboard::rdk::shared::Application::Get()->GetEssCtx();

  if (EssContextGetEGLDisplayType(ctx, &display_type) == false) {
    SB_LOG(ERROR) << "EssContextGetEGLDisplayType failed! Going to try display_id=" << display_id << '.';
    display_type = reinterpret_cast<NativeDisplayType>(display_id);
  }

  // Get the display first
  EGLDisplay display = EGL_NO_DISPLAY;

#ifdef EGL_PLATFORM_WAYLAND_EXT
  if (EssContextGetAppPlatformDisplayType == nullptr) {
    SB_LOG(INFO) << "'EssContextGetAppPlatformDisplayType' is not available. Fallback to eglGetDisplay.";
  }
  else if (EssContextGetAppPlatformDisplayType(ctx) != EssAppPlatformDisplayType_waylandExtension) {
    SB_LOG(INFO) << "Essos app platform display type is not 'WaylandExtension' ("
                 << EssContextGetAppPlatformDisplayType(ctx)
                 << " != "
                 << EssAppPlatformDisplayType_waylandExtension << ")."
                 << " Fallback to eglGetDisplay.";
  }
  else if (!resolveEglPlatfromExtFns()) {
    SB_LOG(INFO) << "eglGetPlatformDisplayEXT is not available or failed. Fallback to eglGetDisplay.";
  }
  else {
    SbEglDisplay result = gEglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, reinterpret_cast<EGLNativeDisplayType>(display_type), nullptr);
    if (result == EGL_NO_DISPLAY) {
      SB_LOG(ERROR) << "eglGetPlatformDisplayEXT returned EGL_NO_DISPLAY. Fallback to eglGetDisplay.";
      gEglGetPlatformDisplayEXT = nullptr;
      gEglCreatePlatformWindowSurfaceEXT = nullptr;
    } else {
      SB_LOG(INFO) << "Using display=" << result << ", returned by eglGetPlatformDisplayEXT.";
      display = result;
    }
  }
#endif

  if (display == EGL_NO_DISPLAY) {
    display = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display_type));
  }

  return display;
}

// Wrapper for eglInitialize that performs capability detection after initialization
SbEglBoolean SbEglInitializeWrapper(SbEglDisplay dpy, SbEglInt32* major, SbEglInt32* minor) {
  SbEglBoolean result = eglInitialize(dpy, major, minor);

  // Check capabilities after successful initialization (requires initialized display)
  if (result && !gCapabilitiesChecked) {
    checkEglCapabilities(dpy);
    SB_LOG(INFO) << "EGL " << (major ? *major : 0) << "." << (minor ? *minor : 0) << " initialized";
  }

  return result;
}

const SbEglInterface g_sb_egl_interface = {
    &SbEglChooseConfigWrapper,
    &SbEglCopyBuffers,
    &eglCreateContext,
    &SbEglCreatePbufferSurfaceStub,
    &SbEglCreatePixmapSurface,
    &SbEglCreateWindowSurface,
    &eglDestroyContext,
    &eglDestroySurface,
    &eglGetConfigAttrib,
    &eglGetConfigs,
    &eglGetCurrentDisplay,
    &eglGetCurrentSurface,
    &SbEglGetDisplay,
    &eglGetError,
    &eglGetProcAddress,
    &SbEglInitializeWrapper,
    &eglMakeCurrent,
    &eglQueryContext,
    &eglQueryString,
    &eglQuerySurface,
    &eglSwapBuffers,
    &eglTerminate,
    &eglWaitGL,
    &eglWaitNative,
    &eglBindTexImage,
    &eglReleaseTexImage,
    &eglSurfaceAttrib,
    &eglSwapInterval,
    &eglBindAPI,
    &eglQueryAPI,
    &eglCreatePbufferFromClientBuffer,
    &eglReleaseThread,
    &eglWaitClient,
    &eglGetCurrentContext,

    nullptr,  // eglCreateSync
    nullptr,  // eglDestroySync
    nullptr,  // eglClientWaitSync
    nullptr,  // eglGetSyncAttrib
    nullptr,  // eglCreateImage
    nullptr,  // eglDestroyImage
    nullptr,  // eglGetPlatformDisplay
    nullptr,  // eglCreatePlatformWindowSurface
    nullptr,  // eglCreatePlatformPixmapSurface
    nullptr,  // eglWaitSync
};

}  // namespace

const SbEglInterface* SbGetEglInterface() {
  return &g_sb_egl_interface;
}
