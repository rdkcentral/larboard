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

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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

std::ostream& operator<<(std::ostream& out, const Firebolt::ErrorInfo& info) {
  return out << info.message();
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

std::ostream& operator<<(std::ostream& out, const IntentParams::Action& action) {
  const auto to_string = [](const IntentParams::Action& action) {
    switch(action) {
#define CASE(x) case IntentParams::Action::x: return #x
      CASE(PreLoad);
      CASE(Home);
      CASE(Launch);
      CASE(Search);
      CASE(PlayEntity);
      CASE(PlayQuery);
      CASE(SkipAd);
      CASE(Next);
      CASE(Previous);
      CASE(Other);
#undef CASE
    }
  };

  return out << to_string(action) << '(' << static_cast<uint32_t>(action) << ')';
}

std::ostream& operator<<(std::ostream& out, const IntentParams::Source& source) {
  const auto to_string = [](const IntentParams::Source& source) {
    switch(source) {
#define CASE(x) case IntentParams::Source::x: return #x
      CASE(Voice);
      CASE(Editorial);
      CASE(Dial);
      CASE(System);
      CASE(AppsRail);
      CASE(Remote);
      CASE(Other);
#undef CASE
    }
  };

  return out << to_string(source) << '(' << static_cast<uint32_t>(source) << ')';
}

std::ostream& operator<<(std::ostream& out, const IntentParams::EntityType& entity_type) {
  const auto to_string = [](const IntentParams::EntityType& entity_type) {
    switch(entity_type) {
#define CASE(x) case IntentParams::EntityType::x: return #x
      CASE(Playlist);
      CASE(Other);
#undef CASE
    }
  };

  return out << to_string(entity_type) << '(' << static_cast<uint32_t>(entity_type) << ')';
}

std::ostream& operator<<(std::ostream& out, const IntentParams::Data& data) {
  std::visit(overloaded{
      [&out](const std::monostate&) {
        out << "{}";
      },
      [&out](const IntentParams::DialData& data_dial) {
        out << "{\"dialPayload\":\"" << data_dial.dialPayload << "\","
            << " \"additionalDataUrl\":\"" << data_dial.additionalDataUrl << "\"}";
      },
      [&out](const IntentParams::PlayEntityData& data_playentity) {
        out << "{\"entity\":{\"entityId\":\"" << data_playentity.entity.entityId << "\","
            << "\"entityType\":\"" << data_playentity.entity.entityType << "\"}}";
      },
      [&out](const IntentParams::QueryData& data_query) {
        out << "{\"query\":\"" << data_query.query << "\"}";
      }
    }, data);
  return out;
}

std::ostream& operator<<(std::ostream& out, const IntentParams& intent) {
  out << "action = " << intent.action
      << ", source = " << intent.context.source;
  if (intent.context.inApp.has_value())
    out << ", inApp = " << intent.context.inApp.value();
  if (intent.data.has_value())
    out << ", data = " << intent.data.value();
  return out;
}

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
