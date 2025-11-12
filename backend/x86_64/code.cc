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

#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/checks.h"
#include "berberis/device_arch_info/x86_64/device_arch_info.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

namespace x86_64 {

namespace {

constexpr MachineInsnInfo<6> kEnterInfo = {
    kMachineOpEnter,
    6,
    {{
        {&kRegisterClass<device_arch_info::R8>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R9>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R10>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R11>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R12>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R13>, MachineRegKind::kDef},
    }},
    // If all guest ABI registers are overwritten without a use in the region
    // Enter may became dead and can be removed.
    // TODO(b/363608817): Try removing side effects declaration.
    kMachineInsnSideEffects};

constexpr MachineInsnInfo<26> kCallImmInfo = {
    kMachineOpCallImm,
    26,
    {{
        {&kRegisterClass<device_arch_info::RAX>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::RDI>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::RSI>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::RDX>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::RCX>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R8>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R9>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R10>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::R11>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM0>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM1>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM2>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM3>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM4>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM5>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM6>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM7>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM8>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM9>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM10>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM11>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM12>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM13>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM14>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::XMM15>, MachineRegKind::kDef},
        {&kRegisterClass<device_arch_info::FLAGS>, MachineRegKind::kDef},
    }},
    kMachineInsnSideEffects};

constexpr MachineInsnInfo<1> kCallImmIntArgInfo = {kMachineOpCallImmArg,
                                                   1,
                                                   {{{&kReg64, MachineRegKind::kUse}}},
                                                   // Is implicitly part of CallImm.
                                                   kMachineInsnSideEffects};

constexpr MachineInsnInfo<1> kCallImmXmmArgInfo = {kMachineOpCallImmArg,
                                                   1,
                                                   {{{&kXmmReg, MachineRegKind::kUse}}},
                                                   // Is implicitly part of CallImm.
                                                   kMachineInsnSideEffects};

constexpr MachineRegKind kPseudoCondBranchInfo[] = {{&kFLAGS, MachineRegKind::kUse}};

constexpr MachineRegKind kPseudoJumpInfoOptimizedABI[] = {
    {&kRegisterClass<device_arch_info::R8>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R9>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R10>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R11>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R12>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R13>, MachineRegKind::kUse},
};

constexpr MachineRegKind kPseudoIndirectJumpInfo[] = {{&kGeneralReg64, MachineRegKind::kUse}};

constexpr MachineRegKind kPseudoIndirectJumpInfoOptimizedABI[] = {
    {&kGeneralReg64, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R8>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R9>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R10>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R11>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R12>, MachineRegKind::kUse},
    {&kRegisterClass<device_arch_info::R13>, MachineRegKind::kUse},
};

constexpr MachineRegKind kPseudoCopyReg32Info[] = {{&kReg32, MachineRegKind::kDef},
                                                   {&kReg32, MachineRegKind::kUse}};

constexpr MachineRegKind kPseudoCopyReg64Info[] = {{&kReg64, MachineRegKind::kDef},
                                                   {&kReg64, MachineRegKind::kUse}};

constexpr MachineRegKind kPseudoCopyXmmInfo[] = {{&kXmmReg, MachineRegKind::kDef},
                                                 {&kXmmReg, MachineRegKind::kUse}};

constexpr MachineRegKind kPseudoDefXmmInfo[] = {{&kXmmReg, MachineRegKind::kDef}};

constexpr MachineRegKind kPseudoDefReg64Info[] = {{&kReg64, MachineRegKind::kDef}};

constexpr MachineRegKind kPseudoReadFlagsInfo[] = {{&kRAX, MachineRegKind::kDef},
                                                   {&kFLAGS, MachineRegKind::kUse}};

constexpr MachineRegKind kPseudoWriteFlagsInfo[] = {{&kRAX, MachineRegKind::kUseDef},
                                                    {&kFLAGS, MachineRegKind::kDef}};

constexpr MachineRegKind kSSAPseudoWriteFlagsInfo[] = {{&kRAX, MachineRegKind::kDef},
                                                       {&kRAX, MachineRegKind::kUse},
                                                       {&kFLAGS, MachineRegKind::kDef}};

}  // namespace

Enter::Enter() : MachineInsnX86_64(&kEnterInfo, x86_64_insn_info_.regs_) {}

Enter::Enter(const Enter& other)
    : MachineInsnX86_64(other, x86_64_insn_info_.regs_),
      x86_64_insn_info_(other.x86_64_insn_info_) {}

berberis::MachineInsn* Enter::Clone(Arena* arena) const {
  return NewInArena<Enter, const Enter&>(arena, *this);
}

MachineInsnList Enter::Lower(Arena* arena) const {
  return {1, NewInArena<Enter, const Enter&>(arena, *this), arena};
}

CallImm::CallImm(uint64_t imm) : MachineInsnX86_64(&kCallImmInfo, x86_64_insn_info_.regs_) {
  set_imm(imm);
}

CallImm::CallImm(const CallImm& other)
    : MachineInsnX86_64(other, x86_64_insn_info_.regs_),
      x86_64_insn_info_(other.x86_64_insn_info_) {}

berberis::MachineInsn* CallImm::Clone(Arena* arena) const {
  return NewInArena<CallImm, const CallImm&>(arena, *this);
}

MachineInsnList CallImm::Lower(Arena* arena) const {
  return {1, NewInArena<CallImm, const CallImm&>(arena, *this), arena};
}

int CallImm::GetIntArgIndex(int i) {
  constexpr int kIntArgIndex[] = {
      1,  // RDI
      2,  // RSI
      3,  // RDX
      4,  // RCX
      5,  // R8
      6,  // R9
  };

  CHECK_LT(static_cast<unsigned>(i), std::size(kIntArgIndex));
  return kIntArgIndex[i];
}

int CallImm::GetXmmArgIndex(int i) {
  constexpr int kXmmArgIndex[] = {
      9,   // XMM0
      10,  // XMM1
      11,  // XMM2
      12,  // XMM3
      13,  // XMM4
      14,  // XMM5
      15,  // XMM6
      16,  // XMM7
  };

  CHECK_LT(static_cast<unsigned>(i), std::size(kXmmArgIndex));
  return kXmmArgIndex[i];
}

int CallImm::GetFlagsArgIndex() {
  return 25;  // FLAGS
}

MachineReg CallImm::IntResultAt(int i) const {
  constexpr int kIntResultIndex[] = {
      0,  // RAX
      3,  // RDX
  };

  CHECK_LT(static_cast<unsigned>(i), std::size(kIntResultIndex));
  return RegAt(kIntResultIndex[i]);
}

MachineReg CallImm::XmmResultAt(int i) const {
  constexpr int kXmmResultIndex[] = {
      9,   // XMM0
      10,  // XMM1
  };

  CHECK_LT(static_cast<unsigned>(i), std::size(kXmmResultIndex));
  return RegAt(kXmmResultIndex[i]);
}

CallImmArg::CallImmArg(MachineReg arg, CallImm::RegType reg_type)
    : MachineInsnX86_64(
          reg_type == CallImm::kIntRegType ? &kCallImmIntArgInfo : &kCallImmXmmArgInfo,
          x86_64_insn_info_.regs_) {
  SetRegAt(0, arg);
}

CallImmArg::CallImmArg(const CallImmArg& other)
    : MachineInsnX86_64(other, x86_64_insn_info_.regs_),
      x86_64_insn_info_(other.x86_64_insn_info_) {}

berberis::MachineInsn* CallImmArg::Clone(Arena* arena) const {
  return NewInArena<CallImmArg, const CallImmArg&>(arena, *this);
}

MachineInsnList CallImmArg::Lower(Arena* arena) const {
  return {1, NewInArena<CallImmArg, const CallImmArg&>(arena, *this), arena};
}

}  // namespace x86_64

