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
#include "gmock/gmock.h"

#include "berberis/backend/common/lifetime.h"

#include <string>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/arena_alloc.h"


namespace berberis {

namespace {

using ::testing::Contains;
using ::testing::ElementsAre;

MATCHER_P2(MatchesLiveRange, begin, end, "") {
  return arg.begin() == begin && arg.end() == end;
}

// TODO(b/459067902): Move this to a common test utils file.
const MachineRegClass kRegClass{
    .debug_name = "GPR",
    .reg_size = 8,
    .reg_mask = 0,
    .num_regs = 0,
    .regs = {},
};

// Sub is subset of Super.
const MachineRegClass kRegSubClass{
    .debug_name = "Sub",
    .reg_size = 8,
    .reg_mask = 0x2,  // bit 1
    .num_regs = 0,
    .regs = {},
};

const MachineRegClass kRegSuperClass{
    .debug_name = "Super",
    .reg_size = 8,
    .reg_mask = 0x6,  // bits 1 and 2
    .num_regs = 0,
    .regs = {},
};

class FlexibleInsn : public MachineInsn {
 public:
  static const MachineOpcode kOpcode;

  FlexibleInsn(MachineReg dst, MachineReg src, const MachineRegKind* reg_kinds)
      : MachineInsn(kOpcode, 2, reg_kinds, regs_, kMachineInsnDefault) {
    SetRegAt(0, dst);
    SetRegAt(1, src);
  }

  std::string GetDebugString() const override { return "FLEXIBLE"; }
  void Emit(CodeEmitter* /*as*/) const override { FATAL("not implemented"); }

 private:
  FlexibleInsn(const FlexibleInsn& other) = delete;
  MachineInsn* Clone(Arena* /*arena*/) const override { FATAL("not implemented"); }
  MachineInsnList Lower(Arena* /*arena*/) const override { FATAL("not implemented"); }

  MachineReg regs_[2];
};

const MachineOpcode FlexibleInsn::kOpcode = MachineOpcode(0);


class VRegLifetimeTest : public ::testing::Test {
 protected:
  VRegLifetimeTest()
      : machine_ir_(&arena_, 0, 0),
        bb_(machine_ir_.NewBasicBlock()),
        lifetime_(&arena_) {}

  template <bool kIsUse = false>
  VRegAccess CreateAccess(int begin, int end, const MachineRegClass* reg_class = &kRegClass) {
    MachineRegKind* reg_kinds = NewArrayInArena<MachineRegKind>(&arena_, 2);
    reg_kinds[0] = {reg_class, MachineRegKind::kDef};
    reg_kinds[1] = {reg_class, MachineRegKind::kUse};

    auto* insn = machine_ir_.NewInsn<FlexibleInsn>(kVRegDst, kVRegSrc, reg_kinds);
    bb_->insn_list().push_back(insn);
    return VRegAccess(MachineInsnListPosition(&bb_->insn_list(), std::prev(bb_->insn_list().end())),
                      kIsUse ? 1 : 0,
                      begin,
                      end);
  }

  void AddAccess(int begin, int end) {
    lifetime_.AppendAccess(CreateAccess(begin, end));
  }

  void StartLiveRange(int begin) {
    lifetime_.StartLiveRange(begin);
  }

  Arena arena_;
  MachineIR machine_ir_;
  MachineBasicBlock* bb_;
  VRegLifetime lifetime_;
  static constexpr MachineReg kVRegSrc = MachineReg::CreateVRegFromIndex(0);
  static constexpr MachineReg kVRegDst = MachineReg::CreateVRegFromIndex(1);
};

TEST_F(VRegLifetimeTest, Split_NoSplitAtEnd) {
  StartLiveRange(0);
  AddAccess(0, 10);

  SplitPos split_pos;
  // Splitting at a point after the last access should return range_list_.end()
  SplitKind kind = lifetime_.FindSplitPos(20, &split_pos);
  ASSERT_EQ(kind, SPLIT_OK);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, kind, &new_lifetimes);

  EXPECT_TRUE(new_lifetimes.empty());
  EXPECT_EQ(lifetime_.end(), 10);
}

TEST_F(VRegLifetimeTest, Split_SingleRange_Middle) {
  StartLiveRange(0);
  AddAccess(0, 9);
  AddAccess(11, 20);

  SplitPos split_pos;
  // Split at 10.
  SplitKind kind = lifetime_.FindSplitPos(10, &split_pos);
  ASSERT_EQ(kind, SPLIT_OK);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, kind, &new_lifetimes);

  EXPECT_EQ(lifetime_.end(), 9);
  EXPECT_THAT(new_lifetimes, ElementsAre(MatchesLiveRange(11, 20)));
}

TEST_F(VRegLifetimeTest, Split_MultipleRanges_SplitInBetween) {
  StartLiveRange(0);
  AddAccess(0, 10);
  // Force new range
  StartLiveRange(21);
  AddAccess(21, 30);

  SplitPos split_pos;
  // Split at 20.
  SplitKind kind = lifetime_.FindSplitPos(20, &split_pos);
  ASSERT_EQ(kind, SPLIT_OK);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, kind, &new_lifetimes);

  EXPECT_EQ(lifetime_.end(), 10);
  EXPECT_THAT(new_lifetimes, ElementsAre(MatchesLiveRange(21, 30)));
}

TEST_F(VRegLifetimeTest, Split_MultipleRanges_SplitFirstRange) {
  StartLiveRange(0);
  AddAccess(0, 9);
  AddAccess(11, 20);
  StartLiveRange(30);
  AddAccess(30, 40);

  SplitPos split_pos;
  // Split at 10.
  SplitKind kind = lifetime_.FindSplitPos(10, &split_pos);
  ASSERT_EQ(kind, SPLIT_OK);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, kind, &new_lifetimes);

  // Expecting 2 new lifetimes: one from the remainder of first range, one from the second range.
  // Old lifetime
  EXPECT_EQ(lifetime_.end(), 9);

  // New lifetimes
  EXPECT_THAT(new_lifetimes, ElementsAre(MatchesLiveRange(11, 20), MatchesLiveRange(30, 40)));
}

