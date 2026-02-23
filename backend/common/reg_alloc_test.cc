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

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Ne;
using ::testing::Property;

MATCHER_P2(MatchesLifetime, begin, end, "") {
  return arg.begin() == begin && arg.end() == end;
}

// Zero is an invalid MachineReg, so we start at 1.
// The bitmask has bit 'i' set if register 'i' is in the class.
const MachineRegClass kGPRRegClass = {"GPR", 8, 0b0110, 2, {MachineReg{1}, MachineReg{2}}};
const MachineRegClass kFPRegClass = {"FP", 8, 0b11000, 2, {MachineReg{3}, MachineReg{4}}};
const MachineRegClass kNarrowRegClass = {"Narrow", 8, 0b0010, 1, {MachineReg{1}}};
const MachineRegClass kWideRegClass = {"Wide", 8, 0b0110, 2, {MachineReg{1}, MachineReg{2}}};

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

class MockMachineTwoDefs : public MachineInsn {
 public:
  MockMachineTwoDefs(MachineReg reg1, const MachineRegClass* rc1,
                     MachineReg reg2, const MachineRegClass* rc2)
      : MachineInsn(static_cast<MachineOpcode>(0),
                    2,
                    reg_kinds_,
                    regs_,
                    kMachineInsnDefault),
        regs_{reg1, reg2},
        reg_kinds_{{rc1, MachineRegKind::kDef}, {rc2, MachineRegKind::kDef}} {}

  std::string GetDebugString() const override { return "mock_two_defs"; }
  void Emit(CodeEmitter*) const override {}

 private:
  friend MockMachineTwoDefs* NewInArena<MockMachineTwoDefs, const MockMachineTwoDefs&>(
      Arena*, const MockMachineTwoDefs&);
  MachineInsn* Clone(Arena* arena) const override {
    return NewInArena<MockMachineTwoDefs>(arena, *this);
  }
  MachineInsnList Lower(Arena* arena) const override {
    FATAL("Should not be called");
    return MachineInsnList(arena);
  }

  MachineReg regs_[2];
  MachineRegKind reg_kinds_[2];
};

class MockMachineOneUse : public MachineInsn {
 public:
  MockMachineOneUse(MachineReg reg, const MachineRegClass* rc)
      : MachineInsn(static_cast<MachineOpcode>(0),
                    1,
                    reg_kinds_,
                    regs_,
                    kMachineInsnDefault),
        regs_{reg},
        reg_kinds_{{rc, MachineRegKind::kUse}} {}

  std::string GetDebugString() const override { return "mock_one_use"; }
  void Emit(CodeEmitter*) const override {}

 private:
  friend MockMachineOneUse* NewInArena<MockMachineOneUse, const MockMachineOneUse&>(
      Arena*, const MockMachineOneUse&);
  MachineInsn* Clone(Arena* arena) const override {
    return NewInArena<MockMachineOneUse>(arena, *this);
  }
  MachineInsnList Lower(Arena* arena) const override {
    FATAL("Should not be called");
    return MachineInsnList(arena);
  }

  MachineReg regs_[1];
  MachineRegKind reg_kinds_[1];
};

class RegAllocTest : public ::testing::Test {
 protected:
  RegAllocTest() : machine_ir_(&arena_, 0, 0), bb_(machine_ir_.NewBasicBlock()) {}

  VRegLifetime* CreateLifetime(int vreg_index,
                               int begin,
                               int end,
                               const MachineRegClass* reg_class = &kGPRRegClass) {
    return CreateLifetimeImpl(vreg_index, reg_class, begin, begin + 1, end - 1, end);
  }

