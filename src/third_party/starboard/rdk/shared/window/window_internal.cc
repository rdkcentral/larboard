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

#include "third_party/starboard/rdk/shared/window/window_internal.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"

using namespace third_party::starboard::rdk::shared;

SbWindowPrivate::SbWindowPrivate(const SbWindowOptions* /* options */) { }

SbWindowPrivate::~SbWindowPrivate() = default;

void* SbWindowPrivate::Native() const {
  return reinterpret_cast<void*>(Application::Get()->GetNativeWindow());
}

int SbWindowPrivate::Width() const {
  return 1280;
}

int SbWindowPrivate::Height() const {
  return 720;
}

float SbWindowPrivate::VideoPixelRatio() const {
  SB_LOG(INFO) << "Forced viewport: 1280x720 with pixel ratio 1.5";
  return 1.5f;
}

float SbWindowPrivate::DiagonalSizeInInches() const {
  return DisplayInfo::GetDiagonalSizeInInches();
}