TEST_F(VRegLifetimeTest, Split_PreservesSpillSlot) {
  StartLiveRange(0);
  AddAccess(0, 9);
  AddAccess(11, 20);

  int kSpillSlot = 5;
  lifetime_.SetSpill(kSpillSlot);

  SplitPos split_pos;
  SplitKind kind = lifetime_.FindSplitPos(10, &split_pos);
  ASSERT_EQ(kind, SPLIT_OK);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, kind, &new_lifetimes);

  ASSERT_EQ(new_lifetimes.size(), 1u);
  EXPECT_EQ(new_lifetimes.front().GetSpill(), kSpillSlot);
}

TEST_F(VRegLifetimeTest, Split_AtBeginning) {
  StartLiveRange(0);
  AddAccess(0, 10);

  SplitPos split_pos;
  // Split at 0.
  SplitKind kind = lifetime_.FindSplitPos(0, &split_pos);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, kind, &new_lifetimes);

  EXPECT_THAT(new_lifetimes, ElementsAre(MatchesLiveRange(0, 10)));
  // The original lifetime is now empty.
  EXPECT_TRUE(lifetime_.GetLiveRangesForTesting().empty());
}

TEST_F(VRegLifetimeTest, LinkCoalescingCandidate) {
  VRegLifetime other(&arena_);

  lifetime_.LinkCoalescingCandidate(&other);

  EXPECT_THAT(lifetime_.coalescing_candidates(), ElementsAre(&other));
  EXPECT_THAT(other.coalescing_candidates(), ElementsAre(&lifetime_));
}

TEST_F(VRegLifetimeTest, Merge) {
  StartLiveRange(0);
  lifetime_.AppendAccess(CreateAccess(0, 10, &kRegSuperClass));

  VRegLifetime other(&arena_);
  other.StartLiveRange(20);
  other.AppendAccess(CreateAccess(20, 30, &kRegSubClass));

  // Setup coalescing candidates.
  VRegLifetime others_candidate(&arena_);
  other.LinkCoalescingCandidate(&others_candidate);

  lifetime_.Merge(&other);

  // Check ranges merged.
  EXPECT_EQ(lifetime_.begin(), 0);
  EXPECT_EQ(lifetime_.end(), 30);
  EXPECT_THAT(lifetime_.GetLiveRangesForTesting(),
              ElementsAre(MatchesLiveRange(0, 10), MatchesLiveRange(20, 30)));

  // Check spill weight merged (1 from lifetime_ + 1 from other).
  EXPECT_EQ(lifetime_.ComputeWeight(), 2);

  // Check coalescing candidates merged.
  EXPECT_THAT(lifetime_.coalescing_candidates(), ElementsAre(&others_candidate));

  // Check candidate points back to lifetime_.
  EXPECT_THAT(others_candidate.coalescing_candidates(), Contains(&lifetime_));
}

TEST_F(VRegLifetimeTest, Merge_InterleavedRanges) {
  StartLiveRange(0);
  AddAccess(0, 10);
  StartLiveRange(20);
  AddAccess(20, 30);

  VRegLifetime other(&arena_);
  other.StartLiveRange(10);
  other.AppendAccess(CreateAccess(10, 20));
  other.StartLiveRange(30);
  other.AppendAccess(CreateAccess(30, 40));

  lifetime_.Merge(&other);

  EXPECT_THAT(lifetime_.GetLiveRangesForTesting(),
              ElementsAre(MatchesLiveRange(0, 10),
                          MatchesLiveRange(10, 20),
                          MatchesLiveRange(20, 30),
                          MatchesLiveRange(30, 40)));
}

TEST_F(VRegLifetimeTest, ComputeWeight) {
  // Empty lifetime
  EXPECT_EQ(lifetime_.ComputeWeight(), 0);

  // Range 1: Def only
  StartLiveRange(0);
  lifetime_.AppendAccess(CreateAccess(0, 10));  // Def
  // Has Def, no Input at start. Weight = 1.
  EXPECT_EQ(lifetime_.ComputeWeight(), 1);

  // Range 2: Input only
  StartLiveRange(20);
  lifetime_.AppendAccess(CreateAccess<true>(20, 30));  // Use
  // Range 2: [20, 30). Input at start. Has Def? No. Weight contribution = 1.
  // Lifetime total = 1 + 1 = 2.
  EXPECT_EQ(lifetime_.ComputeWeight(), 2);

  // Range 3: Input then Def
  StartLiveRange(40);
  lifetime_.AppendAccess(CreateAccess<true>(40, 50));  // Input
  lifetime_.AppendAccess(CreateAccess(50, 60));     // Def
  // Range 3: [40, 60). Input at start? Yes. Has Def? Yes.
  // Weight contribution: 1 + 1 = 2.
  // Lifetime total: 2 + 2 = 4.
  EXPECT_EQ(lifetime_.ComputeWeight(), 4);

  // Range 4: Def then Input
  StartLiveRange(70);
  lifetime_.AppendAccess(CreateAccess(70, 80));     // Def
  lifetime_.AppendAccess(CreateAccess<true>(80, 90));  // Input
  // Range 4: [70, 90). Input at start? No. Has Def? Yes.
  // Weight contribution: 1.
  // Lifetime total: 4 + 1 = 5.
  EXPECT_EQ(lifetime_.ComputeWeight(), 5);
}

} // namespace

} // namespace berberis
