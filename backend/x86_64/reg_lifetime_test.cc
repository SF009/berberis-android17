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

#include "gmock/gmock.h"
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

TEST(MachineIRReadFlagsOptimizer, CountLifetimeCounts) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  x86_64::MachineIRBuilder builder(&machine_ir);

  // Only live_in and live_out - shouldn't be counted because we don't know its
  // type.
  MachineReg unknown_reg = machine_ir.AllocVReg();
  // Unused so shouldn't be counted.
  MachineReg unused_reg = machine_ir.AllocVReg();
  MachineReg reg0 = machine_ir.AllocVReg();
  MachineReg reg1 = machine_ir.AllocVReg();
  MachineReg reg2 = machine_ir.AllocVReg();
  MachineReg reg3 = machine_ir.AllocVReg();
  MachineReg xreg0 = machine_ir.AllocVReg();
  MachineReg xreg1 = machine_ir.AllocVReg();

  auto bb = machine_ir.NewBasicBlock();
  bb->live_in().push_back(unused_reg);
  bb->live_in().push_back(reg0);
  bb->live_in().push_back(unknown_reg);
  bb->live_in().push_back(xreg0);
  bb->live_out().push_back(unknown_reg);
  bb->live_out().push_back(reg2);
  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegImm>(reg0, 1, machine_ir.AllocVReg());
  builder.Gen<MovqRegImm>(reg1, 1);
  builder.Gen<MovqRegImm>(reg2, 1);
  builder.Gen<PxorXRegXReg>(xreg0, xreg1);
  builder.Gen<AddqRegReg>(reg0, reg1, machine_ir.AllocVReg());
  builder.Gen<AddqRegReg>(reg1, reg2, machine_ir.AllocVReg());
  builder.Gen<PxorXRegXReg>(xreg0, xreg1);
  builder.Gen<MovqRegReg>(reg3, reg2);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  auto counts = CountRegLifetimes(&machine_ir, bb);
  auto insn_it = bb->insn_list().begin();

  ASSERT_THAT(counts,
              testing::ElementsAre(
                  // reg0 and xreg0.
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 1, .xmm = 1}),
                  // reg0, reg1, and xreg0
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 2, .xmm = 1}),
                  // reg0, reg1, reg2, and xreg0
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 3, .xmm = 1}),
                  // reg0, reg1, reg2, xreg0, and xreg1
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 3, .xmm = 2}),
                  // reg0, reg1, reg2, xreg0, and xreg1
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 3, .xmm = 2}),
                  // reg1, reg2, xreg0, and xreg1
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 2, .xmm = 2}),
                  // reg2, xreg0, and xreg1
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 1, .xmm = 2}),
                  // reg2 and reg3
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 2, .xmm = 0}),
                  // reg2
                  testing::Pair(*insn_it++, RegLifetimeCount{.general = 1, .xmm = 0})));
}

TEST(MachineIRReadFlagsOptimizer, CountRegLifetimeMap) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg reg0 = machine_ir.AllocVReg();
  MachineReg reg1 = machine_ir.AllocVReg();
  MachineReg reg2 = machine_ir.AllocVReg();
  MachineReg reg3 = machine_ir.AllocVReg();
  MachineReg reg4 = machine_ir.AllocVReg();

  auto bb = machine_ir.NewBasicBlock();
  bb->live_in().push_back(reg0);
  bb->live_in().push_back(reg1);
  bb->live_in().push_back(reg4);
  bb->live_out().push_back(reg2);
  bb->live_out().push_back(reg4);
  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg>(reg0, reg1, machine_ir.AllocVReg());
  auto reg0_end = builder.Gen<AddqRegReg>(reg1, reg1, machine_ir.AllocVReg());
  auto reg3_start = builder.Gen<MovqRegReg>(reg3, reg1);
  auto reg1_end = builder.Gen<PxorXRegXReg>(reg2, reg2);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  auto lifetime_map = CountRegLifetimeMap(&machine_ir, bb);
  ASSERT_TRUE(std::holds_alternative<LiveIn>(lifetime_map[reg0].start));
  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg0].end), reg0_end);
  ASSERT_EQ(lifetime_map[reg0].reg_type, RegType::kGeneral);

  ASSERT_TRUE(std::holds_alternative<LiveIn>(lifetime_map[reg1].start));
  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg1].end), reg1_end);
  ASSERT_EQ(lifetime_map[reg1].reg_type, RegType::kGeneral);

  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg2].start), reg1_end);
  ASSERT_TRUE(std::holds_alternative<LiveOut>(lifetime_map[reg2].end));
  ASSERT_EQ(lifetime_map[reg2].reg_type, RegType::kXmm);

  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg3].start), reg3_start);
  ASSERT_EQ(std::get<berberis::MachineInsn*>(lifetime_map[reg3].end), reg1_end);
  ASSERT_EQ(lifetime_map[reg3].reg_type, RegType::kGeneral);

  ASSERT_TRUE(std::holds_alternative<LiveIn>(lifetime_map[reg4].start));
  ASSERT_TRUE(std::holds_alternative<LiveOut>(lifetime_map[reg4].end));
  ASSERT_EQ(lifetime_map[reg4].reg_type, RegType::kUnknown);
}

}  // namespace

}  // namespace berberis::x86_64
