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

#include "berberis/backend/x86_64/reg_lifetime.h"

#include <variant>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_analysis.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/arena_vector.h"

namespace berberis::x86_64 {

namespace {

TEST(MachineIRReadFlagsOptimizer, CountRegLifetimeMap) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg reg0 = machine_ir.AllocVReg();
  MachineReg reg1 = machine_ir.AllocVReg();
  MachineReg reg2 = machine_ir.AllocVReg();
  MachineReg reg3 = machine_ir.AllocVReg();

  auto bb = machine_ir.NewBasicBlock();
  bb->live_in().push_back(reg0);
  bb->live_in().push_back(reg1);
  bb->live_out().push_back(reg2);
  builder.StartBasicBlock(bb);
  auto reg0_end = builder.Gen<AddqRegReg>(reg0, reg1, machine_ir.AllocVReg());
  builder.Gen<AddqRegReg>(reg1, reg1, machine_ir.AllocVReg());
  auto reg1_end = builder.Gen<MovqRegReg>(reg3, reg1);
  auto reg2_start = builder.Gen<MovqRegImm>(reg2, 5);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  auto lifetime_map = CountRegLifetimeMap(&machine_ir, bb);
  ASSERT_TRUE(std::holds_alternative<LiveIn>(lifetime_map[reg0].start));
  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg0].end), reg0_end);

  ASSERT_TRUE(std::holds_alternative<LiveIn>(lifetime_map[reg1].start));
  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg1].end), reg1_end);

  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg2].start), reg2_start);
  ASSERT_TRUE(std::holds_alternative<LiveOut>(lifetime_map[reg2].end));

  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg3].start), reg1_end);
  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg3].end), reg1_end);
}

}  // namespace

}  // namespace berberis::x86_64
