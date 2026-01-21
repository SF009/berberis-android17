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

#include "berberis/backend/common/lifetime_analysis.h"

#include "berberis/backend/common/lifetime.h"
#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/arena_alloc.h"

#include <iterator>

namespace berberis {

namespace {

class VRegLifetimeAnalysisTest : public ::testing::Test {
 protected:
  VRegLifetimeAnalysisTest() : analysis_(&arena_, kNumVRegs, &lifetimes_) {}

  Arena arena_;
  VRegLifetimeList lifetimes_{&arena_};
  VRegLifetimeAnalysis analysis_;
  static constexpr int kNumVRegs = 3;
  static constexpr bool kCurrentAccessIsInput = true;
  static constexpr bool kCurrentAccessIsNotInput = false;
};

TEST_F(VRegLifetimeAnalysisTest, GetVRegLifetime_NewVReg) {
  MachineReg vreg = MachineReg::CreateVRegFromIndex(0);
  int begin = 10;

  auto* lifetime = analysis_.GetVRegLifetimeForTesting(vreg, begin, kCurrentAccessIsNotInput);

  ASSERT_NE(lifetime, nullptr);
  EXPECT_EQ(lifetimes_.size(), 1u);
  EXPECT_EQ(lifetime, &lifetimes_.back());
  EXPECT_EQ(lifetime->LastLiveRangeBegin(), begin);
}

TEST_F(VRegLifetimeAnalysisTest, GetVRegLifetime_ExistingVRegSameBB) {
  MachineReg vreg = MachineReg::CreateVRegFromIndex(1);
  int begin1 = 20;

  auto* lifetime1 = analysis_.GetVRegLifetimeForTesting(vreg, begin1, kCurrentAccessIsNotInput);
  ASSERT_NE(lifetime1, nullptr);
  EXPECT_EQ(lifetime1->LastLiveRangeBegin(), begin1);

  int begin2 = 25;
  auto* lifetime2 = analysis_.GetVRegLifetimeForTesting(vreg, begin2, kCurrentAccessIsNotInput);
  ASSERT_EQ(lifetime1, lifetime2);
  EXPECT_EQ(lifetimes_.size(), 1u);
  // LastLiveRangeBegin should not change as we are in the same basic block.
  EXPECT_EQ(lifetime2->LastLiveRangeBegin(), begin1);
}

TEST_F(VRegLifetimeAnalysisTest, GetVRegLifetime_ExistingVRegNewBB) {
  MachineReg vreg = MachineReg::CreateVRegFromIndex(2);
  int begin1 = 30;
  analysis_.SetClockForTesting(begin1);

  auto* lifetime1 = analysis_.GetVRegLifetimeForTesting(vreg, begin1, kCurrentAccessIsNotInput);
  ASSERT_NE(lifetime1, nullptr);
  EXPECT_EQ(lifetime1->LastLiveRangeBegin(), begin1);

  // Simulate moving to a new basic block.
  analysis_.TickForTesting();
  analysis_.EndBasicBlock();

  int begin2 = 40;
  auto* lifetime2 = analysis_.GetVRegLifetimeForTesting(vreg, begin2, kCurrentAccessIsNotInput);
  ASSERT_EQ(lifetime1, lifetime2);
  EXPECT_EQ(lifetimes_.size(), 1u);
  // LastLiveRangeBegin should be updated for the new basic block.
  EXPECT_EQ(lifetime2->LastLiveRangeBegin(), begin2);
}

TEST_F(VRegLifetimeAnalysisTest, GetVRegLifetime_VRegIsInputButUndefinedInBB) {
  MachineReg vreg = MachineReg::CreateVRegFromIndex(0);
  auto* lifetime = analysis_.GetVRegLifetimeForTesting(vreg, 0, kCurrentAccessIsInput);
  ASSERT_EQ(lifetime, nullptr);
}

TEST_F(VRegLifetimeAnalysisTest, GetVRegLifetime_ExistingVRegIsInputButUndefinedInBB) {
  MachineReg vreg = MachineReg::CreateVRegFromIndex(0);
  int begin1 = 0;
  analysis_.SetClockForTesting(begin1);

  auto* lifetime1 = analysis_.GetVRegLifetimeForTesting(vreg, begin1, kCurrentAccessIsNotInput);
  ASSERT_NE(lifetime1, nullptr);
  EXPECT_EQ(lifetime1->LastLiveRangeBegin(), begin1);

  // Simulate moving to a new basic block.
  analysis_.TickForTesting();
  analysis_.EndBasicBlock();

  int begin2 = 10;
  auto* lifetime2 = analysis_.GetVRegLifetimeForTesting(vreg, begin2, kCurrentAccessIsInput);
  ASSERT_EQ(lifetime2, nullptr);
}

constexpr MachineRegClass kRegClass{
    .debug_name = "TestGPR",
    .reg_size = 8,
    .reg_mask = 0b11,
    .num_regs = 2,
    .regs = {MachineReg::CreateHardRegFromIndexForTesting(0),
             MachineReg::CreateHardRegFromIndexForTesting(1)},
};

class GenericInsn : public MachineInsn {
 public:
  static constexpr int kNumRegs = 2;
  static constexpr MachineRegKind kRegKinds[kNumRegs] = {
      {&kRegClass, MachineRegKind::kDef},
      {&kRegClass, MachineRegKind::kUse},
  };
  static const MachineOpcode kOpcode;