const MachineOpcode Branch::kOpcode = kMachineOpBranch;
using Assembler = x86_64::Assembler;

Branch::Branch(const MachineBasicBlock* then_bb)
    : MachineInsn(kMachineOpBranch, 0, nullptr, nullptr, kMachineInsnSideEffects),
      then_bb_(then_bb) {}

MachineInsn* Branch::Clone(Arena* arena) const {
  return NewInArena<Branch, const Branch&>(arena, *this);
}

MachineInsnList Branch::Lower(Arena* arena) const {
  return {1, NewInArena<Branch, const Branch&>(arena, *this), arena};
}

const MachineOpcode CondBranch::kOpcode = kMachineOpCondBranch;

CondBranch::CondBranch(Assembler::Condition cond,
                       const MachineBasicBlock* then_bb,
                       const MachineBasicBlock* else_bb,
                       MachineReg eflags)
    : MachineInsn(kMachineOpCondBranch,
                  1,
                  x86_64::kPseudoCondBranchInfo,
                  &eflags_,
                  kMachineInsnSideEffects),
      cond_(cond),
      then_bb_(then_bb),
      else_bb_(else_bb),
      eflags_(eflags) {}

MachineInsn* CondBranch::Clone(Arena* arena) const {
  return NewInArena<CondBranch, const CondBranch&>(arena, *this);
}

