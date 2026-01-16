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

#ifndef BERBERIS_HEAVY_OPTIMIZER_RISCV64_FRONTEND_H_
#define BERBERIS_HEAVY_OPTIMIZER_RISCV64_FRONTEND_H_

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/base/arena_map.h"
#include "berberis/base/checks.h"
#include "berberis/base/dependent_false.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/decoder/riscv64/decoder.h"
#include "berberis/decoder/riscv64/semantics_player.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state_arch.h"
#include "berberis/guest_state/guest_state_opaque.h"
#include "berberis/intrinsics/intrinsics.h"
#include "berberis/intrinsics/macro_assembler.h"
#include "berberis/runtime_primitives/memory_region_reservation.h"
#include "berberis/runtime_primitives/platform.h"

#include "inline_intrinsic.h"

namespace berberis {

class HeavyOptimizerFrontend {
 public:
  using CsrName = berberis::CsrName;
  using Decoder = Decoder<SemanticsPlayer<HeavyOptimizerFrontend>>;
  using Register = MachineReg;
  static constexpr Register no_register = MachineReg{};
  using FpRegister = MachineReg;
  static constexpr MachineReg no_fp_register = MachineReg{};
  using Float32 = Float32;
  using Float64 = Float64;

  using TemplateTypeId = intrinsics::TemplateTypeId;
  template <typename Type>
  static constexpr auto kIdFromType = intrinsics::kIdFromType<Type>;
  template <auto kEnumValue>
  using TypeFromId = intrinsics::TypeFromId<kEnumValue>;
  template <auto ValueParam>
  using MetaValue = MetaValue<ValueParam>;

  template <typename ValueType>
  static constexpr auto ToFloat(ValueType value) {
    return intrinsics::TemplateTypeIdToFloat(value);
  }
  template <typename ValueType>
  static constexpr auto ToInt(ValueType value) {
    return intrinsics::TemplateTypeIdToInt(value);
  }
  template <typename ValueType>
  static constexpr auto ToNarrow(ValueType value) {
    return intrinsics::TemplateTypeIdToNarrow(value);
  }
  template <typename ValueType>
  static constexpr auto ToSigned(ValueType value) {
    return intrinsics::TemplateTypeIdToSigned(value);
  }
  template <typename ValueType>
  static constexpr auto SizeOf(ValueType value) {
    return intrinsics::TemplateTypeIdSizeOf(value);
  }
  template <typename ValueType>
  static constexpr auto ToUnsigned(ValueType value) {
    return intrinsics::TemplateTypeIdToUnsigned(value);
  }
  template <typename ValueType>
  static constexpr auto ToWide(ValueType value) {
    return intrinsics::TemplateTypeIdToWide(value);
  }
  static constexpr TemplateTypeId IntSizeToTemplateTypeId(uint8_t size, bool is_signed = false) {
    return intrinsics::IntSizeToTemplateTypeId(size, is_signed);
  }

  using MemoryOperand = x86_64::MemoryOperand;

  explicit HeavyOptimizerFrontend(x86_64::MachineIR* machine_ir, GuestAddr pc)
      : pc_(pc),
        success_(true),
        builder_(machine_ir),
        flag_register_(machine_ir->AllocVReg()),
        is_uncond_branch_(false),
        branch_targets_(machine_ir->arena()) {
    StartRegion();
  }

  void CompareAndBranch(Decoder::BranchOpcode opcode, Register arg1, Register arg2, int16_t offset);
  void Branch(int32_t offset);
  void BranchRegister(Register base, int16_t offset);

  [[nodiscard]] Register GetImm(int64_t imm);
  [[nodiscard]] Register Copy(Register value) {
    Register result = AllocTempReg();
    builder_.Gen<berberis::Copy>(result, value, kMeta<&x86_64::kGeneralReg64>);
    return result;
  }

  [[nodiscard]] Register GetReg(uint8_t reg);
  void SetReg(uint8_t reg, Register value);

  void Undefined();
  //
  // Instruction implementations.
  //
  void Nop();
  Register Op(Decoder::OpOpcode opcode, Register arg1, Register arg2);
  Register Op32(Decoder::Op32Opcode opcode, Register arg1, Register arg2);
  Register OpImm(Decoder::OpImmOpcode opcode, Register arg, int16_t imm);
  Register OpImm32(Decoder::OpImm32Opcode opcode, Register arg, int16_t imm);
  Register Slli(Register arg, int8_t imm);
  Register Srli(Register arg, int8_t imm);
  Register Srai(Register arg, int8_t imm);
  Register ShiftImm32(Decoder::ShiftImm32Opcode opcode, Register arg, int8_t imm);
  Register Rori(Register arg, int8_t shamt);
  Register Roriw(Register arg, int8_t shamt);
  Register Lui(int32_t imm);
  Register Auipc(int32_t imm);

