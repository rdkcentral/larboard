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

#include "third_party/starboard/rdk/shared/platform/firebolt_lifecycle.h"
#include "third_party/starboard/rdk/shared/platform/firebolt_utility.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/libcobalt.h"
#include "third_party/starboard/rdk/shared/log_override.h"

#include <cstring>

using namespace Firebolt;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

namespace {

const char kPreloadAction[] = "pre-load";
const char* kDefaultExitStrategy = "suspend";

} // namespace

void FireboltLifecycle::teardown() {
  auto &lifecycle = IFireboltAccessor::Instance().LifecycleInterface();
  auto &actions = IFireboltAccessor::Instance().ActionsInterface();
  auto &presentation = IFireboltAccessor::Instance().PresentationInterface();

  presentation.unsubscribeAll();
  actions.unsubscribeAll();
  lifecycle.unsubscribeAll();

  lifecycle.close(Lifecycle::CloseType::UNLOAD);

  SbRdkSetConcealRequestHandler(nullptr, nullptr);
}

void FireboltLifecycle::initialize() {

  // Actions
  {
    auto &actions = IFireboltAccessor::Instance().ActionsInterface();
    auto result = actions.subscribeOnIntent([this](const Actions::Intent& intent){
      on_intent(intent);
    });
    if (!result) {
      SB_LOG(ERROR) << "actions.subscribeOnIntent failed, error code = " << result.error();
    }
    auto intent = actions.intent();
    if (!intent) {
      SB_LOG(ERROR) << "actions.intent failed, error code = " << intent.error();
    } else {
      on_intent(*intent);
    }
  }

  // Lifecycle
  {
    auto &lifecycle = IFireboltAccessor::Instance().LifecycleInterface();
    auto result = lifecycle.subscribeOnStateChanged([this](const std::vector<Lifecycle::StateChange>& changes) {
      for (auto& change : changes) {
        SB_LOG(INFO) << "lifecycle.OnStateChanged:"
                     << " new_state=" << change.newState << ','
                     << " old_state=" << change.oldState;
        on_state_change(change.newState, change.oldState);
      }
    });
    if (!result) {
      SB_LOG(ERROR) << "lifecycle.subscribeOnStateChanged failed, error code = " << result.error();
    }
  }

  // Presentation
  {
    auto &presentation = IFireboltAccessor::Instance().PresentationInterface();
    auto result = presentation.subscribeOnFocusedChanged([this](const bool focused) {
      SB_LOG(INFO) << "presentation.onFocusedChanged: focused=" << focused;
      on_focused_changed(focused);
    });
    if (!result) {
      SB_LOG(ERROR) << "presentation.subscribeOnFocusedChanged failed, error code = " << result.error();
    }
  }

  // Configure 'exit' strategy
  SbRdkSetConcealRequestHandler([](void*) -> int {
    SB_LOG(INFO) << "Conceal requested, sending lifecycle.close(DEACTIVATE).";
    auto &lifecycle = IFireboltAccessor::Instance().LifecycleInterface();
    auto result = lifecycle.close(Lifecycle::CloseType::DEACTIVATE);
    if (!result) {
      SB_LOG(ERROR) << "lifecycle.close(DEACTIVATE) failed, error code = " << result.error();
      Application::Get()->Stop(0);
    } else {
      Application::Get()->Conceal(nullptr, nullptr);
    }
    return 0;  // prevent default handler
  }, nullptr);
  SbRdkSetCobaltExitStrategy(kDefaultExitStrategy);
}

void FireboltLifecycle::on_intent(const Firebolt::Actions::Intent& intent) {
  std::unique_lock<std::mutex> lock { mutex_ };
  if (last_intent_id_ == intent.intentId)
    return;

  auto* app = Application::Get();

  bool has_preload_action = (0 == strncasecmp(intent.intent.action.c_str(), kPreloadAction, strlen(kPreloadAction)));
  if (has_preload_action && lifecycle_state_ == Lifecycle::LifecycleState::INITIALIZING) {
    SB_LOG(INFO) << "Setting preload immediate";
    app->SetPreloadImmediate();
  }

  // TODO: create and set deep link from intent data query

  // if 'pre-load' intent got cleared then reveal the app to start rendering
  // to drive the platform lifecycle to Active
  if (has_preload_action_ && !has_preload_action && lifecycle_state_ == Lifecycle::LifecycleState::PAUSED) {
    SB_LOG(INFO) << "App::Reveal";
    app->Reveal(nullptr, nullptr);
  }

  last_intent_id_ = intent.intentId;
  has_preload_action_ = has_preload_action;
}

