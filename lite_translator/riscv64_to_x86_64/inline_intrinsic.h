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

#ifndef BERBERIS_LITE_TRANSLATOR_RISCV64_TO_X86_64_INLINE_INTRINSIC_H_
#define BERBERIS_LITE_TRANSLATOR_RISCV64_TO_X86_64_INLINE_INTRINSIC_H_

#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

#include "berberis/assembler/x86_64.h"
#include "berberis/base/checks.h"
#include "berberis/base/dependent_false.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/intrinsics/guest_cpu_flags.h"
#include "berberis/intrinsics/intrinsics_process_bindings.h"
#include "berberis/intrinsics/macro_assembler.h"
#include "berberis/runtime_primitives/platform.h"

namespace berberis::inline_intrinsic {

template <auto kFunction,
          typename RegAlloc,
          typename SIMDRegAlloc,
          typename AssemblerResType,
          typename... AssemblerArgType>
bool TryInlineIntrinsic(MacroAssembler<x86_64::Assembler>& as,
                        RegAlloc&& reg_alloc,
                        SIMDRegAlloc&& simd_reg_alloc,
                        AssemblerResType result,
                        AssemblerArgType... args);

template <auto kFunc>
class InlineIntrinsic {
 public:
  template <typename RegAlloc, typename SIMDRegAlloc, typename ResType, typename... ArgType>
  static bool TryInlineWithHostRounding(MacroAssembler<x86_64::Assembler>& as,
                                        RegAlloc&& reg_alloc,
                                        SIMDRegAlloc&& simd_reg_alloc,
                                        ResType result,
                                        ArgType... args) {
    std::tuple args_tuple = std::make_tuple(args...);
    if constexpr (IsTagEq<&intrinsics::FMul<Float64>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FMulHostRounding<Float64>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FMul<Float32>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FMulHostRounding<Float32>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FAdd<Float64>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FAddHostRounding<Float64>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FAdd<Float32>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FAddHostRounding<Float32>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FSub<Float64>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FSubHostRounding<Float64>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FSub<Float32>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FSubHostRounding<Float32>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FDiv<Float64>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FDivHostRounding<Float64>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FDiv<Float32>>) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FDivHostRounding<Float32>>(
          as, reg_alloc, simd_reg_alloc, result, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int64_t, Float64>>) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FCvtFloatToIntegerHostRounding<int64_t, Float64>>(
          as, reg_alloc, simd_reg_alloc, result, src);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int64_t, Float32>>) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FCvtFloatToIntegerHostRounding<int64_t, Float32>>(
          as, reg_alloc, simd_reg_alloc, result, src);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int32_t, Float64>>) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FCvtFloatToIntegerHostRounding<int32_t, Float64>>(
          as, reg_alloc, simd_reg_alloc, result, src);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int32_t, Float32>>) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsic<&intrinsics::FCvtFloatToIntegerHostRounding<int32_t, Float32>>(
          as, reg_alloc, simd_reg_alloc, result, src);
    }
    return false;
  }

 private:
  template <auto kFunction>
  class FunctionCompareTag;

  template <auto kOtherFunction>
  static constexpr bool IsTagEq =
      std::is_same_v<FunctionCompareTag<kFunc>, FunctionCompareTag<kOtherFunction>>;
};

template <typename format, typename DestType, typename SrcType>
auto Mov(MacroAssembler<x86_64::Assembler>& as, DestType dest, SrcType src)
    -> decltype(std::declval<MacroAssembler<x86_64::Assembler>>()
                    .Mov<format>(std::declval<DestType>(), std::declval<SrcType>())) {
  if constexpr (std::is_integral_v<format>) {
    return as.template Mov<format>(dest, src);
  } else if (host_platform::kHasAVX) {
    return as.template Vmov<format>(dest, src);
  } else {
    return as.template Mov<format>(dest, src);
  }
}

