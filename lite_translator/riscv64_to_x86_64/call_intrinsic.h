/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef BERBERIS_LITE_TRANSLATOR_RISCV64_TO_X86_64_CALL_INTRINSIC_H_
#define BERBERIS_LITE_TRANSLATOR_RISCV64_TO_X86_64_CALL_INTRINSIC_H_

#include <array>
#include <cstdint>
#include <type_traits>

#include "berberis/assembler/x86_64.h"
#include "berberis/base/bit_util.h"
#include "berberis/base/checks.h"
#include "berberis/base/dependent_false.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/device_arch_info/x86_64/call_imm.h"
#include "berberis/intrinsics/macro_assembler.h"
#include "berberis/runtime_primitives/platform.h"

namespace berberis::call_intrinsic {

constexpr int8_t kRegIsNotOnStack = -1;

inline constexpr struct StoredRegsInfo {
  // Map from register number to offset in CallIntrinsic save area. Counted in 8-byte slots.
  std::array<int8_t, 16> regs_on_stack;
  std::array<int8_t, 16> simd_regs_on_stack;
  // Save area size for CallIntrinsic save area. Counted in 8-byte slots.
  int8_t save_area_size;
} kRegOffsetsOnStack = []() {
  StoredRegsInfo result;
  result.regs_on_stack.fill(kRegIsNotOnStack);
  result.simd_regs_on_stack.fill(kRegIsNotOnStack);

  result.save_area_size = 0;
  TypesToValues::ForEach<x86_64::device_arch_info::call_imm_impl::ClobberRegisters>(
      [&result]<typename RegisterClass>() {
        if constexpr (!std::is_same_v<RegisterClass, x86_64::device_arch_info::FLAGS>) {
          constexpr auto kRegister =
              RegisterClass::template kMachineRegId<x86_64::Assembler::Registers>;
          if constexpr (std::is_same_v<decltype(kRegister), const x86_64::Assembler::Register>) {
            result.regs_on_stack[kRegister.GetPhysicalIndex()] = result.save_area_size;
            ++result.save_area_size;
          } else if constexpr (std::is_same_v<decltype(kRegister),
                                              const x86_64::Assembler::XMMRegister>) {
            result.save_area_size = AlignUp<2>(result.save_area_size);
            result.simd_regs_on_stack[kRegister.GetPhysicalIndex()] = result.save_area_size;
            result.save_area_size += 2;
          } else {
            static_assert(kDependentTypeFalse<decltype(kRegister)>,
                          "Unknown register type, please add support to CallIntrinsic");
          }
        }
      });
  return result;
}();

// Save area size for CallIntrinsic save area. Counted in 8-byte slots.

inline void PushCallerSaved(MacroAssembler<x86_64::Assembler>& as,
                            const StoredRegsInfo& regs_info) {
  as.Subq(as.rsp, regs_info.save_area_size * 8);

  TypesToValues::ForEach<x86_64::device_arch_info::call_imm_impl::ClobberRegisters>(
      [&as, &regs_info]<typename RegisterClass>() {
        if constexpr (!std::is_same_v<RegisterClass, x86_64::device_arch_info::FLAGS>) {
          constexpr auto kRegister =
              RegisterClass::template kMachineRegId<x86_64::Assembler::Registers>;
          if constexpr (std::is_same_v<decltype(kRegister), const x86_64::Assembler::Register>) {
            as.Movq(
                {.base = as.rsp, .disp = regs_info.regs_on_stack[kRegister.GetPhysicalIndex()] * 8},
                kRegister);
          } else if constexpr (std::is_same_v<decltype(kRegister),
                                              const x86_64::Assembler::XMMRegister>) {
            if (host_platform::kHasAVX) {
              as.Vmovdqa({.base = as.rsp,
                          .disp = regs_info.simd_regs_on_stack[kRegister.GetPhysicalIndex()] * 8},
                         kRegister);
            } else {
              as.Movdqa({.base = as.rsp,
                         .disp = regs_info.simd_regs_on_stack[kRegister.GetPhysicalIndex()] * 8},
                        kRegister);
            }
          } else {
            static_assert(kDependentTypeFalse<decltype(kRegister)>,
                          "Unknown register type, please add support to CallIntrinsic");
          }
        }
      });
}

// Note: regs_on_stack is usually copy of kRegOffsetsOnStack with some registers marked off as
// kRegIsNotOnStack, simd_regs_on_stack is kSimdRegOffsetsOnStack with some registers marked as
// kRegIsNotOnStack. These registers are skipped during restoration process.
inline void PopCallerSaved(MacroAssembler<x86_64::Assembler>& as, const StoredRegsInfo& regs_info) {
  TypesToValues::ForEach<x86_64::device_arch_info::call_imm_impl::ClobberRegisters>(
      [&as, &regs_info]<typename RegisterClass>() {
        if constexpr (!std::is_same_v<RegisterClass, x86_64::device_arch_info::FLAGS>) {
          constexpr auto kRegister =
              RegisterClass::template kMachineRegId<x86_64::Assembler::Registers>;
          if constexpr (std::is_same_v<decltype(kRegister), const x86_64::Assembler::Register>) {
            if (regs_info.regs_on_stack[kRegister.GetPhysicalIndex()] != kRegIsNotOnStack) {
              as.Movq(kRegister,
                      {.base = as.rsp,
                       .disp = regs_info.regs_on_stack[kRegister.GetPhysicalIndex()] * 8});
            }
          } else if constexpr (std::is_same_v<decltype(kRegister),
                                              const x86_64::Assembler::XMMRegister>) {
            if (host_platform::kHasAVX) {
              if (regs_info.simd_regs_on_stack[kRegister.GetPhysicalIndex()] != kRegIsNotOnStack) {
                as.Vmovdqa(
                    kRegister,
                    {.base = as.rsp,
                     .disp = regs_info.simd_regs_on_stack[kRegister.GetPhysicalIndex()] * 8});
              }
            } else {
              if (regs_info.simd_regs_on_stack[kRegister.GetPhysicalIndex()] != kRegIsNotOnStack) {
                as.Movdqa(kRegister,
                          {.base = as.rsp,
                           .disp = regs_info.simd_regs_on_stack[kRegister.GetPhysicalIndex()] * 8});
              }
            }
          } else {
            static_assert(kDependentTypeFalse<decltype(kRegister)>,
                          "Unknown register type, please add support to CallIntrinsic");
          }
        }
      });

  as.Addq(as.rsp, regs_info.save_area_size * 8);
}

// Assumes RSP points to preallocated stack args area.
template <typename... IntrinsicArgTypes, typename MacroAssembler, typename... AssemblerArgTypes>
void InitArgs(MacroAssembler&& as, bool has_avx, AssemblerArgTypes... args) {
  using Assembler = std::decay_t<MacroAssembler>;
  using Register = typename Assembler::Register;
  using XMMRegister = typename Assembler::XMMRegister;

  // All ABI argument registers are saved among caller-saved registers, so we can safely initialize
  // them now. When intrinsic receives its argument from such register we'll read it from stack, so
  // there is no early-clobbering problem. Callee-saved regs are never ABI arguments, so we can move
  // them to ABI reg directly.

  constexpr auto kIndexes = ValuesToValues::ToArray<size_t>(
      TypesToValues::MapWithTemporary<std::tuple<IntrinsicArgTypes...>,
                                      /* gp_index, simd_index = */ std::pair<size_t, size_t>>(
          []<typename IntrinsicArgType>(std::pair<size_t, size_t>& indexes) {
            auto& [gp_index, simd_index] = indexes;
            if constexpr (MetaType<IntrinsicArgType>::IsIntegral()) {
              CHECK_LT(
                  gp_index,
                  std::tuple_size_v<x86_64::device_arch_info::call_imm_impl::GpArgumentRegisters>);
              return gp_index++;
            } else if constexpr (MetaType<IntrinsicArgType>::IsWrappedFloat()) {
              CHECK_LT(
                  simd_index,
                  std::tuple_size_v<x86_64::device_arch_info::call_imm_impl::SSEArgumentRegisters>);
              return simd_index++;
            } else {
              static_assert(kDependentTypeFalse<IntrinsicArgType>,
                            "Unknown parameter type, please add support to CallIntrinsic");
            }
          }));
  ValuesToValues::ForEach(
      ValuesToValues::Zip(std::tuple{kMetaType<IntrinsicArgTypes>...},
                          ValuesToTypes::MetaValues<kIndexes>{},
                          std::tuple{args...}),
      [&as, has_avx]<typename AssemblerType, typename IntrinsicType, size_t kIndex>(
          std::tuple<MetaType<IntrinsicType>, MetaValue<kIndex>, AssemblerType>&& arg_info) {
        auto [_1, _2, arg] = arg_info;

        if constexpr (MetaType<IntrinsicType>::IsIntegral()) {
          constexpr auto kRegister =
              std::tuple_element_t<kIndex,
                                   x86_64::device_arch_info::call_imm_impl::GpArgumentRegisters>::
                  template kMachineRegId<x86_64::Assembler::Registers>;
          // Note, clang errorneously mandates extension up to 32-bit.
          // See: https://github.com/llvm/llvm-project/issues/43573
          if constexpr (MetaType<IntrinsicType>::SizeOf() <= sizeof(int32_t) &&
                        std::is_integral_v<AssemblerType> &&
                        sizeof(AssemblerType) <= sizeof(int32_t)) {
            as.Movl(kRegister, static_cast<int32_t>(arg));
          } else if constexpr (MetaType<IntrinsicType>::SizeOf() == sizeof(int64_t) &&
                               std::is_integral_v<AssemblerType> &&
                               sizeof(AssemblerType) <= sizeof(int64_t)) {
            as.Movq(kRegister, static_cast<int64_t>(arg));
            // Note, clang errorneously mandates extension up to 32-bit.
            // See: https://github.com/llvm/llvm-project/issues/43573
          } else if constexpr (MetaType<IntrinsicType>::SizeOf() <= sizeof(int32_t) &&
                               std::is_same_v<AssemblerType, Register>) {
            if (kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] == kRegIsNotOnStack) {
              as.template Expand<int32_t, IntrinsicType>(kRegister, arg);
            } else {
              as.template Expand<int32_t, IntrinsicType>(
                  kRegister,
                  {.base = Assembler::rsp,
                   .disp = kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] * 8});
            }
          } else if constexpr (MetaType<IntrinsicType>::SizeOf() == sizeof(int64_t) &&
                               std::is_same_v<AssemblerType, Register>) {
            if (kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] == kRegIsNotOnStack) {
              as.template Expand<int64_t, IntrinsicType>(kRegister, arg);
            } else {
              as.template Expand<int64_t, IntrinsicType>(
                  kRegister,
                  {.base = Assembler::rsp,
                   .disp = kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] * 8});
            }
          } else {
            static_assert(kDependentTypeFalse<std::tuple<IntrinsicType, AssemblerType>>,
                          "Unknown parameter type, please add support to CallIntrinsic");
          }
        } else if constexpr (MetaType<IntrinsicType>::IsWrappedFloat()) {
          constexpr auto kRegister =
              std::tuple_element_t<kIndex,
                                   x86_64::device_arch_info::call_imm_impl::SSEArgumentRegisters>::
                  template kMachineRegId<x86_64::Assembler::Registers>;
          if constexpr (std::is_same_v<AssemblerType, XMMRegister>) {
            if (kRegOffsetsOnStack.simd_regs_on_stack[arg.GetPhysicalIndex()] == kRegIsNotOnStack) {
              if (has_avx) {
                as.template Vmovs<IntrinsicType>(kRegister, kRegister, arg);
              } else {
                as.template Movs<IntrinsicType>(kRegister, arg);
              }
            } else {
              if (has_avx) {
                as.template Vmovs<IntrinsicType>(
                    kRegister,
                    {.base = as.rsp,
                     .disp = kRegOffsetsOnStack.simd_regs_on_stack[arg.GetPhysicalIndex()] * 8});
              } else {
                as.template Movs<IntrinsicType>(
                    kRegister,
                    {.base = as.rsp,
                     .disp = kRegOffsetsOnStack.simd_regs_on_stack[arg.GetPhysicalIndex()] * 8});
              }
            }
          } else {
            static_assert(kDependentTypeFalse<std::tuple<IntrinsicType, AssemblerType>>,
                          "Unknown parameter type, please add support to CallIntrinsic");
          }
        } else {
          static_assert(kDependentTypeFalse<std::tuple<IntrinsicType, AssemblerType>>,
                        "Unknown parameter type, please add support to CallIntrinsic");
        }
      });
}

