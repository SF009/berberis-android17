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

// x86_64 machine IR interface.

#ifndef BERBERIS_BACKEND_X86_64_MACHINE_IR_H_
#define BERBERIS_BACKEND_X86_64_MACHINE_IR_H_

#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <tuple>

#include "berberis/assembler/x86_64.h"
#include "berberis/backend/code_emitter.h"
#include "berberis/backend/common/machine_ir.h"  // IWYU pragma: export.
#include "berberis/backend/x86_64/code_emit.h"
#include "berberis/backend/x86_64/memory_operand.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/config_globals.h"
#include "berberis/base/stringprintf.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/device_arch_info/x86_64/device_arch_info.h"
#include "berberis/guest_state/guest_state_arch.h"
#include "berberis/intrinsics/intrinsics_args.h"

namespace berberis {

// Some instructions form groups. E.g. memory-accesses typically have 4 versions: Absolute, Base,
// Index Base+Index.
//
// The top 8 bits (24-31) of the MachineOpcode are reserved for additional information.
// Bit 30 is used to indicate SSA mode.
inline constexpr int kSSAOpcodeBit = 30;

// The bits starting from kLowMachineOpcodeBits (24) are used to encode variations based on memory
// operand forms. Since there can be up to three memory operands, up to 6 bits are used for these
// variations (2 bits per memory operand for Base/Index presence)—plus two bits in case we would
// need three memory operands.
inline constexpr int kLowMachineOpcodeBits = 24;

enum MachineOpcode : int {
  kMachineOpUndefined = 0,
  kMachineOpEnter,
  kMachineOpCallImm,
  kMachineOpBranch,
  kMachineOpCondBranch,
  kMachineOpCopy,
  kMachineOpPseudoDefReg,
  kMachineOpIndirectJump,
  kMachineOpJump,
#include "machine_opcode_x86_64-inl.h"  // NOLINT generated file!
};

namespace x86_64 {

constexpr std::pair<bool, bool> OpcodeHasMemoryBaseIndex(MachineOpcode opcode,
                                                         size_t mem_operand_idx) {
  int base_index_info = opcode >> (kLowMachineOpcodeBits + mem_operand_idx * 2);
  return {base_index_info & 1, base_index_info & 2};
}

class MachineRegs {
 public:
  static constexpr MachineReg kR8{1};
  static constexpr MachineReg kR9{2};
  static constexpr MachineReg kR10{3};
  static constexpr MachineReg kR11{4};
  static constexpr MachineReg kRSI{5};
  static constexpr MachineReg kRDI{6};
  static constexpr MachineReg kRAX{7};
  static constexpr MachineReg kRBX{8};
  static constexpr MachineReg kRCX{9};
  static constexpr MachineReg kRDX{10};
  static constexpr MachineReg kRBP{11};
  static constexpr MachineReg kRSP{12};
  static constexpr MachineReg kR12{13};
  static constexpr MachineReg kR13{14};
  static constexpr MachineReg kR14{15};
  static constexpr MachineReg kR15{16};
  static constexpr MachineReg kFLAGS{19};
  static constexpr MachineReg kXMM0{20};
  static constexpr MachineReg kXMM1{21};
  static constexpr MachineReg kXMM2{22};
  static constexpr MachineReg kXMM3{23};
  static constexpr MachineReg kXMM4{24};
  static constexpr MachineReg kXMM5{25};
  static constexpr MachineReg kXMM6{26};
  static constexpr MachineReg kXMM7{27};
  static constexpr MachineReg kXMM8{28};
  static constexpr MachineReg kXMM9{29};
  static constexpr MachineReg kXMM10{30};
  static constexpr MachineReg kXMM11{31};
  static constexpr MachineReg kXMM12{32};
  static constexpr MachineReg kXMM13{33};
  static constexpr MachineReg kXMM14{34};
  static constexpr MachineReg kXMM15{35};
};

inline constexpr auto kMachineRegFLAGS = MachineRegs::kFLAGS;
inline constexpr auto kMachineRegRBP = MachineRegs::kRBP;
inline constexpr auto kMachineRegRSP = MachineRegs::kRSP;

constexpr bool IsGReg(MachineReg r) {
  return r.reg() >= MachineRegs::kR8.reg() && r.reg() <= MachineRegs::kR15.reg();
}

constexpr bool IsGReg(const MachineRegClass* c) {
  return IsGReg(c->regs[0]);
}

constexpr bool IsXReg(MachineReg r) {
  return r.reg() >= MachineRegs::kXMM0.reg() && r.reg() <= MachineRegs::kXMM15.reg();
}

constexpr bool IsXReg(const MachineRegClass* c) {
  return IsXReg(c->regs[0]);
}

// Context loads and stores use rbp as base.
inline constexpr auto kCPUStatePointer = MachineRegs::kRBP;

// Operands as listed in x86_operands.
enum class X86Operand : uint8_t {
  kRegisterOperand = 0,                   // Regular register.
  kImplicitRegisterOperand = 1,           // Implicit register.
  kSSAUseDefRegisterOperand = 2,          // SSA-split use+def register operand. Two registers.
  kImplicitSSAUseDefRegisterOperand = 3,  // SSA-split use+def register implicit operand.
  kMemoryOperand = 4,  // Memory operand. Zero, one or two registers as opcode signifies.
  kCondition = 5,      // Contion, like in CMOV.
  kImmediate = 6,      // Immediate.
  kComment = 7,        // Comment. Only used for debug print.
};

template <size_t kMaxMachineRegOperands>
struct MachineInsnInfo {
  MachineOpcode opcode;
  int num_reg_operands;
  std::array<MachineRegKind, kMaxMachineRegOperands> reg_kinds;
  MachineInsnKind kind;
  constexpr int InputRegistersCount() const {
    int result = 0;
    for (int index = 0; index < num_reg_operands; ++index) {
      if (reg_kinds[index].IsInput()) {
        result++;
      }
    }
    return result;
  }
  constexpr int OutputRegistersCount() const {
    int result = 0;
    for (int index = 0; index < num_reg_operands; ++index) {
      if (reg_kinds[index].IsDef()) {
        result++;
      }
    }
    return result;
  }
};

template <typename MachineInsnInfoClass>
constexpr MachineRegClass MachineRegClassFromMachineInsnInfoClass() {
  return []<typename... RegisterClass>(const std::tuple<RegisterClass...>&) -> MachineRegClass {
    return {
        .debug_name = MachineInsnInfoClass::kName,
        .reg_size = MachineInsnInfoClass::kSizeInBits / 8,
        .reg_mask = ((1ULL << RegisterClass::template kMachineRegId<MachineRegs>.reg()) | ...),
        .num_regs = sizeof...(RegisterClass),
        .regs = {RegisterClass::template kMachineRegId<MachineRegs>...},
    };
  }(typename MachineInsnInfoClass::RegistersList());
}

template <typename MachineInsnInfoClass>
inline constexpr MachineRegClass kRegisterClass =
    MachineRegClassFromMachineInsnInfoClass<MachineInsnInfoClass>();

inline constexpr auto& kRAX = kRegisterClass<device_arch_info::RAX>;
inline constexpr auto& kGeneralReg32 = kRegisterClass<device_arch_info::GeneralReg32>;
inline constexpr auto& kGeneralReg64 = kRegisterClass<device_arch_info::GeneralReg64>;
inline constexpr auto& kFpReg32 = kRegisterClass<device_arch_info::FpReg32>;
inline constexpr auto& kFpReg64 = kRegisterClass<device_arch_info::FpReg64>;
inline constexpr auto& kXmmReg = kRegisterClass<device_arch_info::XmmReg>;
inline constexpr auto& kFLAGS = kRegisterClass<device_arch_info::FLAGS>;

// Note: this logic have to match the logic of Copy::GetRegInfoByRegClass in x86_64/code.cc
constexpr const MachineRegClass* GetRegClassForCopy(const MachineRegClass* reg_class) {
  if (IsGReg(reg_class)) {
    if (reg_class->reg_size <= int{sizeof(int32_t)}) {
      return &kGeneralReg32;
    } else {
      CHECK_EQ(reg_class->reg_size, int{sizeof(int64_t)});
      return &kGeneralReg64;
    }
  } else if (IsXReg(reg_class)) {
    if (reg_class->reg_size > int{sizeof(int64_t)}) {
      CHECK_EQ(reg_class->reg_size, int{sizeof(__int128)});
      return &kXmmReg;
    } else if (reg_class->reg_size == int{sizeof(int64_t)}) {
      CHECK_EQ(reg_class->reg_size, int{sizeof(Float64)});
      return &kFpReg64;
    } else {
      CHECK_EQ(reg_class->reg_size, int{sizeof(Float32)});
      return &kFpReg32;
    }
  } else {
    CHECK_EQ(reg_class, &kFLAGS);
    return &kFLAGS;
  }
}

class MachineInsnX86_64 : public MachineInsn {
 public:
  ~MachineInsnX86_64() override {
    // No code here - will never be called!
  }