template <typename format, typename DestType, typename SrcType>
auto Mov(MacroAssembler<x86_64::Assembler>& as, DestType dest, SrcType src)
    -> decltype(std::declval<MacroAssembler<x86_64::Assembler>>()
                    .Movs<format>(std::declval<DestType>(), std::declval<SrcType>())) {
  if (host_platform::kHasAVX) {
    if constexpr (std::is_same_v<DestType, MacroAssembler<x86_64::Assembler>::XMMRegister> &&
                  std::is_same_v<SrcType, MacroAssembler<x86_64::Assembler>::XMMRegister>) {
      return as.template Vmovs<format>(dest, dest, src);
    } else {
      return as.template Vmovs<format>(dest, src);
    }
  } else {
    return as.template Movs<format>(dest, src);
  }
}

template <auto kFunction,
          typename RegAlloc,
          typename SIMDRegAlloc,
          typename AssemblerResType,
          typename... AssemblerArgType>
class TryBindingBasedInlineIntrinsic {
  template <auto kFunctionForFriend,
            typename RegAllocForFriend,
            typename SIMDRegAllocForFriend,
            typename AssemblerResTypeForFriend,
            typename... AssemblerArgTypeForFriend>
  friend bool TryInlineIntrinsic(MacroAssembler<x86_64::Assembler>& as,
                                 RegAllocForFriend&& reg_alloc,
                                 SIMDRegAllocForFriend&& simd_reg_alloc,
                                 AssemblerResTypeForFriend result,
                                 AssemblerArgTypeForFriend... args);
  template <auto kFunc,
            typename MacroAssembler,
            typename Result,
            typename Callback,
            typename... Args>
  friend constexpr Result x86_64::intrinsics::bindings::ProcessBindings(Callback callback,
                                                                        Result def_result,
                                                                        Args&&... args);
  template <StringLiteral kIntrinsic, typename... Types>
  friend class intrinsics::bindings::IntrinsicBindingInfo;

  TryBindingBasedInlineIntrinsic() = delete;
  TryBindingBasedInlineIntrinsic(const TryBindingBasedInlineIntrinsic&) = delete;
  TryBindingBasedInlineIntrinsic(TryBindingBasedInlineIntrinsic&&) = default;
  TryBindingBasedInlineIntrinsic& operator=(const TryBindingBasedInlineIntrinsic&) = delete;
  TryBindingBasedInlineIntrinsic& operator=(TryBindingBasedInlineIntrinsic&&) = default;

  TryBindingBasedInlineIntrinsic(MacroAssembler<x86_64::Assembler>& as,
                                 RegAlloc& reg_alloc,
                                 SIMDRegAlloc& simd_reg_alloc,
                                 AssemblerResType result,
                                 AssemblerArgType... args)
      : as_(as),
        reg_alloc_(reg_alloc),
        simd_reg_alloc_(simd_reg_alloc),
        result_{result},
        input_args_(std::tuple{args...}),
        success_(intrinsics::bindings::ProcessBindings<
                 kFunction,
                 typename MacroAssembler<x86_64::Assembler>::Assemblers,
                 bool,
                 TryBindingBasedInlineIntrinsic&>(*this, false)) {}
  operator bool() { return success_; }

