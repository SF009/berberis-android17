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

#include <csignal>
#include <cstdint>
#include <cstring>

#include "berberis/assembler/machine_code.h"
#include "berberis/backend/code_emitter.h"
#include "berberis/backend/common/reg_alloc.h"
#include "berberis/backend/x86_64/lower_ssa_instructions.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/bit_util.h"
#include "berberis/code_gen_lib/code_gen_lib.h"  // EmitFreeStackFrame
#include "berberis/test_utils/scoped_exec_region.h"

namespace berberis {

namespace {

constexpr auto kMachineRegRAX = x86_64::MachineRegs::kRAX;
constexpr auto kMachineRegRBP = x86_64::MachineRegs::kRBP;
constexpr auto kMachineRegXMM0 = x86_64::MachineRegs::kXMM0;

// TODO(b/232598137): Maybe share with
// heavy_optimizer/<guest>_to_<host>/call_intrinsic_tests.cc.
class ExecTest {
 public:
  ExecTest() = default;

  void Init(x86_64::MachineIR& machine_ir) {
    // Add exiting jump if not already.
    auto* last_insn = machine_ir.bb_list().back()->insn_list().back();
    if (!machine_ir.IsControlTransfer(last_insn)) {
      auto* jump = machine_ir.template NewInsn<Jump>(0);
      machine_ir.bb_list().back()->insn_list().push_back(jump);
    }

    EXPECT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

    MachineCode machine_code;
    CodeEmitter as(
        &machine_code, machine_ir.FrameSize(), machine_ir.bb_list().size(), machine_ir.arena());

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

    machine_ir.Emit(&as);

    as.Bind(exit_label);
    // Memorize returned rax.
    as.Movq(as.rbp, bit_cast<int64_t>(&returned_rax_));
    as.Movq({.base = as.rbp}, as.rax);

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

  const RecoveryMap& recovery_map() const { return exec_.recovery_map(); }

  uint64_t returned_rax() const { return returned_rax_; }

 private:
  ScopedExecRegion exec_;
  uint64_t returned_rax_;
};

// Convert flags to LAHF-compatible format.
inline uint16_t MakeFlags(bool n, bool z, bool c, bool o) {
  return (n ? (1 << 15) : 0) | (z ? (1 << 14) : 0) | (c ? (1 << 8) : 0) | (o ? 1 : 0);
}

inline uint16_t MakeFlags(uint8_t nzco_bits) {
  return MakeFlags(nzco_bits & 0b1000, nzco_bits & 0b0100, nzco_bits & 0b0010, nzco_bits & 0b0001);
}

TEST(ExecMachineIR, Smoke) {
  struct Data {
    uint64_t x;
    uint64_t y;
  } data;

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  // Let RBP point to 'data'.
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&data));

  // data.y = data.x;
  builder.Gen<x86_64::MovqRegOp>(kMachineRegRAX,
                                 {.base = kMachineRegRBP, .disp = offsetof(Data, x)});
  builder.Gen<x86_64::MovqOpReg>({.base = kMachineRegRBP, .disp = offsetof(Data, y)},
                                 kMachineRegRAX);

  ExecTest test;
  test.Init(machine_ir);

  data.x = 1;
  data.y = 2;
  test.Exec();
  EXPECT_EQ(1ULL, data.x);
  EXPECT_EQ(1ULL, data.y);
}

TEST(ExecMachineIR, IntrinsicCallByPointer) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  MachineReg flags_register = machine_ir.AllocVReg();

  MachineReg arg = machine_ir.AllocVReg();

  uint64_t data = 0xfeed'f00d'feed'f00dULL;
  builder.Gen<x86_64::MovqRegImm>(arg, data);

  std::array<MachineReg, 1> args = {arg};
  // Note: such case wouldn't be allowed in constexpr context (e.g. as parameter of template), but
  // since we pass pointer to JIT everything works fine with bit_cast here.
  auto [call_insn, results] =
      builder.GenCallImm(bit_cast<void* (*)(void*)>(+[](uint64_t arg) -> uint64_t { return ~arg; }),
                         flags_register,
                         args);

  uint64_t result = 0;
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg}, results[0]);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(result, ~data);
}

TEST(ExecMachineIR, CallImmByPointerImmediate) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  MachineReg flags_register = machine_ir.AllocVReg();

  uint64_t data = 0xfeed'f00d'feed'f00dULL;

  std::array<__int128_t, 1> args = {__int128_t{data} | (~__int128_t{data}) << 64};
  auto [call_insn, results] = builder.GenCallImm(
      bit_cast<__int128_t (*)(__int128_t)>(+[](__int128_t arg) -> __int128_t { return ~arg; }),
      flags_register,
      args);

  __int128_t result = 0;
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg, .disp = 8}, results[1]);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(result, __int128_t{~data} | __int128_t{data} << 64);
}

template <typename IntType>
class CallImmSmallIntegerExpansionTest : public ExecTest {
 public:
  CallImmSmallIntegerExpansionTest() {
    Arena arena;
    x86_64::MachineIR machine_ir(&arena);

    x86_64::MachineIRBuilder builder(&machine_ir);
    builder.StartBasicBlock(machine_ir.NewBasicBlock());

    MachineReg flags_register = machine_ir.AllocVReg();

    MachineReg arg = machine_ir.AllocVReg();

    uint64_t data = 0xaaaa'aaaa'aaaa'aaaaULL;
    builder.Gen<x86_64::MovqRegImm>(arg, data);

    std::array<MachineReg, 1> args = {arg};
    auto [call_insn, results] = builder.GenCallImm(
        +[](IntType arg) -> uint64_t { return ~static_cast<uint64_t>(arg); }, flags_register, args);

    uint64_t result = 0;
    MachineReg result_ptr_reg = machine_ir.AllocVReg();
    builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
    builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg}, results[0]);

    AllocRegs(&machine_ir);

    Init(machine_ir);
    Exec();
    CHECK_EQ(result, ~static_cast<uint64_t>(static_cast<IntType>(data)));
  }
};

