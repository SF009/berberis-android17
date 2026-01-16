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

#include "gtest/gtest.h"

#include <cstddef>
#include <variant>

#include "berberis/backend/x86_64/local_guest_context_optimizer.h"

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

namespace {

bool IsOffsetMappedToReg(size_t offset,
                         const x86_64::MemRegUsageMap& mem_reg_map,
                         const MachineReg& reg) {
  return mem_reg_map[offset].has_value() &&
         std::holds_alternative<MachineReg>(mem_reg_map.at(offset).value().value) &&
         (std::get<MachineReg>(mem_reg_map.at(offset).value().value) == reg);
}

TEST(MachineIRLocalGuestContextOptimizer, UnmapOlderThan) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<x86_64::MovqRegImm>(machine_ir.AllocVReg(), 0);
  builder.GenPut(GetThreadStateRegOffset(0), reg1);
  builder.GenGet(reg2, GetThreadStateRegOffset(1));
  builder.Gen<Jump>(kNullGuestAddr);

  auto optimizer = x86_64::LocalGuestContextOptimizer(&machine_ir);
  optimizer.RemoveLocalGuestContextAccesses(x86_64::OptimizeLocalParams());
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  auto& mem_reg_map = optimizer.GetMemRegUsageMapForTesting();
  ASSERT_TRUE(optimizer.GetLifetimeCounterForTesting().LifetimeAt(reg1).has_value());
  ASSERT_TRUE(IsOffsetMappedToReg(GetThreadStateRegOffset(0), mem_reg_map, reg1));

  // Try clearing an older pos which should do nothing.
  optimizer.UnmapOlderThan(0, x86_64::RegType::kGeneral);
  ASSERT_TRUE(IsOffsetMappedToReg(GetThreadStateRegOffset(0), mem_reg_map, reg1));

  optimizer.UnmapOlderThan(2, x86_64::RegType::kGeneral);
  EXPECT_FALSE(mem_reg_map[GetThreadStateRegOffset(0)].has_value());
  EXPECT_TRUE(mem_reg_map[GetThreadStateRegOffset(1)].has_value());
}

TEST(MachineIRLocalGuestContextOptimizer, RemoveReadAfterWrite) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();
  builder.GenPut(GetThreadStateRegOffset(0), reg1);
  builder.GenGet(reg2, GetThreadStateRegOffset(0));
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 3UL);

  auto* store_insn = *bb->insn_list().begin();
  ASSERT_EQ(store_insn->opcode(), kMachineOpMovqMemBaseDispReg);
  auto disp = x86_64::AsMachineInsnX86_64(store_insn)->disp();
  ASSERT_EQ(disp, berberis::GetThreadStateRegOffset(0));
  auto replaced_reg = store_insn->RegAt(1);
  ASSERT_EQ(store_insn->RegAt(0), x86_64::kMachineRegRBP);

  auto* load_copy_insn = *std::next(bb->insn_list().begin());
  ASSERT_EQ(load_copy_insn->opcode(), kMachineOpCopy);
  ASSERT_EQ(load_copy_insn->RegAt(0), reg2);
  ASSERT_EQ(load_copy_insn->RegAt(1), replaced_reg);
}

TEST(MachineIRLocalGuestContextOptimizer, RemoveReadAfterWriteImmediate) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  builder.GenPutImm(GetThreadStateRegOffset(0), 6);
  builder.GenGet(reg1, GetThreadStateRegOffset(0));
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 3UL);

  auto* store_insn = *bb->insn_list().begin();
  ASSERT_EQ(store_insn->opcode(), kMachineOpMovqMemBaseDispImm);
  auto disp = x86_64::AsMachineInsnX86_64(store_insn)->disp();
  ASSERT_EQ(disp, berberis::GetThreadStateRegOffset(0));
  ASSERT_EQ(store_insn->RegAt(0), x86_64::kMachineRegRBP);

  auto* load_copy_insn = *std::next(bb->insn_list().begin());
  ASSERT_EQ(load_copy_insn->opcode(), kMachineOpMovqRegImm);
  ASSERT_EQ(load_copy_insn->RegAt(0), reg1);
  uint64_t imm = x86_64::AsMachineInsnX86_64(store_insn)->imm();
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(load_copy_insn)->imm(), imm);
}