  VRegLifetime* CreateLifetimeWithLongAccess(int vreg_index,
                                             int begin,
                                             int end,
                                             const MachineRegClass* reg_class = &kGPRRegClass) {
    return CreateLifetimeImpl(vreg_index, reg_class, begin, begin + 2, end - 2, end);
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

 private:
  VRegLifetime* CreateLifetimeImpl(int vreg_index,
                                   const MachineRegClass* reg_class,
                                   int first_access_begin,
                                   int first_access_end,
                                   int second_access_begin,
                                   int second_access_end) {
    MachineReg vreg = MachineReg::CreateVRegFromIndex(vreg_index);
    auto* insn = machine_ir_.NewInsn<MockMachineInsn>(vreg, reg_class);
    bb_->insn_list().push_back(insn);
    MachineInsnListPosition pos(&bb_->insn_list(), std::prev(bb_->insn_list().end()));
    // Pos 0 in MockMachineInsn describes the DEF access.
    VRegAccess access1(pos, 0, first_access_begin, first_access_end);
    // Pos 1 in MockMachineInsn describes the USE access.
    VRegAccess access2(pos, 1, second_access_begin, second_access_end);
    auto* lifetime = NewInArena<VRegLifetime>(&arena_, &arena_, access1.begin());
    lifetime->AppendAccess(access1);
    lifetime->AppendAccess(access2);
    return lifetime;
  }

 protected:
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
  EXPECT_THAT(result, FieldsAre(Ne(kInfiniteSpillWeight), SPLIT_OK));
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_SplitConflict) {
  HardRegAllocation hard_reg_allocation(&arena_);

  // Lifetime 1: access at [10, 11).
  VRegLifetime* lifetime1 = CreateLifetimeWithAccess(0, 10, 11);
  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  // Lifetime 2: starts at 10. Narrow class to make the conflict resolvable.
  VRegLifetime* lifetime2 = CreateLifetime(1, 10, 20, &kNarrowRegClass);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));

  auto result = hard_reg_allocation.ConsiderSpill(lifetime2);
  EXPECT_THAT(result, FieldsAre(Ne(kInfiniteSpillWeight), SPLIT_CONFLICT));
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
  EXPECT_THAT(result, FieldsAre(kInfiniteSpillWeight, SPLIT_IMPOSSIBLE));
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_SplitConflict_Resolvable) {
  HardRegAllocation hard_reg_allocation(&arena_);

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
  EXPECT_THAT(result, FieldsAre(Ne(kInfiniteSpillWeight), SPLIT_CONFLICT));
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_SplitConflictWithAccessSeparation) {
  HardRegAllocation hard_reg_allocation(&arena_);

  // Lifetime 1: overall reg class Narrow (intersection of Narrow and Wide).
  // Access 1: [0, 5), Narrow (1 reg).
  // Access 2: [10, 11), Wide (2 regs).
  // Intersection is Narrow.
  MachineReg vreg0 = MachineReg::CreateVRegFromIndex(0);
  auto* insn1 = machine_ir_.NewInsn<MockMachineInsn>(vreg0, &kNarrowRegClass);
  bb_->insn_list().push_back(insn1);
  MachineInsnListPosition pos1(&bb_->insn_list(), std::prev(bb_->insn_list().end()));
  VRegAccess access1(pos1, 0, 0, 5);

  auto* insn2 = machine_ir_.NewInsn<MockMachineInsn>(vreg0, &kWideRegClass);
  bb_->insn_list().push_back(insn2);
  MachineInsnListPosition pos2(&bb_->insn_list(), std::prev(bb_->insn_list().end()));
  VRegAccess access2(pos2, 0, 10, 11);

  auto* lifetime1 = NewInArena<VRegLifetime>(&arena_, &arena_, access1.begin());
  lifetime1->AppendAccess(access1);
  lifetime1->AppendAccess(access2);

  EXPECT_EQ(lifetime1->GetRegClass()->NumRegs(), kNarrowRegClass.NumRegs());

  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));

  // Lifetime 2: Narrow (1 reg). Starts at 10.
  // Conflicts with Access 2 of Lifetime 1.
  VRegLifetime* lifetime2 = CreateLifetime(1, 10, 20, &kNarrowRegClass);
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));

  // Access 2 of Lifetime 1 is Wide (2 regs) > Lifetime 2 (1 reg).
  // So it should be separable.
  auto result = hard_reg_allocation.ConsiderSpill(lifetime2);
  EXPECT_THAT(result, FieldsAre(Ne(kInfiniteSpillWeight),
                                SPLIT_CONFLICT_WITH_ACCESS_SEPARATION));
}

