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
#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/common/reg_alloc.h"
#include "berberis/base/arena_alloc.h"

#include "reg_alloc_internal.h"

#include <iterator>

namespace berberis {

void CoalesceLifetimes(VRegLifetimeList* lifetimes);

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Property;

MATCHER_P2(MatchesLifetime, begin, end, "") {
  return arg.begin() == begin && arg.end() == end;
}

// Zero is an invalid MachineReg, so we start at 1.
// The bitmask has bit 'i' set if register 'i' is in the class.
const MachineRegClass kGPRRegClass = {"GPR", 8, 0b0110, 2, {MachineReg{1}, MachineReg{2}}};
const MachineRegClass kFPRegClass = {"FP", 8, 0b11000, 2, {MachineReg{3}, MachineReg{4}}};

class MockMachineInsn : public MachineInsn {
 public:
  MockMachineInsn(MachineReg reg, const MachineRegClass* reg_class = &kGPRRegClass)
      : MachineInsn(static_cast<MachineOpcode>(0),
                    2,
                    reg_kinds_,
                    regs_,
                    kMachineInsnCopy),
        regs_{reg, reg},
        reg_kinds_{{reg_class, MachineRegKind::kDef}, {reg_class, MachineRegKind::kUse}} {}

  std::string GetDebugString() const override { return "mock_copy"; }
  void Emit(CodeEmitter*) const override {}

 private:
  friend MockMachineInsn* NewInArena<MockMachineInsn, const MockMachineInsn&>(
      Arena*, const MockMachineInsn&);
  MachineInsn* Clone(Arena* arena) const override {
    return NewInArena<MockMachineInsn>(arena, *this);
  }
  MachineInsnList Lower(Arena* arena) const override {
    FATAL("Should not be called");
    return MachineInsnList(arena);
  }

  MachineReg regs_[2];
  MachineRegKind reg_kinds_[2];
};

class RegAllocTest : public ::testing::Test {
 protected:
  RegAllocTest() : machine_ir_(&arena_, 0, 0), bb_(machine_ir_.NewBasicBlock()) {}

  VRegLifetime* CreateLifetime(int vreg_index,
                               int begin,
                               int end,
                               const MachineRegClass* reg_class = &kGPRRegClass) {
    MachineReg vreg = MachineReg::CreateVRegFromIndex(vreg_index);
    auto* insn = machine_ir_.NewInsn<MockMachineInsn>(vreg, reg_class);
    bb_->insn_list().push_back(insn);
    MachineInsnListPosition pos(&bb_->insn_list(), std::prev(bb_->insn_list().end()));
    // Pos 0 in MockMachineInsn describes the DEF access.
    VRegAccess access1(pos, 0, begin, begin + 1);
    // Pos 1 in MockMachineInsn describes the USE access.
    VRegAccess access2(pos, 1, end - 1, end);
    auto* lifetime = NewInArena<VRegLifetime>(&arena_, &arena_, access1.begin());
    lifetime->AppendAccess(access1);
    lifetime->AppendAccess(access2);
    return lifetime;
  }

  VRegLifetime* CreateLifetimeWithAccess(int vreg_index,
                                         int access_begin,
                                         int access_end,
                                         const MachineRegClass* reg_class = &kGPRRegClass) {
    CHECK_LT(access_begin, access_end);
    CHECK_LE(access_end - access_begin, 2);
    MachineReg vreg = MachineReg::CreateVRegFromIndex(vreg_index);
    auto* insn = machine_ir_.NewInsn<MockMachineInsn>(vreg, reg_class);
    bb_->insn_list().push_back(insn);
    MachineInsnListPosition pos(&bb_->insn_list(), std::prev(bb_->insn_list().end()));
    VRegAccess access(pos, 0, access_begin, access_end);
    auto* lifetime = NewInArena<VRegLifetime>(&arena_, &arena_, access.begin());
    lifetime->AppendAccess(access);
    return lifetime;
  }

  Arena arena_;
  MachineIR machine_ir_;
  MachineBasicBlock* bb_;
};

TEST_F(RegAllocTest, HardRegAllocation_TryAssign_Empty) {
  HardRegAllocation hard_reg_allocation(&arena_);
  VRegLifetime* lifetime = CreateLifetime(0, 10, 20);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime));
}