  Register Ecall(Register /* syscall_nr */,
                 Register /* arg0 */,
                 Register /* arg1 */,
                 Register /* arg2 */,
                 Register /* arg3 */,
                 Register /* arg4 */,
                 Register /* arg5 */) {
    Undefined();
    return {};
  }

  void Store(Decoder::MemoryDataOperandType operand_type,
             Register arg,
             int16_t offset,
             Register data);
  Register Load(Decoder::LoadOperandType operand_type, Register arg, int16_t offset);

  template <typename IntType>
  constexpr Decoder::LoadOperandType ToLoadOperandType() {
    if constexpr (std::is_same_v<IntType, int8_t>) {
      return Decoder::LoadOperandType::k8bitSigned;
    } else if constexpr (std::is_same_v<IntType, int16_t>) {
      return Decoder::LoadOperandType::k16bitSigned;
    } else if constexpr (std::is_same_v<IntType, int32_t>) {
      return Decoder::LoadOperandType::k32bitSigned;
    } else if constexpr (std::is_same_v<IntType, int64_t> || std::is_same_v<IntType, uint64_t>) {
      return Decoder::LoadOperandType::k64bit;
    } else if constexpr (std::is_same_v<IntType, uint8_t>) {
      return Decoder::LoadOperandType::k8bitUnsigned;
    } else if constexpr (std::is_same_v<IntType, uint16_t>) {
      return Decoder::LoadOperandType::k16bitUnsigned;
    } else if constexpr (std::is_same_v<IntType, uint32_t>) {
      return Decoder::LoadOperandType::k32bitUnsigned;
    } else {
      static_assert(kDependentTypeFalse<IntType>);
    }
  }

  template <typename IntType>
  constexpr Decoder::MemoryDataOperandType ToMemoryDataOperandType() {
    if constexpr (std::is_same_v<IntType, int8_t> || std::is_same_v<IntType, uint8_t>) {
      return Decoder::MemoryDataOperandType::k8bit;
    } else if constexpr (std::is_same_v<IntType, int16_t> || std::is_same_v<IntType, uint16_t>) {
      return Decoder::MemoryDataOperandType::k16bit;
    } else if constexpr (std::is_same_v<IntType, int32_t> || std::is_same_v<IntType, uint32_t>) {
      return Decoder::MemoryDataOperandType::k32bit;
    } else if constexpr (std::is_same_v<IntType, int64_t> || std::is_same_v<IntType, uint64_t>) {
      return Decoder::MemoryDataOperandType::k64bit;
    } else {
      static_assert(kDependentTypeFalse<IntType>);
    }
  }

  // Versions without recovery can be used to access non-guest memory (e.g. CPUState).
  Register LoadWithoutRecovery(Decoder::LoadOperandType operand_type, Register base, int32_t disp);
  Register LoadWithoutRecovery(Decoder::LoadOperandType operand_type,
                               Register base,
                               Register index,
                               int32_t disp);
  void StoreWithoutRecovery(Decoder::MemoryDataOperandType operand_type,
                            Register base,
                            int32_t disp,
                            Register val);
  void StoreWithoutRecovery(Decoder::MemoryDataOperandType operand_type,
                            Register base,
                            Register index,
                            int32_t disp,
                            Register val);

  //
  // Atomic extensions.
  //

  template <intrinsics::TemplateTypeId IntType, bool aq, bool rl>
  Register Lr(Register addr, MetaValue<IntType>, MetaValue<aq>, MetaValue<rl>) {
    // The immediate is sign extended to 64-bit.
    auto [aligned_addr, and_flags] =
        Gen<x86_64::AndqRegImm>(addr, ~int32_t{sizeof(Reservation) - 1});

    MemoryRegionReservationLoad(aligned_addr);

    auto [addr_offset, sub_flags] = Gen<x86_64::SubqRegReg>(addr, aligned_addr);

    // Load the requested part from CPUState.
    return LoadWithoutRecovery(ToLoadOperandType<TypeFromId<IntType>>(),
                               x86_64::kMachineRegRBP,
                               addr_offset,
                               GetThreadStateReservationValueOffset());
  }

