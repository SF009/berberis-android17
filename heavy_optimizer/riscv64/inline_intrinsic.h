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

#ifndef BERBERIS_HEAVY_OPTIMIZER_RISCV64_INLINE_INTRINSIC_H_
#define BERBERIS_HEAVY_OPTIMIZER_RISCV64_INLINE_INTRINSIC_H_

#include <cfenv>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "berberis/assembler/x86_64.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/base/checks.h"
#include "berberis/base/config.h"
#include "berberis/base/dependent_false.h"
#include "berberis/intrinsics/intrinsics.h"
#include "berberis/intrinsics/intrinsics_args.h"
#include "berberis/intrinsics/intrinsics_bindings.h"
#include "berberis/intrinsics/intrinsics_process_bindings.h"
#include "berberis/intrinsics/macro_assembler.h"
#include "berberis/runtime_primitives/platform.h"

namespace berberis {

template <auto kFunction, typename ResType, typename FlagRegister, typename... ArgType>
bool TryInlineIntrinsicForHeavyOptimizer(x86_64::MachineIRBuilder* builder,
                                         ResType& result,
                                         FlagRegister flag_register,
                                         ArgType... args);

template <auto kFunc>
class InlineIntrinsic {
 public:
  template <typename ResType, typename FlagRegister, typename... ArgType>
  static bool TryInlineWithHostRounding(x86_64::MachineIRBuilder* builder,
                                        ResType& result,
                                        FlagRegister flag_register,
                                        ArgType... args) {
    std::tuple args_tuple = std::make_tuple(args...);
    if constexpr (IsTagEq<&intrinsics::FMul<Float64>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FMulHostRounding<Float64>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FMul<Float32>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FMulHostRounding<Float32>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FAdd<Float64>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FAddHostRounding<Float64>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FAdd<Float32>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FAddHostRounding<Float32>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FSub<Float64>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FSubHostRounding<Float64>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FSub<Float32>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FSubHostRounding<Float32>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FDiv<Float64>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FDivHostRounding<Float64>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FDiv<Float32>>()) {
      auto [rm, frm, src1, src2] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<&intrinsics::FDivHostRounding<Float32>>(
          builder, result, flag_register, src1, src2);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int64_t, Float64>>()) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<
          &intrinsics::FCvtFloatToIntegerHostRounding<int64_t, Float64>>(
          builder, result, flag_register, src);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int64_t, Float32>>()) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<
          &intrinsics::FCvtFloatToIntegerHostRounding<int64_t, Float32>>(
          builder, result, flag_register, src);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int32_t, Float64>>()) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<
          &intrinsics::FCvtFloatToIntegerHostRounding<int32_t, Float64>>(
          builder, result, flag_register, src);
    } else if constexpr (IsTagEq<&intrinsics::FCvtFloatToInteger<int32_t, Float32>>()) {
      auto [rm, frm, src] = args_tuple;
      if (rm != FPFlags::DYN) {
        return false;
      }
      return TryInlineIntrinsicForHeavyOptimizer<
          &intrinsics::FCvtFloatToIntegerHostRounding<int32_t, Float32>>(
          builder, result, flag_register, src);
    }
    return false;
  }

 private:
  // Comparison of pointers which point to different functions is generally not a
  // constexpr since such functions can be merged in object code (comparing
  // pointers to the same function is constexpr). This helper compares them using
  // templates explicitly telling that we are not worried about such subtleties here.
  template <auto kFunction>
  class FunctionCompareTag;

  // Note, if we define it as a variable clang doesn't consider it a constexpr in TryInline funcs.
  template <auto kOtherFunction>
  static constexpr bool IsTagEq() {
    return std::is_same_v<FunctionCompareTag<kFunc>, FunctionCompareTag<kOtherFunction>>;
  }
};

template <auto kFunction, typename ResType, typename FlagRegister, typename... ArgType>
class TryBindingBasedInlineIntrinsicForHeavyOptimizer {
  template <auto kFunctionForFriend,
            typename ResTypeForFriend,
            typename FlagRegisterForFriend,
            typename... ArgTypeForFriend>
  friend bool TryInlineIntrinsicForHeavyOptimizer(x86_64::MachineIRBuilder* builder,
                                                  ResTypeForFriend& result,
                                                  FlagRegisterForFriend flag_register,
                                                  ArgTypeForFriend... args);
  template <auto kFunctionForFriend, typename FlagRegisterForFriend, typename... ArgTypeForFriend>
  friend bool TryInlineIntrinsicForHeavyOptimizerVoid(x86_64::MachineIRBuilder* builder,
                                                      FlagRegisterForFriend flag_register,
                                                      ArgTypeForFriend... args);

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

