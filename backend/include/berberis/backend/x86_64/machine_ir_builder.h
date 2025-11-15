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

#include "berberis/backend/common/machine_ir_builder.h"
#include "berberis/backend/x86_64/intrinsic_call.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/logging.h"
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

  // Please use GenCallImm instead
  template <typename CallImmType,
            typename IntegralType,
            std::enable_if_t<std::is_same_v<std::decay_t<CallImmType>, CallImm> &&
                                 std::is_integral_v<IntegralType>,
                             bool> = true>
  /*may_discard*/ CallImmType* Gen(IntegralType imm) = delete;

  /*may_discard*/ CallImm* GenCallImm(uint64_t imm, MachineReg flag_register) {
    return GenCallImm(imm, flag_register, std::array<CallImm::Arg, 0>{});
  }

  template <size_t kNumberOfArguments>
  /*may_discard*/ CallImm* GenCallImm(uint64_t imm,
                                      MachineReg flag_register,
                                      const std::array<CallImm::Arg, kNumberOfArguments>& args) {
    auto* call = ir()->NewInsn<CallImm>(imm);
    // Init registers clobbered according to ABI to notify the register allocator.
    for (int i = 0; i < call->NumRegOperands(); ++i) {
      call->SetRegAt(i, ir()->AllocVReg());
    }

    call->SetRegAt(x86_64::CallImm::GetFlagsArgIndex(), flag_register);

    // Now generate CallImmArg instructions for arguments
    GenCallImmArg(call, args);

    InsertInsn(call);
    return call;
  }

  template <typename CallImmArgType,
            typename... Args,
            std::enable_if_t<std::is_same_v<std::decay_t<CallImmArgType>, CallImmArg>, bool> = true>
  /*may_discard*/ CallImmArgType* Gen(Args... args) = delete;

  template <auto kIntrinsic>
  /*may_discard*/ auto GenIntrinsicCall(
      MachineReg flag_register,
      const std::array<MachineReg,
                       std::tuple_size_v<typename IntrinsicCall<kIntrinsic>::ArgumentRegisters>>&
          args,
      std::array<MachineReg,
                 IntrinsicCall<kIntrinsic>::kIsImplicitPointerResult
                     ? 1
                     : std::size(IntrinsicCall<kIntrinsic>::kResultsElements)>& results) {
    std::array<MachineReg, std::tuple_size_v<typename IntrinsicCall<kIntrinsic>::ResultRegisters>>
        call_outs;
    // Each register receives result for some elements, but in some cases more than one. If that
    // happens then we always have argument that receives some other argument in the same register
    // in position zero. We copy value into target register and then either Shrq or Psrlq/Vpsrlq to
    // put result into the position. Note: we don't do zero or sign extension and don't load results
    // from memory. This part may depend on the frontend needs and thus is not done in the backend.
    constexpr bool kNeedExtraProcessing =
        !IntrinsicCall<kIntrinsic>::kIsImplicitPointerResult &&
        ValuesToValues::Any(IntrinsicCall<kIntrinsic>::kResultsElements,
                            []<typename ResultElement>(ResultElement result_element) {
                              return result_element.element_offset != 0;
                            });
    if constexpr (kNeedExtraProcessing) {
      std::size_t call_outs_index = 0;
      for (std::size_t index = 0; index < std::size(IntrinsicCall<kIntrinsic>::kResultsElements);
           ++index) {
        MachineReg reg = ir()->AllocVReg();
        results[index] = reg;
        auto& result_element = IntrinsicCall<kIntrinsic>::kResultsElements[index];
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
    auto* call = ir()->NewInsn<typename IntrinsicCall<kIntrinsic>::MachineInsn>(std::tuple_cat(
        call_outs,
        // We need copies here to ensure that the arguments passed to the intrinsic are in virtual
        // registers that can be independently allocated to the specific physical registers required
        // by the intrinsic call ABI. Worst case scenario we may have one virtual register that
        // would have to go into two different physical registers, in that case it's impossible for
        // the register allocator to satofy the requirements, even in principle. These copies create
        // new virtual registers that the register allocator can then assign to the correct physical
        // registers.
        TypesToValues::FlatMap<
            std::tuple<MetaValue<IntrinsicCall<kIntrinsic>::kIsImplicitPointerResult>>>(
            [&args, this]<typename IsImplicitPointerResult>() {
              if constexpr (IsImplicitPointerResult{}) {
                MachineReg physical_register = ir()->AllocVReg();
                // Pointer to result on stack is always 8 bytes long.
                InsertInsn(ir()->NewInsn<Copy>(physical_register, args[0], 8));
                return std::tuple{physical_register};
              } else {
                return std::tuple{};
              }
            }),
        TypesToValues::FlatMapWithTemporary<TypesToTypes::Zip<
            ValuesToTypes::MetaValues<IntrinsicCall<kIntrinsic>::kArgumentElements>,
            typename IntrinsicCall<kIntrinsic>::CleanParamTypes>>(
            /* index = */ std::size_t{IntrinsicCall<kIntrinsic>::kIsImplicitPointerResult ? 1 : 0},
            [&args, this]<typename ArgumentsElementInfo>(std::size_t& index) {
              constexpr intrinsic_call::ArgumentsElementInfo kElementInfo =
                  std::tuple_element_t<0, ArgumentsElementInfo>{};
              using ParamType = std::tuple_element_t<1, ArgumentsElementInfo>;
              if constexpr (kElementInfo.param_type == intrinsic_call::kInt128) {
                MachineReg physical_register1 = ir()->AllocVReg();
                MachineReg physical_register2 = ir()->AllocVReg();
                InsertInsn(ir()->NewInsn<Copy>(
                    physical_register1, args[index++], kElementInfo.register_classes[0]->reg_size));
                InsertInsn(ir()->NewInsn<Copy>(
                    physical_register2, args[index++], kElementInfo.register_classes[1]->reg_size));
                return std::tuple{physical_register1, physical_register2};
              } else {
                MachineReg physical_register = ir()->AllocVReg();
                if constexpr (std::is_integral_v<ParamType> && sizeof(ParamType) < 4) {
                  if constexpr (std::is_signed_v<ParamType>) {
                    if constexpr (sizeof(ParamType) == 1) {
                      InsertInsn(
                          ir()->NewInsn<x86_64::MovsxblRegReg>(physical_register, args[index++]));
                    } else {
                      static_assert(sizeof(ParamType) == 2);
                      InsertInsn(
                          ir()->NewInsn<x86_64::MovsxwlRegReg>(physical_register, args[index++]));
                    }
                  } else {
                    static_assert(std::is_unsigned_v<ParamType>);
                    if constexpr (sizeof(ParamType) == 1) {
                      InsertInsn(
                          ir()->NewInsn<x86_64::MovzxblRegReg>(physical_register, args[index++]));
                    } else {
                      static_assert(sizeof(ParamType) == 2);
                      InsertInsn(
                          ir()->NewInsn<x86_64::MovzxwlRegReg>(physical_register, args[index++]));
                    }
                  }
                } else {
                  InsertInsn(ir()->NewInsn<Copy>(physical_register,
                                                 args[index++],
                                                 kElementInfo.register_classes[0]->reg_size));
                  return std::tuple{physical_register};
                }
              }
            }),
        TypesToValues::Map<typename IntrinsicCall<kIntrinsic>::ClobberRegisters>(
            [flag_register, this]<typename RegisterClass>() {
              if constexpr (std::is_same_v<RegisterClass, device_arch_info::FLAGS>) {
                return flag_register;
              } else {
                return ir()->AllocVReg();
              }
            })));
    InsertInsn(call);
    if constexpr (kNeedExtraProcessing) {
      MachineReg last_zero_based_register;
      TypesToValues::ForEach<TypesToTypes::Enumerate<
          typename IntrinsicCall<kIntrinsic>::CleanRetType>>([&last_zero_based_register,
                                                              &results,
                                                              flag_register,
                                                              this]<typename IndexAndResultType>() {
        constexpr std::size_t kIdx = std::tuple_element_t<0, IndexAndResultType>{};
        constexpr auto result_element = IntrinsicCall<kIntrinsic>::kResultsElements[kIdx];
        if constexpr (result_element.element_offset == 0) {
          last_zero_based_register = results[kIdx];
        }
        using ElementType = std::tuple_element_t<1, IndexAndResultType>;
        // We need to special handle even the first register if we receive results in RAX/RDX but
        // then have to move it into SSE register as per caller expectations.
        if constexpr (result_element.element_offset != 0 ||
                      ((result_element.register_class == &kRegisterClass<device_arch_info::RAX> ||
                        result_element.register_class == &kRegisterClass<device_arch_info::RDX>) &&
                       (std::is_same_v<ElementType, intrinsics::Float16> ||
                        std::is_same_v<ElementType, intrinsics::Float32>))) {
          if constexpr (result_element.register_class == &kRegisterClass<device_arch_info::RAX> ||
                        result_element.register_class == &kRegisterClass<device_arch_info::RDX>) {
            if constexpr (result_element.element_offset != 0) {
              InsertInsn(ir()->NewInsn<ShrqRegImm, kSSA>(
                  results[kIdx],
                  last_zero_based_register,
                  static_cast<int8_t>(result_element.element_offset * 8),
                  flag_register));
            }
            if constexpr (std::is_same_v<ElementType, intrinsics::Float16>) {
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
            } else if constexpr (std::is_same_v<ElementType, intrinsics::Float32>) {
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
    return call;
  }

 private:
  template <size_t kNumberOfArgumens>
  void GenCallImmArg(CallImm* call, const std::array<CallImm::Arg, kNumberOfArgumens>& args) {
    int general_register_position = 0;
    int xmm_register_position = 0;
    for (const auto& arg : args) {
      MachineReg arg_reg = arg.reg;
      CallImm::RegType reg_type = arg.reg_type;

      // Rename arg vreg in case it's used in several call operands which have non-intersecting
      // register classes. Reg-alloc will eliminate renaming where possible.
      MachineReg renamed_arg_reg = ir()->AllocVReg();
      auto* copy = ir()->NewInsn<Copy>(
          renamed_arg_reg, arg_reg, (reg_type == CallImm::kIntRegType) ? 8 : 16);
      auto* call_arg_insn = ir()->NewInsn<CallImmArg>(renamed_arg_reg, reg_type);
      call->SetRegAt((reg_type == CallImm::kIntRegType)
                         ? CallImm::GetIntArgIndex(general_register_position++)
                         : CallImm::GetXmmArgIndex(xmm_register_position++),
                     renamed_arg_reg);

      InsertInsn(copy);
      InsertInsn(call_arg_insn);
    }
  }
};

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_MACHINE_IR_BUILDER_H_
