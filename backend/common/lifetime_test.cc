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

#include <string>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/arena_alloc.h"


namespace berberis {

namespace {

// TODO(b/459067902): Move this to a common test utils file.
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

class VRegLifetimeTest : public ::testing::Test {
 protected:
  VRegLifetimeTest()
      : machine_ir_(&arena_, 0, 0),
        bb_(machine_ir_.NewBasicBlock()),
        lifetime_(&arena_) {}

  VRegAccess CreateAccess(int begin, int end) {
    auto* insn = machine_ir_.NewInsn<GenericInsn>(kVRegDst, kVRegSrc);
    bb_->insn_list().push_back(insn);
    return VRegAccess(MachineInsnListPosition(&bb_->insn_list(), std::prev(bb_->insn_list().end())),
                      0,
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
  lifetime_.Split(split_pos, &new_lifetimes);

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
  lifetime_.Split(split_pos, &new_lifetimes);

  EXPECT_EQ(new_lifetimes.size(), 1u);
  EXPECT_EQ(lifetime_.end(), 9);
  EXPECT_EQ(new_lifetimes.front().begin(), 11);
  EXPECT_EQ(new_lifetimes.front().end(), 20);
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
  lifetime_.Split(split_pos, &new_lifetimes);

  EXPECT_EQ(new_lifetimes.size(), 1u);
  EXPECT_EQ(lifetime_.end(), 10);
  EXPECT_EQ(new_lifetimes.front().begin(), 21);
  EXPECT_EQ(new_lifetimes.front().end(), 30);
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
  lifetime_.Split(split_pos, &new_lifetimes);

  // Expecting 2 new lifetimes: one from the remainder of first range, one from the second range.
  EXPECT_EQ(new_lifetimes.size(), 2u);

  // Old lifetime
  EXPECT_EQ(lifetime_.end(), 9);

  // New lifetimes
  auto it = new_lifetimes.begin();
  EXPECT_EQ(it->begin(), 11);
  EXPECT_EQ(it->end(), 20);
  ++it;
  EXPECT_EQ(it->begin(), 30);
  EXPECT_EQ(it->end(), 40);
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
  lifetime_.Split(split_pos, &new_lifetimes);

  ASSERT_EQ(new_lifetimes.size(), 1u);
  EXPECT_EQ(new_lifetimes.front().GetSpill(), kSpillSlot);
}

TEST_F(VRegLifetimeTest, Split_AtBeginning) {
  StartLiveRange(0);
  AddAccess(0, 10);

  SplitPos split_pos;
  // Split at 0.
  lifetime_.FindSplitPos(0, &split_pos);

  VRegLifetime::List new_lifetimes(&arena_);
  lifetime_.Split(split_pos, &new_lifetimes);

  EXPECT_EQ(new_lifetimes.size(), 1u);
  EXPECT_EQ(new_lifetimes.front().begin(), 0);
  EXPECT_EQ(new_lifetimes.front().end(), 10);
  // The original lifetime is now empty.
  EXPECT_TRUE(lifetime_.GetLiveRangesForTesting().empty());

}

} // namespace

} // namespace berberis