  template <intrinsics::TemplateTypeId IntType, bool aq, bool rl>
  Register Sc(Register addr, Register data, MetaValue<IntType>, MetaValue<aq>, MetaValue<rl>) {
    // Compute aligned_addr.
    // The immediate is sign extended to 64-bit.
    auto [aligned_addr, and_flags] =
        Gen<x86_64::AndqRegImm>(addr, ~int32_t{sizeof(Reservation) - 1});

    // Load current monitor value before we clobber it.
    int32_t value_offset = GetThreadStateReservationValueOffset();
    auto [reservation_value] =
        Gen<x86_64::MovqRegOp>({.base = x86_64::kMachineRegRBP, .disp = value_offset});

    auto [addr_offset, sub_flags] = Gen<x86_64::SubqRegReg>(addr, aligned_addr);

    // It's okay to clobber reservation_value since we clear out reservation_address in
    // MemoryRegionReservationExchange anyway.
    StoreWithoutRecovery(ToMemoryDataOperandType<TypeFromId<IntType>>(),
                         x86_64::kMachineRegRBP,
                         addr_offset,
                         value_offset,
                         data);

    return MemoryRegionReservationExchange(aligned_addr, reservation_value);
  }

  void Fence(Decoder::FenceOpcode opcode,
             Register src,
             bool sw,
             bool sr,
             bool so,
             bool si,
             bool pw,
             bool pr,
             bool po,
             bool pi);

  //
  // F and D extensions.
  //
  [[nodiscard]] FpRegister GetFpReg(uint8_t reg);

  template <typename FloatType>
  [[nodiscard]] FpRegister GetFRegAndUnboxNan(uint8_t reg) {
    CHECK_LE(reg, kNumGuestFpRegs);
    FpRegister result = AllocTempReg();
    builder_.GenGetSimd<8>(result, GetThreadStateFRegOffset(reg));
    FpRegister unboxed_result = AllocTempReg();
    if (host_platform::kHasAVX) {
      // This code is defined as intrinsic but if we would call it as intrinsic it would be called
      // recursively.
      builder_.Gen<x86_64::MachineInsn<
          device_arch_info::DeviceInsnInfo<
              &MacroAssembler<x86_64::Assembler>::UnboxNanAVX<Float32>,
              "UNBOX_F32",
              true,
              []<typename Opcode> { return Opcode::kMachineOpUnboxNanFloat32AVX; },
              x86_64::device_arch_info::HasAVX,
              std::tuple<device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg32,
                                                       device_arch_info::kDef>,
                         device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg64,
                                                       device_arch_info::kUseDef>>>,
          x86_64::kSSA>>(std::tuple{unboxed_result, AllocTempReg(), result});
    } else {
      // This code is defined as intrinsic but if we would call it as intrinsic it would be called
      // recursively.
      builder_.Gen<x86_64::MachineInsn<
          device_arch_info::DeviceInsnInfo<
              &MacroAssembler<x86_64::Assembler>::UnboxNan<Float32>,
              "UNBOX_F32",
              true,
              []<typename Opcode> { return Opcode::kMachineOpUnboxNanFloat32; },
              device_arch_info::NoCPUIDRestriction,
              std::tuple<device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg32,
                                                       device_arch_info::kDef>,
                         device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg64,
                                                       device_arch_info::kUseDef>>>,
          x86_64::kSSA>>(std::tuple{unboxed_result, AllocTempReg(), result});
    }
    return unboxed_result;
  }

  template <typename FloatType>
  FpRegister NanBoxFpReg(FpRegister value) {
    FpRegister result = AllocTempReg();
    if (host_platform::kHasAVX) {
      // This code is defined as intrinsic but if we would call it as intrinsic it would be called
      // recursively.
      builder_.Gen<x86_64::MachineInsn<
          device_arch_info::DeviceInsnInfo<
              &MacroAssembler<x86_64::Assembler>::NanBoxAVX<Float32>,
              "BOX_F32",
              true,
              []<typename Opcode> { return Opcode::kMachineOpNanBoxFloat32AVX; },
              x86_64::device_arch_info::HasAVX,
              std::tuple<device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg64,
                                                       device_arch_info::kDef>,
                         device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg32,
                                                       device_arch_info::kUseDef>>>,
          x86_64::kSSA>>(std::tuple{result, AllocTempReg(), value});
    } else {
      // This code is defined as intrinsic but if we would call it as intrinsic it would be called
      // recursively.
      builder_.Gen<x86_64::MachineInsn<
          device_arch_info::DeviceInsnInfo<
              &MacroAssembler<x86_64::Assembler>::NanBox<Float32>,
              "BOX_F32",
              true,
              []<typename Opcode> { return Opcode::kMachineOpNanBoxFloat32; },
              device_arch_info::NoCPUIDRestriction,
              std::tuple<device_arch_info::OperandInfo<x86_64::device_arch_info::FpReg64,
                                                       device_arch_info::kUseDef>>>,
          x86_64::kSSA>>(std::tuple{result, value});
    }
    return result;
  }

