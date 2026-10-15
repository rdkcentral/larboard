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

#include <glib.h>

#include <string>
#include <utility>
#include <vector>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

class UriQuery {
 public:
  UriQuery(const UriQuery&) = default;
  UriQuery& operator=(const UriQuery&) = default;
  UriQuery(UriQuery&&) = default;
  UriQuery& operator=(UriQuery&&) = default;
  UriQuery() = default;

  explicit UriQuery(const std::string& query_string);

  // Clear query params
  void clear();

  // Parse and set params from the query string.
  void setFromRawQuery(const std::string& query_string);

  // Returns query param value by key
  std::string itemValue(const std::string& key) const;

  // Append a query parameter.
  void add(const std::string& key, const std::string& value);
  void addEscapedValue(const std::string& key, const std::string& value);

  // Returns true if URI query contains parameter.
  bool contains(const std::string& key) const;

  // Return the URI query as a string.
  std::string toString() const;

 private:
  std::vector<std::pair<std::string, std::string>> query_params_;
};

class Uri {
  // Non-copyable, non-movable
  Uri(const Uri&) = delete;
  Uri& operator=(const Uri&) = delete;
  Uri(Uri&&) = delete;
  Uri& operator=(Uri&&) = delete;

public:
  Uri() = default;
  explicit Uri(const std::string& uri_string);
  ~Uri();

  void setFromString(const std::string&);
  void setScheme(const std::string& scheme);
  void setHost(const std::string& host);
  void setPort(gint port);
  void setPath(const std::string& path);

  UriQuery& query() { return query_; }
  const UriQuery& query() const { return query_; }

  void setQuery(const UriQuery& q) { query_ = q; }
  void setQuery(UriQuery&& q) { query_ = std::move(q); }

  // Return the URI as a string.
  std::string toString() const;

private:
  UriQuery query_;
  GUriFlags flags_ = G_URI_FLAGS_ENCODED_QUERY;
  gchar* scheme_ = nullptr;
  gchar* userinfo_ = nullptr;
  gchar* host_ = nullptr;
  gint   port_ = -1;
  gchar* path_ = nullptr;
  gchar* fragment_ = nullptr;
};

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
