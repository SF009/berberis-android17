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

#ifndef BERBERIS_BACKEND_X86_64_MACHINE_IR_BUILDER_H_
#define BERBERIS_BACKEND_X86_64_MACHINE_IR_BUILDER_H_

#include <array>
#include <iterator>
#include <tuple>
#include <utility>

#include "berberis/backend/common/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/logging.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/device_arch_info/x86_64/call_imm.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state_opaque.h"

namespace berberis::x86_64 {

// Syntax sugar for building machine IR.
class MachineIRBuilder : public MachineIRBuilderBase<MachineIR> {
 public:
  explicit MachineIRBuilder(MachineIR* ir) : MachineIRBuilderBase(ir) {}

  template <typename InsnType, typename... Args>
  /*may_discard*/ InsnType* Gen(Args... args) {
    return MachineIRBuilderBase::Gen<InsnType, Args...>(args...);
  }

  BERBERIS_DECLARE_MACHINE_INSN_ADAPTER(
      /*may_discard*/ auto Gen,
      ConstructorArgsTuple,
      MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, kSSAMode>*,
      MachineIRBuilderBase::Gen,
      kSSAMode,
      auto kSSAMode = false)

  void GenGet(MachineReg dst_reg, int32_t offset) {
    Gen<x86_64::MovqRegOp>(dst_reg, {.base = x86_64::kMachineRegRBP, .disp = offset});
  }

  void GenPut(int32_t offset, MachineReg src_reg) {
    Gen<x86_64::MovqOpReg>({.base = x86_64::kMachineRegRBP, .disp = offset}, src_reg);
  }

  void GenPutImm(int32_t offset, int32_t imm) {
    Gen<x86_64::MovqOpImm>({.base = x86_64::kMachineRegRBP, .disp = offset}, imm);
  }

  template <size_t kSize>
  void GenGetSimd(MachineReg dst_reg, int32_t offset) {
    if constexpr (kSize == 8) {
      Gen<x86_64::MovsdXRegOp>(dst_reg, {.base = x86_64::kMachineRegRBP, .disp = offset});
    } else if constexpr (kSize == 16) {
      Gen<x86_64::MovdqaXRegOp>(dst_reg, {.base = x86_64::kMachineRegRBP, .disp = offset});
    } else {
      static_assert(kDependentValueFalse<kSize>);
    }
  }

  template <size_t kSize>
  void GenSetSimd(int32_t offset, MachineReg src_reg) {
    if constexpr (kSize == 8) {
      Gen<x86_64::MovsdOpXReg>({.base = x86_64::kMachineRegRBP, .disp = offset}, src_reg);
    } else if constexpr (kSize == 16) {
      Gen<x86_64::MovdqaOpXReg>({.base = x86_64::kMachineRegRBP, .disp = offset}, src_reg);
    } else {
      static_assert(kDependentValueFalse<kSize>);
    }
  }

  template <typename IntrinsicType, typename ArgsType>
  /*may_discard*/ auto GenCallImm(IntrinsicType func_ptr,
                                  MachineReg flags_register,
                                  ArgsType&& args) {
    return GenCallImm<static_cast<IntrinsicType>(nullptr)>(
        flags_register, std::forward<ArgsType>(args), func_ptr);
  }

  template <typename IntrinsicType, typename ArgsType>
  /*may_discard*/ auto GenCallImm(IntrinsicType func_ptr,
                                  MachineReg flags_register,
                                  ArgsType&& args,
                                  const char* func_name) {
    return GenCallImm<static_cast<IntrinsicType>(nullptr)>(
        flags_register, std::forward<ArgsType>(args), func_ptr, func_name);
  }

  template <auto kIntrinsic, typename ArgsType, typename... FuncInfo>
  /*may_discard*/ auto GenCallImm(MachineReg flags_register,
                                  ArgsType&& args,
                                  FuncInfo... func_info);

