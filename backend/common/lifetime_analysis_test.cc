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

}  // namespace

}  // namespace berberis
