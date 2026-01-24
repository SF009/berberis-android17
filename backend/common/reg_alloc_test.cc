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

#include "berberis/backend/common/lifetime.h"
#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/common/reg_alloc.h"
#include "berberis/base/arena_alloc.h"

#include "reg_alloc_internal.h"

#include <iterator>

namespace berberis {

void CoalesceLifetimes(VRegLifetimeList* lifetimes);

namespace {

// Zero is an invalid MachineReg, so we start at 1.
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
  EXPECT_NE(hard_reg_allocation.ConsiderSpill(lifetime2), kInfiniteSpillWeight);
}

TEST_F(RegAllocTest, HardRegAllocation_SpillAndAssign) {
  HardRegAllocation hard_reg_allocation(&arena_);

  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 15, 25)}, &arena_);
  auto* lifetime1 = &lifetime_list.front();
  auto* lifetime2 = &lifetime_list.back();

  EXPECT_TRUE(hard_reg_allocation.TryAssign(lifetime1));
  EXPECT_FALSE(hard_reg_allocation.TryAssign(lifetime2));
  EXPECT_NE(hard_reg_allocation.ConsiderSpill(lifetime2), kInfiniteSpillWeight);

  auto it = std::next(lifetime_list.begin());

  hard_reg_allocation.SpillAndAssign(lifetime2, 1, &lifetime_list, it);

  int spill_slot = lifetime1->GetSpill();
  EXPECT_NE(spill_slot, -1);
  // After splitting, the original lifetime should only contain the first access.
  EXPECT_EQ(lifetime1->end(), 10 + 1);
  // One tiny lifetime from the second access should be added.
  EXPECT_EQ(lifetime_list.size(), 3u);
  auto* tiny_lifetime = &lifetime_list.back();
  EXPECT_EQ(tiny_lifetime->GetSpill(), spill_slot);
  EXPECT_EQ(tiny_lifetime->begin(), 20 - 1);
  EXPECT_EQ(tiny_lifetime->end(), 20);
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

  EXPECT_EQ(lifetime_list.size(), 2u);
  EXPECT_EQ(lifetime_list.begin()->begin(), 10);
  EXPECT_EQ(std::next(lifetime_list.begin())->begin(), 20);
}

TEST_F(RegAllocTest, CoalesceLifetimes_SimpleMerge) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 20, 30)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_EQ(lifetime_list.size(), 1u);
  // Lifetime1 should now cover both ranges.
  EXPECT_EQ(lifetime_list.front().begin(), 10);
  EXPECT_EQ(lifetime_list.front().end(), 30);
}

TEST_F(RegAllocTest, CoalesceLifetimes_MergeWithGap) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 30, 40)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_EQ(lifetime_list.size(), 1u);
  EXPECT_EQ(lifetime_list.front().begin(), 10);
  EXPECT_EQ(lifetime_list.front().end(), 40);
}

TEST_F(RegAllocTest, CoalesceLifetimes_NoMerge_Interference) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20), *CreateLifetime(1, 15, 25)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_EQ(lifetime_list.size(), 2u);
}

TEST_F(RegAllocTest, CoalesceLifetimes_NoMerge_DifferentRegClass) {
  VRegLifetimeList lifetime_list({*CreateLifetime(0, 10, 20, &kGPRRegClass),
                                  *CreateLifetime(1, 20, 30, &kFPRegClass)},
                                 &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_EQ(lifetime_list.size(), 2u);
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

  EXPECT_EQ(lifetime_list.size(), 1u);
  EXPECT_EQ(lifetime_list.front().begin(), 10);
  EXPECT_EQ(lifetime_list.front().end(), 40);
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
  EXPECT_EQ(lifetime_list.size(), 1u);
  EXPECT_EQ(lifetime_list.front().begin(), 10);
  EXPECT_EQ(lifetime_list.front().end(), 40);
}

TEST_F(RegAllocTest, CoalesceLifetimes_MergeOrder) {
  // Verify that coalescing works when the candidate appears later in the list.
  // Here, later lifetime2 is first in the list and it should merge earlier
  // lifetime1 (which is second).
  VRegLifetimeList lifetime_list({*CreateLifetime(1, 20, 30), *CreateLifetime(0, 10, 20)}, &arena_);

  lifetime_list.front().LinkCoalescingCandidate(&lifetime_list.back());

  CoalesceLifetimes(&lifetime_list);

  EXPECT_EQ(lifetime_list.size(), 1u);
  // The resulting lifetime should be later lifetime2 (because it was first in list and
  // merged lifetime1).
  EXPECT_EQ(lifetime_list.front().begin(), 10); // lifetime1 started at 10
  EXPECT_EQ(lifetime_list.front().end(), 30); // lifetime2 ended at 30
}

}  // namespace

}  // namespace berberis