  template <typename FloatType>
  void NanBoxAndSetFpReg(uint8_t reg, FpRegister value) {
    CHECK_LE(reg, kNumGuestFpRegs);
    if (success()) {
      FpRegister boxed_reg = NanBoxFpReg<FloatType>(value);
      builder_.GenSetSimd<8>(GetThreadStateFRegOffset(reg), boxed_reg);
    }
  }

  template <typename DataType>
  FpRegister LoadFp(Register arg, int16_t offset) {
    MachineReg res;
    if constexpr (std::is_same_v<DataType, Float32>) {
      res = std::get<0>(Gen<x86_64::MovssXRegOp>({.base = arg, .disp = offset}));
    } else if constexpr (std::is_same_v<DataType, Float64>) {
      res = std::get<0>(Gen<x86_64::MovsdXRegOp>({.base = arg, .disp = offset}));
    } else {
      static_assert(kDependentTypeFalse<DataType>);
    }
    return FpRegister{res};
  }

  template <typename DataType>
  void StoreFp(Register arg, int16_t offset, FpRegister data) {
    if constexpr (std::is_same_v<DataType, Float32>) {
      Gen<x86_64::MovssOpXReg>({.base = arg, .disp = offset}, data);
    } else if constexpr (std::is_same_v<DataType, Float64>) {
      Gen<x86_64::MovsdOpXReg>({.base = arg, .disp = offset}, data);
    } else {
      static_assert(kDependentTypeFalse<DataType>);
    }
  }

  FpRegister Fmv(FpRegister arg) {
    auto res = AllocTempReg();
    builder_.Gen<berberis::Copy>(res, arg, kMeta<&x86_64::kXmmReg>);
    return res;
  }

  //
  // V extension.
  //

  template <typename VOpArgs, typename... ExtraAegs>
  void OpVector(const VOpArgs& /*args*/, ExtraAegs... /*extra_args*/) {
    // TODO(b/300690740): develop and implement strategy which would allow us to support vector
    // intrinsics not just in the interpreter.
    Undefined();
  }

  //
  // Csr
  //

  Register UpdateCsr(Decoder::CsrOpcode opcode, Register arg, Register csr);
  Register UpdateCsr(Decoder::CsrImmOpcode opcode, int8_t imm, Register csr);

  [[nodiscard]] bool success() const { return success_; }

  //
  // Guest state getters/setters.
  //

  [[nodiscard]] GuestAddr GetInsnAddr() const { return pc_; }
  void IncrementInsnAddr(uint8_t insn_size) { pc_ += insn_size; }

  [[nodiscard]] bool IsRegionEndReached() const;
  void StartInsn();
  void Finalize(GuestAddr stop_pc);

  // These methods are exported only for testing.
  [[nodiscard]] const ArenaMap<GuestAddr, MachineInsnPosition>& branch_targets() const {
    return branch_targets_;
  }