  Assembler::ScaleFactor scale() const { return x86_64_insn_info_.scale_; }

  Assembler::ScaleFactor scale2() const { return x86_64_insn_info_.scale2_; }

  Assembler::ScaleFactor scale3() const { return x86_64_insn_info_.scale3_; }

  uint32_t disp() const { return x86_64_insn_info_.disp_; }

  uint32_t disp2() const { return x86_64_insn_info_.disp2_; }

  uint32_t disp3() const { return x86_64_insn_info_.disp3_; }

  Assembler::Condition cond() const { return x86_64_insn_info_.cond_; }

  uint64_t imm() const { return x86_64_insn_info_.imm_; }

  void set_scale(Assembler::ScaleFactor scale) { x86_64_insn_info_.scale_ = scale; }

  void set_scale2(Assembler::ScaleFactor scale2) { x86_64_insn_info_.scale2_ = scale2; }

  void set_scale3(Assembler::ScaleFactor scale3) { x86_64_insn_info_.scale3_ = scale3; }

  void set_disp(uint32_t disp) { x86_64_insn_info_.disp_ = disp; }

  void set_disp2(uint32_t disp2) { x86_64_insn_info_.disp2_ = disp2; }

  void set_disp3(uint32_t disp3) { x86_64_insn_info_.disp3_ = disp3; }

  void set_cond(Assembler::Condition cond) { x86_64_insn_info_.cond_ = cond; }

  void set_imm(uint64_t imm) { x86_64_insn_info_.imm_ = imm; }

 protected:
  explicit MachineInsnX86_64(const MachineInsnX86_64& other, MachineReg* regs)
      : MachineInsn(other, regs), x86_64_insn_info_(other.x86_64_insn_info_) {}

  template <size_t kMaxMachineRegOperands>
  explicit MachineInsnX86_64(const MachineInsnInfo<kMaxMachineRegOperands>* info, MachineReg* regs)
      : MachineInsn(info->opcode,
                    info->num_reg_operands,
                    std::begin(info->reg_kinds),
                    regs,
                    info->kind) {}

 private:
  struct {
    Assembler::ScaleFactor scale_ = Assembler::kTimesOne;
    Assembler::ScaleFactor scale2_ = Assembler::kTimesOne;
    Assembler::ScaleFactor scale3_ = Assembler::kTimesOne;
    Assembler::Condition cond_;
    uint32_t disp_;
    uint32_t disp2_;
    uint32_t disp3_;
    uint64_t imm_;
  } x86_64_insn_info_;
};

// Syntax sugar.
inline const MachineInsnX86_64* AsMachineInsnX86_64(const MachineInsn* insn) {
  return static_cast<const MachineInsnX86_64*>(insn);
}

inline MachineInsnX86_64* AsMachineInsnX86_64(MachineInsn* insn) {
  return static_cast<MachineInsnX86_64*>(insn);
}

class Enter final : public MachineInsnX86_64 {
 public:
  explicit Enter();