  GenericInsn(MachineReg dst, MachineReg src)
      : MachineInsn(kOpcode, kNumRegs, kRegKinds, regs_, kMachineInsnDefault) {
    SetRegAt(0, dst);
    SetRegAt(1, src);
  }

  std::string GetDebugString() const override { return "GENERIC"; }
  void Emit(CodeEmitter* /*as*/) const override { FATAL("not implemented"); }

 private:
  GenericInsn(const GenericInsn& other) = delete;
  MachineInsn* Clone(Arena* /*arena*/) const override { FATAL("not implemented"); }
  MachineInsnList Lower(Arena* /*arena*/) const override { FATAL("not implemented"); }

  MachineReg regs_[kNumRegs];
};

const MachineOpcode GenericInsn::kOpcode = MachineOpcode(0);

class UseDefInsn : public MachineInsn {
 public:
  static constexpr int kNumRegs = 2;
  static constexpr MachineRegKind kRegKinds[kNumRegs] = {
      {&kRegClass, MachineRegKind::kUseDef},
      {&kRegClass, MachineRegKind::kUse},
  };
  static const MachineOpcode kOpcode;

  UseDefInsn(MachineReg dst, MachineReg src)
      : MachineInsn(kOpcode, kNumRegs, kRegKinds, regs_, kMachineInsnDefault) {
    SetRegAt(0, dst);
    SetRegAt(1, src);
  }

  std::string GetDebugString() const override { return "USE_DEF"; }
  void Emit(CodeEmitter* /*as*/) const override { FATAL("not implemented"); }

 private:
  UseDefInsn(const UseDefInsn& other) = delete;
  MachineInsn* Clone(Arena* /*arena*/) const override { FATAL("not implemented"); }
  MachineInsnList Lower(Arena* /*arena*/) const override { FATAL("not implemented"); }

  MachineReg regs_[kNumRegs];
};

const MachineOpcode UseDefInsn::kOpcode = MachineOpcode(1);

class DefEarlyClobberInsn : public MachineInsn {
 public:
  static constexpr int kNumRegs = 1;
  static constexpr MachineRegKind kRegKinds[kNumRegs] = {
      {&kRegClass, MachineRegKind::kDefEarlyClobber},
  };
  static const MachineOpcode kOpcode;

  explicit DefEarlyClobberInsn(MachineReg reg)
      : MachineInsn(kOpcode, kNumRegs, kRegKinds, &reg_, kMachineInsnDefault) {
    SetRegAt(0, reg);
  }

  std::string GetDebugString() const override { return "DEF_EARLY_CLOBBER"; }
  void Emit(CodeEmitter* /*as*/) const override { FATAL("not implemented"); }

 private:
  DefEarlyClobberInsn(const DefEarlyClobberInsn& other) = delete;
  MachineInsn* Clone(Arena* /*arena*/) const override { FATAL("not implemented"); }
  MachineInsnList Lower(Arena* /*arena*/) const override { FATAL("not implemented"); }

  MachineReg reg_;
};

const MachineOpcode DefEarlyClobberInsn::kOpcode = MachineOpcode(2);

class VRegAccessTest : public ::testing::Test {
 protected:
  VRegAccessTest()
      : machine_ir_(&arena_, 0, 0),
        bb_(machine_ir_.NewBasicBlock()) {}

