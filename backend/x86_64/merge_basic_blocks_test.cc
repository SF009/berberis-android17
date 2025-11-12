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

#include "berberis/backend/x86_64/merge_basic_blocks.h"

#include "berberis/backend/code_emitter.h"  // for CodeEmitter::Condition
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/arena_alloc.h"

namespace berberis::x86_64 {

namespace {

TEST(MergeBasicBlocksTest, MergeBasicBlocks) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  auto* bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb2, bb4);

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  auto bb1_insn1 = builder.Gen<MovqRegImm>(vreg, 5);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  auto bb2_insn1 = builder.Gen<AddqRegImm, kNoSSA>(vreg, 10, flags);
  auto bb2_insn2 = builder.Gen<CondBranch>(
      CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb3);
  builder.Gen<MovqRegImm>(vreg, 6);
  builder.Gen<Jump>(kNullGuestAddr);

  builder.StartBasicBlock(bb4);
  builder.Gen<MovqRegImm>(vreg, 7);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  MergeBasicBlocks(&machine_ir);

  ASSERT_EQ(machine_ir.bb_list().size(), 3UL);

  EXPECT_EQ(bb1->insn_list().size(), 3UL);
  auto merged_bb_insn_1_it = bb1->insn_list().begin();
  EXPECT_EQ(*merged_bb_insn_1_it, bb1_insn1);
  auto merged_bb_insn_2_it = std::next(merged_bb_insn_1_it);
  EXPECT_EQ(*merged_bb_insn_2_it, bb2_insn1);
  auto merged_bb_insn_3_it = std::next(merged_bb_insn_2_it);
  EXPECT_EQ(*merged_bb_insn_3_it, bb2_insn2);

  EXPECT_EQ(bb1->out_edges().size(), 2UL);
  EXPECT_EQ(bb1->out_edges().at(0)->dst(), bb3);
  EXPECT_EQ(bb1->out_edges().at(1)->dst(), bb4);

  EXPECT_EQ(bb3->in_edges().size(), 1UL);
  EXPECT_EQ(bb3->in_edges().at(0)->src(), bb1);

  EXPECT_EQ(bb4->in_edges().size(), 1UL);
  EXPECT_EQ(bb4->in_edges().at(0)->src(), bb1);
}

TEST(MergeBasicBlocksTest, MergeDoesNotOccurWhenPredecessorHasMultipleSuccessors) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  auto* bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb1, bb4);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb2, bb4);

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<MovqRegImm>(vreg, 5);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb2);
  builder.Gen<AddqRegImm, kNoSSA>(vreg, 10, flags);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb3);
  builder.Gen<MovqRegImm>(vreg, 6);
  builder.Gen<Jump>(kNullGuestAddr);

  builder.StartBasicBlock(bb4);
  builder.Gen<MovqRegImm>(vreg, 7);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  MergeBasicBlocks(&machine_ir);

  ASSERT_EQ(machine_ir.bb_list().size(), 4UL);
}

TEST(MergeBasicBlocksTest, MergeDoesNotOccurWhenSuccessorHasMultiplePredecessors) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  auto* bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb2, bb4);
  machine_ir.AddEdge(bb4, bb2);

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<MovqRegImm>(vreg, 5);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<MovqRegImm>(vreg, 6);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb3);
  builder.Gen<MovqRegImm>(vreg, 7);
  builder.Gen<Jump>(kNullGuestAddr);

  builder.StartBasicBlock(bb4);
  builder.Gen<MovqRegImm>(vreg, 8);
  builder.Gen<Branch>(bb2);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  MergeBasicBlocks(&machine_ir);

  ASSERT_EQ(machine_ir.bb_list().size(), 4UL);
}

TEST(MergeBasicBlocksTest, ChainMergesOfConsecutiveBasicBlocks) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  auto bb1_insn1 = builder.Gen<MovqRegImm>(vreg, 5);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  auto bb2_insn1 = builder.Gen<MovqRegImm>(vreg, 6);
  builder.Gen<Branch>(bb3);

  builder.StartBasicBlock(bb3);
  auto bb3_insn1 = builder.Gen<MovqRegImm>(vreg, 7);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  MergeBasicBlocks(&machine_ir);

  ASSERT_EQ(machine_ir.bb_list().size(), 1UL);

  EXPECT_EQ(bb1->insn_list().size(), 4UL);
  auto merged_bb_insn_1_it = bb1->insn_list().begin();
  EXPECT_EQ(*merged_bb_insn_1_it, bb1_insn1);
  auto merged_bb_insn_2_it = std::next(merged_bb_insn_1_it);
  EXPECT_EQ(*merged_bb_insn_2_it, bb2_insn1);
  auto merged_bb_insn_3_it = std::next(merged_bb_insn_2_it);
  EXPECT_EQ(*merged_bb_insn_3_it, bb3_insn1);
}

TEST(MergeBasicBlocksTest, MergeBackwardsEdgeBasicBlocks) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  auto* bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb3);
  machine_ir.AddEdge(bb1, bb4);
  machine_ir.AddEdge(bb3, bb2);

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<MovqRegImm>(vreg, 5);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb2);
  auto bb2_insn1 = builder.Gen<MovqRegImm>(vreg, 6);
  builder.Gen<Jump>(kNullGuestAddr);

  builder.StartBasicBlock(bb3);
  auto bb3_insn1 = builder.Gen<MovqRegImm>(vreg, 7);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb4);
  builder.Gen<MovqRegImm>(vreg, 8);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  MergeBasicBlocks(&machine_ir);

  ASSERT_EQ(machine_ir.bb_list().size(), 3UL);

  EXPECT_EQ(bb3->insn_list().size(), 3UL);
  auto merged_bb_insn_1_it = bb3->insn_list().begin();
  EXPECT_EQ(*merged_bb_insn_1_it, bb3_insn1);
  auto merged_bb_insn_2_it = std::next(merged_bb_insn_1_it);
  EXPECT_EQ(*merged_bb_insn_2_it, bb2_insn1);
}

TEST(MergeBasicBlocksTest, MergeBasicBlocksThrowsErrorIfTriesToMergeIRWithLiveOuts) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  auto* bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb2, bb4);

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<MovqRegImm>(vreg, 5);
  builder.Gen<Branch>(bb2);
  bb1->live_out().push_back(vreg);

  builder.StartBasicBlock(bb2);
  bb2->live_in().push_back(vreg);
  builder.Gen<AddqRegImm, kNoSSA>(vreg, 10, flags);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);
  bb2->live_out().push_back(vreg);

  bb3->live_in().push_back(vreg);
  builder.StartBasicBlock(bb3);
  builder.Gen<MovqRegImm>(vreg, 6);
  builder.Gen<Jump>(kNullGuestAddr);

  bb4->live_in().push_back(vreg);
  builder.StartBasicBlock(bb4);
  builder.Gen<MovqRegImm>(vreg, 7);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  ASSERT_DEATH(MergeBasicBlocks(&machine_ir), "");
}

}  // namespace

}  // namespace berberis::x86_64