  template <auto kIntrinsic, typename... FuncInfo>
  /*may_discard*/ auto GenCallImmImpl(
      MachineReg flags_register,
      std::array<
          MachineReg,
          std::tuple_size_v<typename device_arch_info::CallImm<kIntrinsic>::ArgumentRegisters>>
          args,
      FuncInfo... func_info);
};

template <auto kIntrinsic, typename ArgsType, typename... FuncInfo>
/*may_discard*/ auto MachineIRBuilder::GenCallImm(MachineReg flags_register,
                                                  ArgsType&& args,
                                                  FuncInfo... func_info) {
  using CallImm = device_arch_info::CallImm<kIntrinsic>;
  // If result comes in caller-provided buffer then there's hidden argument before first one.
  using ExpandedParamTypes = TypesToTypes::Concat<
      std::conditional_t<CallImm::kIsImplicitPointerResult, std::tuple<int64_t>, std::tuple<>>,
      typename CallImm::CleanParamTypes>;
  // Split GenCallImm argument values: 128-bit immediates have to come as two arguments.
  auto split_args = ValuesToValues::FlatMap(std::move(args), []<typename Arg>(Arg arg) {
    if constexpr (std::is_same_v<Arg, __int128_t> || std::is_same_v<Arg, __uint128_t>) {
      return std::tuple{static_cast<int64_t>(arg), static_cast<int64_t>(arg >> 64)};
    } else {
      return std::tuple{arg};
    }
  });
  // Now lengths of two tuples should match and we may process them.
  auto array = ValuesToValues::ToArray<MachineReg>(ValuesToValues::Map(
      ValuesToValues::Zip(std::move(kTupleMetaTypes<ExpandedParamTypes>), std::move(split_args)),
      [this]<typename SplitParamType, typename SplitArgType>(
          std::pair<SplitParamType, SplitArgType> type_and_arg) {
        auto arg = type_and_arg.second;

        MachineReg physical_register = ir()->AllocVReg();
        if constexpr (std::is_integral_v<SplitArgType>) {
          static_assert(sizeof(SplitArgType) <= sizeof(int64_t));
          // Note that Movq can use Movl if that's produces smaller code.
          InsertInsn(ir()->NewInsn<MovqRegImm>(physical_register, static_cast<int64_t>(arg)));
        } else {
          if constexpr (SplitParamType::IsIntegral() &&
                        SplitParamType::SizeOf() < sizeof(int32_t)) {
            if constexpr (SplitParamType::IsSigned()) {
              if constexpr (SplitParamType::SizeOf() == sizeof(int8_t)) {
                InsertInsn(ir()->NewInsn<x86_64::MovsxblRegReg>(physical_register, arg));
              } else {
                static_assert(SplitParamType::SizeOf() == sizeof(int16_t));
                InsertInsn(ir()->NewInsn<x86_64::MovsxwlRegReg>(physical_register, arg));
              }
            } else {
              static_assert(SplitParamType::IsUnsigned());
              if constexpr (SplitParamType::SizeOf() == sizeof(uint8_t)) {
                InsertInsn(ir()->NewInsn<x86_64::MovzxblRegReg>(physical_register, arg));
              } else {
                static_assert(SplitParamType::SizeOf() == sizeof(uint16_t));
                InsertInsn(ir()->NewInsn<x86_64::MovzxwlRegReg>(physical_register, arg));
              }
            }
          } else {
            // We need copies here to ensure that the arguments passed to the intrinsic are in
            // virtual registers that can be independently allocated to the specific physical
            // registers required by the intrinsic call ABI. Worst case scenario we may have one
            // virtual register that would have to go into two different physical registers, in
            // that case it's impossible for the register allocator to satofy the requirements,
            // even in principle. These copies create new virtual registers that the register
            // allocator can then assign to the correct physical registers.
            InsertInsn(ir()->NewInsn<Copy>(physical_register, arg, SplitParamType::SizeOf()));
          }
        }
        return physical_register;
      }));
  return GenCallImmImpl<kIntrinsic>(flags_register, array, func_info...);
}

template <auto kIntrinsic, typename... FuncInfo>
/*may_discard*/ auto MachineIRBuilder::GenCallImmImpl(
    MachineReg flags_register,
    std::array<MachineReg,
               std::tuple_size_v<typename device_arch_info::CallImm<kIntrinsic>::ArgumentRegisters>>
        args,
    FuncInfo... func_info) {
  using CallImm = device_arch_info::CallImm<kIntrinsic>;
  std::array<MachineReg,
             CallImm::kIsImplicitPointerResult ? 1 : std::size(CallImm::kResultsElements)>
      results;
  const char* kFuncName = nullptr;
  // We may need one extra argument if function address and type are passed separately.
  if constexpr (CallImm::kDynamicFunction) {
    if constexpr (sizeof...(FuncInfo) == 1) {
      // If we have a dynamic intrinsic then we accept one address as 64-bit immediate.
      static_assert(
          std::is_same_v<decltype(kIntrinsic), std::tuple_element_t<0, std::tuple<FuncInfo...>>>);
    } else {
      // If we have a dynamic intrinsic then we accept one address as 64-bit immediate.
      static_assert(sizeof...(FuncInfo) == 2);
      static_assert(
          std::is_same_v<decltype(kIntrinsic), std::tuple_element_t<0, std::tuple<FuncInfo...>>>);
      static_assert(!std::is_same_v<decltype(kIntrinsic), const char*>);
      static_assert(std::is_same_v<const char*, std::tuple_element_t<1, std::tuple<FuncInfo...>>>);
      kFuncName = std::get<1>(std::tuple{func_info...});
    }
  } else {
    // Function name can be supplied with name of the intrinsics, too.
    if constexpr (sizeof...(FuncInfo) == 1) {
      static_assert(std::is_same_v<const char*, std::tuple_element_t<0, std::tuple<FuncInfo...>>>);
      kFuncName = std::get<0>(std::tuple{func_info...});
    } else {
      // Otherwise address is part of the type.
      static_assert(sizeof...(FuncInfo) == 0);
      static constexpr StringLiteral kFuncStaticName = kGetTemplateName<MetaValue<kIntrinsic>>;
      kFuncName = kFuncStaticName;
    }
  }
  std::array<MachineReg, std::tuple_size_v<typename CallImm::ResultRegisters>> call_outs;
  // Each register receives result for some elements, but in some cases more than one. If that
  // happens then we always have argument that receives some other argument in the same register
  // in position zero. We copy value into target register and then either Shrq or Psrlq/Vpsrlq to
  // put result into the position. Note: we don't do zero or sign extension and don't load results
  // from memory. This part may depend on the frontend needs and thus is not done in the backend.
  constexpr bool kNeedExtraProcessing =
      !CallImm::kIsImplicitPointerResult &&
      ValuesToValues::Any(CallImm::kResultsElements,
                          []<typename ResultElement>(ResultElement result_element) {
                            return result_element.element_offset != 0;
                          });
  if constexpr (kNeedExtraProcessing) {
    std::size_t call_outs_index = 0;
    for (std::size_t index = 0; index < std::size(CallImm::kResultsElements); ++index) {
      MachineReg reg = ir()->AllocVReg();
      results[index] = reg;
      auto& result_element = CallImm::kResultsElements[index];
      if (result_element.element_offset == 0) {
        call_outs[call_outs_index++] = reg;
      }
    }
  } else {
    for (auto& reg : call_outs) {
      reg = ir()->AllocVReg();
    }
    results = call_outs;
  }
  auto* call =
      Gen<MachineInsn<typename CallImm::template MachineInsn<CodeEmitter>::DeviceInsnInfo>>(
          std::tuple_cat(
              std::tuple{kFuncName},
              ValuesToValues::Take<CallImm::kDynamicFunction>(
                  std::tuple{bit_cast<const void*>(func_info)...}),
              call_outs,
              args,
              TypesToValues::Map<typename CallImm::ClobberRegisters>(
                  [flags_register, this]<typename RegisterClass>() {
                    if constexpr (std::is_same_v<RegisterClass, device_arch_info::FLAGS>) {
                      return flags_register;
                    } else {
                      return ir()->AllocVReg();
                    }
                  })));
  if constexpr (kNeedExtraProcessing) {
    MachineReg last_zero_based_register;
    ValuesToValues::ForEach(
        ValuesToValues::Enumerate(std::move(kTupleMetaTypes<typename CallImm::CleanRetType>)),
        [&last_zero_based_register,
         &results,
         flags_register,
         this]<std::size_t kIdx, typename ElementType>(std::pair<MetaValue<kIdx>, ElementType>) {
          constexpr auto result_element = CallImm::kResultsElements[kIdx];
          if constexpr (result_element.element_offset == 0) {
            last_zero_based_register = results[kIdx];
          }
          // We need to special handle even the first register if we receive results in RAX/RDX but
          // then have to move it into SSE register as per caller expectations.
          if constexpr (result_element.element_offset != 0 ||
                        ((result_element.clobber_class_index <=
                          std::tuple_size_v<
                              device_arch_info::call_imm_impl::SSEArgumentRegisters>) &&
                         (ElementType::template IsSame<Float16>() ||
                          ElementType::template IsSame<Float32>()))) {
            if constexpr (result_element.clobber_class_index <=
                          std::tuple_size_v<
                              device_arch_info::call_imm_impl::SSEArgumentRegisters>) {
              if constexpr (result_element.element_offset != 0) {
                InsertInsn(ir()->NewInsn<ShrqRegImm, kSSA>(
                    results[kIdx],
                    last_zero_based_register,
                    static_cast<int8_t>(result_element.element_offset * 8),
                    flags_register));
              }
              if constexpr (ElementType::template IsSame<Float16>()) {
                MachineReg empty_xmm_register = ir()->AllocVReg();
                InsertInsn(ir()->NewInsn<PseudoDefReg>(empty_xmm_register));
                MachineReg xmm_register = ir()->AllocVReg();
#ifdef __AVX__
                InsertInsn(ir()->NewInsn<VpinsrwXRegXRegRegImm>(
                    xmm_register, empty_xmm_register, results[kIdx], int8_t{0}));
#else
                InsertInsn(ir()->NewInsn<PinsrwXRegRegImm, kSSA>(
                    xmm_register, empty_xmm_register, results[kIdx], int8_t{0}));
#endif
                results[kIdx] = xmm_register;
              } else if constexpr (ElementType::template IsSame<Float32>()) {
                MachineReg xmm_register = ir()->AllocVReg();
#ifdef __AVX__
                InsertInsn(ir()->NewInsn<VmovdXRegReg>(xmm_register, results[kIdx]));
#else
                InsertInsn(ir()->NewInsn<MovdXRegReg>(xmm_register, results[kIdx]));
#endif
                results[kIdx] = xmm_register;
              }
            } else {
#ifdef __AVX__
              InsertInsn(ir()->NewInsn<VpsrlqXRegXRegImm>(
                  results[kIdx],
                  last_zero_based_register,
                  static_cast<int8_t>(result_element.element_offset * 8)));
#else
              InsertInsn(ir()->NewInsn<PsrlqXRegImm, kSSA>(
                  results[kIdx],
                  last_zero_based_register,
                  static_cast<int8_t>(result_element.element_offset * 8)));
#endif
            }
          }
        });
  }
  return std::pair{call, results};
}

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_MACHINE_IR_BUILDER_H_