// Verify that our workaround for https://github.com/llvm/llvm-project/issues/43573 works.
TEST(ExecMachineIR, CallImmSmallIntegerExpansion) {
  CallImmSmallIntegerExpansionTest<int8_t>{};
  CallImmSmallIntegerExpansionTest<uint8_t>{};
  CallImmSmallIntegerExpansionTest<int16_t>{};
  CallImmSmallIntegerExpansionTest<uint16_t>{};
  CallImmSmallIntegerExpansionTest<int32_t>{};
  CallImmSmallIntegerExpansionTest<uint32_t>{};
  CallImmSmallIntegerExpansionTest<int64_t>{};
  CallImmSmallIntegerExpansionTest<uint64_t>{};
}

template <typename IntType>
class CallImmSmallIntegerImmediateExpansionTest : public ExecTest {
 public:
  CallImmSmallIntegerImmediateExpansionTest() {
    Arena arena;
    x86_64::MachineIR machine_ir(&arena);

    x86_64::MachineIRBuilder builder(&machine_ir);
    builder.StartBasicBlock(machine_ir.NewBasicBlock());

    MachineReg flags_register = machine_ir.AllocVReg();

    uint64_t data = 0xaaaa'aaaa'aaaa'aaaaULL;

    std::array<IntType, 1> args = {static_cast<IntType>(data)};
    auto [call_insn, results] = builder.GenCallImm(
        +[](IntType arg) -> uint64_t { return ~static_cast<uint64_t>(arg); }, flags_register, args);

    uint64_t result = 0;
    MachineReg result_ptr_reg = machine_ir.AllocVReg();
    builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
    builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg}, results[0]);

    AllocRegs(&machine_ir);

    Init(machine_ir);
    Exec();
    CHECK_EQ(result, ~static_cast<uint64_t>(static_cast<IntType>(data)));
  }
};

// Verify that our workaround for https://github.com/llvm/llvm-project/issues/43573 works.
TEST(ExecMachineIR, CallImmSmallIntegerImmediateExpansion) {
  CallImmSmallIntegerImmediateExpansionTest<int8_t>{};
  CallImmSmallIntegerImmediateExpansionTest<uint8_t>{};
  CallImmSmallIntegerImmediateExpansionTest<int16_t>{};
  CallImmSmallIntegerImmediateExpansionTest<uint16_t>{};
  CallImmSmallIntegerImmediateExpansionTest<int32_t>{};
  CallImmSmallIntegerImmediateExpansionTest<uint32_t>{};
  CallImmSmallIntegerImmediateExpansionTest<int64_t>{};
  CallImmSmallIntegerImmediateExpansionTest<uint64_t>{};
}

std::tuple<uint64_t, uint64_t> CallImmInt64OperandsTestFunc(uint64_t arg0,
                                                            uint64_t arg1,
                                                            uint64_t arg2,
                                                            uint64_t arg3,
                                                            uint64_t arg4,
                                                            uint64_t arg5) {
  uint64_t res = arg0 + arg1 + arg2 + arg3 + arg4 + arg5;
  return std::tuple<uint64_t, uint64_t>{res, res * 2};
}

TEST(ExecMachineIR, CallImmInt64Operands) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  uint64_t data = 0xfeed'f00d'feed'f00dULL;
  MachineReg data_reg = machine_ir.AllocVReg();
  MachineReg flags_register = machine_ir.AllocVReg();

  builder.Gen<x86_64::MovqRegImm>(data_reg, data);
  std::array<MachineReg, 6> args = {data_reg, data_reg, data_reg, data_reg, data_reg, data_reg};
  auto [call_insn, results] =
      builder.GenCallImm<CallImmInt64OperandsTestFunc>(flags_register, args);

  std::tuple<uint64_t, uint64_t> result = {0, 0};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg, .disp = 8}, results[1]);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(std::get<0>(result), data * 6);
  EXPECT_EQ(std::get<1>(result), data * 12);
}

std::tuple<uint32_t, uint32_t> CallImmInt32OperandsTestFunc(uint32_t arg0,
                                                            uint32_t arg1,
                                                            uint32_t arg2,
                                                            uint32_t arg3,
                                                            uint32_t arg4,
                                                            uint32_t arg5) {
  uint32_t res = arg0 + arg1 + arg2 + arg3 + arg4 + arg5;
  return std::tuple<uint32_t, uint32_t>{res, res * 2};
}

TEST(ExecMachineIR, CallImmInt32Operands) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  uint64_t data = 0xfeed'f00d'feed'f00dULL;
  MachineReg data_reg = machine_ir.AllocVReg();
  MachineReg flags_register = machine_ir.AllocVReg();

  builder.Gen<x86_64::MovqRegImm>(data_reg, data);
  std::array<MachineReg, 6> args = {data_reg, data_reg, data_reg, data_reg, data_reg, data_reg};
  auto [call_insn, results] =
      builder.GenCallImm<CallImmInt32OperandsTestFunc>(flags_register, args);

  std::tuple<uint32_t, uint32_t> result = {0, 0};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovlOpReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovlOpReg>({.base = result_ptr_reg, .disp = 4}, results[1]);

  LowerSSAInstructions(&machine_ir);
  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(std::get<0>(result), static_cast<uint32_t>(data * 6));
  EXPECT_EQ(std::get<1>(result), static_cast<uint32_t>(data * 12));
}

std::tuple<Float64, Float64> CallImmFloat64OperandsTestFunc(Float64 arg0,
                                                            Float64 arg1,
                                                            Float64 arg2,
                                                            Float64 arg3,
                                                            Float64 arg4,
                                                            Float64 arg5,
                                                            Float64 arg6,
                                                            Float64 arg7) {
  using namespace intrinsics;
  Float64 res = arg0 + arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7;
  return std::tuple{res, res * Float64{2.0}};
}