  template <typename IntrinsicBindingInfo>
  std::optional<bool> /*ProcessBindingsClient*/ operator()(IntrinsicBindingInfo asm_call_info) {
    static_assert(
        std::is_same_v<decltype(kFunction), typename IntrinsicBindingInfo::IntrinsicType>);
    static_assert(std::is_same_v<typename IntrinsicBindingInfo::PreciseNanOperationsHandling,
                                 intrinsics::bindings::NoNansOperation>);
    using CPUIDRestriction = IntrinsicBindingInfo::CPUIDRestriction;
    if constexpr (std::is_same_v<CPUIDRestriction, x86_32_or_x86_64::device_arch_info::HasAVX>) {
      if (!host_platform::kHasAVX) {
        return {};
      }
    } else if constexpr (std::is_same_v<CPUIDRestriction,
                                        x86_32_or_x86_64::device_arch_info::HasAVX2>) {
      if (!host_platform::kHasAVX2) {
        return {};
      }
    } else if constexpr (std::is_same_v<CPUIDRestriction,
                                        x86_32_or_x86_64::device_arch_info::HasBMI>) {
      if (!host_platform::kHasBMI) {
        return {};
      }
    } else if constexpr (std::is_same_v<CPUIDRestriction,
                                        x86_32_or_x86_64::device_arch_info::HasFMA>) {
      if (!host_platform::kHasFMA) {
        return {};
      }
    } else if constexpr (std::is_same_v<CPUIDRestriction,
                                        x86_32_or_x86_64::device_arch_info::HasLZCNT>) {
      if (!host_platform::kHasLZCNT) {
        return {};
      }
    } else if constexpr (std::is_same_v<CPUIDRestriction,
                                        x86_32_or_x86_64::device_arch_info::HasPOPCNT>) {
      if (!host_platform::kHasPOPCNT) {
        return {};
      }
    } else if constexpr (std::is_same_v<CPUIDRestriction,
                                        x86_32_or_x86_64::device_arch_info::NoCPUIDRestriction>) {
      // No restrictions. Do nothing.
    } else {
      static_assert(kDependentValueFalse<IntrinsicBindingInfo::kCPUIDRestriction>);
    }

    // 8086 has pretty convoluted ISA where each of its eight “general purpose” registers have
    // special role. Thankfully 80386 made ISA more uniform but even today there are instructions
    // that can only use Accumulator Register ('a' register), Base Register ('b' register), Counter
    // Register ('c' register) or Data Register ('d' register).
    // The most “popular” are Accumulator Register ('a' register; there are many instructions that
    // can only use accumulator and nothing else) and Counter Register ('c' register; all shifts can
    // only use Counter Register or immediate) – we never allocate these in lite translator thus
    // they are always available for the use in intrinsics.
    // Two other registers, Base Register ('b' register) and Data Register ('d' register) can be
    // allocated but they are never used in the same intrinsic as Counter Register ('c' register).
    // This allows us to do the trick: use Counter Register ('c' register) to temporarily hold the
    // information from Base Register ('b' register) or Data Register ('d' register) if intrinsic
    // clobbers them.
    // Plus we should not do that if either of these two register are used to return the results.

    // Verify that we don't need more than one register out of these three.
    static_assert(!kUntouched<IntrinsicBindingInfo, 'b'> + !kUntouched<IntrinsicBindingInfo, 'c'> +
                      !kUntouched<IntrinsicBindingInfo, 'd'> <=
                  1);
    const bool kRenameRbxInputToRcx = !kUntouched<IntrinsicBindingInfo, 'b'> &&
                                      ValuesToValues::Contains(input_args_, x86_64::Assembler::rbx);
    const bool kSaveRestoreRbxUsingRcx =
        !(kUntouched<IntrinsicBindingInfo, 'b'> ||
          ValuesToValues::Contains(result_, x86_64::Assembler::rbx));
    const bool kRenameRdxInputToRcx = !kUntouched<IntrinsicBindingInfo, 'd'> &&
                                      ValuesToValues::Contains(input_args_, x86_64::Assembler::rdx);
    const bool kSaveRestoreRdxUsingRcx =
        !(kUntouched<IntrinsicBindingInfo, 'd'> ||
          ValuesToValues::Contains(result_, x86_64::Assembler::rdx));

    if (kRenameRbxInputToRcx || kSaveRestoreRbxUsingRcx) {
      as_.Movq(x86_64::Assembler::rcx, x86_64::Assembler::rbx);
    }
    if (kRenameRdxInputToRcx || kSaveRestoreRdxUsingRcx) {
      as_.Movq(x86_64::Assembler::rcx, x86_64::Assembler::rdx);
    }

    x86_64::Assembler::XMMRegister xmm_for_gp_result = x86_64::Assembler::no_xmm_register;
    uint32_t scratch_arg = 0;

    std::apply(
        IntrinsicBindingInfo::kEmitInsnFunc,
        std::tuple_cat(std::tuple<MacroAssembler<x86_64::Assembler>&>{as_},
                       TypesToValues::FlatMap<typename IntrinsicBindingInfo::OperandsAndBindings>(
                           *this,
                           xmm_for_gp_result,
                           scratch_arg,
                           asm_call_info,
                           kRenameRbxInputToRcx,
                           kRenameRdxInputToRcx)));

    TypesToValues::ForEach<typename IntrinsicBindingInfo::OperandsAndBindings>(
        [&xmm_for_gp_result, this]<typename BindingInfo>() -> decltype(auto) {
          using Operand = std::tuple_element_t<0, BindingInfo>;
          using Binding = std::tuple_element_t<1, BindingInfo>;
          if constexpr (HaveOutput(Binding::kArgInfo)) {
            using ReturnType = std::tuple_element_t<Binding::kArgInfo.to,
                                                    typename IntrinsicBindingInfo::OutputArguments>;
            auto reg = std::get<Binding::kArgInfo.to>(result_);
            if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
              static_assert(std::is_integral_v<ReturnType>);
              constexpr auto kRegister =
                  std::tuple_element_t<0, typename Operand::Class::RegistersList>::
                      template kMachineRegId<x86_64::Assembler::Registers>;
              CHECK_EQ(xmm_for_gp_result, x86_64::Assembler::no_xmm_register);
              // Intrinsic should do signed extension on 32-bit return types.
              if constexpr (sizeof(ReturnType) == sizeof(int32_t)) {
                as_.template Expand<int64_t, int32_t>(reg, kRegister);
              } else {
                static_assert(sizeof(ReturnType) == sizeof(int64_t));
                as_.Movq(reg, kRegister);
              }
            } else if constexpr (Operand::Class::kAsRegister == 'x' &&
                                 std::is_integral_v<ReturnType>) {
              CHECK_NE(xmm_for_gp_result, x86_64::Assembler::no_xmm_register);
              Mov<typename TypeTraits<ReturnType>::Float>(as_, reg, xmm_for_gp_result);
              // Intrinsic should do signed extension on 32-bit return types.
            } else if constexpr (std::is_integral_v<ReturnType> &&
                                 sizeof(ReturnType) == sizeof(uint32_t)) {
              CHECK_EQ(xmm_for_gp_result, x86_64::Assembler::no_xmm_register);
              as_.template Expand<int64_t, int32_t>(reg, reg);
            } else if constexpr (std::is_integral_v<ReturnType>) {
              static_assert(sizeof(ReturnType) == sizeof(int64_t));
              CHECK_EQ(xmm_for_gp_result, x86_64::Assembler::no_xmm_register);
            }
          }
        });

