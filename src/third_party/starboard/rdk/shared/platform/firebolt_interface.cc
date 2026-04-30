//
// Copyright 2025 Comcast Cable Communications Management, LLC
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

#include "third_party/starboard/rdk/shared/platform/firebolt_interface.h"

#include "starboard/event.h"
#include "starboard/extension/accessibility.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/log_override.h"

#include <firebolt/firebolt.h>

#include <chrono>
#include <cstring>

using namespace std::chrono_literals;
using starboard::shared::starboard::media::MimeSupportabilityCache;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

namespace {

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
        return "UNKNOWN";
    }
  };

  return out << to_string(state) << '(' << static_cast<int32_t>(state) << ')';
}

} // namespace

class FireboltInterface::FireboltLifecycle {
    std::mutex mutex_;
    using LifecycleState = Firebolt::Lifecycle::LifecycleState;
    using StateChange = Firebolt::Lifecycle::StateChange;
    LifecycleState current_state_ = LifecycleState::INITIALIZING;
    bool is_focused_{};

    class RequestMonitor {
        std::string request_;

      public:
        RequestMonitor(std::string request) : request_(std::move(request)) {
          SB_LOG(INFO) << "Requested: " << request_;
        }

        static void handle(void *ctx) {
          auto thiz = static_cast<RequestMonitor*>(ctx);
          SB_LOG(INFO) << "Completed: " << thiz->request_;
          delete thiz;
        }
    };

    static void lifecycleStateChanged(
      const std::vector<StateChange>& changes, bool focused) {

      for (auto& change : changes) {
        SB_LOG(INFO) << "state change from: " << change.oldState << " to: " << change.newState;

        if (change.oldState == LifecycleState::INITIALIZING) {
          switch (change.newState) {
            case LifecycleState::ACTIVE:
              if (focused) {
                Application::Get()->Focus(new RequestMonitor("Focus()"), &RequestMonitor::handle);
              } else {
                Application::Get()->Focus(new RequestMonitor("Focus()"), &RequestMonitor::handle);
                Application::Get()->Blur(new RequestMonitor("Blur()"), &RequestMonitor::handle);
              }
              break;
            case LifecycleState::PAUSED:
                Application::Get()->Focus(new RequestMonitor("Focus()"), &RequestMonitor::handle);
                Application::Get()->Blur(new RequestMonitor("Blur()"), &RequestMonitor::handle);
                Application::Get()->Conceal(new RequestMonitor("Conceal()"), &RequestMonitor::handle);
              break;
            case LifecycleState::SUSPENDED:
            case LifecycleState::HIBERNATED:
                Application::Get()->Focus(new RequestMonitor("Focus()"), &RequestMonitor::handle);
                Application::Get()->Blur(new RequestMonitor("Blur()"), &RequestMonitor::handle);
                Application::Get()->Conceal(new RequestMonitor("Conceal()"), &RequestMonitor::handle);
                Application::Get()->Freeze(new RequestMonitor("Freeze()"), &RequestMonitor::handle);
              break;
            case LifecycleState::TERMINATING:
              SB_LOG(INFO) << "Triggered: Stop()";
              Application::Get()->Stop(0);
              break;
            case LifecycleState::INITIALIZING:
              break;
          }
        } else {
          switch (change.newState) {
            case LifecycleState::ACTIVE:
              if (change.oldState == LifecycleState::PAUSED) {
                Application::Get()->Reveal(new RequestMonitor("Reveal()"), &RequestMonitor::handle);
              }

              if (focused) {
                Application::Get()->Focus(new RequestMonitor("Focus()"), &RequestMonitor::handle);
              }
              break;
            case LifecycleState::PAUSED:
              if (change.oldState == LifecycleState::SUSPENDED) {
                Application::Get()->Unfreeze(new RequestMonitor("Unfreeze()"), &RequestMonitor::handle);
              } else {
                if (focused) {
                  Application::Get()->Blur(new RequestMonitor("Blur()"), &RequestMonitor::handle);
                }
                Application::Get()->Conceal(new RequestMonitor("Conceal()"), &RequestMonitor::handle);
              }
              break;
            case LifecycleState::SUSPENDED:
              if (change.oldState == LifecycleState::PAUSED) {
                Application::Get()->Freeze(new RequestMonitor("Freeze()"), &RequestMonitor::handle);
              }
              break;
            case LifecycleState::TERMINATING:
              SB_LOG(INFO) << "Triggered: Stop()";
              Application::Get()->Stop(0);
              break;
            case LifecycleState::HIBERNATED:
            case LifecycleState::INITIALIZING:
              break;
          }
        }
      }
    }