TEST(ExecMachineIR, CallImmFloat64Operands) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  Float64 data{42.0};
  MachineReg data_reg = machine_ir.AllocVReg();
  MachineReg data_xreg = machine_ir.AllocVReg();
  MachineReg flags_register = machine_ir.AllocVReg();

  builder.Gen<x86_64::MovqRegImm>(data_reg, bit_cast<uint64_t>(data));
  builder.Gen<x86_64::MovqXRegReg>(data_xreg, data_reg);
  std::array<MachineReg, 8> args = {
      data_xreg, data_xreg, data_xreg, data_xreg, data_xreg, data_xreg, data_xreg, data_xreg};
  auto [call_insn, results] =
      builder.GenCallImm<CallImmFloat64OperandsTestFunc>(flags_register, args);

  std::tuple result = {Float64{0.0}, Float64{0.0}};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovqOpXReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovqOpXReg>({.base = result_ptr_reg, .disp = 8}, results[1]);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(bit_cast<double>(std::get<0>(result)), bit_cast<double>(data) * 8.0);
  EXPECT_EQ(bit_cast<double>(std::get<1>(result)), bit_cast<double>(data) * 16.0);
}

std::tuple<Float64, Float64, Float64> CallImmFloat64OperandsMemResultTestFunc(Float64 arg0,
                                                                              Float64 arg1,
                                                                              Float64 arg2,
                                                                              Float64 arg3,
                                                                              Float64 arg4,
                                                                              Float64 arg5,
                                                                              Float64 arg6,
                                                                              Float64 arg7) {
  using namespace intrinsics;
  Float64 res = arg0 + arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7;
  return std::tuple{res, res * Float64{2.0}, res * Float64{3.0}};
}

TEST(ExecMachineIR, CallImmFloat64MemResultOperands) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  Float64 data{42.0};
  MachineReg data_reg = machine_ir.AllocVReg();
  MachineReg data_xreg = machine_ir.AllocVReg();
  MachineReg flags_register = machine_ir.AllocVReg();

  builder.Gen<x86_64::MovqRegImm>(data_reg, bit_cast<uint64_t>(data));
  builder.Gen<x86_64::MovqXRegReg>(data_xreg, data_reg);
  std::tuple result = {Float64{0.0}, Float64{0.0}, Float64{0.0}};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  std::array<MachineReg, 9> args = {result_ptr_reg,
                                    data_xreg,
                                    data_xreg,
                                    data_xreg,
                                    data_xreg,
                                    data_xreg,
                                    data_xreg,
                                    data_xreg,
                                    data_xreg};
  auto [call_insn, results] =
      builder.GenCallImm<CallImmFloat64OperandsMemResultTestFunc>(flags_register, args);

  uint64_t result_addr;
  builder.Gen<x86_64::MovqRegImm>(data_reg, bit_cast<uint64_t>(&result_addr));
  builder.Gen<x86_64::MovqOpReg>({.base = data_reg}, results[0]);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();

  EXPECT_EQ(result_addr, bit_cast<uint64_t>(&result));
  EXPECT_EQ(bit_cast<double>(std::get<0>(result)), bit_cast<double>(data) * 8.0);
  EXPECT_EQ(bit_cast<double>(std::get<1>(result)), bit_cast<double>(data) * 16.0);
  EXPECT_EQ(bit_cast<double>(std::get<2>(result)), bit_cast<double>(data) * 24.0);
}

std::tuple<Float32, Float32> CallImmFloat32OperandsTestFunc(Float32 arg0,
                                                            Float32 arg1,
                                                            Float32 arg2,
                                                            Float32 arg3,
                                                            Float32 arg4,
                                                            Float32 arg5,
                                                            Float32 arg6,
                                                            Float32 arg7) {
  using namespace intrinsics;
  Float32 res = arg0 + arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7;
  return std::tuple{res, res * Float32{2.0}};
}

TEST(ExecMachineIR, CallImmFloat32Operands) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  Float32 data{42.0};
  MachineReg data_reg = machine_ir.AllocVReg();
  MachineReg data_xreg = machine_ir.AllocVReg();
  MachineReg flags_register = machine_ir.AllocVReg();

  builder.Gen<x86_64::MovqRegImm>(data_reg, bit_cast<uint32_t>(data));
  builder.Gen<x86_64::MovdXRegReg>(data_xreg, data_reg);
  std::array<MachineReg, 8> args = {
      data_xreg, data_xreg, data_xreg, data_xreg, data_xreg, data_xreg, data_xreg, data_xreg};
  auto [call_insn, results] =
      builder.GenCallImm<CallImmFloat32OperandsTestFunc>(flags_register, args);

  std::tuple result = {Float32{0.0}, Float32{0.0}};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovdOpXReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovdOpXReg>({.base = result_ptr_reg, .disp = 4}, results[1]);

  LowerSSAInstructions(&machine_ir);
  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(bit_cast<float>(std::get<0>(result)), bit_cast<float>(data) * 8.0);
  EXPECT_EQ(bit_cast<float>(std::get<1>(result)), bit_cast<float>(data) * 16.0);
}

// RAX+RDX, Float16 have to be moved to SSE registers properly, even from 0 position.
std::tuple<Float16, uint16_t, Float16, uint16_t, Float16, uint16_t>
CallImmFloat16InIntResistersTestFunc() {
  return std::tuple{
      Float16{1.0}, uint16_t{2}, Float16{3.0}, uint16_t{4}, Float16{5.0}, uint16_t{6}};
}

TEST(ExecMachineIR, CallImmFloat16InIntResisters) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  MachineReg flags_register = machine_ir.AllocVReg();

  std::array<MachineReg, 0> args;
  auto [call_insn, results] =
      builder.GenCallImm<CallImmFloat16InIntResistersTestFunc>(flags_register, args);

  std::tuple result{
      Float16{0.0}, uint16_t{0}, Float16{0.0}, uint16_t{0}, Float16{0.0}, uint16_t{0}};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg}, results[0], int8_t{0});
  builder.Gen<x86_64::MovwOpReg>({.base = result_ptr_reg, .disp = 2}, results[1]);
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg, .disp = 4}, results[2], int8_t{0});
  builder.Gen<x86_64::MovwOpReg>({.base = result_ptr_reg, .disp = 6}, results[3]);
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg, .disp = 8}, results[4], int8_t{0});
  builder.Gen<x86_64::MovwOpReg>({.base = result_ptr_reg, .disp = 10}, results[5]);

  LowerSSAInstructions(&machine_ir);
  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(bit_cast<_Float16>(std::get<0>(result)), 1.0);
  EXPECT_EQ(std::get<1>(result), 2);
  EXPECT_EQ(bit_cast<_Float16>(std::get<2>(result)), 3.0);
  EXPECT_EQ(std::get<3>(result), 4);
  EXPECT_EQ(bit_cast<_Float16>(std::get<4>(result)), 5.0);
  EXPECT_EQ(std::get<5>(result), 6);
}