  [[nodiscard]] std::string GetDebugString() const override;
  void Emit(CodeEmitter* as) const override;

 private:
  struct {
    MachineReg regs_[6];
  } x86_64_insn_info_;

  friend Enter* NewInArena<Enter, const Enter&>(Arena*, const Enter&);
  Enter(const Enter&);
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
};

[[clang::noinline]] std::string GetDebugString(const MachineInsnX86_64& insn,
                                               const char* mnemo,
                                               size_t x86_operands_count,
                                               const X86Operand x86_operands[]);

enum SSAMode {
  // We use these for bit encoding so it's important the the specific values are assigned.
  kNoSSA = 0,
  kSSA = 1,
};

template <typename DeviceInsnInfo>
inline constexpr bool kNonSSA =
    TypesToValues::All<typename DeviceInsnInfo::Operands>([]<typename Operand>() {
      return !device_arch_info::kIsRegister<Operand> ||
             Operand::kUsage != device_arch_info::kUseDef;
    });

template <typename DeviceInsnInfo>
inline constexpr auto kForceSSA =
    std::conditional_t<kNonSSA<DeviceInsnInfo>, MetaValue<false>, MetaValue<kSSA>>::kValue;

template <typename DeviceInsnInfo>
inline constexpr auto kForceNoSSA =
    std::conditional_t<kNonSSA<DeviceInsnInfo>, MetaValue<false>, MetaValue<kNoSSA>>::kValue;

template <typename DeviceInsnInfo_,
          // Note kSSAMode = false is default value and means instruction is natively SSA-compliant
          // (== no use_def operands). Only kSSA and kNoSSA should be specified explicitly.
          auto kSSAMode = false>
class MachineInsn final : public MachineInsnX86_64 {
  struct OperandIndexes {
    // Note: we never are supposed to read unused values they are there to trigger compile-time
    // error on tuple element access  if that would ever happen, by accident.
    size_t input_index;   // ~std::size_t{0} is not used.
    size_t output_index;  // ~std::size_t{0} is not used.
  };
  // Precalculate indexes — see http://go/berberis-tuples#mystery-of-valuestotypes-and-metavalues
  // for explanation about why we need that separate pass.
  static constexpr auto kBindingIndexes = ValuesToValues::Zip(
      std::move(kTupleMetaTypes<typename DeviceInsnInfo_::Operands>),
      ValuesToTypes::MetaValues<ValuesToValues::ToArray<OperandIndexes>(
          TypesToValues::MapWithTemporary<typename DeviceInsnInfo_::Operands,
                                          /* in_index, out_index = */ OperandIndexes>(
              []<typename Operand>(OperandIndexes& indexes) -> OperandIndexes {
                auto& [in_index, out_index] = indexes;
                // Only register operands can be outputs.
                if constexpr (device_arch_info::kIsRegister<Operand>) {
                  if constexpr (Operand::kUsage == device_arch_info::kUse) {
                    return {in_index++, ~std::size_t{0}};
                  } else if constexpr (Operand::kUsage == device_arch_info::kDef ||
                                       Operand::kUsage == device_arch_info::kDefEarlyClobber) {
                    return {~std::size_t{0}, out_index++};
                  } else {
                    static_assert(Operand::kUsage == device_arch_info::kUseDef);
                    return {in_index++, out_index++};
                  }
                } else {
                  return {in_index++, ~std::size_t{0}};
                }
              }))>{});
  // Default bindings: all input arguments are InArg/ImmArg, all output registers are OutArgs,
  // kUseDef arguments are InOutArg.
  static constexpr auto kBindingsInfo = ValuesToValues::Map(
      kBindingIndexes,
      []<typename Operand, OperandIndexes kIndexes>(
          std::pair<const MetaType<Operand>, MetaValue<kIndexes>>) {
        if constexpr (device_arch_info::kIsComment<Operand> ||
                      device_arch_info::kIsCondition<Operand> ||
                      device_arch_info::kIsImmediate<Operand>) {
          return kMetaType<ImmArg<kIndexes.input_index, typename Operand::Class::Type>>;
        } else if constexpr (device_arch_info::kIsRegister<Operand>) {
          if constexpr (Operand::kUsage == device_arch_info::kUse) {
            return kMetaType<InArg<kIndexes.input_index>>;
          } else if constexpr (Operand::kUsage == device_arch_info::kDef ||
                               Operand::kUsage == device_arch_info::kDefEarlyClobber) {
            return kMetaType<OutArg<kIndexes.output_index>>;
          } else {
            static_assert(Operand::kUsage == device_arch_info::kUseDef);
            return kMetaType<InOutArg<kIndexes.input_index, kIndexes.output_index>>;
          }
        } else {
          // Note: even if memory operand is not kUse we still have to treat it as input operand in
          // IR, because we care about registers that are used to specify memory address, not about
          // actual memory used by that operand.
          static_assert(device_arch_info::kIsMemoryOperand<Operand>);
          return kMetaType<InArg<kIndexes.input_index>>;
        }
      });

 public:
  // This type simplifies constructing this MachineInsn in intrinsic implementations.
  // Note: bindings are always defined for the physical instruction, not an SSA-converted one.
  using BindingsTuple = Type<&kBindingsInfo>;

 private:
  static_assert((std::is_same_v<decltype(kSSAMode), bool> && kSSAMode == false) ||
                    (std::is_same_v<decltype(kSSAMode), enum SSAMode> && kSSAMode == kNoSSA) ||
                    (std::is_same_v<decltype(kSSAMode), enum SSAMode> && kSSAMode == kSSA),
                "Only kSSA and kNoSSA should be used as kSSAMode");
  static_assert(
      std::is_same_v<decltype(kSSAMode), enum SSAMode> != kNonSSA<DeviceInsnInfo_>,
      "Only instructions without kUseDef operands should be used without kSSAMode specification");

