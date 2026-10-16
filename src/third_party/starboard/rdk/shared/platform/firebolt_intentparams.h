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

#include "third_party/starboard/rdk/shared/uri.h"

#include <firebolt/firebolt.h>
#include <optional>
#include <variant>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

struct IntentParams {

  enum class Action : uint32_t {
    Other,
    PreLoad,
    Home,
    Launch,
    Search,
    PlayEntity,
    PlayQuery,
    SkipAd,
    Next,
    Previous,
  };

  enum class Source : uint32_t {
    Other,
    Voice,
    Editorial,
    Dial,
    System,
    AppsRail,
    Remote,
  };

  enum class EntityType : uint32_t {
    Other,
    Playlist
  };

  struct Context {
    Source source;
    std::optional<bool> inApp;
  };

  struct QueryData {
    std::string query;
  };

  struct PlayEntityData {
    struct {
      std::string entityId;
      EntityType entityType;
    } entity;
  };

  struct DialData {
    std::string dialPayload;
    std::string additionalDataUrl;
  };

  using Data = std::variant<std::monostate,QueryData,PlayEntityData,DialData>;

  Action action;
  Context context;
  std::optional<Data> data;

  bool isValid() const;
  UriQuery toUriQuery(bool is_focused) const;

  static Firebolt::Result<IntentParams> fromString(std::string_view json);

private:
  IntentParams() = default;
};

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
