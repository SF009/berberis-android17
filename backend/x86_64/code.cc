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

constexpr MachineRegKind kPseudoDefReg64Info[] = {{&kReg64, MachineRegKind::kDef}};

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

}  // namespace berberis
