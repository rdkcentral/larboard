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

#include <signal.h>
#include <sys/resource.h>

#include <cstring>

#include "starboard/configuration.h"
#include "starboard/shared/signal/suspend_signals.h"

#include "third_party/starboard/rdk/shared/application_rdk.h"

extern "C" SB_EXPORT_PLATFORM int StarboardMain(int argc, char** argv) {
  tzset();

  rlimit stack_size;
  getrlimit(RLIMIT_STACK, &stack_size);
  stack_size.rlim_cur = 2 * 1024 * 1024;
  setrlimit(RLIMIT_STACK, &stack_size);

  starboard::InstallSuspendSignalHandlers();

  int result = SbRunStarboardMain(argc, argv, SbEventHandle);

  starboard::UninstallSuspendSignalHandlers();

  return result;
}