// RAX+XMM0. The call returns 3rd and 4th floats in XMM0. The 4th float is shifted and must be
// properly extracted into a separate SSE register by our intrinsic-call embedding machinery.
std::tuple<Float16, uint16_t, Float16, uint16_t, Float16, Float16>
CallImmFloat16InIntandSSEResistersTestFunc() {
  return std::tuple{Float16{1.0}, 2, Float16{3.0}, 4, Float16{5.0}, Float16{6}};
}

TEST(ExecMachineIR, CallImmFloat16InIntAndSSEResisters) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  MachineReg flags_register = machine_ir.AllocVReg();

  std::array<MachineReg, 0> args;
  auto [call_insn, results] =
      builder.GenCallImm<CallImmFloat16InIntandSSEResistersTestFunc>(flags_register, args);

  std::tuple result{
      Float16{0.0}, uint16_t{0}, Float16{0.0}, uint16_t{0}, Float16{0.0}, Float16{0.0}};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg}, results[0], int8_t{0});
  builder.Gen<x86_64::MovwOpReg>({.base = result_ptr_reg, .disp = 2}, results[1]);
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg, .disp = 4}, results[2], int8_t{0});
  builder.Gen<x86_64::MovwOpReg>({.base = result_ptr_reg, .disp = 6}, results[3]);
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg, .disp = 8}, results[4], int8_t{0});
  builder.Gen<x86_64::PextrwOpXRegImm>({.base = result_ptr_reg, .disp = 10}, results[5], int8_t{0});

  LowerSSAInstructions(&machine_ir);
  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(bit_cast<_Float16>(std::get<0>(result)), 1.0);
  EXPECT_EQ(std::get<1>(result), 2);
  EXPECT_EQ(bit_cast<_Float16>(std::get<2>(result)), 3.0);
  EXPECT_EQ(std::get<3>(result), 4);
  EXPECT_EQ(bit_cast<_Float16>(std::get<4>(result)), 5.0);
  EXPECT_EQ(bit_cast<_Float16>(std::get<5>(result)), 6.0);
}

// RAX+RDX, Float32 have to be moved to SSE registers properly, from 0 and non-0 positions.
std::tuple<Float32, uint32_t, uint32_t, Float32> CallImmFloat32InIntResistersTestFunc() {
  return std::tuple<Float32, uint32_t, uint32_t, Float32>{Float32{1.0}, 2, 3, Float32{4.0}};
}

TEST(ExecMachineIR, CallImmFloat32InIntResisters) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  MachineReg flags_register = machine_ir.AllocVReg();

  std::array<MachineReg, 0> args;
  auto [call_insn, results] =
      builder.GenCallImm<CallImmFloat32InIntResistersTestFunc>(flags_register, args);

  std::tuple<Float32, uint32_t, uint32_t, Float32> result = {Float32{0.0}, 0, 0, Float32{0.0}};
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovdOpXReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovlOpReg>({.base = result_ptr_reg, .disp = 4}, results[1]);
  builder.Gen<x86_64::MovlOpReg>({.base = result_ptr_reg, .disp = 8}, results[2]);
  builder.Gen<x86_64::MovdOpXReg>({.base = result_ptr_reg, .disp = 12}, results[3]);

  LowerSSAInstructions(&machine_ir);
  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(bit_cast<float>(std::get<0>(result)), 1.0f);
  EXPECT_EQ(std::get<1>(result), 2U);
  EXPECT_EQ(std::get<2>(result), 3U);
  EXPECT_EQ(bit_cast<float>(std::get<3>(result)), 4.0f);
}

// Two inputs would occupy 4 registers and one result would occupy RAX+RDX.
std::tuple<__uint128_t> CallImmInt128ValuesTestFunc(__uint128_t x, __uint128_t y) {
  return std::tuple<__uint128_t>{x + y};
}

TEST(ExecMachineIR, CallImmInt128Values) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  MachineReg flags_register = machine_ir.AllocVReg();

  std::array<MachineReg, 4> args;
  for (auto& arg : args) {
    arg = machine_ir.AllocVReg();
  }

  uint64_t data = 0xfeed'f00d'feed'f00dULL;
  builder.Gen<x86_64::MovqRegImm>(args[0], data);
  builder.Gen<x86_64::LeaqRegOp>(args[1], {.index = args[0], .scale = CodeEmitter::kTimesTwo});
  builder.Gen<x86_64::LeaqRegOp>(args[2], {.index = args[0], .scale = CodeEmitter::kTimesFour});
  builder.Gen<x86_64::LeaqRegOp>(args[3], {.index = args[0], .scale = CodeEmitter::kTimesEight});

  auto [call_insn, results] = builder.GenCallImm<CallImmInt128ValuesTestFunc>(flags_register, args);

  __uint128_t result = 0;
  MachineReg result_ptr_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(result_ptr_reg, bit_cast<uintptr_t>(&result));
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg}, results[0]);
  builder.Gen<x86_64::MovqOpReg>({.base = result_ptr_reg, .disp = 8}, results[1]);

  LowerSSAInstructions(&machine_ir);
  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);
  test.Exec();
  EXPECT_EQ(result, data + (__uint128_t{data} << 65) + (data << 2) + (__uint128_t{data} << 67));
}