    static void focusedChanged(bool focused, LifecycleState state) {
      if (state == LifecycleState::ACTIVE) {
        if (focused) {
          Application::Get()->Focus(new RequestMonitor("Focus()"), &RequestMonitor::handle);
        } else {
          Application::Get()->Blur(new RequestMonitor("Blur()"), &RequestMonitor::handle);
        }
      }
    }

  public:
    void init() {
      using namespace Firebolt;

      auto &presentation = IFireboltAccessor::Instance().PresentationInterface();

      Result<SubscriptionId> result = presentation.subscribeOnFocusedChanged([this](const bool focused) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_focused_ != focused) {
          is_focused_ = focused;
          SB_LOG(INFO) << "lifecycle focus change, focused = " << (is_focused_ ? "true" : "false");
          focusedChanged(focused, current_state_);
        }
      });

      if (!result) {
        SB_LOG(ERROR) << "presentation.subscribeOnFocusedChanged failed, error code = " << result.error();
      }

      auto is_focused = presentation.focused();

      if (!is_focused) {
        SB_LOG(ERROR) << "presentation.focused() failed, error code = " << is_focused.error();
      } else {
        SB_LOG(INFO) << "presentation.focused(): " << (*is_focused ? "true" : "false");
        std::lock_guard<std::mutex> lock(mutex_);
        is_focused_ = *is_focused;
      }

      auto &lifecycle = IFireboltAccessor::Instance().LifecycleInterface();

      result = lifecycle.subscribeOnStateChanged([this](const std::vector<Lifecycle::StateChange>& changes) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (changes.empty() == false) {
          lifecycleStateChanged(changes, is_focused_);
          current_state_ = changes.back().newState;
        }
      });

      if (!result) {
        SB_LOG(ERROR) << "lifecycle.subscribeOnStateChanged failed, error code = " << result.error();
      }

      auto current_state = lifecycle.state();

      if (!current_state) {
        SB_LOG(ERROR) << "lifecycle.state() failed, error code = " << current_state.error();
      } else {
        SB_LOG(INFO) << "lifecycle.state(): " << *current_state;
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_state_ != *current_state) {
          StateChange state_change { };
          state_change.oldState = current_state_;
          state_change.newState = *current_state;
          SB_LOG(INFO) << "handling initial state";
          lifecycleStateChanged({state_change}, is_focused_);
          current_state_ = *current_state;
        }
      }
    }
};

// FireboltDevice
std::optional<Resolution> FireboltInterface::FireboltDevice::video_resolution() {
  return { };
}

std::optional<float> FireboltInterface::FireboltDevice::diagonal_size_in_inches() {
  return { };
}

std::optional<HDRFormat> FireboltInterface::FireboltDevice::hdr() {
  std::unique_lock<std::mutex> lock { mutex_ };

  // lazy init
  if (!hdr_format_.has_value() && !on_hdr_format_changed_id_.has_value()) {
    static const auto convert_hdr_format = [](const Firebolt::Device::HDRFormat& format) {
      platform::HDRFormat hdr_format;
      hdr_format.hdr10 = format.hdr10;
      hdr_format.hdr10Plus = format.hdr10Plus;
      hdr_format.dolbyVision = format.dolbyVision;
      hdr_format.hlg = format.hlg;
      return hdr_format;
    };

    auto &device = Firebolt::IFireboltAccessor::Instance().DeviceInterface();

    // set default value
    hdr_format_.emplace(HDRFormat{ });

    // subscribe to notifications first
    lock.unlock();
    auto subscribe_result = device.subscribeOnHdrChanged([this](const Firebolt::Device::HDRFormat& format) {
      set_hdr_format(convert_hdr_format(format));
    });
    lock.lock();
    if (!subscribe_result)  {
      SB_LOG(ERROR) << "device.subscribeOnHdrChanged() failed, error code = " << subscribe_result.error();
    }
    else {
      on_hdr_format_changed_id_ = *subscribe_result;
    }

    // update the format
    lock.unlock();
    auto format = device.hdr();
    lock.lock();
    if (!format) {
      SB_LOG(ERROR) << "device.hdr() failed, error code = " << format.error();
    } else {
      hdr_format_ = convert_hdr_format(*format);
    }
  }

  return hdr_format_;
}