  using MachineInsnInfo = x86_64::MachineInsnInfo<std::tuple_size_v<
      TypesToTypes::FlatMap<typename DeviceInsnInfo_::Operands, []<typename Operand> {
        if constexpr (device_arch_info::kIsRegister<Operand>) {
          if constexpr (kSSAMode == kSSA && Operand::kUsage == device_arch_info::kUseDef) {
            return kMetaTypes<MachineReg, MachineReg>;
          } else {
            return kMetaTypes<MachineReg>;
          }
        } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          return kMetaTypes<MachineReg, MachineReg>;
        } else {
          return kMetaTypes<>;
        }
      }>>>;
  static constexpr MachineInsnInfo GenMachineInsnInfoByIndex(int);
  static constexpr std::array<
      MachineInsnInfo,
      1 << (2 * device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands>)>
  GenMachineInsnInfos();
  static constexpr std::array<X86Operand, std::tuple_size_v<typename DeviceInsnInfo_::Operands>>
  GenX86Operands();

 public:
  using DeviceInsnInfo = DeviceInsnInfo_;
  using ConstructorArgsTuple =
      TypesToTypes::FlatMap<typename DeviceInsnInfo::Operands, []<typename Operand> {
        if constexpr (device_arch_info::kIsComment<Operand> ||
                      device_arch_info::kIsCondition<Operand> ||
                      device_arch_info::kIsImmediate<Operand>) {
          return kMetaTypes<typename Operand::Class::Type>;
        } else if constexpr (device_arch_info::kIsRegister<Operand>) {
          if constexpr (kSSAMode == kSSA && Operand::kUsage == device_arch_info::kUseDef) {
            return kMetaTypes<MachineReg, MachineReg>;
          } else {
            return kMetaTypes<MachineReg>;
          }
        } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          return kMetaTypes<const MemoryOperand&>;
        } else {
          static_assert(kDependentTypeFalse<Operand>);
        }
      }>;
  using InputArgsTuple =
      TypesToTypes::FlatMap<typename DeviceInsnInfo::Operands, []<typename Operand> {
        if constexpr (device_arch_info::kIsComment<Operand> ||
                      device_arch_info::kIsCondition<Operand> ||
                      device_arch_info::kIsImmediate<Operand>) {
          return kMetaTypes<typename Operand::Class::Type>;
        } else if constexpr (device_arch_info::kIsRegister<Operand>) {
          if constexpr (Operand::kUsage == device_arch_info::kUse ||
                        Operand::kUsage == device_arch_info::kUseDef) {
            return kMetaTypes<MachineReg>;
          } else if constexpr (Operand::kUsage == device_arch_info::kDef ||
                               Operand::kUsage == device_arch_info::kDefEarlyClobber) {
            return kMetaTypes<>;
          } else {
            static_assert(kDependentValueFalse<Operand::kUsage>);
          }
        } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          return kMetaTypes<const MemoryOperand&>;
        } else {
          static_assert(kDependentTypeFalse<Operand>);
        }
      }>;
  // Note: IR instructions may only produce registers right now, thus tuple is an array, currently.
  using OutputArgsTuple =
      TypesToTypes::FlatMap<typename DeviceInsnInfo::Operands, []<typename Operand> {
        if constexpr (device_arch_info::kIsComment<Operand> ||
                      device_arch_info::kIsCondition<Operand> ||
                      device_arch_info::kIsImmediate<Operand>) {
          return kMetaTypes<>;
        } else if constexpr (device_arch_info::kIsRegister<Operand>) {
          if constexpr (Operand::kUsage == device_arch_info::kDef ||
                        Operand::kUsage == device_arch_info::kDefEarlyClobber ||
                        Operand::kUsage == device_arch_info::kUseDef) {
            return kMetaTypes<MachineReg>;
          } else if constexpr (Operand::kUsage == device_arch_info::kUse) {
            return kMetaTypes<>;
          } else {
            static_assert(kDependentValueFalse<Operand::kUsage>);
          }
        } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          return kMetaTypes<>;
        } else {
          static_assert(kDependentTypeFalse<Operand>);
        }
      }>;

  MachineInsn& operator=(const MachineInsn&) = delete;

  explicit MachineInsn(ConstructorArgsTuple args)
      : MachineInsnX86_64(&GenMachineInsnInfo(args), x86_64_insn_info_.regs_) {
    if constexpr (std::tuple_size_v<ConstructorArgsTuple> > 0) {
      static_assert(device_arch_info::kCountConditions<typename DeviceInsnInfo_::Operands> <= 1);
      static_assert(device_arch_info::kCountImmediates<typename DeviceInsnInfo_::Operands> <= 1);
      static_assert(device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands> +
                        2 * device_arch_info::kCountComments<typename DeviceInsnInfo_::Operands> <=
                    3);
      ValuesToValues::ForEachWithTemporary(
          std::move(args),
          /* reg_idx, mem_idx = */ std::tuple{size_t{0}, size_t{0}},
          [this]<typename ConstructorArg>(ConstructorArg&& arg,
                                          std::tuple<size_t, size_t>& indexes) {
            if constexpr (std::is_same_v<ConstructorArg, Assembler::Condition>) {
              MachineInsnX86_64::set_cond(arg);
            } else if constexpr (std::is_integral_v<ConstructorArg>) {
              MachineInsnX86_64::set_imm(arg);
            } else if constexpr (std::is_same_v<ConstructorArg, const void*>) {
              MachineInsnX86_64::set_imm(bit_cast<int64_t>(arg));
            } else if constexpr (std::is_same_v<ConstructorArg, const char*>) {
              int64_t comment = bit_cast<int64_t>(arg);
              MachineInsnX86_64::set_disp2(static_cast<int32_t>(comment));
              MachineInsnX86_64::set_disp3(static_cast<int32_t>(comment >> 32));
            } else if constexpr (std::is_same_v<ConstructorArg, MachineReg>) {
              auto& [reg_idx, mem_idx] = indexes;
              MachineInsnX86_64::SetRegAt(reg_idx++, arg);
            } else if constexpr (std::is_same_v<ConstructorArg, const MemoryOperand&>) {
              auto& [reg_idx, mem_idx] = indexes;
              if (arg.base != kInvalidMachineReg) {
                MachineInsnX86_64::SetRegAt(reg_idx++, arg.base);
              }
              if (arg.index != kInvalidMachineReg) {
                MachineInsnX86_64::SetRegAt(reg_idx++, arg.index);
              }
              if (++mem_idx == 1) {
                MachineInsnX86_64::set_disp(arg.disp);
                MachineInsnX86_64::set_scale(arg.scale);
              } else if (mem_idx == 2) {
                MachineInsnX86_64::set_disp2(arg.disp);
                MachineInsnX86_64::set_scale2(arg.scale);
              } else if (mem_idx == 3) {
                MachineInsnX86_64::set_disp3(arg.disp);
                MachineInsnX86_64::set_scale3(arg.scale);
              }
            } else {
              static_assert(kDependentTypeFalse<ConstructorArg>);
            }
          });
    }
  }

  static constexpr std::array<X86Operand, std::tuple_size_v<typename DeviceInsnInfo::Operands>>
      kX86Operands = GenX86Operands();
  // Note: kInfo has well-defined meaning – it's information about intrinsic with all MemoryOperand
  // types ignored.
  // This is useful not only for instructions without operands, but also for SSA form: since these
  // registers that are passed into MemoryOperand are always kUse and never kDef or kUseDef we may
  // ignore them in our analysis.
  static constexpr const MachineInsnInfo kInfo = GenMachineInsnInfoByIndex(0);

  int NumRegOperands() {
    if constexpr (device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands> == 0) {
      return kInfo.num_reg_operands;
    } else {
      return kInfo.num_reg_operands + std::popcount(opcode() >> kLowMachineOpcodeBits);
    }
  }

  std::string GetDebugString() const override {
    const char* kName = DeviceInsnInfo::kMnemo;
    return x86_64::GetDebugString(*this, kName, std::size(kX86Operands), std::begin(kX86Operands));
  }

  void Emit(CodeEmitter* as) const override {
    if constexpr (kSSAMode == kSSA) {
      FATAL("Attempt to emit SSA pseudo-instruction");
    } else {
      // Code below assumes that we have at most two memory operands.
      static_assert(device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands> +
                        2 * device_arch_info::kCountComments<typename DeviceInsnInfo_::Operands> <=
                    3);
      std::apply(
          DeviceInsnInfo::kEmitInsnFunc,
          std::tuple_cat(
              std::tuple<CodeEmitter&>{*as},
              TypesToValues::FlatMapWithTemporary<
                  typename DeviceInsnInfo::Operands,
                  /* reg_idx, mem_idx = */ std::tuple<size_t, size_t>>(
                  [this]<typename Operand>([[maybe_unused]] std::tuple<size_t, size_t>& indexes) {
                    auto& [reg_idx, mem_idx] = indexes;
                    if constexpr (device_arch_info::kIsComment<Operand>) {
                      return std::tuple{};
                    } else if constexpr (device_arch_info::kIsCondition<Operand>) {
                      return std::tuple{MachineInsnX86_64::cond()};
                    } else if constexpr (device_arch_info::kIsImmediate<Operand>) {
                      if constexpr (std::is_same_v<typename Operand::Class::Type, const void*>) {
                        return std::tuple{bit_cast<const void*>(MachineInsnX86_64::imm())};
                      } else {
                        return std::tuple{
                            static_cast<typename Operand::Class::Type>(MachineInsnX86_64::imm())};
                      }
                    } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
                      auto [has_base, has_index] = OpcodeHasMemoryBaseIndex(opcode(), mem_idx++);
                      Assembler::Operand operand;
                      if (has_base) {
                        operand.base = GetGReg(MachineInsnX86_64::RegAt(reg_idx++));
                      }
                      if (has_index) {
                        operand.index = GetGReg(MachineInsnX86_64::RegAt(reg_idx++));
                      }
                      if (mem_idx == 1) {
                        if (has_index) {
                          operand.scale = scale();
                        }
                        operand.disp = static_cast<int32_t>(disp());
                      } else if (mem_idx == 2) {
                        if (has_index) {
                          operand.scale = scale2();
                        }
                        operand.disp = static_cast<int32_t>(disp2());
                      } else /* mem_idx == 3 */ {
                        CHECK_EQ(mem_idx, 3);
                        if (has_index) {
                          operand.scale = scale3();
                        }
                        operand.disp = static_cast<int32_t>(disp3());
                      }
                      return std::tuple{operand};
                    } else if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
                      return reg_idx++, std::tuple{};
                    } else if constexpr (Operand::Class::kAsRegister == 'x') {
                      return std::tuple{GetXReg(MachineInsnX86_64::RegAt(reg_idx++))};
                    } else if constexpr (Operand::Class::kAsRegister == 'r' ||
                                         Operand::Class::kAsRegister == 'q') {
                      return std::tuple{GetGReg(MachineInsnX86_64::RegAt(reg_idx++))};
                    } else {
                      static_assert(kDependentTypeFalse<Operand>);
                    }
                  })));
    }
  }

 private:
  struct {
    MachineReg regs_[std::tuple_size_v<
        TypesToTypes::FlatMap<typename DeviceInsnInfo::Operands, []<typename Operand> {
          if constexpr (device_arch_info::kIsComment<Operand> ||
                        device_arch_info::kIsCondition<Operand> ||
                        device_arch_info::kIsImmediate<Operand>) {
            return kMetaTypes<>;
          } else if constexpr (device_arch_info::kIsRegister<Operand>) {
            if constexpr (kSSAMode == kSSA && Operand::kUsage == device_arch_info::kUseDef) {
              return kMetaTypes<MachineReg, MachineReg>;
            } else {
              return kMetaTypes<MachineReg>;
            }
          } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
            return kMetaTypes<MachineReg, MachineReg>;
          } else {
            static_assert(kDependentTypeFalse<Operand>);
          }
        }>>];
  } x86_64_insn_info_;

  friend MachineInsn* NewInArena<MachineInsn, const MachineInsn&>(Arena*, const MachineInsn&);
  explicit MachineInsn(const MachineInsn& other)
      : MachineInsnX86_64(other, x86_64_insn_info_.regs_),
        x86_64_insn_info_(other.x86_64_insn_info_) {}
  berberis::MachineInsn* Clone(Arena* arena) const override {
    return NewInArena<MachineInsn, const MachineInsn&>(arena, *this);
  }
  MachineInsnList Lower(Arena* arena) const override {
    if constexpr (kSSAMode == kSSA) {
      MachineInsnList result(arena);
      // Code below assumes that we have at most two memory operands.
      static_assert(device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands> +
                        2 * device_arch_info::kCountComments<typename DeviceInsnInfo_::Operands> <=
                    3);
      result.push_back(
          NewInArena<MachineInsn<DeviceInsnInfo, kNoSSA>>(
              arena,
              TypesToValues::MapWithTemporary<
                  typename DeviceInsnInfo::Operands,
                  /* kind_idx, reg_idx, mem_idx = */ std::tuple<size_t, size_t, size_t>>(
                  [&result, arena, this]<typename Operand>(
                      [[maybe_unused]] std::tuple<size_t, size_t, size_t>& indexes) {
                    auto& [kind_idx, reg_idx, mem_idx] = indexes;
                    if constexpr (device_arch_info::kIsComment<Operand>) {
                      return bit_cast<const char*>(static_cast<uint64_t>(MachineInsnX86_64::disp3())
                                                       << 32 |
                                                   MachineInsnX86_64::disp2());
                    } else if constexpr (device_arch_info::kIsCondition<Operand>) {
                      return MachineInsnX86_64::cond();
                    } else if constexpr (device_arch_info::kIsImmediate<Operand>) {
                      return MachineInsnX86_64::imm();
                    } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
                      auto [has_base, has_index] = OpcodeHasMemoryBaseIndex(opcode(), mem_idx++);
                      MemoryOperand operand;
                      if (has_base) {
                        operand.base = MachineInsnX86_64::RegAt(reg_idx++);
                      }
                      if (has_index) {
                        operand.index = MachineInsnX86_64::RegAt(reg_idx++);
                      }
                      if (mem_idx == 1) {
                        if (has_index) {
                          operand.scale = scale();
                        }
                        operand.disp = static_cast<int32_t>(disp());
                      } else if (mem_idx == 2) {
                        if (has_index) {
                          operand.scale = scale2();
                        }
                        operand.disp = static_cast<int32_t>(disp2());
                      } else /* mem_idx == 3 */ {
                        CHECK_EQ(mem_idx, 3);
                        if (has_index) {
                          operand.scale = scale3();
                        }
                        operand.disp = static_cast<int32_t>(disp3());
                      }
                      return operand;
                    } else if constexpr (device_arch_info::kIsRegister<Operand>) {
                      if (Operand::kUsage == device_arch_info::kUseDef) {
                        auto dst = MachineInsnX86_64::RegAt(reg_idx++);
                        kind_idx++;
                        auto src = MachineInsnX86_64::RegAt(reg_idx++);
                        if (dst != src) {
                          result.push_back(NewInArena<Copy>(
                              arena,
                              dst,
                              src,
                              kMeta<GetRegClassForCopy(&kRegisterClass<typename Operand::Class>)>));
                        }
                        return dst;
                      } else {
                        kind_idx++;
                        return MachineInsnX86_64::RegAt(reg_idx++);
                      }
                    } else {
                      static_assert(kDependentTypeFalse<Operand>);
                    }
                  })));
      return result;
    } else {
      return {1, NewInArena<MachineInsn, const MachineInsn&>(arena, *this), arena};
    }
  }

  // Ensure that bits that we are using to split opcodes are not used by opcode already.
  // Note: we need to do that with all opcodes, including opcodes without memory operands,
  // to guarantee that memory-using opcodes don't clash with memory non-using opcodes.
  static_assert(!(static_cast<int>(DeviceInsnInfo::template kOpcode<MachineOpcode>) &
                  ((~0) << kLowMachineOpcodeBits)));

  static constexpr auto GetInsnKind() {
    if constexpr (DeviceInsnInfo::kSideEffects) {
      return kMachineInsnSideEffects;
    } else {
      return kMachineInsnDefault;
    }
  }

  static const MachineInsnInfo& GenMachineInsnInfo(ConstructorArgsTuple args) {
    // Code below assumes that we have at most two memory operands.
    static_assert(device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands> +
                      2 * device_arch_info::kCountComments<typename DeviceInsnInfo_::Operands> <=
                  3);
    if constexpr (device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo_::Operands> == 0) {
      return kInfo;
    } else {
      static const auto kInfos = GenMachineInsnInfos();
      return kInfos[ValuesToValues::ProduceWithTemporary(
          std::move(args),
          /* index = */ size_t{0},
          /* current_bit = */ size_t{1},
          []<typename Arg>(Arg&& arg, size_t& index, size_t& current_bit) {
            if constexpr (std::is_same_v<Arg, const MemoryOperand&>) {
              if (arg.base != kInvalidMachineReg) {
                index |= current_bit;
              }
              current_bit <<= 1;
              if (arg.index != kInvalidMachineReg) {
                index |= current_bit;
              }
              current_bit <<= 1;
            } else {
              static_assert(std::is_same_v<Arg, Assembler::Condition> || std::is_integral_v<Arg> ||
                            std::is_same_v<Arg, MachineReg>);
            }
          })];
    }
  }
};