MachineInsnList CondBranch::Lower(Arena* arena) const {
  return {1, NewInArena<CondBranch, const CondBranch&>(arena, *this), arena};
}

Jump::Jump(GuestAddr target, Kind kind)
    : MachineInsn(kMachineOpJump, 0, nullptr, nullptr, kMachineInsnSideEffects),
      target_(target),
      kind_(kind) {}

Jump::Jump(GuestAddr target, WithOptimizedABI, Kind kind)
    : MachineInsn(kMachineOpJump,
                  6,
                  x86_64::kPseudoJumpInfoOptimizedABI,
                  args_,
                  kMachineInsnSideEffects),
      target_(target),
      kind_(kind) {}

MachineInsn* Jump::Clone(Arena* arena) const {
  return NewInArena<Jump, const Jump&>(arena, *this);
}

MachineInsnList Jump::Lower(Arena* arena) const {
  return {1, NewInArena<Jump, const Jump&>(arena, *this), arena};
}

IndirectJump::IndirectJump(MachineReg target)
    : MachineInsn(kMachineOpIndirectJump,
                  1,
                  x86_64::kPseudoIndirectJumpInfo,
                  regs_,
                  kMachineInsnSideEffects) {
  regs_[0] = target;
}

IndirectJump::IndirectJump(MachineReg target, WithOptimizedABI)
    : MachineInsn(kMachineOpIndirectJump,
                  1 + 6,
                  x86_64::kPseudoIndirectJumpInfoOptimizedABI,
                  regs_,
                  kMachineInsnSideEffects) {
  regs_[0] = target;
}

IndirectJump::IndirectJump(const IndirectJump& insn) : MachineInsn(insn, regs_) {
  for (size_t i = 0; i < arraysize(regs_); i++) {
    regs_[i] = insn.regs_[i];
  }
}

MachineInsn* IndirectJump::Clone(Arena* arena) const {
  return NewInArena<IndirectJump, const IndirectJump&>(arena, *this);
}

MachineInsnList IndirectJump::Lower(Arena* arena) const {
  return {1, NewInArena<IndirectJump, const IndirectJump&>(arena, *this), arena};
}

const MachineOpcode Copy::kOpcode = kMachineOpCopy;

// Reg class of correct size is essential for current spill/reload code!!!
Copy::Copy(MachineReg dst, MachineReg src, int size)
    : MachineInsn(kMachineOpCopy,
                  2,
                  size > 8   ? x86_64::kPseudoCopyXmmInfo
                  : size > 4 ? x86_64::kPseudoCopyReg64Info
                             : x86_64::kPseudoCopyReg32Info,
                  regs_,
                  kMachineInsnCopy),
      regs_{dst, src} {}

Copy::Copy(const Copy& insn)
    : MachineInsn(insn, regs_), regs_{insn.regs_[0], insn.regs_[1]} {}

MachineInsn* Copy::Clone(Arena* arena) const {
  return NewInArena<Copy, const Copy&>(arena, *this);
}

MachineInsnList Copy::Lower(Arena* arena) const {
  return {1, NewInArena<Copy, const Copy&>(arena, *this), arena};
}

PseudoDefXReg::PseudoDefXReg(MachineReg reg)
    : MachineInsn(kMachineOpPseudoDefXReg,
                  1,
                  x86_64::kPseudoDefXmmInfo,
                  &reg_,
                  kMachineInsnDefault),
      reg_{reg} {}

PseudoDefXReg::PseudoDefXReg(const PseudoDefXReg& insn)
    : MachineInsn(insn, &reg_), reg_{insn.reg_} {}

MachineInsn* PseudoDefXReg::Clone(Arena* arena) const {
  return NewInArena<PseudoDefXReg, const PseudoDefXReg&>(arena, *this);
}

MachineInsnList PseudoDefXReg::Lower(Arena* arena) const {
  return {1, NewInArena<PseudoDefXReg, const PseudoDefXReg&>(arena, *this), arena};
}