void FireboltInterface::FireboltDevice::set_hdr_format(HDRFormat hdr_format) {
  std::unique_lock<std::mutex> lock { mutex_ };
  hdr_format_ = hdr_format;
  lock.unlock();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

std::optional<bool> FireboltInterface::FireboltDevice::audio_configuration(
  int index,
  SbMediaAudioConfiguration* out_audio_configuration) {
  return {};
}

std::optional<std::string> FireboltInterface::FireboltDevice::brand_name() {
  return {};
}

std::optional<std::string> FireboltInterface::FireboltDevice::chipset() {
  return chipset_id_;
}

std::optional<std::string> FireboltInterface::FireboltDevice::device_type() {
  return device_type_;
}

std::optional<std::string> FireboltInterface::FireboltDevice::firmware_version() {
  return {};
}

std::optional<bool> FireboltInterface::FireboltDevice::is_connection_type_wireless() {
  return {};
}

std::optional<bool> FireboltInterface::FireboltDevice::is_disconnected() {
  return {};
}

void FireboltInterface::FireboltDevice::init() {
  using namespace Firebolt::Device;

  auto &device = Firebolt::IFireboltAccessor::Instance().DeviceInterface();
  auto device_class = device.deviceClass();
  if (!device_class) {
    SB_LOG(ERROR) << "device.deviceClass() failed, error code = " << device_class.error();
  } else {
    switch(*device_class) {
      case DeviceClass::STB:
        device_type_ = "STB";
        break;
      case DeviceClass::TV:
        device_type_ = "TV";
        break;
      case DeviceClass::OTT:
        device_type_ = "OTT";
        break;
      default:
        break;
    }
  }

  auto chipset_id = device.chipsetId();
  if (!chipset_id) {
    SB_LOG(ERROR) << "device.chipsetId() failed, error code = " << chipset_id.error();
  } else {
    chipset_id_ = *chipset_id;
  }
}

void FireboltInterface::FireboltDevice::unsubscribe() {
  std::optional<uint64_t> on_hdr_format_changed_id;
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    on_hdr_format_changed_id = std::exchange(on_hdr_format_changed_id_, std::nullopt);
    hdr_format_.reset();
  }
  auto &device = Firebolt::IFireboltAccessor::Instance().DeviceInterface();
  if (on_hdr_format_changed_id.has_value()) {
    auto result = device.unsubscribe(*on_hdr_format_changed_id);
    if (!result) {
      SB_LOG(ERROR) << "Failed to unsubscribe from OnHdrChanged, error code = " << result.error();
    }
  }
}

// FireboltTextToSpeech
std::optional<bool> FireboltInterface::FireboltTextToSpeech::cancel() {
  if (!is_available_.value_or(false))
    return { };

  std::unique_lock<std::mutex> lock { mutex_ };
  if (speech_id_.has_value()) {
    Firebolt::TextToSpeech::SpeechId speech_id = *std::exchange(speech_id_, std::nullopt);
    lock.unlock();
    auto result = Firebolt::IFireboltAccessor::Instance().TextToSpeechInterface().cancel(speech_id);
    if (!result) {
      SB_LOG(ERROR) << "tts.cancel() failed, error code = " << result.error();
      return false;
    }
  }
  return true;
}

std::optional<bool> FireboltInterface::FireboltTextToSpeech::speak(const std::string& text) {
  if (!is_available_.value_or(false))
    return { };

  auto &tts = Firebolt::IFireboltAccessor::Instance().TextToSpeechInterface();

  std::unique_lock<std::mutex> lock { mutex_ };
  if (!on_speech_complete_id_.has_value()) {
    using namespace Firebolt::TextToSpeech;
    on_speech_complete_id_ = -1u;
    lock.unlock();
    auto subscribe_result = tts.subscribeOnSpeechComplete([this](const SpeechIdEvent& speech_id_event) {
      std::unique_lock<std::mutex> lock { mutex_ };
      if (speech_id_.has_value() && *speech_id_ == speech_id_event.speechId) {
        speech_id_.reset();
      }
    });
    lock.lock();
    if (!subscribe_result)  {
      SB_LOG(ERROR) << "tts.subscribeOnSpeechComplete() failed, error code = " << subscribe_result.error();
    } else {
      on_speech_complete_id_ = *subscribe_result;
    }
  }

  lock.unlock();
  auto result = tts.speak(text);
  lock.lock();

  speech_id_.reset();
  if (!result) {
    SB_LOG(ERROR) << "tts.speak() failed, error code = " << result.error();
  }
  else if (result->ttsStatus == 0 && result->speechId >= 0) {
    speech_id_ = result->speechId;
  }
  return true;
}

