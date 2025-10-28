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

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/guest_state/guest_addr.h"

#include "berberis/backend/x86_64/rename_copy_uses.h"

namespace berberis::x86_64 {

constexpr auto kMachineRegRAX = MachineRegs::kRAX;
constexpr auto kMachineRegRCX = MachineRegs::kRCX;
constexpr auto kMachineRegRBX = MachineRegs::kRBX;

namespace {

TEST(MachineIRRenameCopyUsesMapTest, Basic) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  auto* copy_insn = builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUsesMap map(&machine_ir);
  map.StartBasicBlock(bb);

  // Renaming doesn't do anything for not mapped registers.
  map.RenameUseIfMapped(copy_insn, 1);
  EXPECT_EQ(copy_insn->RegAt(1), vreg2);

  // This should map vreg1 -> vreg2.
  map.ProcessCopy(copy_insn);

  // Now it should rename vreg1.
  map.RenameUseIfMapped(add_insn, 1);
  EXPECT_EQ(add_insn->RegAt(1), vreg2);
}

TEST(MachineIRRenameCopyUsesTest, Basic) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUses(&machine_ir);
  EXPECT_EQ(add_insn->RegAt(1), vreg2);
}

TEST(MachineIRRenameCopyUsesTest, RenameCopyChain) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoCopy>(vreg3, vreg1, 8);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg4, vreg3, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUses(&machine_ir);
  EXPECT_EQ(add_insn->RegAt(1), vreg2);
}

TEST(MachineIRRenameCopyUsesTest, DoNotRenameIfCopySourceRedefined) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<x86_64::SubqRegImm, kNoSSA>(vreg2, 1, kMachineRegFLAGS);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUses(&machine_ir);

  // vreg1 is not renamed since vreg2 is redefined after copy.
  EXPECT_EQ(add_insn->RegAt(1), vreg1);
}

TEST(MachineIRRenameCopyUsesTest, DoNotRenameIfCopyResultRedefined) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<x86_64::SubqRegImm, kNoSSA>(vreg1, 1, kMachineRegFLAGS);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUses(&machine_ir);
  // vreg1 is not renamed since it is redefined after copy.
  EXPECT_EQ(add_insn->RegAt(1), vreg1);
}

TEST(MachineIRRenameCopyUsesTest, DoNotRenameNarrowRegClass) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  auto* shift_insn = builder.Gen<x86_64::ShrqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  // Builder normally doesn't allow constructing CallImmArg without CallImm, so we construct in IR
  // directly.
  auto* call_arg_insn = builder.ir()->NewInsn<CallImmArg>(vreg1, CallImm::RegType::kIntType);
  bb->insn_list().push_back(call_arg_insn);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(bb->insn_list().size(), 4u);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUses(&machine_ir);
  // vreg1 is not renamed since Shrq second operand is CL register - narrow class.
  EXPECT_EQ(shift_insn->RegAt(1), vreg1);
  // vreg1 is not renamed since CallImmArg implicitly has narrow class.
  EXPECT_EQ(call_arg_insn->RegAt(0), vreg1);
}

TEST(MachineIRRenameCopyUsesTest, GracefullyIgnoreHardwareRegs) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(kMachineRegRAX, kMachineRegRBX, 8);
  auto* add_insn =
      builder.Gen<x86_64::AddqRegReg, kNoSSA>(kMachineRegRCX, kMachineRegRAX, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUses(&machine_ir);
  // Nothing is renamed.
  EXPECT_EQ(add_insn->RegAt(1), kMachineRegRAX);
}

TEST(MachineIRRenameCopyUsesTest, RenameCopySourceIfDstIsLiveoutAndSrcIsntLiveOut) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  auto* sub_insn = builder.Gen<x86_64::SubqRegReg, kNoSSA>(vreg4, vreg2, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  bb->live_out().push_back(vreg1);

  RenameCopyUses(&machine_ir);
  // Should not rename vreg1.
  EXPECT_EQ(add_insn->RegAt(1), vreg1);
  // Should rename vreg2.
  EXPECT_EQ(sub_insn->RegAt(1), vreg1);
}

