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

const MachineRegClass kRegClass{
    .debug_name = "GPR",
    .reg_size = 8,
    .reg_mask = 0,
    .num_regs = 0,
    .regs = {},
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

class VRegAccessTest : public ::testing::Test {
 protected:
  VRegAccessTest()
      : machine_ir_(&arena_, 0, 0),
        bb_(machine_ir_.NewBasicBlock()),
        vreg_src_(MachineReg::CreateVRegFromIndex(0)),
        vreg_dst_(MachineReg::CreateVRegFromIndex(1)),
        hard_reg_(MachineReg::CreateHardRegFromIndexForTesting(0)),
        slot_(3) {}

  void TestRewriteVReg(MachineInsn* insn, int index) {
    ASSERT_TRUE(bb_->insn_list().empty());
    bb_->insn_list().push_back(insn);

    VRegAccess access(MachineInsnListPosition(&bb_->insn_list(), bb_->insn_list().begin()),
                      index,
                      /*begin=*/0,
                      /*end=*/1);
    ASSERT_EQ(access.GetVReg(), index == 0 ? vreg_dst_ : vreg_src_);
    access.RewriteVReg(&machine_ir_, hard_reg_, slot_);
  }

  Arena arena_;
  MachineIR machine_ir_;
  MachineBasicBlock* bb_;
  MachineReg vreg_src_;
  MachineReg vreg_dst_;
  MachineReg hard_reg_;
  int slot_;
};

TEST_F(VRegAccessTest, RewriteVReg_SpillForGenericInsn) {
  auto* insn = machine_ir_.NewInsn<GenericInsn>(vreg_dst_, vreg_src_);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(insn, /*index=*/0));

  ASSERT_EQ(bb_->insn_list().size(), 2u);
  auto* spill = bb_->insn_list().back();
  EXPECT_NE(spill, insn);
  EXPECT_TRUE(spill->is_copy());
  EXPECT_TRUE(spill->RegAt(0).IsSpilledReg());
  EXPECT_EQ(spill->RegAt(0).GetSpilledRegIndex(), machine_ir_.SpillSlotOffset(slot_));
  EXPECT_EQ(spill->RegAt(1), hard_reg_);
  EXPECT_EQ(insn->RegAt(0), hard_reg_);
}

TEST_F(VRegAccessTest, RewriteVReg_ReloadForGenericInsn) {
  auto* insn = machine_ir_.NewInsn<GenericInsn>(vreg_dst_, vreg_src_);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(insn, /*index=*/1));

  ASSERT_EQ(bb_->insn_list().size(), 2u);
  auto* reload = bb_->insn_list().front();
  EXPECT_NE(reload, insn);
  EXPECT_TRUE(reload->is_copy());
  EXPECT_EQ(reload->RegAt(0), hard_reg_);
  EXPECT_TRUE(reload->RegAt(1).IsSpilledReg());
  EXPECT_EQ(reload->RegAt(1).GetSpilledRegIndex(), machine_ir_.SpillSlotOffset(slot_));
  EXPECT_EQ(insn->RegAt(1), hard_reg_);
}

TEST_F(VRegAccessTest, RewriteVReg_SpillForCopy) {
  auto* copy = machine_ir_.NewInsn<Copy>(vreg_dst_, vreg_src_, 8);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(copy, /*index=*/0));

  auto* spill = bb_->insn_list().back();
  EXPECT_EQ(spill, copy);
  EXPECT_TRUE(spill->is_copy());
  EXPECT_TRUE(spill->RegAt(0).IsSpilledReg());
  EXPECT_EQ(spill->RegAt(0).GetSpilledRegIndex(), machine_ir_.SpillSlotOffset(slot_));
  EXPECT_EQ(spill->RegAt(1), vreg_src_);
}

TEST_F(VRegAccessTest, RewriteVReg_ReloadForCopy) {
  auto* copy = machine_ir_.NewInsn<Copy>(vreg_dst_, vreg_src_, 8);
  ASSERT_NO_FATAL_FAILURE(TestRewriteVReg(copy, /*index=*/1));

  ASSERT_EQ(bb_->insn_list().size(), 1u);
  auto* reload = bb_->insn_list().back();
  EXPECT_EQ(reload, copy);
  EXPECT_TRUE(reload->is_copy());
  EXPECT_EQ(reload->RegAt(0), vreg_dst_);
  EXPECT_TRUE(reload->RegAt(1).IsSpilledReg());
  EXPECT_EQ(reload->RegAt(1).GetSpilledRegIndex(), machine_ir_.SpillSlotOffset(slot_));
}

}  // namespace

}  // namespace berberis