std::optional<bool> FireboltInterface::FireboltTextToSpeech::is_available() {
  return is_available_;
}

std::optional<bool> FireboltInterface::FireboltTextToSpeech::is_enabled() {
  if (!is_available_.has_value())
    return { };

  std::unique_lock<std::mutex> lock { mutex_ };
  if (!is_enabled_.has_value()) {
    using namespace Firebolt::Accessibility;

    lock.unlock();

    bool is_enabled = false;
    auto &accessibility = Firebolt::IFireboltAccessor::Instance().AccessibilityInterface();
    auto subscribe_result = accessibility.subscribeOnVoiceGuidanceSettingsChanged([this](const VoiceGuidanceSettings& settings) {
      SB_LOG(INFO) << "accessibility.voiceGuidanceSettingsChanged, enabled = " << std::boolalpha << settings.enabled;
      set_is_enabled(settings.enabled);
    });
    if (!subscribe_result) {
      SB_LOG(ERROR) << "accessibility.subscribeOnVoiceGuidanceSettingsChanged() failed, error code = " << subscribe_result.error();
    }
    else {
      on_vg_settings_changed_id_ = *subscribe_result;
    }

    auto vg_settings  = accessibility.voiceGuidanceSettings();
    if (!vg_settings) {
      SB_LOG(ERROR) << "accessibility.voiceGuidanceSettings() failed, error code = " << vg_settings.error();
    }
    else {
      is_enabled = vg_settings->enabled;
    }

    lock.lock();

    if (!is_enabled_.has_value())
      is_enabled_ = is_enabled;
  }

  SB_DCHECK(is_enabled_.has_value());
  return is_enabled_;
}

void FireboltInterface::FireboltTextToSpeech::set_is_enabled(bool enabled) {
  bool should_notify_app = false;
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    if (!is_enabled_.has_value() || *is_enabled_ != enabled) {
      is_enabled_ = enabled;
      should_notify_app = true;
    }
  }
  if (should_notify_app && Application::Get()) {
    Application::Get()->InjectAccessibilityTextToSpeechSettingsChanged();
  }
}

void FireboltInterface::FireboltTextToSpeech::init() {
  auto &tts = Firebolt::IFireboltAccessor::Instance().TextToSpeechInterface();

  const char* locale_id = SbSystemGetLocaleId();
  auto result = tts.listVoices(locale_id ? locale_id : "en-US");
  if (!result) {
    SB_LOG(ERROR) << "tts.listVoices() failed, error code = " << result.error();
  }
  else {
    is_available_ = (result->ttsStatus == 0 && !result->voices.empty());
  }
}

void FireboltInterface::FireboltTextToSpeech::unsubscribe() {
  if (!is_available_.has_value())
    return;

  std::optional<uint64_t> on_vg_settings_changed_id, on_speech_complete_id;
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    on_vg_settings_changed_id = std::exchange(on_vg_settings_changed_id_, std::nullopt);
    on_speech_complete_id = std::exchange(on_speech_complete_id_, std::nullopt);
    is_enabled_.reset();
    speech_id_.reset();
  }
  auto &accessibility = Firebolt::IFireboltAccessor::Instance().AccessibilityInterface();
  if (on_vg_settings_changed_id.has_value()) {
    auto result = accessibility.unsubscribe(*on_vg_settings_changed_id);
    if (!result) {
      SB_LOG(ERROR) << "Failed to unsubscribe from OnVoiceGuidanceSettingsChanged, error code = " << result.error();
    }
  }
  if (on_speech_complete_id.has_value() && *on_speech_complete_id != -1u) {
    auto result = accessibility.unsubscribe(*on_speech_complete_id);
    if (!result) {
      SB_LOG(ERROR) << "Failed to unsubscribe from OnSpeechComplete, error code = " << result.error();
    }
  }
}

// FireboltAccessibility
std::optional<bool> FireboltInterface::FireboltAccessibility::display_settings(SbAccessibilityDisplaySettings& out) {
  std::unique_lock<std::mutex> lock { mutex_ };
  lazy_init(lock);
  out.has_high_contrast_text_setting = true;
  out.is_high_contrast_text_enabled = is_high_contrast_text_enabled_;
  return true;
}