template <typename DeviceInsnInfo, auto kSSAMode>
constexpr auto MachineInsn<DeviceInsnInfo, kSSAMode>::GenMachineInsnInfoByIndex(int info_index)
    -> MachineInsn::MachineInsnInfo {
  MachineInsnInfo result = {
      .opcode = static_cast<MachineOpcode>(DeviceInsnInfo::template kOpcode<MachineOpcode> |
                                           (kSSAMode == kSSA ? 1 : 0) << kSSAOpcodeBit),
      .kind = GetInsnKind()};
  TypesToValues::ForEachWithTemporary<typename DeviceInsnInfo::Operands,
                                      /* mem_operand_bit_pos = */ size_t>(
      [&opcode = result.opcode,
       &num_reg_operands = result.num_reg_operands,
       &reg_kinds = result.reg_kinds,
       &info_index]<typename Operand>(size_t& mem_operand_bit_pos) {
        if constexpr (device_arch_info::kIsComment<Operand>) {
          // Do nothing, comments are not represented in MachineInsnInfo.
        } else if constexpr (device_arch_info::kIsRegister<Operand>) {
          static_assert(MachineRegKind::kDef ==
                        static_cast<MachineRegKind::StandardAccess>(device_arch_info::kDef));
          static_assert(
              MachineRegKind::kDefEarlyClobber ==
              static_cast<MachineRegKind::StandardAccess>(device_arch_info::kDefEarlyClobber));
          static_assert(MachineRegKind::kUse ==
                        static_cast<MachineRegKind::StandardAccess>(device_arch_info::kUse));
          static_assert(MachineRegKind::kUseDef ==
                        static_cast<MachineRegKind::StandardAccess>(device_arch_info::kUseDef));
          if (kSSAMode == kSSA && Operand::kUsage == device_arch_info::kUseDef) {
            reg_kinds[num_reg_operands++] = {&kRegisterClass<typename Operand::Class>,
                                             MachineRegKind::kDef};
            reg_kinds[num_reg_operands++] = {&kRegisterClass<typename Operand::Class>,
                                             MachineRegKind::kUse};
          } else {
            reg_kinds[num_reg_operands++] = {
                &kRegisterClass<typename Operand::Class>,
                static_cast<MachineRegKind::StandardAccess>(Operand::kUsage)};
          }
        } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          if (info_index & (1 << mem_operand_bit_pos)) {
            reg_kinds[num_reg_operands++] = {&kGeneralReg64, MachineRegKind::kUse};
            opcode = static_cast<MachineOpcode>(
                opcode | (1 << (kLowMachineOpcodeBits + mem_operand_bit_pos)));
          }
          mem_operand_bit_pos++;
          if (info_index & (1 << mem_operand_bit_pos)) {
            reg_kinds[num_reg_operands++] = {&kGeneralReg64, MachineRegKind::kUse};
            opcode = static_cast<MachineOpcode>(
                opcode | (1 << (kLowMachineOpcodeBits + mem_operand_bit_pos)));
          }
          mem_operand_bit_pos++;
        }
      });
  return result;
}

