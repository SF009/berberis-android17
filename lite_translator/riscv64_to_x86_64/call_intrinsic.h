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
#include "berberis/base/dependent_false.h"
#include "berberis/intrinsics/macro_assembler.h"
#include "berberis/runtime_primitives/platform.h"

namespace berberis::call_intrinsic {

constexpr int8_t kRegIsNotOnStack = -1;

constexpr x86_64::Assembler::Register kCallerSavedRegs[] = {
    x86_64::Assembler::rax,
    x86_64::Assembler::rcx,
    x86_64::Assembler::rdx,
    x86_64::Assembler::rdi,
    x86_64::Assembler::rsi,
    x86_64::Assembler::r8,
    x86_64::Assembler::r9,
    x86_64::Assembler::r10,
    x86_64::Assembler::r11,
};

constexpr x86_64::Assembler::XMMRegister kCallerSavedXMMRegs[] = {
    x86_64::Assembler::xmm0,
    x86_64::Assembler::xmm1,
    x86_64::Assembler::xmm2,
    x86_64::Assembler::xmm3,
    x86_64::Assembler::xmm4,
    x86_64::Assembler::xmm5,
    x86_64::Assembler::xmm6,
    x86_64::Assembler::xmm7,
    x86_64::Assembler::xmm8,
    x86_64::Assembler::xmm9,
    x86_64::Assembler::xmm10,
    x86_64::Assembler::xmm11,
    x86_64::Assembler::xmm12,
    x86_64::Assembler::xmm13,
    x86_64::Assembler::xmm14,
    x86_64::Assembler::xmm15,
};

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
  for (auto reg : kCallerSavedRegs) {
    result.regs_on_stack[reg.GetPhysicalIndex()] = result.save_area_size;
    ++result.save_area_size;
  }

  result.save_area_size = AlignUp(result.save_area_size, 2);
  for (auto reg : kCallerSavedXMMRegs) {
    result.simd_regs_on_stack[reg.GetPhysicalIndex()] = result.save_area_size;
    result.save_area_size += 2;
  }
  return result;
}();

// Save area size for CallIntrinsic save area. Counted in 8-byte slots.

inline void PushCallerSaved(MacroAssembler<x86_64::Assembler>& as, const StoredRegsInfo regs_info) {
  as.Subq(as.rsp, regs_info.save_area_size * 8);

  for (auto reg : kCallerSavedRegs) {
    as.Movq({.base = as.rsp, .disp = regs_info.regs_on_stack[reg.GetPhysicalIndex()] * 8}, reg);
  }

  for (auto reg : kCallerSavedXMMRegs) {
    as.Movdqa({.base = as.rsp, .disp = regs_info.simd_regs_on_stack[reg.GetPhysicalIndex()] * 8},
              reg);
  }
}

// Note: regs_on_stack is usually copy of kRegOffsetsOnStack with some registers marked off as
// kRegIsNotOnStack, simd_regs_on_stack is kSimdRegOffsetsOnStack with some registers marked as
// kRegIsNotOnStack. These registers are skipped during restoration process.
inline void PopCallerSaved(MacroAssembler<x86_64::Assembler>& as, const StoredRegsInfo regs_info) {
  for (auto reg : kCallerSavedRegs) {
    if (regs_info.regs_on_stack[reg.GetPhysicalIndex()] != kRegIsNotOnStack) {
      as.Movq(reg, {.base = as.rsp, .disp = regs_info.regs_on_stack[reg.GetPhysicalIndex()] * 8});
    }
  }
  for (auto reg : kCallerSavedXMMRegs) {
    if (regs_info.simd_regs_on_stack[reg.GetPhysicalIndex()] != kRegIsNotOnStack) {
      as.Movdqa(reg,
                {.base = as.rsp, .disp = regs_info.simd_regs_on_stack[reg.GetPhysicalIndex()] * 8});
    }
  }

  as.Addq(as.rsp, regs_info.save_area_size * 8);
}

static constexpr x86_64::Assembler::Register kAbiArgs[] = {
    x86_64::Assembler::rdi,
    x86_64::Assembler::rsi,
    x86_64::Assembler::rdx,
    x86_64::Assembler::rcx,
    x86_64::Assembler::r8,
    x86_64::Assembler::r9,
};

static constexpr x86_64::Assembler::XMMRegister kAbiSimdArgs[] = {
    x86_64::Assembler::xmm0,
    x86_64::Assembler::xmm1,
    x86_64::Assembler::xmm2,
    x86_64::Assembler::xmm3,
    x86_64::Assembler::xmm4,
    x86_64::Assembler::xmm5,
    x86_64::Assembler::xmm6,
    x86_64::Assembler::xmm7,
};