  void TestRewriteVReg(MachineInsn* insn, int index) {
    ASSERT_TRUE(bb_->insn_list().empty());
    bb_->insn_list().push_back(insn);

    VRegLiveRange range(&arena_,
        VRegAccess(MachineInsnListPosition(&bb_->insn_list(), bb_->insn_list().begin()),
                      index,
                      /*begin=*/0,
                      /*end=*/1));
    ASSERT_EQ(range.access_list().back().GetVReg(), index == 0 ? kVRegDst : kVRegSrc);

    range.RewriteVReg(&machine_ir_, kHardReg, kSpillSlot);
  }

  void TestRewriteVRegWithPriorDef(MachineInsn* insn, int index) {
    MachineReg active_reg = index == 0 ? kVRegDst : kVRegSrc;

    auto& insn_list = bb_->insn_list();
    ASSERT_TRUE(insn_list.empty());

    insn_list.push_back(machine_ir_.NewInsn<PseudoDefReg>(active_reg, kMeta<&kRegClass>));

    insn_list.push_back(insn);

    // First add the access from PseudoDefReg to create a range that starts with def.
    VRegLiveRange range(&arena_,
        VRegAccess(MachineInsnListPosition(&insn_list, insn_list.begin()),
                      /*index=*/0,
                      /*begin=*/0,
                      /*end=*/1));
    // Then add the access from the actual instruction.
    range.AppendAccess(VRegAccess(MachineInsnListPosition(&insn_list, std::next(insn_list.begin())),
                                  index,
                                  /*begin=*/1,
                                  /*end=*/2));
    ASSERT_EQ(range.access_list().back().GetVReg(), active_reg);

    range.RewriteVReg(&machine_ir_, kHardReg, kSpillSlot);
  }


    void TestRewriteVRegWithSubsequentUse(MachineInsn* insn, int index) {
    MachineReg active_reg = index == 0 ? kVRegDst : kVRegSrc;

    auto& insn_list = bb_->insn_list();
    ASSERT_TRUE(insn_list.empty());

    insn_list.push_back(insn);

    // We don't have generic use-only instructions, so we use GenericInsn for the use.
    // Ignore the def here, as it is not relevant for the test.
    insn_list.push_back(
        machine_ir_.NewInsn<GenericInsn>(MachineReg::CreateVRegFromIndex(10), active_reg));

    // First add the access from the actual instruction.
    VRegLiveRange range(&arena_,
        VRegAccess(MachineInsnListPosition(&insn_list, insn_list.begin()),
                      index,
                      /*begin=*/0,
                      /*end=*/1));
    // Then add use access of GenericInsn.
    range.AppendAccess(VRegAccess(MachineInsnListPosition(&insn_list, std::next(insn_list.begin())),
                                  /*index=*/1,
                                  /*begin=*/1,
                                  /*end=*/2));
    ASSERT_EQ(range.access_list().back().GetVReg(), active_reg);

    range.RewriteVReg(&machine_ir_, kHardReg, kSpillSlot);
  }

  bool IsSpill(MachineInsn* spill_insn, MachineReg expected_src = kHardReg) {
    if (!spill_insn->is_copy() || spill_insn->RegAt(1) != expected_src ||
        !spill_insn->RegAt(0).IsSpilledReg()) {
      return false;
    }
    return spill_insn->RegAt(0).GetSpilledRegIndex() == machine_ir_.SpillSlotOffset(kSpillSlot);
  }