template <typename DeviceInsnInfo, auto kSSAMode>
constexpr auto MachineInsn<DeviceInsnInfo, kSSAMode>::GenMachineInsnInfos() -> std::array<
    MachineInsn::MachineInsnInfo,
    1 << (2 * device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo::Operands>)> {
  std::array<MachineInsn::MachineInsnInfo,
             1 << (2 * device_arch_info::kCountMemoryOperands<typename DeviceInsnInfo::Operands>)>
      result;
  for (size_t index = 0; index < result.size(); ++index) {
    result[index] = GenMachineInsnInfoByIndex(index);
  }
  return result;
}

template <typename DeviceInsnInfo, auto kSSAMode>
constexpr auto MachineInsn<DeviceInsnInfo, kSSAMode>::GenX86Operands()
    -> std::array<X86Operand, std::tuple_size_v<typename DeviceInsnInfo::Operands>> {
  return ValuesToValues::ToArray<X86Operand>(
      TypesToValues::Map<typename DeviceInsnInfo::Operands>([]<typename Operand>() {
        if constexpr (device_arch_info::kIsComment<Operand>) {
          return X86Operand::kComment;
        } else if constexpr (device_arch_info::kIsCondition<Operand>) {
          return X86Operand::kCondition;
        } else if constexpr (device_arch_info::kIsImmediate<Operand>) {
          return X86Operand::kImmediate;
        } else if constexpr (device_arch_info::kIsMemoryOperand<Operand>) {
          return X86Operand::kMemoryOperand;
        } else if constexpr (kSSAMode == kSSA && device_arch_info::kIsImplicitReg<Operand> &&
                             Operand::kUsage == device_arch_info::kUseDef) {
          return X86Operand::kImplicitSSAUseDefRegisterOperand;
        } else if constexpr (device_arch_info::kIsImplicitReg<Operand>) {
          return X86Operand::kImplicitRegisterOperand;
        } else if constexpr (kSSAMode == kSSA && device_arch_info::kIsRegister<Operand> &&
                             Operand::kUsage == device_arch_info::kUseDef) {
          return X86Operand::kSSAUseDefRegisterOperand;
        } else if constexpr (device_arch_info::kIsRegister<Operand>) {
          return X86Operand::kRegisterOperand;
        } else {
          static_assert(kDependentTypeFalse<Operand>);
        }
      }));
}

