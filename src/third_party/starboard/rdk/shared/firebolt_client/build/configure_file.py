# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2017-2020 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/usr/bin/env python3

import argparse
import re
import sys

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--var', action='append', default=[],
                        help='KEY=VALUE pairs to substitute @KEY@')
    args = parser.parse_args()

    substitutions = {}
    for var in args.var:
        key, value = var.split('=', 1)
        substitutions[key] = value

    with open(args.input, 'r') as f:
        content = f.read()

    def replace(match):
        key = match.group(1)
        if key not in substitutions:
            sys.exit(f'Error: no substitution for @{key}@')
        return substitutions[key]

    content = re.sub(r'@(\w+)@', replace, content)

    with open(args.output, 'w') as f:
        f.write(content)

if __name__ == '__main__':
    main()
