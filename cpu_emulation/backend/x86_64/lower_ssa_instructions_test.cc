/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "berberis/backend/x86_64/lower_ssa_instructions.h"

#include "berberis/backend/code_emitter.h"  // for CodeEmitter::Condition
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/arena_alloc.h"

namespace berberis::x86_64 {

namespace {

TEST(LowerSSAInstructionsTest, LowerAddqRegReg) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg_dest = machine_ir.AllocVReg();
  MachineReg vreg_src1 = machine_ir.AllocVReg();
  MachineReg vreg_src2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg, kSSA>(vreg_dest, vreg_src1, vreg_src2, flags);

  LowerSSAInstructions(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 2UL);
  EXPECT_EQ(bb->insn_list().front()->opcode(), kMachineOpCopy);
  EXPECT_EQ(bb->insn_list().back()->opcode(), kMachineOpAddqRegReg);
}

TEST(LowerSSAInstructionsCheckTest, NonVReg) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);

  bb->live_in().push_back(MachineRegs::kRAX);
  EXPECT_FALSE(CheckSSA(&machine_ir));
}

TEST(LowerSSAInstructionsCheckTest, HardRegInLiveIns) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);

  auto vreg = machine_ir.AllocVReg();

  bb->live_in().push_back(vreg);
  EXPECT_TRUE(CheckSSA(&machine_ir));
  bb->live_in().push_back(vreg);
  EXPECT_FALSE(CheckSSA(&machine_ir));
}

TEST(LowerSSAInstructionsCheckTest, UseDefReg) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);

  MachineReg vreg_dest = machine_ir.AllocVReg();
  MachineReg vreg_src = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg, kNoSSA>(vreg_dest, vreg_src, flags);

  EXPECT_FALSE(CheckSSA(&machine_ir));
}

TEST(LowerSSAInstructionsCheckTest, HardRegDefReg) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);

  MachineReg vreg_src1 = machine_ir.AllocVReg();
  MachineReg vreg_src2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  // Note: we allow “use” for hardware register to access stack with bp, context with bp and so on.
  // But all “def” registers should be vregs before lowering.
  builder.Gen<AddqRegReg, kSSA>(MachineRegs::kRAX, vreg_src1, vreg_src2, flags);

  EXPECT_FALSE(CheckSSA(&machine_ir));
}

TEST(LowerSSAInstructionsCheckTest, HardRegUseReg) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);

  MachineReg vreg_dest = machine_ir.AllocVReg();
  MachineReg vreg_src = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  // Note: we allow “use” for hardware register to access stack with bp, context with bp and so on.
  // But all “def” registers should be vregs before lowering.
  builder.Gen<AddqRegReg, kSSA>(vreg_dest, MachineRegs::kRAX, vreg_src, flags);

  EXPECT_TRUE(CheckSSA(&machine_ir));
}

TEST(LowerSSAInstructionsCheckTest, DoubleDefReg) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  builder.StartBasicBlock(bb);

  MachineReg vreg_dest = machine_ir.AllocVReg();
  MachineReg vreg_src1 = machine_ir.AllocVReg();
  MachineReg vreg_src2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg, kSSA>(vreg_dest, vreg_src1, vreg_src2, flags);
  builder.Gen<AddqRegReg, kSSA>(vreg_dest, vreg_src1, vreg_src2, flags);

  EXPECT_FALSE(CheckSSA(&machine_ir));
}

}  // namespace

}  // namespace berberis::x86_64