TEST(MachineIRLocalGuestContextOptimizer, RemoveReadAfterRead) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();
  builder.GenGet(reg1, GetThreadStateRegOffset(0));
  builder.GenGet(reg2, GetThreadStateRegOffset(0));
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 3UL);
  auto* load_insn = *bb->insn_list().begin();
  ASSERT_EQ(load_insn->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(load_insn)->disp(), berberis::GetThreadStateRegOffset(0));
  ASSERT_EQ(load_insn->RegAt(0), reg1);
  ASSERT_EQ(load_insn->RegAt(1), x86_64::kMachineRegRBP);

  auto* copy_insn = *std::next(bb->insn_list().begin());
  ASSERT_EQ(copy_insn->opcode(), kMachineOpCopy);
  ASSERT_EQ(copy_insn->RegAt(0), reg2);
  ASSERT_EQ(copy_insn->RegAt(1), reg1);
}

TEST(MachineIRLocalGuestContextOptimizer, RemoveWriteBeforeWrite) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();
  builder.GenPut(GetThreadStateRegOffset(0), reg1);
  builder.GenPut(GetThreadStateRegOffset(0), reg2);
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 2UL);
  auto* store_insn = *bb->insn_list().begin();
  ASSERT_EQ(store_insn->opcode(), kMachineOpMovqMemBaseDispReg);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(store_insn)->disp(), berberis::GetThreadStateRegOffset(0));
  ASSERT_EQ(store_insn->RegAt(1), reg2);
  ASSERT_EQ(store_insn->RegAt(0), x86_64::kMachineRegRBP);
}

TEST(MachineIRLocalGuestContextOptimizer, RemoveWriteImmediateBeforeWrite) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();
  builder.GenPutImm(GetThreadStateRegOffset(0), 5);
  builder.GenGet(reg2, GetThreadStateRegOffset(0));
  builder.GenPut(GetThreadStateRegOffset(0), reg1);
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 3UL);

  auto* load_insn = *bb->insn_list().begin();
  ASSERT_EQ(load_insn->opcode(), kMachineOpMovqRegImm);
  ASSERT_EQ(load_insn->RegAt(0), reg2);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(load_insn)->imm(), 5UL);

  auto* store_insn = *std::next(bb->insn_list().begin());
  ASSERT_EQ(store_insn->opcode(), kMachineOpMovqMemBaseDispReg);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(store_insn)->disp(), berberis::GetThreadStateRegOffset(0));
  ASSERT_EQ(store_insn->RegAt(1), reg1);
  ASSERT_EQ(store_insn->RegAt(0), x86_64::kMachineRegRBP);
}

TEST(MachineIRLocalGuestContextOptimizer, RemoveWriteBeforeWriteImmediate) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();
  builder.GenPut(GetThreadStateRegOffset(0), reg1);
  builder.GenGet(reg2, GetThreadStateRegOffset(0));
  builder.GenPutImm(GetThreadStateRegOffset(0), 5);
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 3UL);

  auto* load_insn = *bb->insn_list().begin();
  ASSERT_EQ(load_insn->opcode(), kMachineOpCopy);
  ASSERT_EQ(load_insn->RegAt(0), reg2);
  ASSERT_EQ(load_insn->RegAt(1), reg1);

  auto* store_insn = *std::next(bb->insn_list().begin());
  ASSERT_EQ(store_insn->opcode(), kMachineOpMovqMemBaseDispImm);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(store_insn)->disp(), berberis::GetThreadStateRegOffset(0));
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(store_insn)->imm(), 5UL);
  ASSERT_EQ(store_insn->RegAt(0), x86_64::kMachineRegRBP);
}