void ClobberAllCallerSaved() {
  constexpr uint64_t kClobberValue = 0xdead'beef'dead'beefULL;
  asm volatile(
      "Movq %0, %%rax\n"
      "Movq %0, %%rcx\n"
      "Movq %0, %%rdx\n"
      "Movq %0, %%rdi\n"
      "Movq %0, %%rsi\n"
      "Movq %0, %%r8\n"
      "Movq %0, %%r9\n"
      "Movq %0, %%r10\n"
      "Movq %0, %%r11\n"
      "Movq %%rax, %%xmm0\n"
      "Movq %%rax, %%xmm1\n"
      "Movq %%rax, %%xmm2\n"
      "Movq %%rax, %%xmm3\n"
      "Movq %%rax, %%xmm4\n"
      "Movq %%rax, %%xmm5\n"
      "Movq %%rax, %%xmm6\n"
      "Movq %%rax, %%xmm7\n"
      "Movq %%rax, %%xmm8\n"
      "Movq %%rax, %%xmm9\n"
      "Movq %%rax, %%xmm10\n"
      "Movq %%rax, %%xmm11\n"
      "Movq %%rax, %%xmm12\n"
      "Movq %%rax, %%xmm13\n"
      "Movq %%rax, %%xmm14\n"
      "Movq %%rax, %%xmm15\n"
      :
      : "r"(kClobberValue)
      : "rax",
        "rcx",
        "rdx",
        "rdi",
        "rsi",
        "r8",
        "r9",
        "r10",
        "r11",
        "xmm0",
        "xmm1",
        "xmm2",
        "xmm3",
        "xmm4",
        "xmm5",
        "xmm6",
        "xmm7",
        "xmm8",
        "xmm9",
        "xmm10",
        "xmm11",
        "xmm12",
        "xmm13",
        "xmm14",
        "xmm15");
}

template <bool kWithCallImm>
void TestRegAlloc() {
  constexpr int N = 128;

  struct Data {
    uint64_t in_array[N];
    uint64_t out;
  } data{};

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  // Let rbp point to 'data'.
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&data));

  // Read data.in_array into vregs, xor and write to data.out.

  MachineReg vregs[N];
  MachineReg xmm_vregs[N];

  for (int i = 0; i < N; ++i) {
    MachineReg v = machine_ir.AllocVReg();
    vregs[i] = v;
    builder.Gen<x86_64::MovqRegOp>(
        v,
        {.base = kMachineRegRBP,
         .disp = static_cast<int32_t>(offsetof(Data, in_array) + i * sizeof(data.in_array[0]))});
    MachineReg vx = machine_ir.AllocVReg();
    xmm_vregs[i] = vx;
    builder.Gen<x86_64::MovqXRegReg>(vx, v);
  }

  if (kWithCallImm) {
    // If there is no CallImm reg-alloc assigns vregs to hard-regs until available.
    // When CallImm is here it must not allocate caller-saved regs to live across function call.
    // Ideally we should have allocated hard-regs around the call explicitly and verify that
    // reg-alloc would spill/fill them, but reg-alloc doesn't support that.
    MachineReg flag_register = machine_ir.AllocVReg();
    builder.GenCallImm<ClobberAllCallerSaved>(flag_register, std::tuple<>{});
  }

  MachineReg v0 = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(v0, 0);
  MachineReg vx0 = machine_ir.AllocVReg();
  builder.Gen<PseudoDefReg>(vx0);
  builder.Gen<x86_64::XorpdXRegXReg, x86_64::kNoSSA>(vx0, vx0);

  for (int i = 0; i < N; ++i) {
    MachineReg vflags = machine_ir.AllocVReg();
    builder.Gen<x86_64::XorqRegReg, x86_64::kNoSSA>(v0, vregs[i], vflags);
    builder.Gen<x86_64::XorpdXRegXReg, x86_64::kNoSSA>(vx0, xmm_vregs[i]);
  }

  MachineReg v1 = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegXReg>(v1, vx0);
  MachineReg vflags = machine_ir.AllocVReg();
  builder.Gen<x86_64::AddqRegReg, x86_64::kNoSSA>(v1, v0, vflags);
  builder.Gen<x86_64::MovqOpReg>({.base = kMachineRegRBP, .disp = offsetof(Data, out)}, v1);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);

  uint64_t res = 0;
  for (int i = 0; i < N; ++i) {
    // Add some irregularity to ensure the result isn't zero.
    data.in_array[i] = i + (res << 4);
    res ^= data.in_array[i];
  }
  // Sum for vregs and xmm_regs.
  res *= 2;
  test.Exec();
  EXPECT_EQ(res, data.out);
}

TEST(ExecMachineIR, SmokeRegAlloc) {
  TestRegAlloc<false>();
}

TEST(ExecMachineIR, RegAllocWithCallImm) {
  TestRegAlloc<true>();
}