std::optional<bool> FireboltInterface::FireboltAccessibility::caption_settings(SbAccessibilityCaptionSettings& out) {
  std::unique_lock<std::mutex> lock { mutex_ };
  lazy_init(lock);
  out.supports_is_enabled = true;
  out.supports_set_enabled = false;
  out.is_enabled = is_cc_enabled_;
  return true;
}

void FireboltInterface::FireboltAccessibility::set_high_contrast_ui(bool enabled) {
  bool should_notify_app = false;
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    if (is_high_contrast_text_enabled_ != enabled) {
      is_high_contrast_text_enabled_ = enabled;
      should_notify_app = true;
    }
  }
  if (should_notify_app && Application::Get()) {
    Application::Get()->InjectAccessibilitySettingsChanged();
  }
}

void FireboltInterface::FireboltAccessibility::set_cc_enabled(bool enabled) {
  bool should_notify_app = false;
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    if (is_cc_enabled_ != enabled) {
      is_cc_enabled_ = enabled;
      should_notify_app = true;
    }
  }
  if (should_notify_app && Application::Get()) {
    Application::Get()->InjectAccessibilityCaptionSettingsChanged();
  }
}

void FireboltInterface::FireboltAccessibility::unsubscribe() {
  std::optional<uint64_t> on_high_contrast_ui_changed_id, on_cc_settings_changed_id;
  {
    std::unique_lock<std::mutex> lock { mutex_ };
    on_high_contrast_ui_changed_id = std::exchange(on_high_contrast_ui_changed_id_, std::nullopt);
    on_cc_settings_changed_id = std::exchange(on_cc_settings_changed_id_, std::nullopt);
    did_init_ = false;
  }
  auto &accessibility = Firebolt::IFireboltAccessor::Instance().AccessibilityInterface();
  if (on_high_contrast_ui_changed_id.has_value()) {
    auto result = accessibility.unsubscribe(*on_high_contrast_ui_changed_id);
    if (!result) {
      SB_LOG(ERROR) << "Failed to unsubscribe from OnHighContrastUIChanged, error code = " << result.error();
    }
  }
  if (on_cc_settings_changed_id.has_value()) {
    auto result = accessibility.unsubscribe(*on_cc_settings_changed_id);
    if (!result) {
      SB_LOG(ERROR) << "Failed to unsubscribe from OnClosedCaptionsSettingsChanged, error code = " << result.error();
    }
  }
}

void FireboltInterface::FireboltAccessibility::lazy_init(std::unique_lock<std::mutex>& lock) {
  SB_DCHECK(lock.owns_lock());
  if (did_init_)
    return;
  did_init_ = true;

  bool high_contrast_text_enabled = false;
  bool cc_enabled = false;

  using namespace Firebolt::Accessibility;
  auto &accessibility = Firebolt::IFireboltAccessor::Instance().AccessibilityInterface();
  auto subscribe_result = accessibility.subscribeOnHighContrastUIChanged([this](bool enabled) {
    SB_LOG(INFO) << "accessibility.highContrastUIChanged, enabled = " << std::boolalpha << enabled;
    set_high_contrast_ui(enabled);
  });
  if (!subscribe_result) {
    SB_LOG(ERROR) << "accessibility.subscribeOnHighContrastUIChanged() failed, error code = "
                  << subscribe_result.error();
  } else {
    on_high_contrast_ui_changed_id_ = *subscribe_result;
    auto result = accessibility.highContrastUI();
    if (!result) {
      SB_LOG(ERROR) << "accessibility.highContrastUI() failed, error code = "
                    << result.error();
    } else {
      high_contrast_text_enabled = *result;
    }
  }

  subscribe_result = accessibility.subscribeOnClosedCaptionsSettingsChanged([this](const ClosedCaptionsSettings& settings) {
    SB_LOG(INFO) << "accessibility.closedCaptionsSettingsChanged, enabled = " << std::boolalpha << settings.enabled;
    set_cc_enabled(settings.enabled);
  });
  if (!subscribe_result)  {
    SB_LOG(ERROR) << "accessibility.subscribeOnClosedCaptionsSettingsChanged() failed, error code = "
                  << subscribe_result.error();
  } else {
    on_cc_settings_changed_id_ = *subscribe_result;
    auto result = accessibility.closedCaptionsSettings();
    if (!result) {
      SB_LOG(ERROR) << "accessibility.closedCaptionsSettings() failed, error code = "
                    << result.error();
    } else {
      cc_enabled = result->enabled;
    }
  }

  is_high_contrast_text_enabled_ = high_contrast_text_enabled;
  is_cc_enabled_ = cc_enabled;
}