  TryBindingBasedInlineIntrinsicForHeavyOptimizer() = delete;
  TryBindingBasedInlineIntrinsicForHeavyOptimizer(
      const TryBindingBasedInlineIntrinsicForHeavyOptimizer&) = delete;
  TryBindingBasedInlineIntrinsicForHeavyOptimizer(
      TryBindingBasedInlineIntrinsicForHeavyOptimizer&&) = delete;
  TryBindingBasedInlineIntrinsicForHeavyOptimizer& operator=(
      const TryBindingBasedInlineIntrinsicForHeavyOptimizer&) = delete;
  TryBindingBasedInlineIntrinsicForHeavyOptimizer& operator=(
      TryBindingBasedInlineIntrinsicForHeavyOptimizer&&) = delete;

  TryBindingBasedInlineIntrinsicForHeavyOptimizer(x86_64::MachineIRBuilder* builder,
                                                  ResType& result,
                                                  FlagRegister flag_register,
                                                  ArgType... args)
      : builder_(builder),
        result_{result},
        flag_register_{flag_register},
        input_args_(std::tuple{args...}),
        success_(intrinsics::bindings::ProcessBindings<
                 kFunction,
                 typename MacroAssembler<x86_64::Assembler>::Assemblers,
                 bool,
                 TryBindingBasedInlineIntrinsicForHeavyOptimizer&>(*this, false)) {}

  operator bool() { return success_; }

  // TODO(b/232598137) The MachineIR bindings for some macros can't be instantiated yet. This should
  // be removed once they're supported.
  template <typename IntrinsicBindingInfo,
            std::enable_if_t<IntrinsicBindingInfo::template kOpcode<MachineOpcode> ==
                                 MachineOpcode::kMachineOpUndefined,
                             bool> = true>
  std::optional<bool> /*ProcessBindingsClient*/ operator()(
      IntrinsicBindingInfo /* asm_call_info */) {
    return false;
  }

  template <typename IntrinsicBindingInfo,
            std::enable_if_t<IntrinsicBindingInfo::template kOpcode<MachineOpcode> !=
                                 MachineOpcode::kMachineOpUndefined,
                             bool> = true>
  std::optional<bool> /*ProcessBindingsClient*/ operator()(IntrinsicBindingInfo) {
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
      static_assert(berberis::kDependentValueFalse<IntrinsicBindingInfo::kCPUIDRestriction>);
    }