TEST(ExecMachineIR, MemoryOperand) {
  struct Data {
    uint64_t in_base_disp;
    uint64_t in_index_disp;
    uint64_t in_base_index_disp[3];

    uint64_t out_base_disp;
    uint64_t out_index_disp;
    uint64_t out_base_index_disp;
  } data = {};

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  data.in_base_disp = 0xaaaa'bbbb'cccc'ddddULL;
  data.in_index_disp = 0xdead'beef'dead'beefULL;
  data.in_base_index_disp[2] = 0xcafe'feed'f00d'feedULL;

  // Base address.
  MachineReg base_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(base_reg, bit_cast<uintptr_t>(&data));

  MachineReg data_reg;

  // BaseDisp
  x86_64::MemoryOperand mem_base_disp{.base = base_reg, .disp = offsetof(Data, in_base_disp)};
  data_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovzxblRegOp>(data_reg, mem_base_disp);
  builder.Gen<x86_64::MovqOpReg>({.base = base_reg, .disp = offsetof(Data, out_base_disp)},
                                 data_reg);

  // IndexDisp
  MachineReg index_reg = machine_ir.AllocVReg();
  static_assert(alignof(struct Data) >= 2);
  builder.Gen<x86_64::MovqRegImm>(index_reg, bit_cast<uintptr_t>(&data) / 2);
  x86_64::MemoryOperand mem_index_disp = {
      .index = index_reg, .scale = CodeEmitter::kTimesTwo, offsetof(Data, in_index_disp)};
  data_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovzxblRegOp>(data_reg, mem_index_disp);
  builder.Gen<x86_64::MovqOpReg>({.base = base_reg, .disp = offsetof(Data, out_index_disp)},
                                 data_reg);

  // BaseIndexDisp
  MachineReg tmp_base_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(tmp_base_reg, bit_cast<uintptr_t>(&data.in_base_index_disp[0]));
  MachineReg tmp_index_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(tmp_index_reg, 2);
  x86_64::MemoryOperand mem_base_index_disp = {
      .base = tmp_base_reg, .index = tmp_index_reg, .scale = CodeEmitter::kTimesFour, .disp = 8};
  data_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovzxblRegOp>(data_reg, mem_base_index_disp);
  builder.Gen<x86_64::MovqOpReg>({.base = base_reg, .disp = offsetof(Data, out_base_index_disp)},
                                 data_reg);

  AllocRegs(&machine_ir);

  ExecTest test;
  test.Init(machine_ir);

  test.Exec();
  EXPECT_EQ(data.out_base_disp, 0xddU);
  EXPECT_EQ(data.out_index_disp, 0xefU);
  EXPECT_EQ(data.out_base_index_disp, 0xedU);
}

const MachineReg kGRegs[]{
    x86_64::MachineRegs::kR8,
    x86_64::MachineRegs::kR9,
    x86_64::MachineRegs::kR10,
    x86_64::MachineRegs::kR11,
    x86_64::MachineRegs::kRSI,
    x86_64::MachineRegs::kRDI,
    x86_64::MachineRegs::kRAX,
    x86_64::MachineRegs::kRBX,
    x86_64::MachineRegs::kRCX,
    x86_64::MachineRegs::kRDX,
    x86_64::MachineRegs::kR12,
    x86_64::MachineRegs::kR13,
    x86_64::MachineRegs::kR14,
    x86_64::MachineRegs::kR15,
};

const MachineReg kXmms[]{
    x86_64::MachineRegs::kXMM0,
    x86_64::MachineRegs::kXMM1,
    x86_64::MachineRegs::kXMM2,
    x86_64::MachineRegs::kXMM3,
    x86_64::MachineRegs::kXMM4,
    x86_64::MachineRegs::kXMM5,
    x86_64::MachineRegs::kXMM6,
    x86_64::MachineRegs::kXMM7,
    x86_64::MachineRegs::kXMM8,
    x86_64::MachineRegs::kXMM9,
    x86_64::MachineRegs::kXMM10,
    x86_64::MachineRegs::kXMM11,
    x86_64::MachineRegs::kXMM12,
    x86_64::MachineRegs::kXMM13,
    x86_64::MachineRegs::kXMM14,
    x86_64::MachineRegs::kXMM15,
};

class ExecMachineIRTest : public ::testing::Test {
 protected:
  struct Xmm {
    uint64_t lo;
    uint64_t hi;
  };
  static_assert(sizeof(Xmm) == 16, "bad xmm type");

  struct Data {
    uint64_t gregs[std::size(kGRegs)];
    Xmm xmms[std::size(kXmms)];
    Xmm slots[16];
  };
  static_assert(sizeof(Data) % sizeof(uint64_t) == 0, "bad data type");

  static void InitData(Data* data) {
    // Try to have all 4-byte pieces different. This way we ensure that the
    // upper half of gregs is also meaningful.
    char* p = reinterpret_cast<char*>(data);
    constexpr size_t kUnitSize = 4;
    static_assert((sizeof(Data) % kUnitSize) == 0);
    for (size_t i = 0; i < (sizeof(Data) / kUnitSize); ++i) {
      static_assert(sizeof(i) >= kUnitSize);
      memcpy(p + kUnitSize * i, &i, kUnitSize);
    }
  }

  static void ExpectEqualData(const Data& x, const Data& y) {
    for (size_t i = 0; i < std::size(x.gregs); ++i) {
      EXPECT_EQ(x.gregs[i], y.gregs[i]) << "gregs differ at index " << i;
    }
    for (size_t i = 0; i < std::size(x.xmms); ++i) {
      EXPECT_EQ(x.xmms[i].lo, y.xmms[i].lo) << "xmms lo differ at index " << i;
      EXPECT_EQ(x.xmms[i].hi, y.xmms[i].hi) << "xmms hi differ at index " << i;
    }
    for (size_t i = 0; i < std::size(x.slots); ++i) {
      EXPECT_EQ(x.slots[i].lo, y.slots[i].lo) << "slots lo differ at index " << i;
      EXPECT_EQ(x.slots[i].hi, y.slots[i].hi) << "slots hi differ at index " << i;
    }
  }

