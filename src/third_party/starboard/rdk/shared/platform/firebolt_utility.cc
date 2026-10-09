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

#include "third_party/starboard/rdk/shared/platform/firebolt_utility.h"
#include "third_party/starboard/rdk/shared/log_override.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

std::ostream& operator<<(std::ostream& out, const Firebolt::Error& e) {
  const auto to_string = [](const Firebolt::Error& e) {
    switch(e) {
#define CASE(x) case Firebolt::Error::x: return #x
      CASE(None);
      CASE(General);
      CASE(Timedout);
      CASE(NotConnected);
      CASE(AlreadyConnected);
      CASE(InvalidRequest);
      CASE(MethodNotFound);
      CASE(InvalidParams);
      CASE(CapabilityNotAvailable);
      CASE(CapabilityNotSupported);
      CASE(CapabilityGet);
      CASE(CapabilityNotPermitted);
#undef CASE
      default:
        return "Unknown";
    }
  };

  return out << to_string(e) << '(' << static_cast<int32_t>(e) << ')';
}

std::ostream& operator<<(std::ostream& out, const Firebolt::Lifecycle::LifecycleState& state) {
  const auto to_string = [](const Firebolt::Lifecycle::LifecycleState& state) {
    switch(state) {
#define CASE(x) case Firebolt::Lifecycle::LifecycleState::x: return #x
      CASE(INITIALIZING);
      CASE(ACTIVE);
      CASE(PAUSED);
      CASE(SUSPENDED);
      CASE(HIBERNATED);
      CASE(TERMINATING);
#undef CASE
      default:
        SB_NOTREACHED();
        return "Unknown";
    }
  };

  return out << to_string(state) << '(' << static_cast<int32_t>(state) << ')';
}

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