TEST(MachineIRRenameCopyUsesTest, RenameCopyDstIfDstAndSrcAreLiveOut) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  auto* add_insn = builder.Gen<x86_64::AddqRegReg, kNoSSA>(vreg3, vreg1, kMachineRegFLAGS);
  auto* sub_insn = builder.Gen<x86_64::SubqRegReg, kNoSSA>(vreg4, vreg2, kMachineRegFLAGS);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  bb->live_out().push_back(vreg1);
  bb->live_out().push_back(vreg2);

  RenameCopyUses(&machine_ir);
  // Should rename vreg1.
  EXPECT_EQ(add_insn->RegAt(1), vreg2);
  // Should not rename vreg2.
  EXPECT_EQ(sub_insn->RegAt(1), vreg2);
}

TEST(MachineIRRenameCopyUsesTest, FindDuplicateLiveOuts) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoCopy>(vreg3, vreg4, 8);
  builder.Gen<PseudoBranch>(bb2);
  machine_ir.AddEdge(bb1, bb2);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);
  bb1->live_out().push_back(vreg3);
  bb1->live_out().push_back(vreg4);

  builder.StartBasicBlock(bb2);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoCopy>(vreg3, vreg4, 8);
  builder.Gen<PseudoBranch>(bb3);
  machine_ir.AddEdge(bb2, bb3);
  bb2->live_out().push_back(vreg1);
  bb2->live_out().push_back(vreg2);
  bb2->live_out().push_back(vreg1);  // test that repeating live outs are handled correctly.

  builder.StartBasicBlock(bb3);
  builder.Gen<PseudoJump>(kNullGuestAddr);
  bb2->live_out().push_back(vreg1);
  bb2->live_out().push_back(vreg2);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  RenameCopyUsesMap rename_copy_uses_map(&machine_ir);
  DuplicateLiveOutsMap duplicate_live_outs_map(&machine_ir);

  for (auto* bb : machine_ir.bb_list()) {
    RenameCopyUsesInBasicBlock(bb, &rename_copy_uses_map, &duplicate_live_outs_map);
    ComputeDuplicateLiveOuts(&machine_ir, bb, &rename_copy_uses_map, &duplicate_live_outs_map);
  }

  auto CheckDuplicates =
      [&](const berberis::MachineBasicBlock* bb, MachineReg vreg1, MachineReg vreg2) {
        EXPECT_EQ(duplicate_live_outs_map.GetDuplicateForTesting(bb, vreg1), vreg2);
      };

  CheckDuplicates(bb1, vreg1, vreg2);
  CheckDuplicates(bb1, vreg2, kInvalidMachineReg);
  CheckDuplicates(bb1, vreg3, vreg4);
  CheckDuplicates(bb1, vreg4, kInvalidMachineReg);
  EXPECT_TRUE(duplicate_live_outs_map.BasicBlockContainsDuplicateLiveOuts(bb1));

  CheckDuplicates(bb2, vreg1, vreg2);
  CheckDuplicates(bb2, vreg2, kInvalidMachineReg);
  CheckDuplicates(bb2, vreg3, kInvalidMachineReg);
  CheckDuplicates(bb2, vreg4, kInvalidMachineReg);
  EXPECT_TRUE(duplicate_live_outs_map.BasicBlockContainsDuplicateLiveOuts(bb2));

  CheckDuplicates(bb3, vreg1, kInvalidMachineReg);
  CheckDuplicates(bb3, vreg2, kInvalidMachineReg);
  EXPECT_FALSE(duplicate_live_outs_map.BasicBlockContainsDuplicateLiveOuts(bb3));
}