  ExecMachineIRTest() : machine_ir_(&arena_), builder_(&machine_ir_), data_{} {
    bb_ = machine_ir_.NewBasicBlock();
    builder_.StartBasicBlock(bb_);

    // Let rbp point to 'data'.
    builder_.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&data_));

    for (size_t i = 0; i < std::size(data_.slots); ++i) {
      slots_[i] = MachineReg::CreateSpilledRegFromIndex(
          machine_ir_.SpillSlotOffset(machine_ir_.AllocSpill()));

      builder_.Gen<x86_64::MovdquXRegOp>(
          kMachineRegXMM0,
          {.base = kMachineRegRBP,
           .disp = static_cast<int32_t>(offsetof(Data, slots) + i * sizeof(data_.slots[0]))});
      builder_.Gen<Copy>(slots_[i], kMachineRegXMM0, 16);
    }

    for (size_t i = 0; i < std::size(kXmms); ++i) {
      builder_.Gen<x86_64::MovdquXRegOp>(
          kXmms[i],
          {.base = kMachineRegRBP,
           .disp = static_cast<int32_t>(offsetof(Data, xmms) + i * sizeof(data_.xmms[0]))});
    }

    for (size_t i = 0; i < std::size(kGRegs); ++i) {
      builder_.Gen<x86_64::MovqRegOp>(
          kGRegs[i],
          {.base = kMachineRegRBP,
           .disp = static_cast<int32_t>(offsetof(Data, gregs) + i * sizeof(data_.gregs[0]))});
    }
  }

  void Finalize() {
    for (size_t i = 0; i < std::size(kGRegs); ++i) {
      builder_.Gen<x86_64::MovqOpReg>(
          {.base = kMachineRegRBP,
           .disp = static_cast<int32_t>(offsetof(Data, gregs) + i * sizeof(data_.gregs[0]))},
          kGRegs[i]);
    }

    for (size_t i = 0; i < std::size(kXmms); ++i) {
      builder_.Gen<x86_64::MovdquOpXReg>(
          {.base = kMachineRegRBP,
           .disp = static_cast<int32_t>(offsetof(Data, xmms) + i * sizeof(data_.xmms[0]))},
          kXmms[i]);
    }

    for (size_t i = 0; i < std::size(data_.slots); ++i) {
      builder_.Gen<Copy>(kMachineRegXMM0, slots_[i], 16);
      builder_.Gen<x86_64::MovdquOpXReg>(
          {.base = kMachineRegRBP,
           .disp = static_cast<int32_t>(offsetof(Data, slots) + i * sizeof(data_.slots[0]))},
          kMachineRegXMM0);
    }

    test_.Init(machine_ir_);
  }

  Arena arena_;
  x86_64::MachineIR machine_ir_;
  x86_64::MachineIRBuilder builder_;
  MachineBasicBlock* bb_;
  Data data_;
  MachineReg slots_[std::size(Data{}.slots)];
  ExecTest test_;
};

TEST_F(ExecMachineIRTest, Copy) {
  InitData(&data_);
  Data dst_data = data_;

  builder_.Gen<Copy>(kGRegs[1], kGRegs[0], 8);
  dst_data.gregs[1] = data_.gregs[0];

  builder_.Gen<Copy>(slots_[0], kXmms[0], 8);
  dst_data.slots[0].lo = data_.xmms[0].lo;

  builder_.Gen<Copy>(slots_[1], kXmms[1], 16);
  dst_data.slots[1] = data_.xmms[1];

  builder_.Gen<Copy>(kXmms[3], kXmms[2], 16);
  dst_data.xmms[3] = data_.xmms[2];

  // The minimum copy amount is 8 bytes. Copy of a smaller size will copy
  // garbage in upper bytes. This is in compliance with MachineIR assumptions,
  // but we cannot reliably test it.
  builder_.Gen<Copy>(slots_[5], slots_[4], 8);
  dst_data.slots[5].lo = data_.slots[4].lo;

  builder_.Gen<Copy>(slots_[7], slots_[6], 16);
  dst_data.slots[7] = data_.slots[6];

  Finalize();
  test_.Exec();
  ExpectEqualData(data_, dst_data);
}

// TODO(b/200327919): Share with tests in runtime.
class ScopedSignalHandler {
 public:
  ScopedSignalHandler(int sig, void (*action)(int, siginfo_t*, void*)) : sig_(sig) {
    struct sigaction act {};
    act.sa_sigaction = action;
    act.sa_flags = SA_SIGINFO;
    sigaction(sig_, &act, &old_act_);
  }

  ~ScopedSignalHandler() { sigaction(sig_, &old_act_, nullptr); }

 private:
  int sig_;
  struct sigaction old_act_;
};

const RecoveryMap* g_recovery_map;

void SigsegvHandler(int sig, siginfo_t*, void* context) {
  ASSERT_EQ(sig, SIGSEGV);

  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  uintptr_t rip = ucontext->uc_mcontext.gregs[REG_RIP];
  auto it = g_recovery_map->find(rip);
  ASSERT_TRUE(it != g_recovery_map->end());
  ucontext->uc_mcontext.gregs[REG_RIP] = it->second;
}

TEST(ExecMachineIR, RecoveryBlock) {
  ScopedSignalHandler handler(SIGSEGV, SigsegvHandler);

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  constexpr auto kScratchReg = kMachineRegRBP;
  auto* main_bb = machine_ir.NewBasicBlock();
  auto* exit_bb = machine_ir.NewBasicBlock();
  auto* recovery_bb = machine_ir.NewBasicBlock();

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(main_bb);
  // Cause a SIGSEGV.
  builder.Gen<x86_64::XorqRegReg, x86_64::kNoSSA>(
      kScratchReg, kScratchReg, x86_64::kMachineRegFLAGS);
  builder.Gen<x86_64::MovqOpReg>({.base = kScratchReg}, kScratchReg);
  builder.SetRecoveryPointAtLastInsn(recovery_bb);
  builder.Gen<Branch>(exit_bb);

  builder.StartBasicBlock(exit_bb);
  builder.Gen<Jump>(42ULL);

  builder.StartBasicBlock(recovery_bb);
  builder.Gen<Jump>(42ULL);

  machine_ir.AddEdge(main_bb, recovery_bb);
  machine_ir.AddEdge(main_bb, exit_bb);

  ExecTest test;
  test.Init(machine_ir);
  g_recovery_map = &test.recovery_map();

  test.Exec();

  // Guest PC for recovery is set in RAX.
  EXPECT_EQ(test.returned_rax(), 42ULL);
}

TEST(ExecMachineIR, RecoveryWithGuestPC) {
  ScopedSignalHandler handler(SIGSEGV, SigsegvHandler);

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);
  constexpr auto kScratchReg = kMachineRegRBP;

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());
  // Cause a SIGSEGV.
  builder.Gen<x86_64::XorqRegReg, x86_64::kNoSSA>(
      kScratchReg, kScratchReg, x86_64::kMachineRegFLAGS);
  builder.Gen<x86_64::MovqOpReg>({.base = kScratchReg}, kScratchReg);
  builder.SetRecoveryWithGuestPCAtLastInsn(42ULL);

  ExecTest test;
  test.Init(machine_ir);
  g_recovery_map = &test.recovery_map();

  test.Exec();

  // Guest PC for recovery is set to RAX.
  EXPECT_EQ(test.returned_rax(), 42ULL);
}