FireboltInterface::FireboltInterface() : lifecycle_(std::make_unique<FireboltLifecycle>()) {
}

FireboltInterface::~FireboltInterface() {
}

// static
bool FireboltInterface::is_available() {
  return !!getenv("FIREBOLT_ENDPOINT");
}

bool FireboltInterface::lazy_init() {
  std::unique_lock<std::mutex> lock { mutex_ };
  if (connected_.has_value())
    return *connected_;

  const char* kFireboltEndpoint = getenv("FIREBOLT_ENDPOINT");
  const bool kEnableLegacyRPCv1 = false;
  const auto kConnectionTimeout = 3s;

  SB_DCHECK(kFireboltEndpoint && kFireboltEndpoint[0] != '\0');
  if (!kFireboltEndpoint || kFireboltEndpoint[0] == '\0') {
    connected_ = false;
    return *connected_;
  }

  Firebolt::Config cfg { };
  cfg.wsUrl = kFireboltEndpoint;
  cfg.legacyRPCv1 = kEnableLegacyRPCv1;
#if !defined(COBALT_BUILD_TYPE_GOLD)
  cfg.log.level = Firebolt::LogLevel::Debug;
#endif

  const auto start_tp = std::chrono::steady_clock::now();

  auto rc = Firebolt::IFireboltAccessor::Instance().Connect(
    cfg, [this](const bool connected, const Firebolt::Error error) mutable {
      if (!connected) {
        SB_LOG(INFO) << "Firebolt client disconnected, code = " << error;
      }
      {
        std::unique_lock<std::mutex> lock{mutex_};
        connected_ = connected;
      }
      cv_.notify_all();
    });

  switch (rc) {
    case Firebolt::Error::None:
      if (!cv_.wait_for(lock, kConnectionTimeout, [this](){ return connected_.has_value(); })) {
        SB_LOG(ERROR) << "Firebolt client connection timed out.";
        connected_ = false;
      }
      break;
    case Firebolt::Error::AlreadyConnected:
      connected_ = true;
      break;
    default:
      connected_ = false;
      SB_LOG(ERROR) << "Firebolt client connection failed, error = " << rc;
      break;
  }

  if (*connected_) {
    SB_LOG(INFO) << "Firebolt client connected.";
    const auto connected_tp = std::chrono::steady_clock::now();
    device_.init();
    text_to_speech_.init();
    const auto init_completed_tp = std::chrono::steady_clock::now();
    SB_LOG(INFO) << "Firebolt init completed."
                 << " Connect took: " << std::chrono::duration_cast<std::chrono::milliseconds>(connected_tp - start_tp).count() << " ms,"
                 << " init took: " << std::chrono::duration_cast<std::chrono::milliseconds>(init_completed_tp - connected_tp).count() << " ms,"
                 << " total: " << std::chrono::duration_cast<std::chrono::milliseconds>(init_completed_tp - start_tp).count() << " ms.";
  }

  return *connected_;
}

IDevice& FireboltInterface::device() {
  lazy_init();
  return device_;
}

ITextToSpeech& FireboltInterface::text_to_speech() {
  lazy_init();
  return text_to_speech_;
}

IAccessibility& FireboltInterface::accessibility() {
  lazy_init();
  return accessibility_;
}

void FireboltInterface::completeInit() {
  if (lazy_init()) {
    lifecycle_->init();
  }
}

void FireboltInterface::teardown() {
  SB_LOG(INFO) << "teardown()";
  Firebolt::IFireboltAccessor::Instance().LifecycleInterface().close(Firebolt::Lifecycle::CloseType::DEACTIVATE);
  Firebolt::IFireboltAccessor::Instance().Disconnect();
}

void FireboltInterface::suspend() {
  SB_LOG(INFO) << "suspend()";
  std::unique_lock<std::mutex> lock { mutex_ };
  if (connected_.value_or(false)) {
    accessibility_.unsubscribe();
    text_to_speech_.unsubscribe();
    device_.unsubscribe();
  }
}

void FireboltInterface::resume() {
  SB_LOG(INFO) << "resume()";
}

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
