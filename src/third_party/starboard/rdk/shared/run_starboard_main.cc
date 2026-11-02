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
//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <gst/gst.h>

#include "starboard/event.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/media/gst_media_utils.h"
#include <cstdio>

namespace {

void debug_log_override(GstDebugCategory *category, GstDebugLevel level,
                        const gchar *file, const gchar *function, gint line,
                        GObject *object, GstDebugMessage *message,
                        gpointer data) {
  gchar *log_line = gst_debug_log_get_line(category, level, file, function,
                                           line, object, message);
  gint64 ts = g_get_monotonic_time();
  fprintf(stderr, "%.010" G_GINT64_FORMAT ".%.06" G_GINT64_FORMAT " %s",
          reinterpret_cast<gint64>(ts / G_USEC_PER_SEC),
          reinterpret_cast<gint64>(ts % G_USEC_PER_SEC), log_line);
  g_free(log_line);
}

} // namespace

int SbRunStarboardMain(int argc, char **argv, SbEventHandleCallback callback) {
  third_party::starboard::rdk::shared::media::EnsureGstInit();

  if (const char *env = std::getenv("COBALT_OVERRIDE_GST_DEBUG_LOG");
      env && g_str_equal(env, "1")) {
    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(debug_log_override, nullptr, nullptr);
  }

  third_party::starboard::rdk::shared::Application application(callback);
  int result = application.Run(argc, argv);

  gst_deinit();

  return result;
}