TEST_F(RegAllocTest, HardRegAllocation_ConsiderSpill_Priorities) {
  // Test that IMPOSSIBLE > CONFLICT > OK.
  // Note: We deliberately violate the "assign in order" rule to setup
  // multiple disjoint lifetimes in the register that all interfere with
  // the new lifetime (which overlaps them all).

  // Scenario 1: OK + CONFLICT (Resolvable) -> CONFLICT
  {
    HardRegAllocation hra(&arena_);
    // L1: [0, 5). Disjoint from L2 and New. Assigned to same hard reg as L2.
    // Split at 5 (New start) -> OK (no access at 5).
    VRegLifetime* l1 = CreateLifetime(0, 0, 5);
    EXPECT_TRUE(hra.TryAssign(l1));

    // L2: [5, 15). Access [5, 6). Intersects New.
    // Split at 5 -> CONFLICT (access at 5).
    // Use Wide/Narrow to make it resolvable.
    const MachineRegClass kWideRegClass = {"Wide", 8, 0b0110, 2, {MachineReg{1}, MachineReg{2}}};
    VRegLifetime* l2 = CreateLifetime(1, 5, 15, &kWideRegClass);
    EXPECT_TRUE(hra.TryAssign(l2));

    // New: [5, 25).
    VRegLifetime* l_new = CreateLifetime(2, 5, 25, &kNarrowRegClass);
    EXPECT_FALSE(hra.TryAssign(l_new));

    auto res = hra.ConsiderSpill(l_new);
    EXPECT_THAT(res, FieldsAre(Ne(kInfiniteSpillWeight), SPLIT_CONFLICT));
  }

  // Scenario 2: OK + IMPOSSIBLE -> IMPOSSIBLE
  {
    HardRegAllocation hra(&arena_);
    // L1: [0, 5). Split at 5 -> OK.
    VRegLifetime* l1 = CreateLifetime(0, 0, 5);
    EXPECT_TRUE(hra.TryAssign(l1));

    // L2: [5, 15). Access [5, 6).
    // Split at 5 -> IMPOSSIBLE (access at 5, Subset).
    VRegLifetime* l2 = CreateLifetime(1, 5, 15);  // Default GPR
    EXPECT_TRUE(hra.TryAssign(l2));

    // New: [5, 25).
    // Intersects L1 (OK) and L2 (IMPOSSIBLE).
    VRegLifetime* l_new = CreateLifetime(2, 5, 25);  // Default GPR
    EXPECT_FALSE(hra.TryAssign(l_new));

    auto res = hra.ConsiderSpill(l_new);
    EXPECT_THAT(res, FieldsAre(kInfiniteSpillWeight, SPLIT_IMPOSSIBLE));
  }
}

TEST_F(RegAllocTest, HardRegAllocation_SpillAndAssign) {
  HardRegAllocation hard_reg_allocation(&arena_);

  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 15, 25)}, &arena_);
  auto* lifetime1 = &lifetime_list.front();
  auto* lifetime2 = &lifetime_list.back();

  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));
  EXPECT_THAT(hard_reg_allocation.ConsiderSpill(lifetime2),
              FieldsAre(Ne(kInfiniteSpillWeight), _));

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