// Assumes RSP points to preallocated stack args area.
template <typename... IntrinsicArgType, typename MacroAssembler, typename... AssemblerArgType>
void InitArgs(MacroAssembler&& as, bool has_avx, AssemblerArgType... args) {
  using Assembler = std::decay_t<MacroAssembler>;
  using Register = typename Assembler::Register;
  using XMMRegister = typename Assembler::XMMRegister;

  // All ABI argument registers are saved among caller-saved registers, so we can safely initialize
  // them now. When intrinsic receives its argument from such register we'll read it from stack, so
  // there is no early-clobbering problem. Callee-saved regs are never ABI arguments, so we can move
  // them to ABI reg directly.

  constexpr std::size_t kIntArgs =
      TypesToTypes::CountIf<std::tuple<IntrinsicArgType...>, []<typename IntrinsicType>() {
        return kMetaType<IntrinsicType>.IsIntegral();
      }>{};
  static_assert(kIntArgs < std::size(kAbiArgs));
  constexpr std::size_t kSimdArgs =
      TypesToTypes::CountIf<std::tuple<IntrinsicArgType...>, []<typename IntrinsicType>() {
        return kMetaType<IntrinsicType>.IsWrappedFloat();
      }>{};
  static_assert(kSimdArgs < std::size(kAbiSimdArgs));
  ValuesToValues::ForEachWithTemporary</* gp_index, simd_index = */ std::pair<size_t, size_t>>(
      ValuesToValues::Zip(std::tuple{kMetaType<IntrinsicArgType>...}, std::tuple{args...}),
      [&as, has_avx]<typename AssemblerType, typename IntrinsicType>(
          std::pair<MetaType<IntrinsicType>, AssemblerType>&& arg_info,
          std::pair<size_t, size_t>& indexes) {
        auto [_, arg] = arg_info;
        auto& [gp_index, simd_index] = indexes;

        if constexpr (MetaType<IntrinsicType>::IsIntegral()) {
          // Note, clang errorneously mandates extension up to 32-bit.
          // See: https://github.com/llvm/llvm-project/issues/43573
          if constexpr (MetaType<IntrinsicType>::SizeOf() <= sizeof(int32_t) &&
                        std::is_integral_v<AssemblerType> &&
                        sizeof(AssemblerType) <= sizeof(int32_t)) {
            as.Movl(kAbiArgs[gp_index], static_cast<int32_t>(arg));
          } else if constexpr (MetaType<IntrinsicType>::SizeOf() == sizeof(int64_t) &&
                               std::is_integral_v<AssemblerType> &&
                               sizeof(AssemblerType) == sizeof(int64_t)) {
            as.template Expand<int64_t, IntrinsicType>(kAbiArgs[gp_index],
                                                       static_cast<int64_t>(arg));
            // Note, clang errorneously mandates extension up to 32-bit.
            // See: https://github.com/llvm/llvm-project/issues/43573
          } else if constexpr (MetaType<IntrinsicType>::SizeOf() <= sizeof(int32_t) &&
                               std::is_same_v<AssemblerType, Register>) {
            if (kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] == kRegIsNotOnStack) {
              as.template Expand<int32_t, IntrinsicType>(kAbiArgs[gp_index], arg);
            } else {
              as.template Expand<int32_t, IntrinsicType>(
                  kAbiArgs[gp_index],
                  {.base = Assembler::rsp,
                   .disp = kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] * 8});
            }
          } else if constexpr (MetaType<IntrinsicType>::SizeOf() == sizeof(int64_t) &&
                               std::is_same_v<AssemblerType, Register>) {
            if (kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] == kRegIsNotOnStack) {
              as.template Expand<int64_t, IntrinsicType>(kAbiArgs[gp_index], arg);
            } else {
              as.template Expand<int64_t, IntrinsicType>(
                  kAbiArgs[gp_index],
                  {.base = Assembler::rsp,
                   .disp = kRegOffsetsOnStack.regs_on_stack[arg.GetPhysicalIndex()] * 8});
            }
          } else {
            static_assert(kDependentTypeFalse<std::tuple<IntrinsicType, AssemblerType>>,
                          "Unknown parameter type, please add support to CallIntrinsic");
          }
          ++gp_index;
        } else if constexpr (MetaType<IntrinsicType>::IsWrappedFloat()) {
          if constexpr (std::is_same_v<AssemblerType, XMMRegister>) {
            if (kRegOffsetsOnStack.simd_regs_on_stack[arg.GetPhysicalIndex()] == kRegIsNotOnStack) {
              if (has_avx) {
                as.template Vmovs<IntrinsicType>(
                    kAbiSimdArgs[simd_index], kAbiSimdArgs[simd_index], arg);
              } else {
                as.template Movs<IntrinsicType>(kAbiSimdArgs[simd_index], arg);
              }
            } else {
              if (has_avx) {
                as.template Vmovs<IntrinsicType>(
                    kAbiSimdArgs[simd_index],
                    {.base = as.rsp,
                     .disp = kRegOffsetsOnStack.simd_regs_on_stack[arg.GetPhysicalIndex()] * 8});
              } else {
                as.template Movs<IntrinsicType>(
                    kAbiSimdArgs[simd_index],
                    {.base = as.rsp,
                     .disp = kRegOffsetsOnStack.simd_regs_on_stack[arg.GetPhysicalIndex()] * 8});
              }
            }
          } else {
            static_assert(kDependentTypeFalse<std::tuple<IntrinsicType, AssemblerType>>,
                          "Unknown parameter type, please add support to CallIntrinsic");
          }
          ++simd_index;
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
          typename... IntrinsicArgType,
          typename... AssemblerArgType>
void CallIntrinsic(MacroAssembler<x86_64::Assembler>& as,
                   IntrinsicResType (*function)(IntrinsicArgType...),
                   AssemblerResType result,
                   AssemblerArgType... args) {
  PushCallerSaved(as, kRegOffsetsOnStack);

  InitArgs<IntrinsicArgType...>(as, host_platform::kHasAVX, args...);

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