class MachineIR : public berberis::MachineIR {
 public:
  enum class ABI {
    kRegular,
    kOptimizedEnabled,
    kOptimizedDisabled,
  };

  enum class BasicBlockOrder {
    kUnordered,
    kReversePostOrder,
  };

  explicit MachineIR(Arena* arena, ABI abi = ABI::kRegular, int num_vreg = 0)
      : berberis::MachineIR(arena, num_vreg, 0),
        abi_{abi},
        bb_order_(BasicBlockOrder::kUnordered),
        insn_folding_executed_(false),
        contains_irreducible_loops_(false) {}

  void AddEdge(MachineBasicBlock* src, MachineBasicBlock* dst) {
    MachineEdge* edge = NewInArena<MachineEdge>(arena(), arena(), src, dst);
    src->out_edges().push_back(edge);
    dst->in_edges().push_back(edge);
    bb_order_ = BasicBlockOrder::kUnordered;
  }

  [[nodiscard]] bool IsCPUStateGet(const berberis::MachineInsn* insn) const {
    // Insn folding can introduce new insns to the IR which read from CPU state. Thus, once insn
    // folding has been executed IsCPUStateGet calls are no longer valid.
    if (insn_folding_executed_) {
      FATAL("IsCPUStateGet called after insn folding.");
    }
    if (insn->opcode() != kMachineOpMovqRegMemBaseDisp &&
        insn->opcode() != kMachineOpMovdqaXRegMemBaseDisp &&
        insn->opcode() != kMachineOpMovwRegMemBaseDisp &&
        insn->opcode() != kMachineOpMovsdXRegMemBaseDisp) {
      return false;
    }

    auto x86_insn = AsMachineInsnX86_64(insn);

    // Check that it is not for ThreadState fields outside of CPUState.
    if (x86_insn->disp() >= sizeof(CPUState)) {
      return false;
    }

    // reservation_value is loaded in HeavyOptimizerFrontend::AtomicLoad and written
    // in HeavyOptimizerFrontend::AtomicStore partially (for performance
    // reasons), which is not supported by our context optimizer.
    auto reservation_value_offset = offsetof(ThreadState, cpu.reservation_value);
    if (x86_insn->disp() >= reservation_value_offset &&
        x86_insn->disp() < reservation_value_offset + sizeof(Reservation)) {
      return false;
    }

    return x86_insn->RegAt(1) == kCPUStatePointer;
  }