  bool IsReload(MachineInsn* reload_insn, MachineReg expected_dst = kHardReg) {
    if (!reload_insn->is_copy() || reload_insn->RegAt(0) != expected_dst ||
        !reload_insn->RegAt(1).IsSpilledReg()) {
      return false;
    }
    return reload_insn->RegAt(1).GetSpilledRegIndex() == machine_ir_.SpillSlotOffset(kSpillSlot);
  }
  Arena arena_;
  MachineIR machine_ir_;
  MachineBasicBlock* bb_;
  static constexpr MachineReg kVRegSrc = MachineReg::CreateVRegFromIndex(0);
  static constexpr MachineReg kVRegDst = MachineReg::CreateVRegFromIndex(1);
  static constexpr MachineReg kHardReg = MachineReg::CreateHardRegFromIndexForTesting(0);
  static constexpr int kSpillSlot = 3;
};



TEST_F(VRegAccessTest, RewriteVReg_SpillForGenericInsn) {
  auto* insn = machine_ir_.NewInsn<GenericInsn>(kVRegDst, kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(insn, /*index=*/ 0));
  ASSERT_EQ(bb_->insn_list().size(), 2u);
  auto* spill = bb_->insn_list().back();
  ASSERT_NE(spill, insn); // The spill is a newly inserted instruction
  ASSERT_TRUE(IsSpill(spill));
  ASSERT_EQ(insn->RegAt(0), kHardReg);
}

TEST_F(VRegAccessTest, RewriteVReg_SpillOnlyLastDefForGenericInsnWithPriorDef) {
  auto* insn = machine_ir_.NewInsn<GenericInsn>(kVRegDst, kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVRegWithPriorDef(insn, /*index=*/ 0));

  ASSERT_EQ(bb_->insn_list().size(), 3u);
  // The first instruction is the PseudoDefReg, rewritten to use kHardReg.
  ASSERT_EQ(bb_->insn_list().front()->opcode(), PseudoDefReg::kOpcode);
  ASSERT_EQ(bb_->insn_list().front()->RegAt(0), kHardReg);
  // No spill for the first def.
  // The original instruction, rewritten to use kHardReg for its def.
  ASSERT_EQ(*std::next(bb_->insn_list().begin()), insn);
  ASSERT_EQ(insn->RegAt(0), kHardReg);
  // A spill is inserted after the last def (the GenericInsn).
  ASSERT_TRUE(IsSpill(bb_->insn_list().back()));
}

TEST_F(VRegAccessTest, RewriteVReg_ReloadForGenericInsn) {
  auto* insn = machine_ir_.NewInsn<GenericInsn>(kVRegDst, kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(insn, /*index=*/ 1));
  ASSERT_EQ(bb_->insn_list().size(), 2u);
  auto* reload = bb_->insn_list().front();
  ASSERT_NE(reload, insn); // The reload is a newly inserted instruction
  ASSERT_TRUE(IsReload(reload));
}

TEST_F(VRegAccessTest, RewriteVReg_NoReloadForGenericInsnWithPriorDef) {
  auto* insn = machine_ir_.NewInsn<GenericInsn>(kVRegDst, kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVRegWithPriorDef(insn, /*index=*/ 1));
  ASSERT_EQ(bb_->insn_list().size(), 3u);
  ASSERT_EQ(bb_->insn_list().front()->opcode(), PseudoDefReg::kOpcode);
  // The spill for the def.
  ASSERT_TRUE(IsSpill(*std::next(bb_->insn_list().begin())));
  // No reload for the use.
  ASSERT_EQ(insn, *std::next(bb_->insn_list().begin(), 2));
  ASSERT_EQ(insn->RegAt(1), kHardReg);
}

TEST_F(VRegAccessTest, RewriteVReg_NoReloadForGenericInsnWithPriorUse) {
  // The first instruction is a GenericInsn, and we are rewriting its source (use) operand.
  auto* insn = machine_ir_.NewInsn<GenericInsn>(MachineReg::CreateVRegFromIndex(20), kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVRegWithSubsequentUse(insn, /*index=*/ 1));

  ASSERT_EQ(bb_->insn_list().size(), 3u);

  // Check the reload for the first use.
  auto* reload = bb_->insn_list().front();
  ASSERT_NE(reload, insn);
  ASSERT_TRUE(IsReload(reload));
  // The original instruction.
  ASSERT_EQ(*std::next(bb_->insn_list().begin()), insn);
  ASSERT_EQ(insn->RegAt(1), kHardReg);
  // No reload for the subsequent GenericInsn.
  ASSERT_EQ(bb_->insn_list().back()->opcode(), GenericInsn::kOpcode);
  ASSERT_EQ(bb_->insn_list().back()->RegAt(1), kHardReg);
}


TEST_F(VRegAccessTest, RewriteVReg_ReloadAndSpillForUseDefInsn) {
  auto* insn = machine_ir_.NewInsn<UseDefInsn>(kVRegDst, kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(insn, /*index=*/ 0));
  ASSERT_EQ(bb_->insn_list().size(), 3u);

  auto* reload = bb_->insn_list().front();
  ASSERT_NE(reload, insn); // The reload is a newly inserted instruction
  ASSERT_TRUE(IsReload(reload));

  auto* spill = bb_->insn_list().back();
  ASSERT_NE(spill, insn); // The spill is a newly inserted instruction.
  ASSERT_TRUE(IsSpill(spill));

  ASSERT_EQ(insn->RegAt(0), kHardReg);
}

TEST_F(VRegAccessTest, RewriteVReg_NoReloadForUseDefWithPriorDef) {
  auto* insn = machine_ir_.NewInsn<UseDefInsn>(kVRegDst, kVRegSrc);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVRegWithPriorDef(insn, /*index=*/ 0));

