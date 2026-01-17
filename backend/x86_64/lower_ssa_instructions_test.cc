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

}  // namespace

}  // namespace berberis::x86_64
