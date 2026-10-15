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

#include "third_party/starboard/rdk/shared/platform/firebolt_intentparams.h"
#include "third_party/starboard/rdk/shared/platform/firebolt_utility.h"

#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"
#include "third_party/starboard/rdk/shared/log_override.h"

using namespace Firebolt;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

namespace {

const char kActionKey[]  = "action";
const char kDataKey[]    = "data";
const char kContextKey[] = "context";
const char kQueryKey[]   = "query";
const char kSourceKey[]  = "source";
const char kInAppKey[]   = "in-app";
const char kEntityKey[]  = "entity";
const char kEntityTypeKey[] = "entityType";
const char kEntityIdKey[] = "entityId";
const char kAdditionalDataUrlKey[] = "additionalDataUrl";
const char kDialPayloadKey[] = "dialPayload";

const std::map<std::string_view, IntentParams::Action> kActionTypeMap = {
  {"pre-load",   IntentParams::Action::PreLoad},
  {"home",       IntentParams::Action::Home},
  {"launch",     IntentParams::Action::Launch},
  {"search",     IntentParams::Action::Search},
  {"play-query", IntentParams::Action::PlayQuery},
  {"play-entity",IntentParams::Action::PlayEntity},
  {"skip-ad",    IntentParams::Action::SkipAd},
  {"next",       IntentParams::Action::Next},
  {"previous",   IntentParams::Action::Previous},
};

const std::map<std::string_view, IntentParams::Source> kSourceTypeMap = {
  {"voice",     IntentParams::Source::Voice},
  {"editorial", IntentParams::Source::Editorial},
  {"dial",      IntentParams::Source::Dial},
  {"system",    IntentParams::Source::System},
  {"apps_rail", IntentParams::Source::AppsRail},
  {"remote",    IntentParams::Source::Remote},
};

const std::map<std::string_view, IntentParams::EntityType> kEntityTypeMap = {
  {"playlist",  IntentParams::EntityType::Playlist},
};

std::string to_lower(const std::string_view str) {
  std::string result{str.begin(), str.end()};
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

const Json::Value* find_value(const Json::Value& obj, const std::string_view name, Json::ValueType type) {
  if (obj.isObject()) {
    if (const Json::Value* result = obj.find(name.data(), name.data() + name.size());
        result != nullptr && result->type() == type)
      return result;
  }
  return nullptr;
}

IntentParams::Action parse_intent_action(const Json::Value& action_json) {
  SB_DCHECK(action_json.isString());
  std::string action = to_lower(action_json.asString());
  if (auto it = kActionTypeMap.find(action); it != kActionTypeMap.end()) {
    return it->second;
  } else {
    SB_LOG(WARNING) << "unrecognized intent action: \"" << action << '"';
  }
  return IntentParams::Action::Other;
}

IntentParams::Source parse_intent_source(const Json::Value& source_json) {
  SB_DCHECK(source_json.isString());
  std::string source = to_lower(source_json.asString());
  if (auto it = kSourceTypeMap.find(source); it != kSourceTypeMap.end()) {
    return it->second;
  } else {
    SB_LOG(WARNING) << "unrecognized intent source: \"" << source << '"';
  }
  return IntentParams::Source::Other;
}

IntentParams::EntityType parse_entity_type(const Json::Value* entity_json) {
  if (entity_json) {
    SB_DCHECK(entity_json->isString());
    std::string entity_type = to_lower(entity_json->asString());
    if (auto it = kEntityTypeMap.find(entity_type); it != kEntityTypeMap.end()) {
      return it->second;
    } else {
      SB_LOG(WARNING) << "unrecognized entity type: \"" << entity_type << '"';
    }
  }
  return IntentParams::EntityType::Other;
}

IntentParams::Context parse_intent_context(const Json::Value& context_json) {
  SB_DCHECK(context_json.isObject());
  IntentParams::Context context {};
  if (auto* source = find_value(context_json, kSourceKey, Json::stringValue))
    context.source = parse_intent_source(*source);
  if (auto* in_app = find_value(context_json, kInAppKey, Json::booleanValue))
    context.inApp = in_app->asBool();
  return context;
}

IntentParams::Data parse_intent_data(
  const Json::Value& data_json,
  const IntentParams::Action action,
  const IntentParams::Source source) {

  SB_DCHECK(data_json.isObject());

  if (source == IntentParams::Source::Dial) {
    IntentParams::DialData dial_data;
    if (auto *additional_data_url = find_value(data_json, kAdditionalDataUrlKey, Json::stringValue))
      dial_data.additionalDataUrl = additional_data_url->asString();
    if (auto *dial_payload = find_value(data_json, kDialPayloadKey, Json::stringValue))
      dial_data.dialPayload = dial_payload->asString();
    return dial_data;
  }
  else if (action == IntentParams::Action::Search ||
           action == IntentParams::Action::PlayQuery) {
    if (auto* query = find_value(data_json, kQueryKey, Json::stringValue)) {
      return IntentParams::QueryData {
        .query = query->asString()
      };
    }
  }
  else if (action == IntentParams::Action::PlayEntity) {
    if (auto* entity = find_value(data_json, kEntityKey, Json::objectValue)) {
      auto *entity_id = find_value(*entity, kEntityIdKey, Json::stringValue);
      auto *entity_type = find_value(*entity, kEntityTypeKey, Json::stringValue);
      if (entity_id) {
        return IntentParams::PlayEntityData {
          .entity = {
            .entityId = entity_id->asString(),
            .entityType = parse_entity_type(entity_type),
          }
        };
      }
    }
  }

  return std::monostate{};
}

}  // namespace

bool IntentParams::isValid() const {
  if (action == Action::PreLoad)
    return true;

  const auto check_source_action = [this](){
    switch (context.source) {
      case Source::AppsRail:
      case Source::Editorial:
      case Source::Dial:
      case Source::Remote:
        return action == Action::Launch;
      case Source::Voice:
        return (action == Action::Search ||
                action == Action::PlayQuery ||
                action == Action::Launch ||
                action == Action::SkipAd ||
                action == Action::Next ||
                action == Action::Previous);
      case Source::System:
      case Source::Other:
        return (action == Action::Search ||
                action == Action::PlayEntity ||
                action == Action::Launch);
    }
    return true;
  };

  if (!check_source_action()) {
    SB_LOG(WARNING) << "Unsupported intent action=" << action
                    << " for source=" << context.source;
    return false;
  }

  if (context.source == Source::Dial)
    return data.has_value() && std::holds_alternative<IntentParams::DialData>(*data);

  if (action == Action::Search || action == Action::PlayQuery)
    return data.has_value() && std::holds_alternative<IntentParams::QueryData>(*data);

  if (action == Action::PlayEntity)
    return data.has_value() && std::holds_alternative<IntentParams::PlayEntityData>(*data);

  return true;
}

UriQuery IntentParams::toUriQuery(bool is_focused) const {
  UriQuery result;

  if (context.source == Source::Voice) {
    result.add("utm_content", "v");
    result.add("launch", "voice");
    result.add("launch_tag", "voice");
    result.add("vs","0");
    result.add("inApp", context.inApp.value_or(is_focused) ? "true" : "false");

    switch(action) {
      case Action::Search: {
        const auto& data_query = std::get<IntentParams::QueryData>(*data);
        result.add("va", "search");
        result.add("vaa", data_query.query);
        break;
      }

      case Action::PlayQuery: {
        const auto& data_query = std::get<IntentParams::QueryData>(*data);
        result.add("va", "play");
        result.add("vaa", data_query.query);
        break;
      }

      case Action::SkipAd:
        result.add("va", "skipAd");
        break;

      case Action::Next:
        result.add("va", "mediaNextVideo");
        break;

      case Action::Previous:
        result.add("va", "mediaPreviousVideo");
        break;

      case Action::Launch:
      default:
        break;
    }
  }
  else if (context.source == Source::Dial) {
    const auto &dial_data = std::get<IntentParams::DialData>(*data);
    result.setFromRawQuery(dial_data.dialPayload);
    result.add("utm_content", "d");
    result.add("launch", "dial");
    if (!dial_data.additionalDataUrl.empty())
      result.addEscapedValue("additionalDataUrl", dial_data.additionalDataUrl);
  }
  else {
    switch(action) {
      case Action::Search: {
        const auto& data_query = std::get<IntentParams::QueryData>(*data);
        result.add("utm_content", "s");
        result.add("launch", "search");
        result.add("q", data_query.query);
        break;
      }

      case Action::PlayEntity: {
        const auto &playentity_data = std::get<IntentParams::PlayEntityData>(*data);
        result.add("utm_content", "l");
        result.add("launch", "launcher");
        if (playentity_data.entity.entityType == EntityType::Playlist)
          result.add("list", playentity_data.entity.entityId);
        else
          result.add("v", playentity_data.entity.entityId);
        break;
      }

      case Action::Launch: {
        if (context.source == Source::Editorial) {
          result.add("utm_content", "g");
          result.add("launch", "guide");
        }
        else if (context.source == Source::Remote) {
          result.add("utm_content", "r");
          result.add("launch", "remote");
        }
        else {
          result.add("utm_content", "m");
          result.add("launch", "menu");
        }
        break;
      }
      default:
        break;
    }
  }

  return result;
}

// static
Firebolt::Result<IntentParams> IntentParams::fromString(std::string_view json) {
  SB_LOG(INFO) << "Parsing intent string: " << json;

  Json::Reader reader;
  Json::Value intent_obj;

  if (!reader.parse(json.data(), json.data() + json.size(), intent_obj)) {
    auto code = Firebolt::Error::InvalidParams;
    return Firebolt::Result<IntentParams>{
      code, Firebolt::ErrorInfo {static_cast<int32_t>(code), reader.getFormattedErrorMessages()} };
  }

  if (!intent_obj.isObject()) {
    auto code = Firebolt::Error::InvalidParams;
    return Firebolt::Result<IntentParams>{
      code, Firebolt::ErrorInfo {static_cast<int32_t>(code), "intent is not an object"} };
  }

  auto *action = find_value(intent_obj, kActionKey, Json::stringValue);
  if (!action) {
    auto code = Firebolt::Error::InvalidParams;
    return Firebolt::Result<IntentParams>{
      code, Firebolt::ErrorInfo {static_cast<int32_t>(code), "missing required fields"} };
  }

  IntentParams intent{};
  intent.action = parse_intent_action(*action);
  if (auto *context = find_value(intent_obj, kContextKey, Json::objectValue)) {
    intent.context = parse_intent_context(*context);
  }
  if (auto *data = find_value(intent_obj, kDataKey, Json::objectValue)) {
    intent.data = parse_intent_data(*data, intent.action, intent.context.source);
  }

  SB_LOG(INFO) << "Parsed intent: " << intent;
  SB_DCHECK(intent.isValid());
  if (!intent.isValid()) {
    auto code = Firebolt::Error::InvalidParams;
    return Firebolt::Result<IntentParams>{
      code, Firebolt::ErrorInfo {static_cast<int32_t>(code),
        "unsupported combination of intent action, source and data"} };
  }

  return Firebolt::Result<IntentParams>{ intent };
}

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