TEST_F(RegAllocTest, HardRegAllocation_TryAssign_NoInterference) {
  HardRegAllocation hard_reg_allocation(&arena_);

  VRegLifetime* lifetime1 = CreateLifetime(0, 10, 20);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  VRegLifetime* lifetime2 = CreateLifetime(1, 20, 30);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime2));
}

TEST_F(RegAllocTest, HardRegAllocation_TryAssign_Interference) {
  HardRegAllocation hard_reg_allocation(&arena_);

  VRegLifetime* lifetime1 = CreateLifetime(0, 10, 20);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  VRegLifetime* lifetime2 = CreateLifetime(1, 15, 25);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill) {
  HardRegAllocation hard_reg_allocation(&arena_);

  VRegLifetime* lifetime1 = CreateLifetime(0, 10, 20);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  VRegLifetime* lifetime2 = CreateLifetime(1, 15, 25);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));
  auto result = hard_reg_allocation.ConsiderSpill(lifetime2);
  EXPECT_NE(std::get<0>(result), kInfiniteSpillWeight);
  EXPECT_EQ(std::get<1>(result), SPLIT_OK);
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_SplitImpossible) {
  HardRegAllocation hard_reg_allocation(&arena_);

  // Lifetime 1: access at [9, 11).
  VRegLifetime* lifetime1 = CreateLifetimeWithAccess(0, 9, 11);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  // Lifetime 2: starts at 10.
  VRegLifetime* lifetime2 = CreateLifetime(1, 10, 20);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));

  auto result = hard_reg_allocation.ConsiderSpill(lifetime2);
  EXPECT_EQ(std::get<0>(result), kInfiniteSpillWeight);
  EXPECT_EQ(std::get<1>(result), SPLIT_IMPOSSIBLE);
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_SplitConflict_Unresolvable) {
  HardRegAllocation hard_reg_allocation(&arena_);

  // Lifetime 1: [10, 20], first access at [10, 11).
  VRegLifetime* lifetime1 = CreateLifetime(0, 10, 20);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  // Lifetime 2: [10, 20], first access at [10, 11).
  VRegLifetime* lifetime2 = CreateLifetime(1, 10, 20);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));

  // Both have same reg class (GPR), so subset check passes (Unresolvable).
  auto result = hard_reg_allocation.ConsiderSpill(lifetime2);
  EXPECT_EQ(std::get<0>(result), kInfiniteSpillWeight);
  EXPECT_EQ(std::get<1>(result), SPLIT_IMPOSSIBLE);
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_SplitConflict_Resolvable) {
  HardRegAllocation hard_reg_allocation(&arena_);

  static const MachineRegClass kWideRegClass = {
      "Wide", 8, 0b0110, 2, {MachineReg{1}, MachineReg{2}}};
  static const MachineRegClass kNarrowRegClass = {"Narrow", 8, 0b0010, 1, {MachineReg{1}}};

  // Lifetime 1: Wide class. Assigned to R1.
  VRegLifetime* lifetime1 = CreateLifetime(0, 10, 20, &kWideRegClass);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  // Lifetime 2: Narrow class (R1 only). Starts at 10.
  VRegLifetime* lifetime2 = CreateLifetime(1, 10, 20, &kNarrowRegClass);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));

  // Lifetime 1 starts at 10, so conflict at start.
  // Wide is NOT subset of Narrow.
  // Should be resolvable.
  auto result = hard_reg_allocation.ConsiderSpill(lifetime2);
  EXPECT_NE(std::get<0>(result), kInfiniteSpillWeight);
  EXPECT_EQ(std::get<1>(result), SPLIT_CONFLICT);
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_Priorities) {
  // Test that IMPOSSIBLE > CONFLICT > OK.
  // Note: We deliberately violate the "assign in order" rule to setup
  // multiple disjoint lifetimes in the register that all interfere with
  // the new lifetime (which overlaps them all).

  // Scenario 1: OK + CONFLICT (Resolvable) -> CONFLICT
  {
    HardRegAllocation hra(&arena_);
    // L1: [15, 25). Access [15, 16).
    // Disjoint from L2. Intersects New.
    // Split at 5 (New start) -> OK (no access at 5).
    VRegLifetime* l1 = CreateLifetime(0, 15, 25);
    EXPECT_TRUE(hra.TryAssign(l1));

    // L2: [5, 15). Access [5, 6).
    // Intersects New.
    // Split at 5 -> CONFLICT (access at 5).
    // Use Wide/Narrow to make it resolvable.
    static const MachineRegClass kWideRegClass = {
        "Wide", 8, 0b0110, 2, {MachineReg{1}, MachineReg{2}}};
    static const MachineRegClass kNarrowRegClass = {"Narrow", 8, 0b0010, 1, {MachineReg{1}}};
    VRegLifetime* l2 = CreateLifetime(1, 5, 15, &kWideRegClass);
    EXPECT_TRUE(hra.TryAssign(l2));

    // New: [5, 25).
    VRegLifetime* l_new = CreateLifetime(2, 5, 25, &kNarrowRegClass);
    EXPECT_FALSE(hra.TryAssign(l_new));

    auto res = hra.ConsiderSpill(l_new);
    EXPECT_NE(std::get<0>(res), kInfiniteSpillWeight);
    EXPECT_EQ(std::get<1>(res), SPLIT_CONFLICT);
  }

  // Scenario 2: OK + IMPOSSIBLE -> IMPOSSIBLE
  {
    HardRegAllocation hra(&arena_);
    // L1: [15, 25). Access [15, 16).
    // Split at 5 -> OK.
    VRegLifetime* l1 = CreateLifetime(0, 15, 25);
    EXPECT_TRUE(hra.TryAssign(l1));

    // L3: [5, 15). Access [5, 6).
    // Split at 5 -> IMPOSSIBLE (access at 5, Subset).
    VRegLifetime* l3 = CreateLifetime(1, 5, 15);  // Default GPR
    EXPECT_TRUE(hra.TryAssign(l3));

    // New: [5, 25).
    // Intersects L1 (OK) and L3 (IMPOSSIBLE).
    VRegLifetime* l_new = CreateLifetime(2, 5, 25);  // Default GPR
    EXPECT_FALSE(hra.TryAssign(l_new));

    auto res = hra.ConsiderSpill(l_new);
    EXPECT_EQ(std::get<0>(res), kInfiniteSpillWeight);
    EXPECT_EQ(std::get<1>(res), SPLIT_IMPOSSIBLE);
  }
}