  template <CsrName kName>
  [[nodiscard]] Register GetCsr() {
    if constexpr (std::is_same_v<CsrFieldType<kName>, uint8_t>) {
      return std::get<0>(Gen<x86_64::MovzxblRegOp>(
          {.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>}));
    } else if constexpr (std::is_same_v<CsrFieldType<kName>, uint64_t>) {
      return std::get<0>(
          Gen<x86_64::MovqRegOp>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>}));
    } else {
      static_assert(kDependentTypeFalse<CsrFieldType<kName>>);
    }
  }

  template <CsrName kName>
  void SetCsr(uint8_t imm) {
    // Note: csr immediate only have 5 bits in RISC-V encoding which guarantess us that
    // “imm & kCsrMask<kName>”can be used as 8-bit immediate.
    if constexpr (std::is_same_v<CsrFieldType<kName>, uint8_t>) {
      Gen<x86_64::MovbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>},
                             static_cast<int8_t>(imm & kCsrMask<kName>));
    } else if constexpr (std::is_same_v<CsrFieldType<kName>, uint64_t>) {
      Gen<x86_64::MovbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>},
                             static_cast<int8_t>(imm & kCsrMask<kName>));
    } else {
      static_assert(kDependentTypeFalse<CsrFieldType<kName>>);
    }
  }

  template <CsrName kName>
  void SetCsr(Register arg) {
    if constexpr (sizeof(CsrFieldType<kName>) == 1) {
      if constexpr (kCsrMask<kName> == 0xff) {
        Gen<x86_64::MovbOpReg>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>},
                               arg);
      } else {
        auto [tmp, and_flags] = Gen<x86_64::AndbRegImm>(arg, int8_t{kCsrMask<kName>});
        Gen<x86_64::MovbOpReg>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>},
                               tmp);
      }
    } else if constexpr (sizeof(CsrFieldType<kName>) == 8) {
      auto [tmp, and_flags] =
          Gen<x86_64::AndqRegOp>(arg, {.disp = constants_pool::kConst<uint64_t{kCsrMask<kName>}>});
      Gen<x86_64::MovqOpReg>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<kName>}, tmp);
    } else {
      static_assert(kDependentTypeFalse<CsrFieldType<kName>>);
    }
  }

  // public for tests.
  template <typename FunctionType, typename... AssemblerArgType>
  auto CallIntrinsic(FunctionType func_ptr, const char* func_name, AssemblerArgType... args)
      -> std::array<
          MachineReg,
          std::size(x86_64::CallImm<static_cast<FunctionType>(nullptr)>::kResultsElements)>;

 private:
  template <auto kFunction,
            StringLiteral kFunctionName = kGetTemplateName<MetaValue<kFunction>>,
            typename... AssemblerArgType>
  auto CallIntrinsic(AssemblerArgType... args) {
    using CallImm = x86_64::CallImm<static_cast<decltype(kFunction)>(nullptr)>;
    typename CallImm::template ResultRegisterTypes<MachineReg, FpRegister> result;

    if (!TryInlineIntrinsicForHeavyOptimizer<kFunction>(
            &builder_, result, GetFlagsRegister(), args...)) {
      const char* func_name = nullptr;
      if constexpr (config::kPrintCallIntrinsicNamesMode == config::kPrintCallIntrinsicStemNames &&
                    !std::is_same_v<decltype(kFunctionName), StringLiteral<0>>) {
        func_name = kMemoizedValue<StringLiteral{kGetTemplateName<MetaValue<kFunction>>}>;
      } else if constexpr (config::kPrintCallIntrinsicNamesMode ==
                               config::kPrintCallIntrinsicFullNames &&
                           !std::is_same_v<decltype(kFunctionName), StringLiteral<0>>) {
        func_name = kMemoizedValue<kFunctionName>;
      }
      result = CallIntrinsic<decltype(kFunction), AssemblerArgType...>(
          kFunction, func_name, std::forward<AssemblerArgType>(args)...);
    }

    if constexpr (std::size(CallImm::kResultsElements)) {
      if constexpr (std::tuple_size_v<typename CallImm::CleanRetType> == 1) {
        return std::get<0>(result);
      } else {
        return result;
      }
    }
  }

 public:
  //
  // Intrinsic proxy methods.
  //