TEST(MachineIRRenameCopyUsesTest, FindRegistersDefinedInBasicBlock) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  auto* insn_1 = builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  auto* insn_2 = builder.Gen<PseudoCopy>(vreg3, vreg4, 8);

  RenameCopyUsesMap rename_copy_uses_map(&machine_ir);
  DuplicateLiveOutsMap duplicate_live_outs_map(&machine_ir);

  auto CheckBasicBlockDefinesRegister =
      [&](const berberis::MachineBasicBlock* bb, MachineReg vreg, bool expected) {
        EXPECT_EQ(duplicate_live_outs_map.BasicBlockDefinesRegisterForTesting(bb, vreg), expected);
      };

  CheckBasicBlockDefinesRegister(bb, vreg1, false);
  CheckBasicBlockDefinesRegister(bb, vreg2, false);
  CheckBasicBlockDefinesRegister(bb, vreg3, false);
  CheckBasicBlockDefinesRegister(bb, vreg4, false);

  duplicate_live_outs_map.ProcessDef(bb, insn_1, 0);
  duplicate_live_outs_map.ProcessDef(bb, insn_2, 0);

  CheckBasicBlockDefinesRegister(bb, vreg1, true);
  CheckBasicBlockDefinesRegister(bb, vreg2, false);
  CheckBasicBlockDefinesRegister(bb, vreg3, true);
  CheckBasicBlockDefinesRegister(bb, vreg4, false);
}

TEST(MachineIRRenameCopyUsesTest, DuplicateLiveInsGetRenamed) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoBranch>(bb2);
  machine_ir.AddEdge(bb1, bb2);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);

  builder.StartBasicBlock(bb2);
  bb2->live_in().push_back(vreg1);
  bb2->live_in().push_back(vreg2);
  auto* add_insn1 = builder.Gen<AddqRegReg>(vreg3, vreg2, flags);
  auto* add_insn2 = builder.Gen<AddqRegReg>(vreg3, vreg1, flags);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);
  RenameCopyUses(&machine_ir);
  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  EXPECT_EQ(add_insn1->RegAt(1), vreg2);
  EXPECT_EQ(add_insn2->RegAt(1), vreg2);

  EXPECT_FALSE(Contains(bb2->live_in(), vreg1));
}

TEST(MachineIRRenameCopyUsesTest, ChainedDuplicateLiveInsGetRenamed) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg2, vreg1, 8);
  builder.Gen<PseudoCopy>(vreg3, vreg2, 8);
  builder.Gen<PseudoBranch>(bb2);
  machine_ir.AddEdge(bb1, bb2);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);
  bb1->live_out().push_back(vreg3);

  builder.StartBasicBlock(bb2);
  bb2->live_in().push_back(vreg1);
  bb2->live_in().push_back(vreg2);
  bb2->live_in().push_back(vreg3);
  auto* add_insn1 = builder.Gen<AddqRegReg>(vreg4, vreg3, flags);
  auto* add_insn2 = builder.Gen<AddqRegReg>(vreg4, vreg2, flags);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);
  RenameCopyUses(&machine_ir);
  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  EXPECT_EQ(add_insn1->RegAt(1), vreg1);
  EXPECT_EQ(add_insn2->RegAt(1), vreg1);

  EXPECT_FALSE(Contains(bb2->live_in(), vreg2));
  EXPECT_FALSE(Contains(bb2->live_in(), vreg3));
}

TEST(MachineIRRenameCopyUsesTest, DuplicateLiveInsDontGetRenamedWhenBBHasOutEdge) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoBranch>(bb2);
  machine_ir.AddEdge(bb1, bb2);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);

  builder.StartBasicBlock(bb2);
  bb2->live_in().push_back(vreg1);
  bb2->live_in().push_back(vreg2);
  auto* add_insn1 = builder.Gen<AddqRegReg>(vreg3, vreg2, flags);
  auto* add_insn2 = builder.Gen<AddqRegReg>(vreg3, vreg1, flags);
  builder.Gen<PseudoBranch>(bb3);
  machine_ir.AddEdge(bb2, bb3);

  builder.StartBasicBlock(bb3);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);
  RenameCopyUses(&machine_ir);
  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  EXPECT_EQ(add_insn1->RegAt(1), vreg2);
  EXPECT_EQ(add_insn2->RegAt(1), vreg1);

  EXPECT_TRUE(Contains(bb2->live_in(), vreg1));
  EXPECT_TRUE(Contains(bb2->live_in(), vreg2));
}