// Forward results from ABI registers to result-specified registers and mark registers in the
// returned StoredRegsInfo with kRegIsNotOnStack to prevent restoration from stack.
template <typename IntrinsicResType, typename AssemblerResType>
StoredRegsInfo ForwardResults(MacroAssembler<x86_64::Assembler>& as, AssemblerResType result) {
  using Assembler = MacroAssembler<x86_64::Assembler>;
  using Register = Assembler::Register;
  using XMMRegister = Assembler::XMMRegister;

  StoredRegsInfo regs_info = kRegOffsetsOnStack;

  if constexpr (Assembler::kFormatIs<IntrinsicResType, std::tuple<int32_t>, std::tuple<uint32_t>> &&
                std::is_same_v<AssemblerResType, std::tuple<Register>>) {
    // Note: even unsigned 32-bit results are sign-extended to 64bit register on RV64.
    regs_info.regs_on_stack[std::get<0>(result).GetPhysicalIndex()] = kRegIsNotOnStack;
    as.Expand<int64_t, int32_t>(std::get<0>(result), Assembler::rax);
  } else if constexpr (Assembler::
                           kFormatIs<IntrinsicResType, std::tuple<int64_t>, std::tuple<uint64_t>> &&
                       std::is_same_v<AssemblerResType, std::tuple<Register>>) {
    regs_info.regs_on_stack[std::get<0>(result).GetPhysicalIndex()] = kRegIsNotOnStack;
    as.Mov<int64_t>(std::get<0>(result), Assembler::rax);
  } else if constexpr (Assembler::
                           kFormatIs<IntrinsicResType, std::tuple<Float32>, std::tuple<Float64>> &&
                       std::is_same_v<AssemblerResType, std::tuple<XMMRegister>>) {
    using ResType0 = std::tuple_element_t<0, IntrinsicResType>;
    regs_info.simd_regs_on_stack[std::get<0>(result).GetPhysicalIndex()] = kRegIsNotOnStack;
    if (host_platform::kHasAVX) {
      as.Vmovs<ResType0>(std::get<0>(result), std::get<0>(result), Assembler::xmm0);
    } else {
      as.Movs<ResType0>(std::get<0>(result), Assembler::xmm0);
    }
  } else if constexpr (std::tuple_size_v<IntrinsicResType> == 2) {
    using ResType0 = std::tuple_element_t<0, IntrinsicResType>;
    using ResType1 = std::tuple_element_t<1, IntrinsicResType>;
    auto [result0, result1] = result;
    // Process rdx first because it can be equal to result0.
    if constexpr (Assembler::kFormatIs<ResType1, int64_t, uint64_t> &&
                  std::is_same_v<std::tuple_element_t<1, AssemblerResType>, Register>) {
      regs_info.regs_on_stack[result1.GetPhysicalIndex()] = kRegIsNotOnStack;
      as.Mov<int64_t>(result1, Assembler::rdx);
    } else {
      static_assert(kDependentTypeFalse<std::tuple<IntrinsicResType, AssemblerResType>>,
                    "Unknown result type, please add support to CallIntrinsic");
    }
    // Process rax now, it's not clobbered yet, because rax cannot be allocated.
    if constexpr (Assembler::kFormatIs<ResType0, int64_t, uint64_t> &&
                  std::is_same_v<std::tuple_element_t<0, AssemblerResType>, Register>) {
      regs_info.regs_on_stack[result0.GetPhysicalIndex()] = kRegIsNotOnStack;
      as.Mov<int64_t>(result0, Assembler::rax);
    } else {
      static_assert(kDependentTypeFalse<std::tuple<IntrinsicResType, AssemblerResType>>,
                    "Unknown result type, please add support to CallIntrinsic");
    }
  } else {
    static_assert(kDependentTypeFalse<std::tuple<IntrinsicResType, AssemblerResType>>,
                  "Unknown result type, please add support to CallIntrinsic");
  }
  return regs_info;
}

template <typename AssemblerResType,
          typename IntrinsicResType,
          typename... IntrinsicArgTypes,
          typename... AssemblerArgTypes>
void CallIntrinsic(MacroAssembler<x86_64::Assembler>& as,
                   IntrinsicResType (*function)(IntrinsicArgTypes...),
                   AssemblerResType result,
                   AssemblerArgTypes... args) {
  PushCallerSaved(as, kRegOffsetsOnStack);

  InitArgs<IntrinsicArgTypes...>(as, host_platform::kHasAVX, args...);

  as.Call(reinterpret_cast<void*>(function));

  if constexpr (std::is_same_v<IntrinsicResType, void>) {
    PopCallerSaved(as, kRegOffsetsOnStack);
  } else {
    auto regs_info = ForwardResults<IntrinsicResType>(as, result);

    PopCallerSaved(as, regs_info);
  }
}

}  // namespace berberis::call_intrinsic

#endif  // BERBERIS_LITE_TRANSLATOR_RISCV64_TO_X86_64_CALL_INTRINSIC_H_
