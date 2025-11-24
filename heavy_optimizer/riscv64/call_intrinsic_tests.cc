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

#include <cstdint>
#include <tuple>

#include "berberis/assembler/machine_code.h"
#include "berberis/backend/x86_64/code_gen.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/bit_util.h"
#include "berberis/base/logging.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/test_utils/scoped_exec_region.h"
#include "berberis/test_utils/testing_run_generated_code.h"

#include "frontend.h"

namespace berberis {

namespace {

// Upper 16-bits must be zero in a valid address.
constexpr GuestAddr kStartGuestAddr = 0x0000'aaaa'bbbb'ccccULL;

class HeavyOptimizerCallIntrinsicTest : public ::testing::Test {
 public:
  HeavyOptimizerCallIntrinsicTest()
      : arena_(), machine_ir_(&arena_), state_{}, frontend_(&machine_ir_, kStartGuestAddr) {}

  void RunOneInsn() { RunImpl(kStartGuestAddr + 4); }

  void RunBranch(GuestAddr stop_pc) { RunImpl(stop_pc); }

 private:
  void RunImpl(GuestAddr stop_pc) {
    // We can run the same code multiple times, but compile it once.
    if (machine_code_.install_size() == 0) {
      frontend_.Finalize(kStartGuestAddr + 4);
      x86_64::GenCode(&machine_ir_, &machine_code_);
    }

    ScopedExecRegion exec(&machine_code_);

    TestingRunGeneratedCode(&state_, exec.get(), stop_pc);
  }

  Arena arena_;
  x86_64::MachineIR machine_ir_;
  MachineCode machine_code_;

 protected:
  ThreadState state_;
  HeavyOptimizerFrontend frontend_;
};

#define DEFINE_CALL_INTRINSIC_INT_TEST(IntType)                                               \
  TEST_F(HeavyOptimizerCallIntrinsicTest, CallIntrinsic_##IntType) {                          \
    auto [result] = frontend_.CallIntrinsic(                                                  \
        +[]() -> IntType { return static_cast<IntType>(0xaaaa'aaaa'aaaa'aaaa); });            \
                                                                                              \
    frontend_.SetReg(3, result);                                                              \
                                                                                              \
    RunOneInsn();                                                                             \
    EXPECT_EQ(state_.cpu.x[3], ExpectedResult<IntType>(0xaaaa'aaaa'aaaa'aaaa));               \
  }                                                                                           \
                                                                                              \
  TEST_F(HeavyOptimizerCallIntrinsicTest, CallIntrinsic_##IntType##_x2) {                     \
    auto [result1, result2] = frontend_.CallIntrinsic(+[]() -> std::tuple<IntType, IntType> { \
      return {static_cast<IntType>(0xaaaa'aaaa'aaaa'aaaa),                                    \
              static_cast<IntType>(0xbbbb'bbbb'bbbb'bbbb)};                                   \
    });                                                                                       \
                                                                                              \
    frontend_.SetReg(3, result1);                                                             \
    frontend_.SetReg(4, result2);                                                             \
                                                                                              \
    RunOneInsn();                                                                             \
    EXPECT_EQ(state_.cpu.x[3], ExpectedResult<IntType>(0xaaaa'aaaa'aaaa'aaaa));               \
    EXPECT_EQ(state_.cpu.x[4], ExpectedResult<IntType>(0xbbbb'bbbb'bbbb'bbbb));               \
  }                                                                                           \
                                                                                              \
  TEST_F(HeavyOptimizerCallIntrinsicTest, CallIntrinsic_##IntType##_x3) {                     \
    auto [result1, result2, result3] =                                                        \
        frontend_.CallIntrinsic(+[]() -> std::tuple<uint64_t, uint64_t, IntType> {            \
          return {0xaaaa'aaaa'aaaa'aaaa,                                                      \
                  0xbbbb'bbbb'bbbb'bbbb,                                                      \
                  static_cast<IntType>(0xcccc'cccc'cccc'cccc)};                               \
        });                                                                                   \
                                                                                              \
    frontend_.SetReg(3, result1);                                                             \
    frontend_.SetReg(4, result2);                                                             \
    ;                                                                                         \
    frontend_.SetReg(5, result3);                                                             \
                                                                                              \
    RunOneInsn();                                                                             \
    EXPECT_EQ(state_.cpu.x[3], 0xaaaa'aaaa'aaaa'aaaa);                                        \
    EXPECT_EQ(state_.cpu.x[4], 0xbbbb'bbbb'bbbb'bbbb);                                        \
    EXPECT_EQ(state_.cpu.x[5], ExpectedResult<IntType>(0xcccc'cccc'cccc'cccc));               \
  }                                                                                           \
                                                                                              \
  TEST_F(HeavyOptimizerCallIntrinsicTest, CallIntrinsic_##IntType##_x4) {                     \
    auto [result1, result2, result3, result4] =                                               \
        frontend_.CallIntrinsic(+[]() -> std::tuple<uint64_t, uint64_t, IntType, IntType> {   \
          return {0xaaaa'aaaa'aaaa'aaaa,                                                      \
                  0xbbbb'bbbb'bbbb'bbbb,                                                      \
                  static_cast<IntType>(0xcccc'cccc'cccc'cccc),                                \
                  static_cast<IntType>(0xdddd'dddd'dddd'dddd)};                               \
        });                                                                                   \
                                                                                              \
    frontend_.SetReg(3, result1);                                                             \
    frontend_.SetReg(4, result2);                                                             \
    frontend_.SetReg(5, result3);                                                             \
    frontend_.SetReg(6, result4);                                                             \
                                                                                              \
    RunOneInsn();                                                                             \
    EXPECT_EQ(state_.cpu.x[3], 0xaaaa'aaaa'aaaa'aaaa);                                        \
    EXPECT_EQ(state_.cpu.x[4], 0xbbbb'bbbb'bbbb'bbbb);                                        \
    EXPECT_EQ(state_.cpu.x[5], ExpectedResult<IntType>(0xcccc'cccc'cccc'cccc));               \
    EXPECT_EQ(state_.cpu.x[6], ExpectedResult<IntType>(0xdddd'dddd'dddd'dddd));               \
  }

// Determines what happens when source value is truncated to ResultType and then processed by
// CallIntrinsic to produce value in 64-bit register.
template <typename IntermediateIntType>
uint64_t ExpectedResult(uint64_t original_value) {
  if constexpr (sizeof(IntermediateIntType) < 8) {
    return static_cast<uint64_t>(
        static_cast<int32_t>(static_cast<IntermediateIntType>(original_value)));
  } else {
    return original_value;
  }
}

DEFINE_CALL_INTRINSIC_INT_TEST(int8_t)
DEFINE_CALL_INTRINSIC_INT_TEST(uint8_t)
DEFINE_CALL_INTRINSIC_INT_TEST(int16_t)
DEFINE_CALL_INTRINSIC_INT_TEST(uint16_t)
DEFINE_CALL_INTRINSIC_INT_TEST(int32_t)
DEFINE_CALL_INTRINSIC_INT_TEST(uint32_t)
DEFINE_CALL_INTRINSIC_INT_TEST(int64_t)
DEFINE_CALL_INTRINSIC_INT_TEST(uint64_t)

#undef DEFINE_CALL_INTRINSIC_INT_TEST

}  // namespace

}  // namespace berberis