  [[nodiscard]] bool IsCPUStatePut(const berberis::MachineInsn* insn) const {
    // Insn folding can introduce new insns to the IR which write to CPU state. Thus, once insn
    // folding has been executed IsCPUStatePut calls are no longer valid.
    if (insn_folding_executed_) {
      FATAL("IsCPUStatePut called after insn folding.");
    }
    if (insn->opcode() != kMachineOpMovqMemBaseDispReg &&
        insn->opcode() != kMachineOpMovdqaMemBaseDispXReg &&
        insn->opcode() != kMachineOpMovwMemBaseDispReg &&
        insn->opcode() != kMachineOpMovsdMemBaseDispXReg &&
        insn->opcode() != kMachineOpMovqMemBaseDispImm) {
      return false;
    }

    auto x86_insn = AsMachineInsnX86_64(insn);

    // Check that it is not for ThreadState fields outside of CPUState.
    if (x86_insn->disp() >= sizeof(CPUState)) {
      return false;
    }

    // reservation_value is loaded in HeavyOptimizerFrontend::AtomicLoad and written
    // in HeavyOptimizerFrontend::AtomicStore partially (for performance
    // reasons), which is not supported by our context optimizer.
    auto reservation_value_offset = offsetof(ThreadState, cpu.reservation_value);
    if (x86_insn->disp() >= reservation_value_offset &&
        x86_insn->disp() < reservation_value_offset + sizeof(Reservation)) {
      return false;
    }

    return x86_insn->RegAt(0) == kCPUStatePointer;
  }

  // Instruction iterators are preserved after splitting basic block and moving
  // instructions to the new basic block.
  [[nodiscard]] MachineBasicBlock* SplitBasicBlock(MachineBasicBlock* bb,
                                                   MachineInsnList::iterator insn_it) {
    MachineBasicBlock* new_bb = NewBasicBlock();

    new_bb->insn_list().splice(
        new_bb->insn_list().begin(), bb->insn_list(), insn_it, bb->insn_list().end());
    bb->insn_list().push_back(NewInsn<Branch>(new_bb));

    // Relink out edges from bb.
    for (auto out_edge : bb->out_edges()) {
      out_edge->set_src(new_bb);
    }
    new_bb->out_edges().swap(bb->out_edges());

    AddEdge(bb, new_bb);
    bb_list().push_back(new_bb);
    return new_bb;
  }

  [[nodiscard]] static bool IsControlTransfer(berberis::MachineInsn* insn) {
    return insn->opcode() == kMachineOpBranch || insn->opcode() == kMachineOpCondBranch ||
           insn->opcode() == kMachineOpIndirectJump || insn->opcode() == kMachineOpJump;
  }

  [[nodiscard]] bool contains_irreducible_loops() const { return contains_irreducible_loops_; }

  [[nodiscard]] BasicBlockOrder bb_order() const { return bb_order_; }

  void set_bb_order(BasicBlockOrder order) { bb_order_ = order; }

  [[nodiscard]] ABI abi() const { return abi_; }

  void set_insn_folding_executed() { insn_folding_executed_ = true; }
  void set_contains_irreducible_loops() { contains_irreducible_loops_ = true; }

  using berberis::MachineIR::NewInsn;

  template <typename T>
  [[nodiscard]] T* NewInsn(std::tuple<>) {
    return berberis::MachineIR::template NewInsn<T, std::tuple<>>({});
  }

  template <typename T, typename... Args>
  [[nodiscard]] T* NewInsn(Args&&... args) {
    return berberis::MachineIR::template NewInsn<T, Args...>(std::forward<Args>(args)...);
  }

  BERBERIS_DECLARE_MACHINE_INSN_ADAPTER(
      [[nodiscard]] auto NewInsn,
      ConstructorArgsTuple,
      MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo, kSSAMode>*,
      NewInsn,
      kSSAMode,
      auto kSSAMode = false)

 private:
  ABI abi_;
  BasicBlockOrder bb_order_;
  bool insn_folding_executed_;
  bool contains_irreducible_loops_;
};

}  // namespace x86_64

}  // namespace berberis

#endif  // BERBERIS_BACKEND_X86_64_MACHINE_IR_H_