TEST_F(RegAllocTest, HardRegAllocation_SpillAndAssign) {
  HardRegAllocation hard_reg_allocation(&arena_);

  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 15, 25)}, &arena_);
  auto* lifetime1 = &lifetime_list.front();
  auto* lifetime2 = &lifetime_list.back();

  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));
  EXPECT_NE(std::get<0>(hard_reg_allocation.ConsiderSpill(lifetime2)), kInfiniteSpillWeight);

  auto it = std::next(lifetime_list.begin());

  hard_reg_allocation.SpillAndAssign(lifetime2, 1, &lifetime_list, it);

  int spill_slot = lifetime1->GetSpill();
  EXPECT_NE(spill_slot, -1);
  // After splitting, the original lifetime should only contain the first access.
  // One tiny lifetime from the second access should be added.
  EXPECT_THAT(
      lifetime_list,
      ElementsAre(MatchesLifetime(10, 11),
                  MatchesLifetime(15, 25),
                  AllOf(MatchesLifetime(19, 20), Property(&VRegLifetime::GetSpill, spill_slot))));
}

TEST_F(RegAllocTest, VRegLifetimeAllocator_Allocate_Simple) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20)}, &arena_);

  VRegLifetimeAllocator allocator(&machine_ir_, &lifetime_list);
  allocator.Allocate();

  EXPECT_TRUE(lifetime_list.front().hard_reg().IsHardReg());
}

