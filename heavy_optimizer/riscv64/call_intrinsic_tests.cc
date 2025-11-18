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

#include "call_intrinsic.h"

#include <cstdint>
#include <tuple>

#include "berberis/assembler/machine_code.h"
#include "berberis/backend/code_emitter.h"
#include "berberis/backend/x86_64/code_gen.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/bit_util.h"
#include "berberis/code_gen_lib/code_gen_lib.h"
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

namespace {

class ExecTest {
 public:
  ExecTest() = default;

  void Init(x86_64::MachineIR* machine_ir) {
    auto* jump = machine_ir->template NewInsn<Jump>(0);
    machine_ir->bb_list().back()->insn_list().push_back(jump);

    EXPECT_EQ(x86_64::CheckMachineIR(*machine_ir), x86_64::kMachineIRCheckSuccess);

    MachineCode machine_code;
    CodeEmitter as(
        &machine_code, machine_ir->FrameSize(), machine_ir->NumBasicBlocks(), machine_ir->arena());

    // We need to set exit_label_for_testing before Emit, which checks it.
    auto* exit_label = as.MakeLabel();
    as.set_exit_label_for_testing(exit_label);

    // Save callee saved regs.
    as.Push(as.rbp);
    as.Push(as.rbx);
    as.Push(as.r12);
    as.Push(as.r13);
    as.Push(as.r14);
    as.Push(as.r15);
    // Align stack for calls.
    as.Subq(as.rsp, 8);

    x86_64::GenCode(machine_ir, &machine_code, x86_64::GenCodeParams{.skip_emit = true});
    machine_ir->Emit(&as);

    as.Bind(exit_label);

    as.Addq(as.rsp, 8);
    // Restore callee saved regs.
    as.Pop(as.r15);
    as.Pop(as.r14);
    as.Pop(as.r13);
    as.Pop(as.r12);
    as.Pop(as.rbx);
    as.Pop(as.rbp);

    as.Ret();

    as.Finalize();

    exec_.Init(&machine_code);
  }

  void Exec() const { exec_.get<void()>()(); }

 private:
  ScopedExecRegion exec_;
};

__attribute__((naked)) std::tuple<uint64_t> CopyU64(uint64_t) {
  asm(R"(
    movq %rdi, %rax
    ret
  )");
}

template <typename IntrinsicFunc,
          typename T,
          typename std::enable_if_t<std::is_integral_v<T>, bool> = true>
void CallOneArgumentIntrinsicUseIntegral(IntrinsicFunc func, T argument, uint64_t* result) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());
  MachineReg flag_register = builder.ir()->AllocVReg();
  MachineReg result_register = builder.ir()->AllocVReg();
  MachineReg result_value_addr_reg = builder.ir()->AllocVReg();

  CallIntrinsicImpl(&builder, func, result_register, flag_register, argument);

  builder.Gen<x86_64::MovqRegImm>(result_value_addr_reg, bit_cast<uintptr_t>(result));
  builder.Gen<x86_64::MovqOpReg>({.base = result_value_addr_reg}, result_register);

  ExecTest test;
  test.Init(&machine_ir);
  test.Exec();
}

template <typename IntrinsicFunc>
void CallOneArgumentIntrinsicUseRegister(IntrinsicFunc func, uint64_t argument, uint64_t* result) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());
  MachineReg flag_register = builder.ir()->AllocVReg();
  MachineReg argument_register = builder.ir()->AllocVReg();
  MachineReg result_register = builder.ir()->AllocVReg();
  MachineReg result_value_addr_reg = builder.ir()->AllocVReg();

  builder.Gen<x86_64::MovqRegImm>(argument_register, argument);

  CallIntrinsicImpl(&builder, func, result_register, flag_register, argument_register);

  builder.Gen<x86_64::MovqRegImm>(result_value_addr_reg, bit_cast<uintptr_t>(result));
  builder.Gen<x86_64::MovqOpReg>({.base = result_value_addr_reg}, result_register);

  ExecTest test;
  test.Init(&machine_ir);
  test.Exec();
}

TEST(HeavyOptimizerCallIntrinsicTest22, U32Result) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint32_t> (*)(uint64_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint32_t> (*)(uint64_t)>(CopyU64),
                                      0xaaaa'bbbb'5ccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0x5ccc'eeffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, I32Result) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<int32_t> (*)(uint64_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseRegister(
      reinterpret_cast<std::tuple<int32_t> (*)(uint64_t)>(CopyU64), 0xcccc'eeffULL, &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, ZeroExtendU8Arg) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint64_t> (*)(uint8_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(uint8_t)>(CopyU64),
                                      static_cast<uint8_t>(0xff),
                                      &result);
  EXPECT_EQ(result, 0xffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, ZeroExtendU16Arg) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint64_t> (*)(uint16_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xeeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseRegister(
      reinterpret_cast<std::tuple<uint64_t> (*)(uint16_t)>(CopyU64), 0xeeffULL, &result);
  EXPECT_EQ(result, 0xeeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(uint16_t)>(CopyU64),
                                      static_cast<uint16_t>(0xaaaa'bbbb'cccc'eeffULL),
                                      &result);
  EXPECT_EQ(result, 0xeeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(uint16_t)>(CopyU64),
                                      static_cast<uint16_t>(0xeeff),
                                      &result);
  EXPECT_EQ(result, 0xeeffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, SignExtendU32Arg) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint64_t> (*)(uint32_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(uint32_t)>(CopyU64),
                                      static_cast<uint32_t>(0xcccc'eeff),
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, SignExtendI8Arg) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint64_t> (*)(int8_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'ffff'ffffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(int8_t)>(CopyU64),
                                      static_cast<int8_t>(0xff),
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'ffff'ffffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, SignExtendI16Arg) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint64_t> (*)(int16_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'ffff'eeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(int16_t)>(CopyU64),
                                      static_cast<int16_t>(0xeeffULL),
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'ffff'eeffULL);
}

TEST(HeavyOptimizerCallIntrinsicTest22, SignExtendI32Arg) {
  uint64_t result = 0;
  CallOneArgumentIntrinsicUseRegister(reinterpret_cast<std::tuple<uint64_t> (*)(int32_t)>(CopyU64),
                                      0xaaaa'bbbb'cccc'eeffULL,
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);

  result = 0;
  CallOneArgumentIntrinsicUseIntegral(reinterpret_cast<std::tuple<uint64_t> (*)(int32_t)>(CopyU64),
                                      static_cast<int32_t>(0xcccc'eeff),
                                      &result);
  EXPECT_EQ(result, 0xffff'ffff'cccc'eeffULL);
}

}  // namespace

}  // namespace berberis