#ifdef BERBERIS_INTRINSICS_HOOKS_INLINE_DEMULTIPLEXER
#include "berberis/intrinsics/demultiplexer_intrinsics_hooks-inl.h"
#endif
#include "berberis/intrinsics/translator_intrinsics_hooks-inl.h"

 private:
  void MemoryRegionReservationLoad(Register aligned_addr);
  Register MemoryRegionReservationExchange(Register aligned_addr, Register curr_reservation_value);
  void MemoryRegionReservationSwapWithLockedOwner(Register aligned_addr,
                                                  Register curr_reservation_value,
                                                  Register new_reservation_value,
                                                  MachineBasicBlock* failure_bb);

  // Syntax sugar.
  template <typename InsnType>
  auto Gen(typename InsnType::InputArgsTuple&& args_tuple) {
    return builder_.GenMachineInsn<InsnType>(std::move(args_tuple), GetFlagsRegister());
  }

  BERBERIS_DECLARE_MACHINE_INSN_ADAPTER(
      /*may_discard*/ auto Gen,
      InputArgsTuple,
      typename x86_64::MachineInsn<
          typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,
          x86_64::kForceSSA<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo>>::
          OutputArgsTuple,
      Gen,
      x86_64::kForceSSA<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo>)

  static x86_64::Assembler::Condition ToAssemblerCond(Decoder::BranchOpcode opcode);

  [[nodiscard]] Register AllocTempReg();
  [[nodiscard]] Register GetFlagsRegister() const { return flag_register_; };

  void GenJump(GuestAddr target);
  void ExitGeneratedCode(GuestAddr target);
  void ExitRegionIndirect(Register target);

  void GenRecoveryBlockForLastInsn();

  void ResolveJumps();
  void ReplaceJumpWithBranch(MachineBasicBlock* bb, MachineBasicBlock* target_bb);
  void UpdateBranchTargetsAfterSplit(GuestAddr addr,
                                     const MachineBasicBlock* old_bb,
                                     MachineBasicBlock* new_bb);

  void StartRegion() {
    auto* region_entry_bb = builder_.ir()->NewBasicBlock();
    auto* cont_bb = builder_.ir()->NewBasicBlock();
    builder_.ir()->AddEdge(region_entry_bb, cont_bb);
    builder_.StartBasicBlock(region_entry_bb);
    builder_.Gen<berberis::Branch>(cont_bb);
    builder_.StartBasicBlock(cont_bb);
  }

  GuestAddr pc_;
  bool success_;
  x86_64::MachineIRBuilder builder_;
  MachineReg flag_register_;
  bool is_uncond_branch_;
  // Contains IR positions of all guest instructions of the current region.
  // Also contains all branch targets which the current region jumps to.
  // If the target is outside of the current region the position is uninitialized,
  // i.e. it's basic block (position.first) is nullptr.
  ArenaMap<GuestAddr, MachineInsnPosition> branch_targets_;
};