TEST_F(RegAllocTest, VRegLifetimeAllocator_Spill_SpilledLifetimeBeforeNew) {
  // L1: [9, 30). Assigned to R1. First access [9, 11).
  // Use kGPRRegClass to allow reallocation to R2 after spill.
  VRegLifetime* l1 = CreateLifetimeWithLongAccess(0, 9, 30, &kGPRRegClass);
  // L2: [10, 20). First access [10, 11). Wants R1.
  VRegLifetime* l2 = CreateLifetime(1, 10, 20, &kNarrowRegClass);

  // L2 evicts L1.
  // L1 split creates spilled lifetime [9, 30) which is reallocated AFTER L2 (which starts at 10).
  // This triggers the reallocation with slightly violated lifetime begin order.

  VRegLifetimeList lifetime_list({*l1, *l2}, &arena_);

  VRegLifetimeAllocator allocator(&machine_ir_, &lifetime_list);
  allocator.Allocate();

  auto it = lifetime_list.begin();
  auto* l = &*it++;
  // Original L1 is spilled and split from the beginning leaving an empty lifetime.
  EXPECT_TRUE(l->IsEmpty());
  EXPECT_EQ(l->hard_reg(), MachineReg{1});
  EXPECT_NE(l->GetSpill(), -1);
  l = &*it++;
  EXPECT_EQ(l->begin(), 10);
  EXPECT_EQ(l->end(), 20);
  EXPECT_EQ(l->hard_reg(), MachineReg{1});
  EXPECT_EQ(l->GetSpill(), -1);
  l = &*it++;
  // Split part of L1 is spilled (evicted) and reallocated to R2.
  EXPECT_EQ(l->begin(), 9);
  EXPECT_EQ(l->end(), 30);
  EXPECT_EQ(l->hard_reg(), MachineReg{2});
  EXPECT_NE(l->GetSpill(), -1);
}

TEST_F(RegAllocTest, VRegLifetimeAllocator_SplitConflictingNarrowLifetime) {
  MachineReg v1 = MachineReg::CreateVRegFromIndex(0);
  MachineReg v2 = MachineReg::CreateVRegFromIndex(1);

  auto* insn1 = machine_ir_.NewInsn<MockMachineTwoDefs>(v1, &kWideRegClass, v2, &kWideRegClass);
  bb_->insn_list().push_back(insn1);
  auto* insn2 = machine_ir_.NewInsn<MockMachineOneUse>(v1, &kNarrowRegClass);
  bb_->insn_list().push_back(insn2);
  auto* insn3 = machine_ir_.NewInsn<MockMachineOneUse>(v2, &kNarrowRegClass);
  bb_->insn_list().push_back(insn3);

  MachineInsnListPosition pos1(&bb_->insn_list(), bb_->insn_list().begin());
  MachineInsnListPosition pos2(&bb_->insn_list(), std::next(bb_->insn_list().begin()));
  MachineInsnListPosition pos3(&bb_->insn_list(), std::next(bb_->insn_list().begin(), 2));

  // v1: [10, 20). Wide def at 10, Narrow use at 29.
  // v2: [10, 30). Wide def at 10, Narrow use at 19.
  // v2 evicts v1 creating wide tiny lifetime at 10, which can be successfully reallocated.

  auto* l1 = NewInArena<VRegLifetime>(&arena_, &arena_, 10);
  l1->AppendAccess(VRegAccess(pos1, 0, 10, 11));
  l1->AppendAccess(VRegAccess(pos2, 0, 29, 30));


  auto* l2 = NewInArena<VRegLifetime>(&arena_, &arena_, 10);
  l2->AppendAccess(VRegAccess(pos1, 1, 10, 11));
  l2->AppendAccess(VRegAccess(pos3, 0, 19, 20));

  VRegLifetimeList lifetime_list({*l1, *l2}, &arena_);

  VRegLifetimeAllocator allocator(&machine_ir_, &lifetime_list);
  allocator.Allocate();

  bool tiny_wide_lifetime_found = false;
  bool first_narrow_lifetime_found = false;
  bool second_narrow_lifetime_found = false;

  for (const auto& lifetime : lifetime_list) {
    if (lifetime.IsEmpty()) continue;
    EXPECT_TRUE(lifetime.hard_reg().IsHardReg());

    if (lifetime.GetSpill() != -1) {
      if (lifetime.begin() == 10 && lifetime.end() == 11) {
        tiny_wide_lifetime_found = true;
      } else if (lifetime.begin() == 29 && lifetime.end() == 30) {
        first_narrow_lifetime_found = true;
      }
    } else if (lifetime.begin() == 10 && lifetime.end() == 20) {
      second_narrow_lifetime_found = true;
    }
  }

  EXPECT_TRUE(tiny_wide_lifetime_found);
  EXPECT_TRUE(first_narrow_lifetime_found);
  EXPECT_TRUE(second_narrow_lifetime_found);
}

}  // namespace

}  // namespace berberis