void FireboltLifecycle::on_state_change(Lifecycle::LifecycleState new_state, Lifecycle::LifecycleState old_state) {
  std::unique_lock<std::mutex> lock { mutex_ };
  if (lifecycle_state_ == new_state)
    return;

#if SB_DCHECK_ENABLED
  // Valid firebolt lifecycle transitions:
  //   initializing => paused     = if (preload) conceal() else if (!focused) blur() else reveal()
  //   initializing => suspended  = freeze()
  //   paused => active           = if (focused) focus() else if (preload) reveal()
  //   active => paused           = conceal()
  //   paused => suspended        = freeze()
  //   suspended => paused        = reveal()
  //   suspended => hibernated
  //   hibernated => suspended
  //   any => terminating         = stop()
  switch(new_state) {
    case Lifecycle::LifecycleState::ACTIVE:
      SB_DCHECK(old_state == Lifecycle::LifecycleState::PAUSED);
      break;
    case Lifecycle::LifecycleState::PAUSED:
      SB_DCHECK(old_state == Lifecycle::LifecycleState::INITIALIZING ||
                old_state == Lifecycle::LifecycleState::ACTIVE ||
                old_state == Lifecycle::LifecycleState::SUSPENDED);
      break;
    case Lifecycle::LifecycleState::SUSPENDED:
      SB_DCHECK(old_state == Lifecycle::LifecycleState::INITIALIZING ||
                old_state == Lifecycle::LifecycleState::PAUSED ||
                old_state == Lifecycle::LifecycleState::HIBERNATED);
      break;
      break;
    case Lifecycle::LifecycleState::HIBERNATED:
      SB_DCHECK(old_state == Lifecycle::LifecycleState::SUSPENDED);
      break;
    case Lifecycle::LifecycleState::TERMINATING:
    case Lifecycle::LifecycleState::INITIALIZING:
      break;
  }
#endif

  SB_DCHECK(lifecycle_state_ == old_state);
  lifecycle_state_ = new_state;

  auto* app = Application::Get();
  switch(new_state) {
    case Lifecycle::LifecycleState::ACTIVE:
      if (is_focused_) {
        SB_LOG(INFO) << "App::Focus";
        app->Focus(nullptr, nullptr);
      } else if (has_preload_action_) {
        SB_LOG(INFO) << "App::Reveal";
        app->Reveal(nullptr, nullptr);
      }
      break;
    case Lifecycle::LifecycleState::PAUSED:
      if ((old_state == Lifecycle::LifecycleState::ACTIVE) ||
          (old_state == Lifecycle::LifecycleState::INITIALIZING && has_preload_action_)) {
        SB_LOG(INFO) << "App::Conceal";
        app->Conceal(nullptr, nullptr);
      } else if (old_state == Lifecycle::LifecycleState::INITIALIZING && !is_focused_) {
        SB_DCHECK(!has_preload_action_);
        SB_LOG(INFO) << "App::Blur";
        app->Blur(nullptr, nullptr);
      } else {
        SB_LOG(INFO) << "App::Reveal";
        app->Reveal(nullptr, nullptr);
      }
      break;
    case Lifecycle::LifecycleState::SUSPENDED:
      if (old_state == Lifecycle::LifecycleState::INITIALIZING ||
          old_state == Lifecycle::LifecycleState::PAUSED) {
        SB_LOG(INFO) << "App::Freeze";
        app->Freeze(nullptr, nullptr);
      }
      break;
    case Lifecycle::LifecycleState::TERMINATING:
      SB_LOG(INFO) << "App::Stop";
      app->Stop(0);
      break;
    case Lifecycle::LifecycleState::HIBERNATED:
    case Lifecycle::LifecycleState::INITIALIZING:
      // no-op
      break;
  }
}

void FireboltLifecycle::on_focused_changed(const bool focused) {
  std::unique_lock<std::mutex> lock { mutex_ };
  if (is_focused_ == focused)
    return;
  is_focused_ = focused;
  if (lifecycle_state_ == Lifecycle::LifecycleState::ACTIVE) {
    auto* app = Application::Get();
    if (focused) {
      SB_LOG(INFO) << "App::Focus";
      app->Focus(nullptr, nullptr);
    } else {
      SB_LOG(INFO) << "App::Blur";
      app->Blur(nullptr, nullptr);
    }
  }
}

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