template <typename FunctionType, typename... AssemblerArgType>
auto HeavyOptimizerFrontend::CallIntrinsic(FunctionType func_ptr,
                                           const char* func_name,
                                           AssemblerArgType... args)
    -> std::array<
        MachineReg,
        std::size(x86_64::CallImm<static_cast<FunctionType>(nullptr)>::kResultsElements)> {
  using CallImm = x86_64::CallImm<static_cast<FunctionType>(nullptr)>;

  if constexpr (CallImm::kIsImplicitPointerResult) {
    builder_.ir()->ReserveArgs(sizeof(typename CallImm::CleanRetType));
  }

  std::array<MachineReg,
             CallImm::kIsImplicitPointerResult ? 1 : std::size(CallImm::kResultsElements)>
      register_results = std::get<1>(builder_.GenCallImm(
          func_ptr,
          GetFlagsRegister(),
          std::tuple_cat(
              TypesToValues::FlatMap<std::tuple<MetaValue<CallImm::kIsImplicitPointerResult>>>(
                  []<typename IsImplicitPointerResult>() {
                    if constexpr (IsImplicitPointerResult{}) {
                      return std::tuple{x86_64::kMachineRegRSP};
                    } else {
                      return std::tuple{};
                    }
                  }),
              std::tuple{args...}),
          func_name));

  auto result = ValuesToValues::ToArray<MachineReg>(
      TypesToValues::FlatMap<TypesToTypes::Enumerate<typename CallImm::CleanRetType>>(
          [register_results, this]<typename RetElementInfo>() {
            // Suppress spurious warnings.
            // See  https://github.com/llvm/llvm-project/issues/34798#issuecomment-980989495
            (void)register_results;
            constexpr std::size_t kIdx = std::tuple_element_t<0, RetElementInfo>{};
            using ResType = std::tuple_element_t<1, RetElementInfo>;
            if constexpr (CallImm::kIsImplicitPointerResult) {
              const int32_t kElementOffset = CallImm::kResultsElements[kIdx].element_offset;
              CHECK(kElementOffset == CallImm::kResultsElements[kIdx].element_offset);
              MemoryOperand memory_operand = {.base = x86_64::kMachineRegRSP,
                                              .disp = kElementOffset};
              if constexpr (std::is_integral_v<ResType> || std::is_pointer_v<ResType>) {
                if constexpr (sizeof(ResType) == 1) {
                  if constexpr (std::is_signed_v<ResType>) {
                    return Gen<x86_64::MovsxbqRegOp>(memory_operand);
                  } else {
                    return Gen<x86_64::MovzxblRegOp>(memory_operand);
                  }
                } else if constexpr (sizeof(ResType) == 2) {
                  if constexpr (std::is_signed_v<ResType>) {
                    return Gen<x86_64::MovsxwqRegOp>(memory_operand);
                  } else {
                    return Gen<x86_64::MovzxwlRegOp>(memory_operand);
                  }
                } else if constexpr (sizeof(ResType) == 4) {
                  return Gen<x86_64::MovsxlqRegOp>(memory_operand);
                } else {
                  static_assert(sizeof(ResType) == 8);
                  return Gen<x86_64::MovqRegOp>(memory_operand);
                }
              } else {
                static_assert(sizeof(ResType) == 16);
                return Gen<x86_64::MovdquXRegOp>(memory_operand);
              }
            } else {
              if constexpr (std::is_integral_v<ResType> || std::is_pointer_v<ResType>) {
                if constexpr (sizeof(ResType) == 1) {
                  if constexpr (std::is_signed_v<ResType>) {
                    return Gen<x86_64::MovsxbqRegReg>(register_results[kIdx]);
                  } else {
                    return Gen<x86_64::MovzxblRegReg>(register_results[kIdx]);
                  }
                } else if constexpr (sizeof(ResType) == 2) {
                  if constexpr (std::is_signed_v<ResType>) {
                    return Gen<x86_64::MovsxwqRegReg>(register_results[kIdx]);
                  } else {
                    return Gen<x86_64::MovzxwlRegReg>(register_results[kIdx]);
                  }
                } else if constexpr (sizeof(ResType) == 4) {
                  return Gen<x86_64::MovsxlqRegReg>(register_results[kIdx]);
                } else {
                  static_assert(sizeof(ResType) == 8);
                  return std::tuple{register_results[kIdx]};
                }
              } else {
                return std::tuple{register_results[kIdx]};
              }
            }
          }));
  return result;
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::FpRegister
HeavyOptimizerFrontend::GetFRegAndUnboxNan<Float64>(uint8_t reg) {
  return GetFpReg(reg);
}

template <>
inline HeavyOptimizerFrontend::FpRegister HeavyOptimizerFrontend::NanBoxFpReg<Float64>(
    FpRegister value) {
  return value;
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::Register
HeavyOptimizerFrontend::GetCsr<CsrName::kCycle>() {
  return CPUClockCount();
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::Register
HeavyOptimizerFrontend::GetCsr<CsrName::kFCsr>() {
  std::tuple<MachineReg> tmp;
  InlineIntrinsicForHeavyOptimizer<&intrinsics::FeGetExceptions>(
      &builder_, tmp, GetFlagsRegister());
  auto [csr_reg] = Gen<x86_64::MovzxbqRegOp>(
      {.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kFrm>});
  auto [shifted_reg, shl_flags] = Gen<x86_64::ShlbRegImm>(csr_reg, int8_t{5});
  auto [ored_reg, or_flags] = Gen<x86_64::OrbRegReg>(shifted_reg, std::get<0>(tmp));
  return ored_reg;
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::Register
HeavyOptimizerFrontend::GetCsr<CsrName::kFFlags>() {
  return FeGetExceptions();
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::Register
HeavyOptimizerFrontend::GetCsr<CsrName::kVlenb>() {
  return GetImm(16);
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::Register
HeavyOptimizerFrontend::GetCsr<CsrName::kVxrm>() {
  auto [reg] = Gen<x86_64::MovzxbqRegOp>(
      {.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>});
  auto [res, and_flags] = Gen<x86_64::AndbRegImm>(reg, int8_t{0b11});
  return res;
}

template <>
[[nodiscard]] inline HeavyOptimizerFrontend::Register
HeavyOptimizerFrontend::GetCsr<CsrName::kVxsat>() {
  auto [reg] = Gen<x86_64::MovzxbqRegOp>(
      {.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>});
  auto [res, shr_flags] = Gen<x86_64::ShrbRegImm>(reg, int8_t{2});
  return res;
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kFCsr>(uint8_t imm) {
  // Note: instructions Csrrci or Csrrsi couldn't affect Frm because immediate only has five bits.
  // But these instruction don't pass their immediate-specified argument into `SetCsr`, they combine
  // it with register first. Fixing that can only be done by changing code in the semantics player.
  //
  // But Csrrwi may clear it.  And we actually may only arrive here from Csrrwi.
  // Thus, technically, we know that imm >> 5 is always zero, but it doesn't look like a good idea
  // to rely on that: it's very subtle and it only affects code generation speed.
  Gen<x86_64::MovbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kFrm>},
                         static_cast<int8_t>(imm >> 5));
  InlineIntrinsicForHeavyOptimizerVoid<&intrinsics::FeSetExceptionsAndRoundImm>(
      &builder_, GetFlagsRegister(), imm);
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kFCsr>(Register arg) {
  // Check size to be sure we can use Andb and Movb below.
  static_assert(sizeof(kCsrMask<CsrName::kFrm>) == 1);

  auto [exceptions, and_flags] = Gen<x86_64::AndlRegImm>(arg, 0b1'1111);
  // We don't care about the data in rounding_mode because we will shift in the
  // data we need.
  auto undef_rounding = AllocTempReg();
  builder_.Gen<PseudoDefReg>(undef_rounding);
  auto [rounding_mode, shld_flags] =
      Gen<x86_64::ShldlRegRegImm>(undef_rounding, arg, int8_t{32 - 5});
  auto [cleaned_rounding, and_flаgs] =
      Gen<x86_64::AndbRegImm>(rounding_mode, int8_t{kCsrMask<CsrName::kFrm>});
  Gen<x86_64::MovbOpReg>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kFrm>},
                         cleaned_rounding);
  InlineIntrinsicForHeavyOptimizerVoid<&intrinsics::FeSetExceptionsAndRound>(
      &builder_, GetFlagsRegister(), exceptions, cleaned_rounding);
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kFFlags>(uint8_t imm) {
  FeSetExceptionsImm(static_cast<int8_t>(imm & 0b1'1111));
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kFFlags>(Register arg) {
  FeSetExceptions(std::get<0>(Gen<x86_64::AndlRegImm>(arg, 0b1'1111)));
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kFrm>(uint8_t imm) {
  Gen<x86_64::MovbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kFrm>},
                         static_cast<int8_t>(imm & kCsrMask<CsrName::kFrm>));
  FeSetRoundImm(static_cast<int8_t>(imm & kCsrMask<CsrName::kFrm>));
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kFrm>(Register arg) {
  // Use RCX as temporary register. We know it would be used by FeSetRound, too.
  auto [tmp, and_flags] = Gen<x86_64::AndbRegImm>(arg, int8_t{kCsrMask<CsrName::kFrm>});
  Gen<x86_64::MovbOpReg>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kFrm>},
                         tmp);
  FeSetRound(tmp);
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kVxrm>(uint8_t imm) {
  imm &= 0b11;
  if (imm != 0b11) {
    Gen<x86_64::AndbOpImm>(
        {.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>}, 0b100);
  }
  if (imm != 0b00) {
    Gen<x86_64::OrbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>},
                          imm);
  }
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kVxrm>(Register arg) {
  Gen<x86_64::AndbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>},
                         0b100);
  auto [tmp, and_flags] = Gen<x86_64::AndbRegImm>(arg, int8_t{0b11});
  Gen<x86_64::OrbOpReg>({x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>}, tmp);
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kVxsat>(uint8_t imm) {
  if (imm & 0b1) {
    Gen<x86_64::OrbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>},
                          0b100);
  } else {
    Gen<x86_64::AndbOpImm>(
        {.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>}, 0b11);
  }
}

template <>
inline void HeavyOptimizerFrontend::SetCsr<CsrName::kVxsat>(Register arg) {
  using Condition = x86_64::Assembler::Condition;
  Gen<x86_64::AndbOpImm>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>},
                         0b11);
  auto [test_flags] = Gen<x86_64::TestbRegImm>(arg, int8_t{1});
  auto [tmp] = Gen<x86_64::SetccReg>(Condition::kNotZero, test_flags);
  auto [expanded] = Gen<x86_64::MovzxbqRegReg>(tmp);
  auto [res, shl_flags] = Gen<x86_64::ShlbRegImm>(expanded, int8_t{2});
  Gen<x86_64::OrbOpReg>({.base = x86_64::kMachineRegRBP, .disp = kCsrFieldOffset<CsrName::kVcsr>},
                        res);
}

}  // namespace berberis

#endif /* BERBERIS_HEAVY_OPTIMIZER_RISCV64_FRONTEND_H_ */
