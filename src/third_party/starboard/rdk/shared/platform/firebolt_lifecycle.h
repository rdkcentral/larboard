//
// Copyright 2026 Comcast Cable Communications Management, LLC
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

#pragma once

#include "starboard/configuration.h"
#include <firebolt/firebolt.h>
#include <mutex>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

class FireboltLifecycle final {
public:
  void initialize();
  void teardown();
private:
  void on_state_change(
    Firebolt::Lifecycle::LifecycleState new_state,
    Firebolt::Lifecycle::LifecycleState old_state);

  void on_intent(const Firebolt::Actions::Intent& intent);

  void on_focused_changed(const bool);

  std::mutex mutex_;
  Firebolt::Lifecycle::LifecycleState lifecycle_state_ { Firebolt::Lifecycle::LifecycleState::INITIALIZING };
  bool is_focused_ { false };
  bool has_preload_action_ { false };
  uint32_t last_intent_id_ { -1u };
};

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