PseudoDefReg::PseudoDefReg(MachineReg reg)
    : MachineInsn(kMachineOpPseudoDefReg,
                  1,
                  x86_64::kPseudoDefReg64Info,
                  &reg_,
                  kMachineInsnDefault),
      reg_{reg} {}

PseudoDefReg::PseudoDefReg(const PseudoDefReg& insn) : MachineInsn(insn, &reg_), reg_{insn.reg_} {}

MachineInsn* PseudoDefReg::Clone(Arena* arena) const {
  return NewInArena<PseudoDefReg, const PseudoDefReg&>(arena, *this);
}

MachineInsnList PseudoDefReg::Lower(Arena* arena) const {
  return {1, NewInArena<PseudoDefReg, const PseudoDefReg&>(arena, *this), arena};
}

const MachineOpcode PseudoReadFlags::kOpcode = kMachineOpPseudoReadFlags;

PseudoReadFlags::PseudoReadFlags(WithOverflowEnum with_overflow, MachineReg dst, MachineReg flags)
    : MachineInsn(kMachineOpPseudoReadFlags,
                  2,
                  x86_64::kPseudoReadFlagsInfo,
                  regs_,
                  kMachineInsnDefault),
      regs_{dst, flags},
      with_overflow_(with_overflow == kWithOverflow) {}

PseudoReadFlags::PseudoReadFlags(const PseudoReadFlags& other)
    : MachineInsn(other, regs_),
      regs_{other.regs_[0], other.regs_[1]},
      with_overflow_{other.with_overflow_} {}

MachineInsn* PseudoReadFlags::Clone(Arena* arena) const {
  return NewInArena<PseudoReadFlags, const PseudoReadFlags&>(arena, *this);
}

MachineInsnList PseudoReadFlags::Lower(Arena* arena) const {
  return {1, NewInArena<PseudoReadFlags, const PseudoReadFlags&>(arena, *this), arena};
}

const MachineOpcode PseudoWriteFlags::kOpcode = kMachineOpPseudoWriteFlags;

PseudoWriteFlags::PseudoWriteFlags(MachineReg src, MachineReg flags)
    : MachineInsn(kMachineOpPseudoWriteFlags,
                  2,
                  x86_64::kPseudoWriteFlagsInfo,
                  regs_,
                  kMachineInsnDefault),
      regs_{src, flags} {}

PseudoWriteFlags::PseudoWriteFlags(const PseudoWriteFlags& insn)
    : MachineInsn(insn), regs_{insn.regs_[0], insn.regs_[1]} {}

MachineInsn* PseudoWriteFlags::Clone(Arena* arena) const {
  return NewInArena<PseudoWriteFlags, const PseudoWriteFlags&>(arena, *this);
}

MachineInsnList PseudoWriteFlags::Lower(Arena* arena) const {
  return {1, NewInArena<PseudoWriteFlags, const PseudoWriteFlags&>(arena, *this), arena};
}

const MachineOpcode SSAPseudoWriteFlags::kOpcode =
    static_cast<MachineOpcode>(kMachineOpPseudoWriteFlags | (x86_64::kSSA << kSSAOpcodeBit));

SSAPseudoWriteFlags::SSAPseudoWriteFlags(MachineReg clobber, MachineReg src, MachineReg flags)
    : MachineInsn(SSAPseudoWriteFlags::kOpcode,
                  3,
                  x86_64::kSSAPseudoWriteFlagsInfo,
                  regs_,
                  kMachineInsnDefault),
      regs_{clobber, src, flags} {}

SSAPseudoWriteFlags::SSAPseudoWriteFlags(const SSAPseudoWriteFlags& insn)
    : MachineInsn(insn), regs_{insn.regs_[0], insn.regs_[1], insn.regs_[2]} {}

MachineInsn* SSAPseudoWriteFlags::Clone(Arena* arena) const {
  return NewInArena<SSAPseudoWriteFlags, const SSAPseudoWriteFlags&>(arena, *this);
}

MachineInsnList SSAPseudoWriteFlags::Lower(Arena* arena) const {
  return {{NewInArena<Copy>(arena, regs_[0], regs_[1], 4),
           NewInArena<PseudoWriteFlags>(arena, regs_[1], regs_[2])},
          arena};
}

}  // namespace berberis