TEST(ExecMachineIR, RecoveryWithGuestPCAndSpills) {
  ScopedSignalHandler handler(SIGSEGV, SigsegvHandler);

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  // Create a lot of virtual registers to force spilling, which in turn will
  // create a non-zero stack frame.
  constexpr int kNumVRegs = 32;
  MachineReg vregs[kNumVRegs];
  for (int i = 0; i < kNumVRegs; ++i) {
    vregs[i] = machine_ir.AllocVReg();
    builder.Gen<x86_64::MovqRegImm>(vregs[i], i);
  }

  // Cause a SIGSEGV by dereferencing a null pointer.
  // Use one of the virtual registers to hold the null pointer.
  MachineReg zero_reg = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(zero_reg, 0);
  builder.Gen<x86_64::MovqOpReg>({.base = zero_reg}, zero_reg);
  builder.SetRecoveryWithGuestPCAtLastInsn(123ULL);

  // Use the other vregs after the faulting instruction to ensure they are live
  // across it. This forces the register allocator to spill them if necessary.
  // This code path is not expected to be executed but is needed for liveness
  // analysis.
  MachineReg sum = machine_ir.AllocVReg();
  builder.Gen<x86_64::MovqRegImm>(sum, 0);
  for (int i = 0; i < kNumVRegs; ++i) {
    MachineReg vflags = machine_ir.AllocVReg();
    builder.Gen<x86_64::AddqRegReg, x86_64::kNoSSA>(sum, vregs[i], vflags);
  }

  AllocRegs(&machine_ir);
  EXPECT_GT(machine_ir.FrameSize(), 0u);

  ExecTest test;
  test.Init(machine_ir);
  g_recovery_map = &test.recovery_map();

  test.Exec();

  // Verify that recovery was successful and the correct guest PC was returned.
  // A successful execution implies the stack was correctly restored by
  // EmitFreeStackFrame in the recovery path.
  EXPECT_EQ(test.returned_rax(), 123ULL);
}

TEST(ExecMachineIR, ReadFlagsWithOverflow) {
  struct Data {
    uint64_t x;
    uint64_t y;
  } data{};
  uint64_t res_flags;

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  // Let RBP point to 'data'.
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&data));
  builder.Gen<x86_64::MovqRegOp>(kMachineRegRAX,
                                 {.base = kMachineRegRBP, .disp = offsetof(Data, x)});
  builder.Gen<x86_64::AddqRegOp, x86_64::kNoSSA>(
      kMachineRegRAX,
      {.base = kMachineRegRBP, .disp = offsetof(Data, y)},
      x86_64::kMachineRegFLAGS);
  builder.Gen<x86_64::ReadFlagsWithOverflow>(kMachineRegRAX, x86_64::kMachineRegFLAGS);
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&res_flags));
  builder.Gen<x86_64::MovqOpReg>({.base = kMachineRegRBP}, kMachineRegRAX);

  ExecTest test;
  test.Init(machine_ir);

  data.x = 1;
  data.y = 1;
  test.Exec();
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b0000));

  data.x = ~0ULL;
  data.y = 1;
  test.Exec();
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b0110));

  data.x = (~0ULL) >> 1;
  data.y = 1;
  test.Exec();
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b1001));
}

TEST(ExecMachineIR, ReadFlagsWithoutOverflow) {
  struct Data {
    uint64_t x;
    uint64_t y;
  } data{};
  uint64_t res_flags;

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  // Let RBP point to 'data'.
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&data));
  builder.Gen<x86_64::MovqRegOp>(kMachineRegRAX,
                                 {.base = kMachineRegRBP, .disp = offsetof(Data, x)});
  builder.Gen<x86_64::AddqRegOp, x86_64::kNoSSA>(
      kMachineRegRAX,
      {.base = kMachineRegRBP, .disp = offsetof(Data, y)},
      x86_64::kMachineRegFLAGS);
  // ReadFlags must reset overflow to zero, even if it's set in RAX.
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRAX, MakeFlags(0b0001));
  builder.Gen<x86_64::ReadFlagsWithoutOverflow>(kMachineRegRAX, x86_64::kMachineRegFLAGS);
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&res_flags));
  builder.Gen<x86_64::MovqOpReg>({.base = kMachineRegRBP}, kMachineRegRAX);

  ExecTest test;
  test.Init(machine_ir);

  data.x = (~0ULL) >> 1;
  data.y = 1;
  test.Exec();
  // Overflow happens but is not returned.
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b1000));
}

TEST(ExecMachineIR, WriteFlags) {
  uint64_t arg_flags;
  uint64_t res_flags;

  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);
  builder.StartBasicBlock(machine_ir.NewBasicBlock());

  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&arg_flags));
  builder.Gen<x86_64::MovqRegOp>(kMachineRegRAX, {.base = kMachineRegRBP});
  builder.Gen<x86_64::WriteFlags, x86_64::kNoSSA>(kMachineRegRAX, x86_64::kMachineRegFLAGS);
  // Assume PseudoReadFlags is verified by another test.
  builder.Gen<x86_64::ReadFlagsWithOverflow>(kMachineRegRAX, x86_64::kMachineRegFLAGS);
  builder.Gen<x86_64::MovqRegImm>(kMachineRegRBP, bit_cast<uintptr_t>(&res_flags));
  builder.Gen<x86_64::MovqOpReg>({.base = kMachineRegRBP}, kMachineRegRAX);

  ExecTest test;
  test.Init(machine_ir);

  arg_flags = MakeFlags(0b1111);
  res_flags = 0;
  test.Exec();
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b1111));

  arg_flags = MakeFlags(0b1010);
  res_flags = 0;
  test.Exec();
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b1010));

  arg_flags = MakeFlags(0b0101);
  res_flags = 0;
  test.Exec();
  EXPECT_EQ(res_flags & MakeFlags(0b1111), MakeFlags(0b0101));
}

}  // namespace

}  // namespace berberis
