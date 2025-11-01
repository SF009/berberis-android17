/*
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

#ifndef BERBERIS_BACKEND_X86_64_MEM_OPERAND_H_
#define BERBERIS_BACKEND_X86_64_MEM_OPERAND_H_

#include <cstdint>

#include "berberis/backend/common/machine_ir.h"

namespace berberis {

namespace x86_64 {

struct MemoryOperand {
  MachineReg base = kInvalidMachineReg;
  MachineReg index = kInvalidMachineReg;
  Assembler::ScaleFactor scale = Assembler::kTimesOne;
  // Note: x86-64 only supports 64bit offset in one instruction: movabs – and that one may only be
  // be used to move a value to or from RAX. We don't use it in our code anywhere and it would be
  // better to treat it as a special case, rather than pretend that other instruction may support
  // 64bit offset.
  int32_t disp = 0;
};

// To be able to write
//   Gen<x86_64::MovssXRegOp>(res.machine_reg(), {.base = arg, .disp = offset});
// wea need a set of instructions, because “Args... args” argument couldn't accept it.
//
// See: “Perfect forwarding forwards objects, not braced things that are trying to become objects”:
// https://devblogs.microsoft.com/oldnewthing/20230727-00/?p=108494
//
// But that means that we need to duplicate the same tedious logic in many places: MachineIR,
// MachineIRBuilder, HeavyOptimizerFrontend, etc.
//
// BERBERIS_DECLARE_MACHINE_INSN_ADAPTER makes that work easy (on the assumption that these
// helper functions only exist to solve this particular problem and delegate actual work to
// another functions).

#define BERBERIS_DECLARE_MACHINE_INSN_ADAPTER(                                                    \
    AdapterName, TupleName, ResultType1, ResultType2, ForwarderName, kSSAMode, ...)               \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName()                                                                                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 0,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>>(std::tuple<>{});                                           \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 1,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0});                                                        \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 2,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1});                                                  \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 3,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2});                                            \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 4,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3});                                      \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(4, TupleName, kSSAMode) arg4)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 5,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3, arg4});                                \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(4, TupleName, kSSAMode) arg4,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(5, TupleName, kSSAMode) arg5)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 6,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3, arg4, arg5});                          \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(4, TupleName, kSSAMode) arg4,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(5, TupleName, kSSAMode) arg5,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(6, TupleName, kSSAMode) arg6)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 7,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3, arg4, arg5, arg6});                    \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(4, TupleName, kSSAMode) arg4,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(5, TupleName, kSSAMode) arg5,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(6, TupleName, kSSAMode) arg6,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(7, TupleName, kSSAMode) arg7)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 8,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7});              \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(4, TupleName, kSSAMode) arg4,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(5, TupleName, kSSAMode) arg5,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(6, TupleName, kSSAMode) arg6,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(7, TupleName, kSSAMode) arg7,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(8, TupleName, kSSAMode) arg8)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 9,                                          \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8});        \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__>                     \
  AdapterName(BERBERIS_DECLARE_MACHINE_INSN_TUPLE(0, TupleName, kSSAMode) arg0,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(1, TupleName, kSSAMode) arg1,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(2, TupleName, kSSAMode) arg2,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(3, TupleName, kSSAMode) arg3,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(4, TupleName, kSSAMode) arg4,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(5, TupleName, kSSAMode) arg5,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(6, TupleName, kSSAMode) arg6,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(7, TupleName, kSSAMode) arg7,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(8, TupleName, kSSAMode) arg8,                   \
              BERBERIS_DECLARE_MACHINE_INSN_TUPLE(9, TupleName, kSSAMode) arg9)                   \
      ->std::enable_if_t<std::tuple_size_v<typename x86_64::MachineInsn<                          \
                             typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                             kSSAMode>::TupleName> == 10,                                         \
                         ResultType1,                                                             \
                         ResultType2> {                                                           \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        typename x86_64::MachineInsn<                                                             \
            typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,                  \
            kSSAMode>::TupleName>({arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9});  \
  }                                                                                               \
                                                                                                  \
  template <template <typename> typename InsnType __VA_OPT__(, ) __VA_ARGS__, typename... Args>   \
  AdapterName(Args... args)->std::enable_if_t<10 < sizeof...(Args), ResultType1, ResultType2> {   \
    return ForwarderName<                                                                         \
        x86_64::MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,  \
                            kSSAMode>,                                                            \
        std::tuple<Args...>>({args...});                                                          \
  }

#define BERBERIS_DECLARE_MACHINE_INSN_TUPLE(N, TupleName, kSSAMode)                             \
  std::tuple_element_t<N,                                                                       \
                       typename x86_64::MachineInsn<                                            \
                           typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, \
                           kSSAMode>::TupleName>

}  // namespace x86_64

}  // namespace berberis

#endif  // BERBERIS_BACKEND_X86_64_MEM_OPERAND_H_