TEST(MachineIRLocalGuestContextOptimizer, DoNotRemoveAccessToMonitorValue) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();
  constexpr auto offset = offsetof(ProcessState, cpu.reservation_value);
  builder.Gen<x86_64::MovqOpReg>({.base = x86_64::kMachineRegRBP, .disp = offset}, reg1);
  builder.Gen<x86_64::MovqOpReg>({.base = x86_64::kMachineRegRBP, .disp = offset}, reg2);
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir);
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ(bb->insn_list().size(), 3UL);
  auto* store_insn_1 = *bb->insn_list().begin();
  ASSERT_EQ(store_insn_1->opcode(), kMachineOpMovqMemBaseDispReg);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(store_insn_1)->disp(), offset);

  auto* store_insn_2 = *std::next(bb->insn_list().begin());
  ASSERT_EQ(store_insn_2->opcode(), kMachineOpMovqMemBaseDispReg);
  ASSERT_EQ(x86_64::AsMachineInsnX86_64(store_insn_2)->disp(), offset);
}

TEST(MachineIRLocalGuestContextOptimizer, LimitRegisters) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  auto reg0 = machine_ir.AllocVReg();
  auto reg1 = machine_ir.AllocVReg();
  auto xreg0 = machine_ir.AllocVReg();
  auto xreg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(1));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(1));
  builder.Gen<x86_64::MovqRegImm>(reg0, 0);
  builder.Gen<x86_64::MovqRegImm>(reg1, 0);
  builder.Gen<x86_64::AddqRegReg, x86_64::kNoSSA>(reg0, reg1, x86_64::kMachineRegFLAGS);
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(1));

  if (DoesCpuStateHaveDedicatedSimdRegs()) {
    builder.GenGet(machine_ir.AllocVReg(), GetThreadStateSimdRegOffset(5));
    builder.GenGet(machine_ir.AllocVReg(), GetThreadStateSimdRegOffset(2));
    builder.GenGet(machine_ir.AllocVReg(), GetThreadStateSimdRegOffset(5));
    builder.GenGet(machine_ir.AllocVReg(), GetThreadStateSimdRegOffset(2));
    builder.Gen<x86_64::PxorXRegXReg, x86_64::kNoSSA>(xreg0, xreg0);
    builder.Gen<x86_64::PxorXRegXReg, x86_64::kNoSSA>(xreg1, xreg1);
    builder.GenGet(machine_ir.AllocVReg(), GetThreadStateSimdRegOffset(0));
    builder.GenGet(machine_ir.AllocVReg(), GetThreadStateSimdRegOffset(5));
    builder.Gen<x86_64::PxorXRegXReg, x86_64::kNoSSA>(xreg0, xreg1);
  }
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir,
                                          x86_64::OptimizeLocalParams{
                                              .general_reg_limit = 2,
                                              .simd_reg_limit = 2,
                                          });
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  // Check instructions with general regs replaced.
  auto insn_it = bb->insn_list().begin();
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
  // Last Get shouldn't get replaced because we go over the reg limit.
  insn_it = std::next(insn_it, 3);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);

  // Check instructions with simd regs replaced.
  if (DoesCpuStateHaveDedicatedSimdRegs()) {
    ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
    ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
    ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
    ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
    insn_it = std::next(insn_it, 2);
    // Should not be replaced since we would be over register limit.
    ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
    ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);

    insn_it++;
    ASSERT_EQ((*insn_it)->opcode(), kMachineOpJump);
  }
}

