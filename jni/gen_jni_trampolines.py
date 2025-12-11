#!/usr/bin/python
#
# Copyright (C) 2023 The Android Open Source Project
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

"""Parse ABI functions declarations and generate trampolines.

Input files are C++ files with very limited syntax. They can contain function
declarations and comments only.

Function declaration should not specify parameter names, just parameter types.

Comments should be C++-comments and they should appear before or after function
declaration but not inside it.

Special comments define attributes of the subsequent declaration:
- // BERBERIS_CUSTOM_TRAMPOLINE(<name>)
  This declaration already has a custom hand-coded trampoline <name>;

Unrecognized comment discards all preceding attributes, so if declaration with
attributes is commented out, its attributes are not attached to the next
declaration.
"""

import json
import sys


_ARG_JNI = {
    'JNIEnv *': {
        'init': '  JNIEnv* arg_{arg_index} = ToHostJNIEnv(arg_env);'
    },
    '...': {
        'init': """\
  std::vector<jvalue> arg_vector = ConvertVAList(
    arg_0, arg_{arg_va_method_id}, GuestParamsValues<PFN_callee>(state));
  jvalue* arg_{arg_index} = &arg_vector[0];""",
    },
  # Note: we couldn't teach GuestParams class to distinguish Va_list because on
  # ARM it's simply a char*, not a distint type.
    'va_list': {
        'init': """\
  std::vector<jvalue> arg_vector = ConvertVAList(
    arg_0, arg_{arg_va_method_id}, ToGuestAddr(arg_va));
  jvalue* arg_{arg_index} = &arg_vector[0];""",
    },
}

_ARG_JVMTI = {
    'jvmtiEnv*': {
        'init': '  jvmtiEnv* arg_{arg_index} = ToHostJvmtiEnv(arg_env);'
    },
}


def _set_callee(decl, is_jvmti):
  param_types = decl['param_types']
  # TODO(eaeltsin): introduce attributes to
  # - indicate this is a virtual method and not a global function
  # - specify custom thunk
  # - specify how to convert variable arguments list
  # instead of the code below.
  if is_jvmti:
    decl['callee'] = 'arg_0->functions->%s' % decl['name']
    return

  if 'jmethodID' in param_types and 'va_list' in param_types:
    # NewObjectV, CallObjectMethodV, ...
    assert decl['name'].endswith('V')
    decl['callee'] = 'arg_0->functions->%sA' % decl['name'][:-1]
    decl['arg_va_method_id'] = param_types.index('jmethodID')
  elif 'jmethodID' in param_types and '...' in param_types:
    # NewObject, CallObjectMethod, ...
    decl['callee'] = 'arg_0->functions->%sA' % decl['name']
    decl['arg_va_method_id'] = param_types.index('jmethodID')
  else:
    decl['callee'] = 'arg_0->functions->%s' % decl['name']


def _print_jni_call(interface, out, args, decl):
  # TODO(eaeltsin): implement printing using printf-style LOG_JNI!
  print("""
  LOG_JNI("DoTrampoline_{interface}_{name} called");""".format(interface=interface, **decl), file=out)


def _print_jni_result(interface, out, res, decl):
  # TODO(eaeltsin): implement printing using printf-style LOG_JNI!
  pass


def _gen_trampoline(out, decl, interface, _ARG):
  is_jvmti = interface == 'jvmtiEnv'
  _set_callee(decl, is_jvmti)

  args = decl['param_types']

  # In JVMTI the first param is `jvmtiEnv* env`. For JNI it is `JNIEnv *`.
  # The script assumes the first param is the env.
  first_param_type = 'jvmtiEnv*' if is_jvmti else 'JNIEnv *'

  if '...' in args:
    assert not is_jvmti, "'...' arguments are not supported for jvmti interface"
    assert args[-1] == '...'
    arglist = ['arg_%d' %i for i in range(1, len(args) - 1)]
  elif 'va_list' in args:
    assert args[-1] == 'va_list'
    arglist = ['arg_%d' %i for i in range(1, len(args) - 1)] + ['arg_va']
  else:
    arglist = ['arg_%d' %i for i in range(1, len(args))]
  assert args[0] == first_param_type
  decl['arglist'] = ', '.join(['arg_env'] + arglist)

  print("""
void DoTrampoline_{interface}_{name}(
    HostCode /* callee */,
    ProcessState* state) {{
  using PFN_callee = decltype(std::declval<{interface}>().functions->{name});
  auto [{arglist}] = GuestParamsValues<PFN_callee>(state);""".format(interface=interface, **decl), file=out)

  for i, type in enumerate(args):
    if type in _ARG:
      arg = _ARG[type]
      print(arg['init'].format(arg_index=i, **decl), file=out)

  _print_jni_call(interface, out, args, decl)
  if decl['return_type'] == 'void':
    print(' {callee}('.format(**decl), file=out)
  else:
    print('  auto&& [ret] = GuestReturnReference<PFN_callee>(state);', file=out)
    print('  ret = {callee}('.format(**decl), file=out)

  for i in range(len(args) - 1):
    print('      arg_%d,' % i, file=out)
  print('      arg_%d);' % (len(args) - 1), file=out)
  if decl['return_type'] != 'void':
    _print_jni_result(interface, out, 'ret', decl)

  print('}', file=out)


def main(argv):
  # Usage: gen_jni_trampolines.py <gen-header> <abi-def>
  header_name = argv[1]
  abi_def_name = argv[2]

  with open(abi_def_name) as json_file:
    abi_data = json.load(json_file)

  interface = abi_data['interface']
  decls = abi_data['functions']

  if interface == 'jvmtiEnv':
    _ARG = _ARG_JVMTI
  elif interface == 'JNIEnv':
    _ARG = _ARG_JNI
  else:
    sys.exit('Unknown interface: ' + interface)

  is_jvmti = (interface == 'jvmtiEnv')

  out = open(header_name, 'w')

  for decl in decls:
    if 'trampoline' not in decl:
      _gen_trampoline(out, decl, interface, _ARG)

  if is_jvmti:
    print("""
void WrapJvmtiEnv(jvmtiEnv* env) {
  HostCode* jvmti_vtable = bit_cast<HostCode*>(env->functions);""", file=out)
    for decl in decls:
      if 'index' not in decl:
        sys.exit("missing index for {decl['name']}")
      # Index starts with 1 in the json - so we decrement it.
      decl['index'] -= 1;
      print("""
  WrapHostFunctionImpl(
    jvmti_vtable[{index}],
    DoTrampoline_{interface}_{name},
    "{interface}::{name}");""".format(interface=interface, **decl), file=out)
  else: # JNI
    print("""
void WrapJNIEnv(JNIEnv* env) {
  HostCode* jni_vtable = bit_cast<HostCode*>(env->functions);""", file=out)
    print("""  // jni_vtable[0] is NULL
  // jni_vtable[1] is NULL
  // jni_vtable[2] is NULL
  // jni_vtable[3] is NULL""", file=out)
    for i, decl in enumerate(decls):
      print("""
  WrapHostFunctionImpl(
    jni_vtable[%d],
    DoTrampoline_JNIEnv_%s,
    "JNIEnv::%s");""" % (4 + i, decl['name'], decl['name']), file=out)

  print('}', file=out)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
