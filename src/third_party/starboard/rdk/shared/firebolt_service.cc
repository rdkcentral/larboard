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

#ifdef ENABLE_FIREBOLT

#include "firebolt_service.h"

#include <cstdio>              // fprintf
#include <cstdlib>             // std::getenv
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

// Helper logging macros (simple, safe)
#define FB_LOGI(fmt, ...) std::fprintf(stderr, "[Firebolt][INFO] " fmt "\n", ##__VA_ARGS__)
#define FB_LOGW(fmt, ...) std::fprintf(stderr, "[Firebolt][WARN] " fmt "\n", ##__VA_ARGS__)
#define FB_LOGE(fmt, ...) std::fprintf(stderr, "[Firebolt][ERROR] " fmt "\n", ##__VA_ARGS__)

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

constexpr const char* IFA_TYPE_SESSION_ID = "sessionId";


void FireboltService::onFireboltConnectionChanged(const bool connected_, const Firebolt::Error error_)
{
    FB_LOGI(" patel Connection state changed: %s, error=%d",
            connected_ ? "Connected" : "Disconnected",
            static_cast<int>(error_));

    FireboltService* instance = FireboltService::Instance();
    std::unique_lock<std::mutex> lock{instance->m_mutex};
    instance->m_connected = connected_;
    instance->m_cv.notify_one();

    FB_LOGI("patel Connection state changed end: %s",
            connected_ ? "Connected" : "Disconnected");
}

FireboltService* FireboltService::Instance()
{
    static FireboltService instance;
    return &instance;
}

FireboltService::FireboltService()
    : m_initialized(false)
    , m_connected(false)
    , m_isAdvertisingDataFetched(false)
{
}

void FireboltService::initialize()
{
    FB_LOGI("patel FireboltService initialize");

    if (!m_initialized) {
        if (!connectToFirebolt()) {
            FB_LOGE("patel Firebolt connection failed during initialize");
        }
        m_initialized = true;
        FB_LOGI("patel FireboltService initialized");
    }
}

void FireboltService::uninitialize()
{
    FB_LOGI("patel FireboltService uninitialize");

    if (m_initialized) {
        if (m_connected) {
            Firebolt::IFireboltAccessor::Instance().Disconnect();
        }
        m_initialized = false;
        m_connected = false;
        FB_LOGI("patel FireboltService uninitialized");
    }
}

bool FireboltService::connectToFirebolt()
{
    FB_LOGI("Connecting to Firebolt daemon...");

    std::string url = "ws://127.0.0.1:9998";
    const char* fireboltEndpoint = std::getenv("FIREBOLT_ENDPOINT");
    if (fireboltEndpoint && fireboltEndpoint[0] != '\0') {
        url = fireboltEndpoint;
    }

    Firebolt::Config config;
    config.wsUrl = url;
    config.waitTime_ms = 3000;
    config.log.level = Firebolt::LogLevel::Warning;

    auto error = Firebolt::IFireboltAccessor::Instance().Connect(
        config, &FireboltService::onFireboltConnectionChanged);

    if (error != Firebolt::Error::None) {
        FB_LOGE(" patel Cannot connect, error=%d", static_cast<int>(error));
        return false;
    }

    FB_LOGI("patel Connection initiated (async)");
    return true;
}

bool FireboltService::fetchAdvertisingData()
{
    if (m_isAdvertisingDataFetched) {
        FB_LOGI("patel Using cached advertising data");
        return true;
    }

    FB_LOGI("patel Fetching advertising data from Firebolt API...");

    if (!m_initialized) {
        FB_LOGW("patel Service not initialized; initializing now");
        initialize();
    }

    if (!m_connected) {
        FB_LOGI("patel Waiting for connection to establish...");
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cv.wait_for(lock, std::chrono::seconds(5), [this] { return m_connected; })) {
            FB_LOGE("patel Connection timeout");
            return false;
        }
        FB_LOGI("patel Connection established");
    }

    Firebolt::Advertising::IAdvertising& adv =
        Firebolt::IFireboltAccessor::Instance().AdvertisingInterface();

    auto result = adv.advertisingId();
    if (result.error() == Firebolt::Error::None) {
        m_advertisingData = result.value();
        m_isAdvertisingDataFetched = true;

        FB_LOGI("patel Advertising data - UUID=%s, type=%s, LMT=%s",
                m_advertisingData.ifa.c_str(),
                m_advertisingData.ifa_type.c_str(),
                m_advertisingData.lmt.c_str());

        return true;
    }

    FB_LOGE("patel Failed to fetch advertising data, error=%d", static_cast<int>(result.error()));
    return false;
}

void FireboltService::getAdvertisingId(std::string& outValue)
{
    FB_LOGI("patel Getting AdvertisingId...");
    outValue.clear();

    if (!fetchAdvertisingData()) {
        FB_LOGW("Failed to fetch advertising data");
        return;
    }

    if (m_advertisingData.ifa_type == IFA_TYPE_SESSION_ID) {
        FB_LOGI("patel No data agreement with the partner for Advertising ID usage");
        return;
    }

    outValue = m_advertisingData.ifa;
    FB_LOGI("patel AdvertisingId returned: %s", outValue.c_str());
}

void FireboltService::isAdvertisingOptOut(bool& outValue)
{
    FB_LOGI("patel Getting AdvertisingOptOut...");

    // Make sure caller always gets a defined value
    outValue = false;

    if (!fetchAdvertisingData()) {
        FB_LOGW("patel Failed to fetch advertising data");
        return;
    }

    if (m_advertisingData.ifa_type == IFA_TYPE_SESSION_ID) {
        FB_LOGI("patel No data agreement with the partner for Advertising ID usage");
        return;
    }

    outValue = (m_advertisingData.lmt == "1");
    FB_LOGI("patel AdvertisingOptOut returned: LMT=%s, OptOut=%d",
            m_advertisingData.lmt.c_str(), outValue ? 1 : 0);
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  // ENABLE_FIREBOLT