TEST(MachineIRRenameCopyUsesTest, DuplicateLiveInsDontGetRenamedWhenBBHasMultipleInEdges) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoBranch>(bb3);
  machine_ir.AddEdge(bb1, bb3);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);

  builder.StartBasicBlock(bb2);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoBranch>(bb3);
  machine_ir.AddEdge(bb2, bb3);
  bb2->live_out().push_back(vreg1);
  bb2->live_out().push_back(vreg2);

  builder.StartBasicBlock(bb3);
  bb3->live_in().push_back(vreg1);
  bb3->live_in().push_back(vreg2);
  auto bb3_insn1 = builder.Gen<PseudoCopy>(vreg3, vreg2, 8);
  auto bb3_insn2 = builder.Gen<PseudoCopy>(vreg3, vreg1, 8);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);
  RenameCopyUses(&machine_ir);
  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  EXPECT_EQ(bb3_insn1->RegAt(1), vreg2);
  EXPECT_EQ(bb3_insn2->RegAt(1), vreg1);

  EXPECT_TRUE(Contains(bb3->live_in(), vreg1));
  EXPECT_TRUE(Contains(bb3->live_in(), vreg2));
}

TEST(MachineIRRenameCopyUsesTest, DuplicateLiveInsWhichAreOverwrittenDoNotGetRenamed) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoBranch>(bb2);
  machine_ir.AddEdge(bb1, bb2);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);

  builder.StartBasicBlock(bb2);
  bb2->live_in().push_back(vreg1);
  bb2->live_in().push_back(vreg2);
  builder.Gen<AddqRegReg>(vreg3, vreg2, flags);
  auto* bb2_add_insn = builder.Gen<AddqRegReg>(vreg3, vreg1, flags);
  builder.Gen<MovqRegImm>(vreg2, 5);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);
  RenameCopyUses(&machine_ir);
  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  EXPECT_EQ(bb2_add_insn->RegAt(1), vreg1);

  EXPECT_TRUE(Contains(bb2->live_in(), vreg1));
}

TEST(MachineIRRenameCopyUsesTest, DoNotRenameDuplicateRegisterWhichIsMappedToNonLiveIn) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb1);
  builder.Gen<PseudoCopy>(vreg1, vreg2, 8);
  builder.Gen<PseudoCondBranch>(CodeEmitter::Condition::kZero, bb2, bb3, x86_64::kMachineRegFLAGS);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb1, bb3);
  bb1->live_out().push_back(vreg1);
  bb1->live_out().push_back(vreg2);

  builder.StartBasicBlock(bb2);
  bb2->live_in().push_back(vreg1);
  bb2->live_in().push_back(vreg2);
  auto* bb2_insn_1 = builder.Gen<PseudoCopy>(vreg3, vreg1, 8);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  builder.StartBasicBlock(bb3);
  bb3->live_in().push_back(vreg1);
  auto* bb3_insn_1 = builder.Gen<PseudoCopy>(vreg3, vreg1, 8);
  builder.Gen<PseudoJump>(kNullGuestAddr);

  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);
  RenameCopyUses(&machine_ir);
  ASSERT_EQ(CheckMachineIR(machine_ir), kMachineIRCheckSuccess);

  EXPECT_EQ(bb2_insn_1->RegAt(1), vreg2);
  EXPECT_EQ(bb3_insn_1->RegAt(1), vreg1);

  EXPECT_FALSE(Contains(bb2->live_in(), vreg1));
  EXPECT_TRUE(Contains(bb3->live_in(), vreg1));
}

}  // namespace

}  // namespace berberis::x86_64
