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

#include "starboard/system.h"
#include "starboard/common/log.h"

bool SbSystemRaisePlatformError(SbSystemPlatformErrorType type,
                                SbSystemPlatformErrorCallback callback,
                                void* user_data) {
  SB_LOG(INFO) << "SbSystemRaisePlatformError called with error type: " << type;

  // If no callback provided, platform cannot respond to this error
  if (!callback) {
    SB_LOG(WARNING) << "SbSystemRaisePlatformError callback is null";
    return false;
  }

  // Invoke the callback to notify platform of error response
  SB_LOG(INFO) << "SbSystemRaisePlatformError invoking callback with "
               << "kSbSystemPlatformErrorResponseNegative";
  callback(kSbSystemPlatformErrorResponseNegative, user_data);

  // Platform handled the error successfully
  return true;
}