    if (kSaveRestoreRdxUsingRcx) {
      as_.Movq(x86_64::Assembler::rdx, x86_64::Assembler::rcx);
    }
    if (kSaveRestoreRbxUsingRcx) {
      as_.Movq(x86_64::Assembler::rbx, x86_64::Assembler::rcx);
    }

    return {true};
  }

  template <typename BindingInfo, typename IntrinsicBindingInfo>
  auto /*FlatMapClient*/ operator()(x86_64::Assembler::XMMRegister& xmm_for_gp_result,
                                    uint32_t& scratch_arg,
                                    IntrinsicBindingInfo,
                                    bool kRenameRbxInputToRcx,
                                    bool kRenameRdxInputToRcx) {
    using Operand = std::tuple_element_t<0, BindingInfo>;
    using Binding = std::tuple_element_t<1, BindingInfo>;
    if constexpr (Binding::kArgInfo.arg_type == ArgInfo::IMM_ARG) {
      return std::tuple{std::get<Binding::kArgInfo.from>(input_args_)};
    } else {
      using RegisterClass = typename Operand::Class;
      static constexpr auto kUsage = Operand::kUsage;
      if constexpr (Binding::kArgInfo.arg_type == ArgInfo::IN_ARG ||
                    Binding::kArgInfo.arg_type == ArgInfo::IN_TMP_ARG) {
        static_assert(kUsage == device_arch_info::kUse || kUsage == device_arch_info::kUseDef);
        auto arg = std::get<Binding::kArgInfo.from>(input_args_);
        using Type = std::tuple_element_t<Binding::kArgInfo.from,
                                          typename IntrinsicBindingInfo::InputArguments>;
        if constexpr (RegisterClass::kAsRegister == 'x' && std::is_integral_v<Type>) {
          static_assert(!device_arch_info::kIsImplicitReg<Operand>);
          auto reg = simd_reg_alloc_();
          Mov<typename TypeTraits<int64_t>::Float>(as_, reg, arg);
          return std::tuple{reg};
        } else if constexpr (RegisterClass::kAsRegister == 'x') {
          static_assert(!device_arch_info::kIsImplicitReg<Operand>);
          if constexpr (kUsage == device_arch_info::kUse) {
            return std::tuple{arg};
          } else {
            static_assert(!device_arch_info::kIsImplicitReg<Operand>);
            auto reg = simd_reg_alloc_();
            Mov<std::tuple_element_t<Binding::kArgInfo.from,
                                     typename IntrinsicBindingInfo::InputArguments>>(as_, reg, arg);
            return std::tuple{reg};
          }
        } else if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
          constexpr auto kRegister =
              std::tuple_element_t<0, typename Operand::Class::RegistersList>::
                  template kMachineRegId<x86_64::Assembler::Registers>;
          if (arg != kRegister) {
            as_.template Mov<std::tuple_element_t<Binding::kArgInfo.from,
                                                  typename IntrinsicBindingInfo::InputArguments>>(
                kRegister, arg);
          }
          return std::tuple{};
        } else if (kUsage == device_arch_info::kUseDef) {
          auto reg = reg_alloc_();
          Mov<std::tuple_element_t<Binding::kArgInfo.from,
                                   typename IntrinsicBindingInfo::InputArguments>>(as_, reg, arg);
          return std::tuple{reg};
        } else if ((kRenameRbxInputToRcx && arg == x86_64::Assembler::rbx) ||
                   (kRenameRdxInputToRcx && arg == x86_64::Assembler::rdx)) {
          return std::tuple{x86_64::Assembler::rcx};
        } else {
          return std::tuple{arg};
        }
      } else if constexpr (Binding::kArgInfo.arg_type == ArgInfo::IN_OUT_ARG ||
                           Binding::kArgInfo.arg_type == ArgInfo::IN_OUT_TMP_ARG) {
        using Type = std::tuple_element_t<Binding::kArgInfo.from,
                                          typename IntrinsicBindingInfo::InputArguments>;
        static_assert(kUsage == device_arch_info::kUseDef);
        if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
          static_assert(RegisterClass::kAsRegister == 'a');
          Mov<Type>(as_, as_.rax, std::get<Binding::kArgInfo.from>(input_args_));
          return std::tuple{};
        } else {
          auto arg = std::get<Binding::kArgInfo.from>(input_args_);
          if constexpr (RegisterClass::kAsRegister == 'x' && std::is_integral_v<Type>) {
            static_assert(std::is_integral_v<
                          std::tuple_element_t<Binding::kArgInfo.to,
                                               typename IntrinsicBindingInfo::OutputArguments>>);
            CHECK_EQ(xmm_for_gp_result, x86_64::Assembler::no_xmm_register);
            xmm_for_gp_result = simd_reg_alloc_();
            Mov<typename TypeTraits<int64_t>::Float>(as_, xmm_for_gp_result, arg);
            return std::tuple{xmm_for_gp_result};
          } else {
            auto reg = std::get<Binding::kArgInfo.to>(result_);
            Mov<std::tuple_element_t<Binding::kArgInfo.from,
                                     typename IntrinsicBindingInfo::InputArguments>>(as_, reg, arg);
            return std::tuple{reg};
          }
        }
      } else if constexpr (Binding::kArgInfo.arg_type == ArgInfo::OUT_ARG) {
        static_assert(kUsage == device_arch_info::kDef ||
                      kUsage == device_arch_info::kDefEarlyClobber);
        using Type = std::tuple_element_t<Binding::kArgInfo.to,
                                          typename IntrinsicBindingInfo::OutputArguments>;
        if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
          return std::tuple{};
        } else if constexpr (RegisterClass::kAsRegister == 'x' && std::is_integral_v<Type>) {
          CHECK_EQ(xmm_for_gp_result, x86_64::Assembler::no_xmm_register);
          xmm_for_gp_result = simd_reg_alloc_();
          return std::tuple{xmm_for_gp_result};
        } else {
          return std::tuple{std::get<Binding::kArgInfo.to>(result_)};
        }
      } else if constexpr (Binding::kArgInfo.arg_type == ArgInfo::OUT_TMP_ARG) {
        static_assert(device_arch_info::kIsImplicitReg<Operand>);
        return std::tuple{};
      } else if constexpr (Binding::kArgInfo.arg_type == ArgInfo::TMP_ARG) {
        static_assert(kUsage == device_arch_info::kDef ||
                      kUsage == device_arch_info::kDefEarlyClobber);
        if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          if (scratch_arg >= config::kScratchAreaSize / config::kScratchAreaSlotSize) {
            FATAL("Only two scratch registers are supported for now");
          }
          return std::tuple{x86_64::Assembler::Operand{
              .base = as_.rbp,
              .disp = static_cast<int>(offsetof(ThreadState, intrinsics_scratch_area) +
                                       config::kScratchAreaSlotSize * scratch_arg++)}};
        } else if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
          return std::tuple{};
        } else if constexpr (RegisterClass::kAsRegister == 'x') {
          return std::tuple{simd_reg_alloc_()};
        } else {
          return std::tuple{reg_alloc_()};
        }
      }
    }
  }

  template <typename IntrinsicBindingInfo, char kAsRegister>
  static constexpr bool kUntouched =
      TypesToValues::All<typename IntrinsicBindingInfo::Operands>([]<typename Operand>() {
        // Immediate argument doesn't change anything and FLAGS is distinct from other registers,
        // but we must check them first, otherwise access to Operand::Class::kAsRegister would fail
        // to compile.
        if constexpr (device_arch_info::kIsImmediate<Operand> ||
                      device_arch_info::kIsFLAGS<Operand>) {
          return true;
        } else {
          return Operand::Class::kAsRegister != kAsRegister;
        }
      });

 private:
  friend class berberis::TypesToValues;
  MacroAssembler<x86_64::Assembler>& as_;
  RegAlloc& reg_alloc_;
  SIMDRegAlloc& simd_reg_alloc_;
  AssemblerResType result_;
  std::tuple<AssemblerArgType...> input_args_;
  bool success_;
};

template <auto kFunction,
          typename RegAlloc,
          typename SIMDRegAlloc,
          typename AssemblerResType,
          typename... AssemblerArgType>
bool TryInlineIntrinsic(MacroAssembler<x86_64::Assembler>& as,
                        RegAlloc&& reg_alloc,
                        SIMDRegAlloc&& simd_reg_alloc,
                        AssemblerResType result,
                        AssemblerArgType... args) {
  if (InlineIntrinsic<kFunction>::TryInlineWithHostRounding(
          as, reg_alloc, simd_reg_alloc, result, args...)) {
    return true;
  }

  return TryBindingBasedInlineIntrinsic<kFunction,
                                        RegAlloc,
                                        SIMDRegAlloc,
                                        AssemblerResType,
                                        AssemblerArgType...>(
      as, reg_alloc, simd_reg_alloc, result, args...);
}

}  // namespace berberis::inline_intrinsic

#endif  // BERBERIS_LITE_TRANSLATOR_RISCV64_TO_X86_64_CALL_INTRINSIC_H_