// Test that we correctly clear mapping when the optimization pushes us over reg
// limit.
TEST(MachineIRLocalGuestContextOptimizer, LimitRegistersAfterOptimizing) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  auto reg0 = machine_ir.AllocVReg();
  auto reg1 = machine_ir.AllocVReg();
  auto reg2 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.GenGet(reg0, GetThreadStateRegOffset(0));
  builder.GenGet(reg1, GetThreadStateRegOffset(1));
  builder.GenGet(reg2, GetThreadStateRegOffset(2));
  builder.Gen<x86_64::AddqRegReg, x86_64::kNoSSA>(
      machine_ir.AllocVReg(), machine_ir.AllocVReg(), x86_64::kMachineRegFLAGS);
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(0));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(1));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(2));
  builder.Gen<Copy>(machine_ir.AllocVReg(), reg2, kMeta<&x86_64::kGeneralReg64>);
  builder.Gen<Jump>(kNullGuestAddr);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir,
                                          x86_64::OptimizeLocalParams{
                                              .general_reg_limit = 4,
                                              .simd_reg_limit = 2,
                                          });
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  // Check instructions with general regs replaced.
  auto insn_it = bb->insn_list().begin();
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  insn_it++;
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
  // This shouldn't be optimized as we should have hit the limit after the
  // previous optimization with reg0, reg2, and the two from Addq.
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  // This one should be optimized still because reg2 lifetime wouldn't be
  // extended.
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
  ASSERT_EQ((*insn_it)->opcode(), kMachineOpJump);
}

TEST(MachineIRLocalGuestContextOptimizer, LimitRegistersWithOptimizedABI) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena, x86_64::MachineIR::ABI::kOptimizedEnabled);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  auto reg0 = machine_ir.AllocVReg();
  auto reg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<x86_64::Enter>();
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(0));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(0));
  builder.Gen<x86_64::MovqRegImm>(reg0, 0);
  builder.Gen<x86_64::MovqRegImm>(reg1, 0);
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(0));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(0));
  builder.Gen<x86_64::XorqRegReg, x86_64::kNoSSA>(reg0, reg1, x86_64::kMachineRegFLAGS);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  // We subtract reg_limit by 6 when using OptimizedABI.
  x86_64::RemoveLocalGuestContextAccesses(&machine_ir,
                                          x86_64::OptimizeLocalParams{
                                              .general_reg_limit = 9,
                                              .simd_reg_limit = 0,
                                          });
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  auto insn_it = bb->insn_list().begin();

  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpEnter);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegImm);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegImm);
  // Nothing else should be optimized as we hit reg limit with rbp, reg0, and reg1.
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpXorqRegReg);
  ASSERT_EQ((*insn_it)->opcode(), kMachineOpJump);
}

// Test that we correctly clear mappings when the block uses too many registers
// not caused by optimizations.
TEST(MachineIRLocalGuestContextOptimizer, UnmapWhenOverused) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  x86_64::MachineIRBuilder builder(&machine_ir);

  auto bb = machine_ir.NewBasicBlock();
  auto reg0 = machine_ir.AllocVReg();
  auto reg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.GenGet(reg0, GetThreadStateRegOffset(0));
  builder.GenGet(reg1, GetThreadStateRegOffset(1));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(0));
  builder.GenGet(machine_ir.AllocVReg(), GetThreadStateRegOffset(1));
  builder.Gen<x86_64::XorqRegReg, x86_64::kNoSSA>(reg0, reg0, x86_64::kMachineRegFLAGS);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  x86_64::RemoveLocalGuestContextAccesses(&machine_ir,
                                          x86_64::OptimizeLocalParams{
                                              .general_reg_limit = 2,
                                              .simd_reg_limit = 0,
                                          });

  auto insn_it = bb->insn_list().begin();
  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  // First copy should still work because reg0 lifetime not extended.
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpCopy);
  // This shouldn't be optimized since this would extend reg1's lifetime.
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpMovqRegMemBaseDisp);
  ASSERT_EQ((*insn_it++)->opcode(), kMachineOpXorqRegReg);
  ASSERT_EQ((*insn_it)->opcode(), kMachineOpJump);
}

}  // namespace

}  // namespace berberis
