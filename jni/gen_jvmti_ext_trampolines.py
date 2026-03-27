#!/usr/bin/python
#
# Copyright (C) 2025 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import sys

_ARG_EXT = {
    'jvmtiEnv*': {
        'init': '  jvmtiEnv* arg_{arg_index} = ToHostJvmtiEnv(arg_env);'
    },
}

def _get_trampoline_name(name):
    return f'DoTrampoline_jvmtiExtension_{name}'

def _gen_trampoline(out, decl):
    name = decl['name']
    params = decl['param_types']
    ret_type = decl['return_type']

    trampoline_name = _get_trampoline_name(name)

    # Construct the PFN type string.
    # params[0] is jvmtiEnv*.
    pfn_params = ', '.join(params)
    pfn_type = f'{ret_type} (*)({pfn_params})'

    # Arguments handling.
    # arg_0 is env (from GuestParamsValues).
    # arg_1 ... are other args.

    # We need to map param types to arg names for GuestParamsValues,
    # GuestParamsValues returns a tuple.
    # We assume standard mapping.

    arg_names = []
    arg_names.append('arg_env')
    for i in range(1, len(params)):
        arg_names.append(f'arg_{i}')

    arg_list_str = ', '.join(arg_names)

    print(f'void {trampoline_name}(HostCode callee, ProcessState* state) {{', file=out)
    print(f'  LOG_JNI("{trampoline_name} called");', file=out)
    print(f'  using PFN = {pfn_type};', file=out)
    print(f'  auto [{arg_list_str}] = GuestParamsValues<PFN>(state);', file=out)

    # Conversions.
    call_args = []

    # Handle arg 0 (env).
    assert params[0] == 'jvmtiEnv*'
    print('  jvmtiEnv* env = ToHostJvmtiEnv(arg_env);', file=out)
    call_args.append('env')

    for i in range(1, len(params)):
        p_type = params[i]
        arg_name = f'arg_{i}'

        if p_type == 'const jvmtiHeapCallbacks*':
            print(f'  auto host_callbacks_{i} = WrapGuestJvmtiHeapCallbacks({arg_name});', file=out)
            call_args.append(f'&host_callbacks_{i}')
        # Add other special conversions here if needed.
        # For now, pass others directly (assuming GuestParamsValues handles simple types,
        # and we assume pointers are compatible or handled elsewhere).
        else:
            call_args.append(arg_name)

    print('  auto&& [ret] = GuestReturnReference<PFN>(state);', file=out)

    call_args_str = ', '.join(call_args)
    print(f'  ret = std::bit_cast<PFN>(callee)({call_args_str});', file=out)
    print('}', file=out)
    print('', file=out)

def main(argv):
    if len(argv) < 3:
        print("Usage: gen_jvmti_ext_trampolines.py <output_file> <input_json>")
        sys.exit(1)

    output_file = argv[1]
    input_file = argv[2]

    with open(input_file, 'r') as f:
        data = json.load(f)

    functions = data['functions']

    with open(output_file, 'w') as out:
        print("""/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
""", file=out)

        # Generate Trampolines.
        for func in functions:
            _gen_trampoline(out, func)

        # Generate WrapJvmtiExtensionFunctionInfos.
        print('void WrapJvmtiExtensionFunctionInfos(jint extension_count, jvmtiExtensionFunctionInfo* extensions) {', file=out)
        print('  for (int i = 0; i < extension_count; ++i) {', file=out)
        print('    jvmtiExtensionFunctionInfo& info = extensions[i];', file=out)

        first = True
        for func in functions:
            func_id = func['id']
            trampoline_name = _get_trampoline_name(func['name'])

            if first:
                print(f'    if (strcmp(info.id, "{func_id}") == 0) {{', file=out)
                first = False
            else:
                print(f'    }} else if (strcmp(info.id, "{func_id}") == 0) {{', file=out)

            print('      WrapHostFunctionImpl(', file=out)
            print('          std::bit_cast<HostCode>(info.func),', file=out)
            print(f'          {trampoline_name},', file=out)
            print('          info.id);', file=out)

        print('    }', file=out)
        print('  }', file=out)
        print('}', file=out)

if __name__ == '__main__':
    main(sys.argv)