TEST_F(RegAllocTest, VRegLifetimeAllocator_Allocate_ReuseHardReg) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20),
                                  *CreateLifetime(1, 15, 25),
                                  *CreateLifetime(2, 20, 30)},
                                 &arena_);

  VRegLifetimeAllocator allocator(&machine_ir_, &lifetime_list);
  allocator.Allocate();

  auto it = lifetime_list.begin();
  auto reg1 = it->hard_reg();
  ++it;
  auto reg2 = it->hard_reg();
  ++it;
  auto reg3 = it->hard_reg();

  EXPECT_TRUE(reg1.IsHardReg());
  EXPECT_TRUE(reg2.IsHardReg());
  EXPECT_TRUE(reg3.IsHardReg());
  EXPECT_NE(reg1, reg2);
  EXPECT_EQ(reg1, reg3);
}

TEST_F(RegAllocTest, VRegLifetimeAllocator_Allocate_Spill) {
  VRegLifetimeList lifetime_list(&arena_);

  const int num_regs = kGPRRegClass.NumRegs();
  const int num_lifetimes = num_regs + 1;

  // Create overlapping lifetimes.
  // At time 10 + num_lifetimes - 1, all lifetimes are live.
  for (int i = 0; i < num_lifetimes; ++i) {
    VRegLifetime* lifetime = CreateLifetime(i, 10 + i, 10 + num_lifetimes + i);
    lifetime_list.push_back(*lifetime);
  }

  VRegLifetimeAllocator allocator(&machine_ir_, &lifetime_list);
  allocator.Allocate();

  // One of the original lifetimes must have been spilled.
  // The list will contain more than 3 lifetimes because of splitting.
  bool spilled = false;
  for (const auto& lifetime : lifetime_list) {
    if (lifetime.GetSpill() != -1) {
      spilled = true;
      break;
    }
  }
  EXPECT_TRUE(spilled);
}

TEST_F(RegAllocTest, CoalesceLifetimes_NoMerge) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 20, 30)}, &arena_);

  CoalesceLifetimes(&lifetime_list);

  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 20), MatchesLifetime(20, 30)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_SimpleMerge) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 20, 30)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 30)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_MergeWithGap) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 30, 40)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 40)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_NoMerge_Interference) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 15, 25)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 20), MatchesLifetime(15, 25)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_NoMerge_DifferentRegClass) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20, &kGPRRegClass),
                                  *CreateLifetime(1, 20, 30, &kFPRegClass)},
                                 &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 20), MatchesLifetime(20, 30)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_MultipleCandidates) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20),
                                  *CreateLifetime(1, 20, 30),
                                  *CreateLifetime(2, 30, 40)},
                                 &arena_);

  // lifetime1 can merge with lifetime2 and lifetime3.
  auto it = lifetime_list.begin();
  VRegLifetime* lifetime1 = &*it++;
  VRegLifetime* lifetime2 = &*it++;
  VRegLifetime* lifetime3 = &*it;
  lifetime1->LinkCoalescingCandidate(lifetime2);
  lifetime1->LinkCoalescingCandidate(lifetime3);

  CoalesceLifetimes(&lifetime_list);

  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 40)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_ChainedMerge) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20),
                                  *CreateLifetime(1, 20, 30),
                                  *CreateLifetime(2, 30, 40)},
                                 &arena_);

  // lifetime1 -> lifetime2 -> lifetime3
  auto it = lifetime_list.begin();
  VRegLifetime* lifetime1 = &*it++;
  VRegLifetime* lifetime2 = &*it++;
  VRegLifetime* lifetime3 = &*it;
  lifetime1->LinkCoalescingCandidate(lifetime2);
  lifetime2->LinkCoalescingCandidate(lifetime3);

  CoalesceLifetimes(&lifetime_list);

  // When lifetime1 merges lifetime2, it inherits candidates of lifetime2, which includes lifetime3.
  // So lifetime1 should then merge lifetime3.
  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 40)));
}

TEST_F(RegAllocTest, CoalesceLifetimes_MergeOrder) {
  // Verify that coalescing works when the candidate appears later in the list.
  // Here, later lifetime2 is first in the list and it should merge earlier
  // lifetime1 (which is second).
  VRegLifetimeList lifetime_list({*CreateLifetime(1, 20, 30), *CreateLifetime(0, 10, 20)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  // The resulting lifetime should be later lifetime2 (because it was first in list and
  // merged lifetime1).
  EXPECT_THAT(lifetime_list, ElementsAre(MatchesLifetime(10, 30)));
}

}  // namespace

}  // namespace berberis