  ASSERT_EQ(bb_->insn_list().size(), 3u);
  ASSERT_EQ(bb_->insn_list().front()->opcode(), PseudoDefReg::kOpcode);
  // No spill for the first def.
  ASSERT_EQ(*std::next(bb_->insn_list().begin()), insn);
  // Spill for the last def from UseDefInsn.
  auto* spill = bb_->insn_list().back();
  ASSERT_NE(spill, insn); // The spill is a newly inserted instruction.
  ASSERT_TRUE(IsSpill(spill));
  ASSERT_EQ(insn->RegAt(0), kHardReg);
}

TEST_F(VRegAccessTest, RewriteVReg_NoReloadForDefEarlyClobber) {
  auto* insn = machine_ir_.NewInsn<DefEarlyClobberInsn>(kVRegDst);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(insn, /*index=*/0));

  // No reload is inserted, only spill.
  ASSERT_EQ(bb_->insn_list().size(), 2u);

  auto* original_insn = bb_->insn_list().front();
  ASSERT_EQ(original_insn, insn);

  auto* spill = bb_->insn_list().back();
  ASSERT_NE(spill, insn); // The spill is a newly inserted instruction.
  ASSERT_TRUE(IsSpill(spill));
  ASSERT_EQ(insn->RegAt(0), kHardReg);
}

TEST_F(VRegAccessTest, RewriteVReg_SpillForCopy) {
  auto* copy = machine_ir_.NewInsn<Copy>(kVRegDst, kVRegSrc, kMeta<&kRegClass>);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(copy, /*index=*/ 0));

  ASSERT_EQ(bb_->insn_list().size(), 1u);
  auto* spill = bb_->insn_list().back();
  ASSERT_EQ(spill, copy); // The copy instruction itself is rewritten to a spill.
  ASSERT_TRUE(IsSpill(spill, kVRegSrc));
}

TEST_F(VRegAccessTest, RewriteVReg_SpillForCopyWithSubsequentUse) {
  auto* copy = machine_ir_.NewInsn<Copy>(kVRegDst, kVRegSrc, kMeta<&kRegClass>);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVRegWithSubsequentUse(copy, /*index=*/ 0));

  ASSERT_EQ(bb_->insn_list().size(), 3u);
  ASSERT_EQ(bb_->insn_list().front(), copy);
  // Copy has to write to hard-reg since there is a subsequent use.
  ASSERT_EQ(copy->RegAt(0), kHardReg);
  // The spill for the copy.
  auto* spill = *std::next(bb_->insn_list().begin());
  ASSERT_NE(spill, copy); // The spill is a newly inserted instruction.
  ASSERT_TRUE(IsSpill(spill));
  // No reload for the subsequent use.
  ASSERT_EQ(bb_->insn_list().back()->opcode(), GenericInsn::kOpcode);
}

TEST_F(VRegAccessTest, RewriteVReg_ReloadForCopy) {
  auto* copy = machine_ir_.NewInsn<Copy>(kVRegDst, kVRegSrc, kMeta<&kRegClass>);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(copy, /*index=*/ 1));

  ASSERT_EQ(bb_->insn_list().size(), 1u);
  auto* reload = bb_->insn_list().back();
  ASSERT_EQ(reload, copy);
  ASSERT_TRUE(IsReload(reload, kVRegDst));
}

TEST_F(VRegAccessTest, RewriteVReg_ReloadForCopyWithSubsequentUse) {
  auto* copy = machine_ir_.NewInsn<Copy>(kVRegDst, kVRegSrc, kMeta<&kRegClass>);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVRegWithSubsequentUse(copy, /*index=*/ 1));

  ASSERT_EQ(bb_->insn_list().size(), 3u);
  // We need a separate reload since the value is used after the copy.
  auto reload = bb_->insn_list().front();
  ASSERT_NE(reload, copy);
  ASSERT_TRUE(IsReload(reload));
  // The original copy.
  ASSERT_EQ(*std::next(bb_->insn_list().begin()), copy);
  // Copy has to write to hard-reg since there is a subsequent use.
  ASSERT_EQ(copy->RegAt(1), kHardReg);
  // No reload for the subsequent use.
  ASSERT_EQ(bb_->insn_list().back()->opcode(), GenericInsn::kOpcode);
}

}  // namespace

}  // namespace berberis
