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

#include "third_party/starboard/rdk/shared/uri.h"

#include "starboard/common/log.h"

#include "third_party/starboard/rdk/shared/log_override.h"

#include <algorithm>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

UriQuery::UriQuery(const std::string& query_string) {
  setFromRawQuery(query_string);
}

void UriQuery::clear() {
  query_params_.clear();
}

void UriQuery::setFromRawQuery(const std::string& query_string) {
  query_params_.clear();
  if (query_string.empty())
    return;

  GError* error = nullptr;
  GUriParamsIter iter;
  gchar *key, *value;
  g_uri_params_iter_init(&iter, query_string.c_str(), -1, "&", G_URI_PARAMS_NONE);
  while (g_uri_params_iter_next(&iter, &key, &value, &error)) {
    if (key) {
      query_params_.emplace_back(
        std::pair{std::string(key), value ? std::string{value}: std::string{}});
    }
    g_free(key);
    g_free(value);
  }
  if (error) {
    SB_LOG(WARNING) << "UriQuery: failed to parse query params '" << query_string << "': " << error->message;
    g_error_free(error);
  }
}

std::string UriQuery::itemValue(const std::string& key) const {
  auto it = std::find_if(query_params_.begin(), query_params_.end(),
                         [key](const auto& p) { return p.first == key; });
  if (it != query_params_.end())
    return it->second;
  return {};
}

void UriQuery::add(const std::string& key, const std::string& value) {
  query_params_.emplace_back(std::pair{key, value});
}

bool UriQuery::contains(const std::string& key) const {
  auto it = std::find_if(query_params_.begin(), query_params_.end(),
                         [key](const auto& p) { return p.first == key; });
  return (it != query_params_.end());
}

std::string UriQuery::toString() const {
  // Rebuild the query string, percent-encoding each token.
  std::string query;
  for (const auto& [key, value] : query_params_) {
    if (!query.empty())
      query += '&';

    gchar* encodedKey = g_uri_escape_string(key.c_str(), nullptr, FALSE);
    query += encodedKey;
    g_free(encodedKey);

    gchar* encodedValue = g_uri_escape_string(value.c_str(), nullptr, FALSE);
    query += '=';
    query += encodedValue;
    g_free(encodedValue);
  }
  return query;
}

Uri::Uri(const std::string& uri_string) {
  setFromString(uri_string);
}

Uri::~Uri() {
  g_free(scheme_);
  g_free(userinfo_);
  g_free(host_);
  g_free(path_);
  g_free(fragment_);
}

void Uri::setScheme(const std::string& scheme) {
  g_clear_pointer(&scheme_, g_free);
  if (!scheme.empty())
    scheme_ = g_strdup(scheme.c_str());
}

void Uri::setHost(const std::string& host) {
  g_clear_pointer(&host_, g_free);
  if (!host.empty())
    host_ = g_strdup(host.c_str());
}

void Uri::setPort(gint port) {
  port_ = port;
}

void Uri::setPath(const std::string& path) {
  g_clear_pointer(&path_, g_free);
  if (!path.empty())
    path_ = g_strdup(path.c_str());
}

void Uri::setFromString(const std::string& uri_string) {
  g_clear_pointer(&scheme_, g_free);
  g_clear_pointer(&userinfo_, g_free);
  g_clear_pointer(&host_, g_free);
  g_clear_pointer(&path_, g_free);
  g_clear_pointer(&fragment_, g_free);
  port_ = -1;
  query_.clear();

  GError* error = nullptr;
  gchar *scheme = nullptr, *userinfo = nullptr, *host = nullptr;
  gchar *path = nullptr, *query = nullptr, *fragment = nullptr;
  gint port = -1;

  gboolean ok = g_uri_split(uri_string.c_str(), flags_, &scheme, &userinfo,
                            &host, &port, &path, &query, &fragment, &error);
  if (!ok) {
    SB_LOG(WARNING) << "Uri: failed to parse '" << uri_string << "': " << error->message;
    g_error_free(error);
    g_free(scheme);
    g_free(userinfo);
    g_free(host);
    g_free(path);
    g_free(query);
    g_free(fragment);
    return;
  }

  scheme_ = scheme;
  userinfo_ = userinfo;
  host_ = host;
  port_ = port;
  path_ = path;
  fragment_ = fragment;

  if (query && *query)
    query_.setFromRawQuery(query);
  g_free(query);
}

std::string Uri::toString() const {
  if (!scheme_)
    return {};

  std::string query = query_.toString();
  std::string result;
  gchar* str = g_uri_join(
    flags_,
    scheme_,
    userinfo_,
    host_,
    port_,
    path_ ? path_ : "",
    query.empty() ? nullptr : query.c_str(),
    fragment_);

  if (str) {
    result.assign(str);
    g_free(str);
  }

  return result;
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