    GenMachineInsn<IntrinsicBindingInfo>();
    return {true};
  }

  template <typename IntrinsicBindingInfo>
  void GenMachineInsn() {
    TypesToValues::ForEach<TypesToTypes::Zip<typename IntrinsicBindingInfo::Operands,
                                             typename IntrinsicBindingInfo::Bindings>>(
        [&args_tuple = input_args_, builder = builder_]<typename BindingInfo>() {
          using Operand = std::tuple_element_t<0, BindingInfo>;
          using Binding = std::tuple_element_t<1, BindingInfo>;
          if constexpr (HaveInput(Binding::kArgInfo) && device_arch_info::kIsRegister<Operand>) {
            if constexpr (std::is_integral_v<std::tuple_element_t<
                              Binding::kArgInfo.from,
                              typename IntrinsicBindingInfo::InputArguments>> &&
                          Operand::Class::kAsRegister == 'x') {
              auto src = std::get<Binding::kArgInfo.from>(args_tuple);
              auto dst = builder->ir()->AllocVReg();
              if constexpr (Operand::Class::kSizeInBits == 32) {
                if (host_platform::kHasAVX) {
                  builder->Gen<x86_64::VmovdXRegReg>(dst, src);
                } else {
                  builder->Gen<x86_64::MovdXRegReg>(dst, src);
                }
              } else {
                static_assert(Operand::Class::kSizeInBits == 64);
                if (host_platform::kHasAVX) {
                  builder->Gen<x86_64::VmovqXRegReg>(dst, src);
                } else {
                  builder->Gen<x86_64::MovqXRegReg>(dst, src);
                }
              }
              std::get<Binding::kArgInfo.from>(args_tuple) = dst;
            }
          }
        });
    result_ = std::tuple_cat(
        builder_
            ->GenMachineInsn<berberis::x86_64::MachineInsn<
                                 typename IntrinsicBindingInfo::DeviceInsnInfo,
                                 x86_64::kForceSSA<typename IntrinsicBindingInfo::DeviceInsnInfo>>,
                             typename IntrinsicBindingInfo::Bindings>(input_args_, flag_register_));
    TypesToValues::ForEach<TypesToTypes::Zip<typename IntrinsicBindingInfo::Operands,
                                             typename IntrinsicBindingInfo::Bindings>>(
        [&args_tuple = result_, builder = builder_]<typename BindingInfo>() {
          using Operand = std::tuple_element_t<0, BindingInfo>;
          using Binding = std::tuple_element_t<1, BindingInfo>;
          if constexpr (HaveOutput(Binding::kArgInfo) && device_arch_info::kIsRegister<Operand>) {
            using ReturnType = std::tuple_element_t<Binding::kArgInfo.to,
                                                    typename IntrinsicBindingInfo::OutputArguments>;
            if constexpr (std::is_integral_v<ReturnType>) {
              if (Operand::Class::kAsRegister == 'x') {
                auto src = std::get<Binding::kArgInfo.to>(args_tuple);
                auto dst = builder->ir()->AllocVReg();
                if constexpr (Operand::Class::kSizeInBits == 32) {
                  if (host_platform::kHasAVX) {
                    builder->Gen<x86_64::VmovdRegXReg>(dst, src);
                  } else {
                    builder->Gen<x86_64::MovdRegXReg>(dst, src);
                  }
                } else {
                  static_assert(Operand::Class::kSizeInBits == 64);
                  if (host_platform::kHasAVX) {
                    builder->Gen<x86_64::VmovqRegXReg>(dst, src);
                  } else {
                    builder->Gen<x86_64::MovqRegXReg>(dst, src);
                  }
                }
                std::get<Binding::kArgInfo.to>(args_tuple) = dst;
              }
              if constexpr (sizeof(ReturnType) == sizeof(int32_t)) {
                // Expands 32 bit values as signed. Even if actual results are processed as
                // unsigned!
                // TODO(b/308951522) replace with Expand node when it's created.
                MachineReg expanded_reg = builder->ir()->AllocVReg();
                builder->Gen<x86_64::MovsxlqRegReg>(expanded_reg,
                                                    std::get<Binding::kArgInfo.to>(args_tuple));
                std::get<Binding::kArgInfo.to>(args_tuple) = expanded_reg;
              } else {
                static_assert(sizeof(ReturnType) == sizeof(int64_t));
              }
            }
          }
        });
  }

 private:
  x86_64::MachineIRBuilder* builder_;
  ResType& result_;
  FlagRegister flag_register_;
  std::tuple<ArgType...> input_args_;
  bool success_;
};

template <auto kFunction, typename ResType, typename FlagRegister, typename... ArgType>
bool TryInlineIntrinsicForHeavyOptimizer(x86_64::MachineIRBuilder* builder,
                                         ResType& result,
                                         FlagRegister flag_register,
                                         ArgType... args) {
  if (InlineIntrinsic<kFunction>::TryInlineWithHostRounding(
          builder, result, flag_register, args...)) {
    return true;
  }

  return TryBindingBasedInlineIntrinsicForHeavyOptimizer<kFunction,
                                                         ResType,
                                                         FlagRegister,
                                                         ArgType...>(
      builder, result, flag_register, args...);
}

template <auto kFunction, typename ResType, typename FlagRegister, typename... ArgType>
void InlineIntrinsicForHeavyOptimizer(x86_64::MachineIRBuilder* builder,
                                      ResType& result,
                                      FlagRegister flag_register,
                                      ArgType... args) {
  bool success = TryInlineIntrinsicForHeavyOptimizer<kFunction, ResType, FlagRegister, ArgType...>(
      builder, result, flag_register, args...);
  CHECK(success);
}

template <auto kFunction, typename FlagRegister, typename... ArgType>
bool TryInlineIntrinsicForHeavyOptimizerVoid(x86_64::MachineIRBuilder* builder,
                                             FlagRegister flag_register,
                                             ArgType... args) {
  std::tuple<> empty_result{};
  return TryBindingBasedInlineIntrinsicForHeavyOptimizer<kFunction,
                                                         std::tuple<>,
                                                         FlagRegister,
                                                         ArgType...>(
      builder, empty_result, flag_register, args...);
}

template <auto kFunction, typename FlagRegister, typename... ArgType>
void InlineIntrinsicForHeavyOptimizerVoid(x86_64::MachineIRBuilder* builder,
                                          FlagRegister flag_register,
                                          ArgType... args) {
  bool success = TryInlineIntrinsicForHeavyOptimizerVoid<kFunction, FlagRegister, ArgType...>(
      builder, flag_register, args...);
  CHECK(success);
}

}  // namespace berberis

#endif  // BERBERIS_HEAVY_OPTIMIZER_RISCV64_INLINE_INTRINSIC_H_
