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

#include <tuple>

#include "berberis/backend/x86_64/insn_folding.h"

#include "berberis/backend/code_emitter.h"  // for CodeEmitter::Condition
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/device_arch_info/x86_64/device_arch_info.h"
#include "berberis/guest_state/guest_addr.h"

#include "x86_64/read_flags_variants_test_helper.h"

namespace berberis::x86_64 {

namespace {

constexpr auto kMachineRegRAX = MachineRegs::kRAX;
constexpr auto kMachineRegRDI = MachineRegs::kRDI;

template <template <typename> typename InsnType>
using MachineInsnType =
    MachineInsn<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo,
                kForceNoSSA<typename InsnType<typename CodeEmitter::Assemblers>::DeviceInsnInfo>>;

MachineInsnList::iterator FoldInsnsAndGetLastInsnIt(MachineIR* machine_ir, MachineBasicBlock* bb) {
  FoldInsns(machine_ir);
  return std::prev(bb->insn_list().end());
}

// By default for the successful folding the immediate must be sign-extended from 32-bit to the same
// 64-bit integer number.
template <template <typename> typename InsnTypeRegReg,
          template <typename> typename InsnTypeRegImm,
          bool kExpectSuccess = true>
void TryRegRegInsnFolding(bool is_64bit_mov_imm, uint64_t imm = 0x7777'ffffULL) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  if (is_64bit_mov_imm) {
    builder.Gen<MovqRegImm>(vreg1, imm);
  } else {
    builder.Gen<MovlRegImm>(vreg1, imm);
  }
  builder.Gen<MachineInsnType<InsnTypeRegReg>>(std::tuple{vreg2, vreg1, flags});

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  if (!kExpectSuccess) {
    EXPECT_EQ(MachineInsnType<InsnTypeRegReg>::kInfo.opcode, folded_insn->opcode());
    return;
  }
  EXPECT_EQ(MachineInsnType<InsnTypeRegImm>::kInfo.opcode, folded_insn->opcode());
  EXPECT_EQ(vreg2, folded_insn->RegAt(0));
  EXPECT_EQ(static_cast<uint64_t>(static_cast<int32_t>(imm)),
            AsMachineInsnX86_64(folded_insn)->imm());
  if (MachineInsnType<InsnTypeRegImm>::kInfo.opcode == kMachineOpImulqRegRegImm ||
      MachineInsnType<InsnTypeRegImm>::kInfo.opcode == kMachineOpImullRegRegImm) {
    EXPECT_EQ(vreg2, folded_insn->RegAt(1));
    EXPECT_EQ(flags, folded_insn->RegAt(2));
  } else {
    EXPECT_EQ(flags, folded_insn->RegAt(1));
  }
}

template <template <typename> typename InsnTypeRegReg, template <typename> typename InsnTypeRegImm>
void TryRegRegInsnFoldingExtraCopy(bool is_64bit_mov_imm, uint64_t imm = 0x7777'ffffULL) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  if (is_64bit_mov_imm) {
    builder.Gen<MovqRegImm>(vreg1, imm);
  } else {
    builder.Gen<MovlRegImm>(vreg1, imm);
  }
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<MachineInsnType<InsnTypeRegReg>>(std::tuple{vreg3, vreg2, flags});

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;
  EXPECT_EQ(MachineInsnType<InsnTypeRegImm>::kInfo.opcode, folded_insn->opcode());
  EXPECT_EQ(vreg3, folded_insn->RegAt(0));
  EXPECT_EQ(static_cast<uint64_t>(static_cast<int32_t>(imm)),
            AsMachineInsnX86_64(folded_insn)->imm());
  if (MachineInsnType<InsnTypeRegImm>::kInfo.opcode == kMachineOpImulqRegRegImm ||
      MachineInsnType<InsnTypeRegImm>::kInfo.opcode == kMachineOpImullRegRegImm) {
    EXPECT_EQ(vreg3, folded_insn->RegAt(1));
    EXPECT_EQ(flags, folded_insn->RegAt(2));
  } else {
    EXPECT_EQ(flags, folded_insn->RegAt(1));
  }

  auto prev_insn_it = std::prev(folded_insn_it);
  berberis::MachineInsn* prev_insn = *prev_insn_it;
  EXPECT_EQ(prev_insn->opcode(), kMachineOpCopy);
}

template <template <typename> typename InsnTypeRegReg, template <typename> typename InsnTypeRegImm>
void TryMovInsnFolding(bool is_64bit_mov_imm, uint64_t imm) {
  Arena arena;
  MachineIR machine_ir(&arena);
  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  if (is_64bit_mov_imm) {
    builder.Gen<MovqRegImm>(vreg1, imm);
  } else {
    builder.Gen<MovlRegImm>(vreg1, imm);
  }
  builder.Gen<InsnTypeRegReg>(vreg2, vreg1);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(MachineInsnType<InsnTypeRegImm>::kInfo.opcode, folded_insn->opcode());
  EXPECT_EQ(vreg2, folded_insn->RegAt(0));
  // MovqRegReg is the only instruction that can take full 64-bit imm.
  if (MachineInsnType<InsnTypeRegReg>::kInfo.opcode == MachineInsnType<MovqRegReg>::kInfo.opcode) {
    // Take into account zero-extension when MOVL.
    EXPECT_EQ(is_64bit_mov_imm ? imm : static_cast<uint32_t>(imm),
              AsMachineInsnX86_64(folded_insn)->imm());
  } else {
    EXPECT_EQ(static_cast<uint64_t>(static_cast<int32_t>(imm)),
              AsMachineInsnX86_64(folded_insn)->imm());
  }
}

template <template <typename> typename InsnTypeRegImm, bool kInsnIs64Bit>
void TryTwoImmediatesRegImmInsnFolding(uint64_t imm1, int32_t imm2, uint64_t expected_op_result) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  if (kInsnIs64Bit) {
    builder.Gen<MovqRegImm>(vreg1, imm1);
  } else {
    builder.Gen<MovlRegImm>(vreg1, static_cast<uint32_t>(imm1));
  }
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<InsnTypeRegImm, kNoSSA>(vreg2, imm2, flags);

  MachineInsnList::iterator insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* insn = *insn_it;
  if (kInsnIs64Bit) {
    EXPECT_EQ(insn->opcode(), kMachineOpMovqRegImm);
    EXPECT_EQ(AsMachineInsnX86_64(insn)->imm(), expected_op_result);
  } else {
    EXPECT_EQ(insn->opcode(), kMachineOpMovlRegImm);
    EXPECT_EQ(static_cast<uint32_t>(AsMachineInsnX86_64(insn)->imm()),
              static_cast<uint32_t>(expected_op_result));
  }
  auto prev_insn_it = std::prev(insn_it);
  berberis::MachineInsn* prev_insn = *prev_insn_it;
  EXPECT_EQ(prev_insn->opcode(), MachineInsnType<InsnTypeRegImm>::kInfo.opcode);
}

template <template <typename> typename InsnTypeRegReg, bool kInsnIs64Bit>
void TryTwoImmediatesRegRegInsnFolding(uint64_t imm1, uint64_t imm2, uint64_t expected_op_result) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  if (kInsnIs64Bit) {
    builder.Gen<MovqRegImm>(vreg1, imm1);
    builder.Gen<MovqRegImm>(vreg2, imm2);
  } else {
    builder.Gen<MovlRegImm>(vreg1, static_cast<uint32_t>(imm1));
    builder.Gen<MovlRegImm>(vreg2, static_cast<uint32_t>(imm2));
  }
  builder.Gen<InsnTypeRegReg, kNoSSA>(vreg1, vreg2, flags);

  MachineInsnList::iterator insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* insn = *insn_it;
  if (kInsnIs64Bit) {
    EXPECT_EQ(insn->opcode(), kMachineOpMovqRegImm);
    EXPECT_EQ(AsMachineInsnX86_64(insn)->imm(), expected_op_result);
  } else {
    EXPECT_EQ(insn->opcode(), kMachineOpMovlRegImm);
    EXPECT_EQ(static_cast<uint32_t>(AsMachineInsnX86_64(insn)->imm()),
              static_cast<uint32_t>(expected_op_result));
  }
  auto prev_insn_it = std::prev(insn_it);
  berberis::MachineInsn* prev_insn = *prev_insn_it;
  EXPECT_EQ(prev_insn->opcode(), MachineInsnType<InsnTypeRegReg>::kInfo.opcode);
}

template <template <typename> typename InsnTypeRegReg,
          berberis::MachineOpcode MachineOpInsnTypeRegMemBaseDisp,
          bool kArithmeticInsnUsesXMMRegs>
void TryFoldContextReadIntoRegMemArithmetic() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  if constexpr (kArithmeticInsnUsesXMMRegs) {
    builder.Gen<InsnTypeRegReg, kNoSSA>(vreg2, vreg1);
  } else {
    builder.Gen<MachineInsnType<InsnTypeRegReg>>(std::tuple{vreg2, vreg1, flags});
  }

  auto* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  ASSERT_EQ(MachineOpInsnTypeRegMemBaseDisp, folded_insn->opcode());
  if constexpr (MachineOpInsnTypeRegMemBaseDisp == kMachineOpTestqMemBaseDispReg ||
                MachineOpInsnTypeRegMemBaseDisp == kMachineOpTestlMemBaseDispReg) {
    // Since the Test insn has a TestMemReg version but no TestRegMem version, the order of
    // operands are swapped in the folded instruction.
    EXPECT_EQ(kCPUStatePointer, folded_insn->RegAt(0));
    EXPECT_EQ(vreg2, folded_insn->RegAt(1));
  } else {
    EXPECT_EQ(vreg2, folded_insn->RegAt(0));
    EXPECT_EQ(kCPUStatePointer, folded_insn->RegAt(1));
  }
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  if constexpr (!kArithmeticInsnUsesXMMRegs) {
    EXPECT_EQ(flags, folded_insn->RegAt(2));
  }
}

template <template <typename> typename InsnTypeRegReg,
          berberis::MachineOpcode MachineOpInsnTypeRegMemBaseDisp>
void TryFoldContextReadIntoMemRegArithmetic() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});

  builder.Gen<MachineInsnType<InsnTypeRegReg>>(std::tuple{vreg1, vreg2, flags});

  auto* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  ASSERT_EQ(MachineOpInsnTypeRegMemBaseDisp, folded_insn->opcode());
  EXPECT_EQ(kCPUStatePointer, folded_insn->RegAt(0));
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
  EXPECT_EQ(flags, folded_insn->RegAt(2));
}

template <template <typename> typename InsnTypeRegImm,
          berberis::MachineOpcode MachineOpInsnTypeMemBaseDispImm>
void TryFoldContextReadIntoMemImmArithmetic() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});

  builder.Gen<MachineInsnType<InsnTypeRegImm>>(std::tuple{vreg1, 5, flags});

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  ASSERT_EQ(MachineOpInsnTypeMemBaseDispImm, folded_insn->opcode());
  EXPECT_EQ(kCPUStatePointer, folded_insn->RegAt(0));
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(static_cast<uint32_t>(AsMachineInsnX86_64(folded_insn)->imm()),
            static_cast<uint32_t>(5));
  EXPECT_EQ(flags, folded_insn->RegAt(1));
}

template <template <typename> typename InsnTypeRegImm,
          berberis::MachineOpcode MachineOpInsnTypeMemBaseDispImm>
void TryFoldContextReadAndImmediateMemImmArithmetic() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<MovqRegImm>(vreg2, 5);
  builder.Gen<InsnTypeRegImm>(vreg1, vreg2, flags);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(MachineOpInsnTypeMemBaseDispImm, folded_insn->opcode());
  EXPECT_EQ(kCPUStatePointer, folded_insn->RegAt(0));
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(static_cast<uint32_t>(AsMachineInsnX86_64(folded_insn)->imm()),
            static_cast<uint32_t>(5));
  EXPECT_EQ(flags, folded_insn->RegAt(1));
}

template <template <typename> typename InsnTypeRegReg,
          berberis::MachineOpcode MachineOpInsnTypeMemBaseDispImm>
void TrySwapRegOperandsAndFoldContextReadArithmetic() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<InsnTypeRegReg, kNoSSA>(vreg1, vreg2, flags);

  auto final_folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* second_folded_copy = *final_folded_insn_it;
  EXPECT_EQ(kMachineOpCopy, second_folded_copy->opcode());
  EXPECT_EQ(vreg1, second_folded_copy->RegAt(0));
  auto new_vreg = second_folded_copy->RegAt(1);
  EXPECT_FALSE(Contains(std::list{vreg1, vreg2, flags}, new_vreg));

  berberis::MachineInsn* folded_op_insn = *std::prev(final_folded_insn_it);
  EXPECT_EQ(MachineOpInsnTypeMemBaseDispImm, folded_op_insn->opcode());
  EXPECT_EQ(new_vreg, folded_op_insn->RegAt(0));
  EXPECT_EQ(kCPUStatePointer, folded_op_insn->RegAt(1));
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_op_insn)->disp());
  EXPECT_EQ(flags, folded_op_insn->RegAt(2));

  berberis::MachineInsn* first_folded_copy = *std::prev(final_folded_insn_it, 2);
  EXPECT_EQ(kMachineOpCopy, first_folded_copy->opcode());
  EXPECT_EQ(new_vreg, first_folded_copy->RegAt(0));
  EXPECT_EQ(vreg2, first_folded_copy->RegAt(1));
}

template <template <typename> typename InsnTypeRegReg,
          berberis::MachineOpcode ReplacementCmpInsnOpcode>
void TryReplaceWriteFlagsWithCmpRegRegInsn() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg_rax = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegReg>(vreg1, vreg3);
  builder.Gen<MovqRegReg>(vreg2, vreg4);
  builder.Gen<InsnTypeRegReg>(vreg1, vreg2, kMachineRegFLAGS);
  builder.Gen<ReadFlagsWithOverflow>(vreg_rax, kMachineRegFLAGS);
  builder.Gen<AddqRegReg, kNoSSA>(vreg3, vreg4, kMachineRegFLAGS);
  builder.Gen<WriteFlags, kNoSSA>(vreg_rax, kMachineRegFLAGS);

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;

  EXPECT_EQ(ReplacementCmpInsnOpcode, folded_insn->opcode());
  EXPECT_EQ(vreg1, folded_insn->RegAt(0));
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
  EXPECT_EQ(kMachineRegFLAGS, folded_insn->RegAt(2));
  EXPECT_EQ(bb->insn_list().size(), 6UL);
}

template <template <typename> typename InsnTypeRegImm,
          berberis::MachineOpcode ReplacementCmpInsnOpcode>
void TryReplaceWriteFlagsWithCmpRegImmInsn() {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg_rax = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegReg>(vreg1, vreg2);
  builder.Gen<InsnTypeRegImm>(vreg1, 5, kMachineRegFLAGS);
  builder.Gen<ReadFlagsWithOverflow>(vreg_rax, kMachineRegFLAGS);
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg3, kMachineRegFLAGS);
  builder.Gen<WriteFlags, kNoSSA>(vreg_rax, kMachineRegFLAGS);

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;

  EXPECT_EQ(ReplacementCmpInsnOpcode, folded_insn->opcode());
  EXPECT_EQ(vreg1, folded_insn->RegAt(0));
  EXPECT_EQ(5UL, AsMachineInsnX86_64(folded_insn)->imm());
  EXPECT_EQ(kMachineRegFLAGS, folded_insn->RegAt(1));
  EXPECT_EQ(bb->insn_list().size(), 5UL);
}

TEST(InsnFoldingTest, DefMapGetsLatestDef) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 0);
  builder.Gen<MovqRegImm>(vreg2, 0);
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg1, flags);
  builder.Gen<Jump>(kNullGuestAddr);

  bb->live_out().push_back(vreg1);
  bb->live_out().push_back(vreg2);

  DefMap def_map(&machine_ir);
  for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end(); ++insn_it) {
    def_map.ProcessInsn(insn_it);
  }

  auto [vreg1_def_it, index1, _1] = def_map.Get(vreg1);
  ASSERT_TRUE(vreg1_def_it.has_value());
  const berberis::MachineInsn* vreg1_def = *vreg1_def_it.value();
  EXPECT_EQ(kMachineOpMovqRegImm, vreg1_def->opcode());
  EXPECT_EQ(vreg1, vreg1_def->RegAt(0));
  EXPECT_EQ(index1, 0);

  auto [vreg2_def_it, index2, _2] = def_map.Get(vreg2);
  ASSERT_TRUE(vreg2_def_it.has_value());
  const berberis::MachineInsn* vreg2_def = *vreg2_def_it.value();
  EXPECT_EQ(kMachineOpAddqRegReg, vreg2_def->opcode());
  EXPECT_EQ(vreg2, vreg2_def->RegAt(0));
  EXPECT_EQ(index2, 2);
}

TEST(InsnFoldingTest, DefMapReturnsNoDefIfVRegIsOverwrittenByInsn) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 0);
  builder.Gen<MovqRegImm>(vreg2, 0);
  builder.Gen<AddqRegReg, kNoSSA>(vreg1, vreg2, flags);
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg1, flags);

  DefMap def_map(&machine_ir);
  for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end(); ++insn_it) {
    def_map.ProcessInsn(insn_it);
  }

  auto [vreg1_def_insn_it, vreg_def_insn_pos, _] = def_map.Get(vreg1);
  ASSERT_TRUE(vreg1_def_insn_it.has_value());
  EXPECT_EQ(kMachineOpAddqRegReg, (*vreg1_def_insn_it.value())->opcode());

  // Checking def_map for vreg1 at the position of an instruction that overwrites it.
  auto vreg1_overwritten_def_it = std::get<0>(def_map.Get(vreg1, vreg_def_insn_pos));
  EXPECT_FALSE(vreg1_overwritten_def_it.has_value());
}

TEST(InsnFoldingTest, DefMapReturnsCorrectRegisterPosition) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg, kNoSSA>(vreg1, vreg2, flags);

  DefMap def_map(&machine_ir);
  for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end(); ++insn_it) {
    def_map.ProcessInsn(insn_it);
  }

  EXPECT_EQ(std::get<2>(def_map.Get(vreg1)), 0);
  EXPECT_EQ(std::get<0>(def_map.Get(vreg2)), std::nullopt);
  EXPECT_EQ(std::get<2>(def_map.Get(flags)), 2);
}

TEST(InsnFoldingTest, DefMapHandlesNewlyAllocatedVreg) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();
  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  DefMap def_map(&machine_ir);

  MachineReg vreg2 = machine_ir.AllocVReg();
  builder.Gen<SubqRegReg, kNoSSA>(vreg2, vreg1, flags);
  auto failed_get_def_it = std::get<0>(def_map.SafeGetForTesting(vreg2));
  ASSERT_EQ(std::nullopt, failed_get_def_it);

  def_map.SetForTesting(vreg2, std::prev(bb->insn_list().end()), 0);

  auto [vreg2_def_it, vreg2_def_insn_pos, vreg2_def_reg_pos] = def_map.Get(vreg2);
  ASSERT_TRUE(vreg2_def_it.has_value());
  berberis::MachineInsn* vreg2_def_insn = *vreg2_def_it.value();
  EXPECT_EQ(kMachineOpSubqRegReg, vreg2_def_insn->opcode());
  EXPECT_EQ(0, vreg2_def_insn_pos);
  EXPECT_EQ(0, vreg2_def_reg_pos);
}

TEST(InsnFoldingTest, MovFolding) {
  constexpr uint64_t kSignExtendableImm = 0xffff'ffff'8000'0000ULL;
  constexpr uint64_t kNotSignExtendableImm = 0xffff'ffff'0000'0000ULL;
  for (bool is_64bit_mov_imm : {true, false}) {
    // MovqRegReg is the only instruction that allow 64-bit immediates.
    TryMovInsnFolding<MovqRegReg, MovqRegImm>(is_64bit_mov_imm, kSignExtendableImm);
    TryMovInsnFolding<MovqRegReg, MovqRegImm>(is_64bit_mov_imm, kNotSignExtendableImm);
    // Movl isn't sensetive to upper immediate bits.
    TryMovInsnFolding<MovlRegReg, MovlRegImm>(is_64bit_mov_imm, kSignExtendableImm);
    TryMovInsnFolding<MovlRegReg, MovlRegImm>(is_64bit_mov_imm, kNotSignExtendableImm);
  }
}

TEST(InsnFoldingTest, SingleMovqMemBaseDispImm32Folding) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  auto* recovery_bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovlRegImm>(vreg1, 2);
  builder.Gen<MovqOpReg>({.base = kMachineRegRAX, .disp = 4}, vreg1);
  builder.SetRecoveryPointAtLastInsn(recovery_bb);
  builder.SetRecoveryWithGuestPCAtLastInsn(42);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(kMachineOpMovqMemBaseDispImm, folded_insn->opcode());
  EXPECT_EQ(kMachineRegRAX, folded_insn->RegAt(0));
  EXPECT_EQ(2UL, AsMachineInsnX86_64(folded_insn)->imm());
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(folded_insn->recovery_pc(), 42UL);
  EXPECT_EQ(folded_insn->recovery_bb(), recovery_bb);
}

TEST(InsnFoldingTest, SingleMovlMemBaseDispImm32Folding) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  auto* recovery_bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 0x3'0000'0003);
  builder.Gen<MovlOpReg>({.base = kMachineRegRAX, .disp = 4}, vreg1);
  builder.SetRecoveryPointAtLastInsn(recovery_bb);
  builder.SetRecoveryWithGuestPCAtLastInsn(42);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(kMachineOpMovlMemBaseDispImm, folded_insn->opcode());
  EXPECT_EQ(kMachineRegRAX, folded_insn->RegAt(0));
  EXPECT_EQ(3UL, AsMachineInsnX86_64(folded_insn)->imm());
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(folded_insn->recovery_pc(), 42UL);
  EXPECT_EQ(folded_insn->recovery_bb(), recovery_bb);
}

TEST(InsnFoldingTest, RedundantMovlFolding) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<AddlRegReg, kNoSSA>(vreg2, vreg3, flags);
  builder.Gen<MovlRegReg>(vreg1, vreg2);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(kMachineOpCopy, folded_insn->opcode());
  EXPECT_EQ(vreg1, folded_insn->RegAt(0));
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
}

TEST(InsnFoldingTest, RedundantMovlFoldingExtraCopy) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<XorlRegReg, kNoSSA>(vreg3, vreg4, flags);
  builder.Gen<Copy>(vreg2, vreg3, 8);
  builder.Gen<MovlRegReg>(vreg1, vreg2);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(kMachineOpCopy, folded_insn->opcode());
  EXPECT_EQ(vreg1, folded_insn->RegAt(0));
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
}

TEST(InsnFoldingTest, RedundantMovlFoldingCancelledIfNonZeroExtendingInsn) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<Copy>(vreg2, vreg3, 4);
  builder.Gen<MovlRegReg>(vreg1, vreg2);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(kMachineOpMovlRegReg, folded_insn->opcode());
  EXPECT_EQ(vreg1, folded_insn->RegAt(0));
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
}

TEST(InsnFoldingTest, GracefulHandlingOfVRegDefinedInPreviousBasicBlock) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();

  bb->live_in().push_back(vreg2);

  builder.StartBasicBlock(bb);
  builder.Gen<MovlRegReg>(vreg1, vreg2);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(folded_insn->opcode(), kMachineOpMovlRegReg);
  EXPECT_EQ(vreg1, folded_insn->RegAt(0));
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
}

TEST(InsnFoldingTest, RegRegInsnTypeFolding) {
  for (bool is_64bit_mov_imm : {true, false}) {
    TryRegRegInsnFolding<AddqRegReg, AddqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<SubqRegReg, SubqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<CmpqRegReg, CmpqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<OrqRegReg, OrqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<XorqRegReg, XorqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<AndqRegReg, AndqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<TestqRegReg, TestqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<ShlqRegReg, ShlqRegImm>(is_64bit_mov_imm, 10);
    TryRegRegInsnFolding<ShrqRegReg, ShrqRegImm>(is_64bit_mov_imm, 11);
    TryRegRegInsnFolding<ImulqRegReg, ImulqRegRegImm>(is_64bit_mov_imm);

    TryRegRegInsnFolding<AddlRegReg, AddlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<SublRegReg, SublRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<CmplRegReg, CmplRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<OrlRegReg, OrlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<XorlRegReg, XorlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<AndlRegReg, AndlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<TestlRegReg, TestlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFolding<ShllRegReg, ShllRegImm>(is_64bit_mov_imm, 10);
    TryRegRegInsnFolding<ShrlRegReg, ShrlRegImm>(is_64bit_mov_imm, 11);
    TryRegRegInsnFolding<ImullRegReg, ImullRegRegImm>(is_64bit_mov_imm);

    TryRegRegInsnFoldingExtraCopy<AddqRegReg, AddqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<SubqRegReg, SubqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<CmpqRegReg, CmpqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<OrqRegReg, OrqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<XorqRegReg, XorqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<AndqRegReg, AndqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<TestqRegReg, TestqRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<ImulqRegReg, ImulqRegRegImm>(is_64bit_mov_imm);

    TryRegRegInsnFoldingExtraCopy<AddlRegReg, AddlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<SublRegReg, SublRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<CmplRegReg, CmplRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<OrlRegReg, OrlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<XorlRegReg, XorlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<AndlRegReg, AndlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<TestlRegReg, TestlRegImm>(is_64bit_mov_imm);
    TryRegRegInsnFoldingExtraCopy<ImullRegReg, ImullRegRegImm>(is_64bit_mov_imm);
  }
}

TEST(InsnFoldingTest, 32To64SignExtendableImm) {
  // The signed immediate is 32->64 sign-extend to the same integer value.
  constexpr uint64_t kImm = 0xffff'ffff'8000'0000ULL;
  // Can fold into 64-bit instruction.
  TryRegRegInsnFolding<AddqRegReg,
                       AddqRegImm,
                       /* kExpectSuccess */ true>(/* is_64bit_mov_imm */ true, kImm);
  // But cannot fold if the upper bits are cleared out by MOVL, since it's not sign-extable anymore.
  TryRegRegInsnFolding<AddqRegReg,
                       AddqRegImm,
                       /* kExpectSuccess */ false>(/* is_64bit_mov_imm */ false, kImm);

  for (bool is_64bit_mov_imm : {true, false}) {
    // Can fold into 32-bit instruction since the upper bits are not used.
    TryRegRegInsnFolding<AddlRegReg,
                         AddlRegImm,
                         /* kExpectSuccess */ true>(is_64bit_mov_imm, kImm);
  }
}

TEST(InsnFoldingTest, Not32To64SignExtendableImm) {
  // The immediate doesn't 32->64 sign-extend to the same integer value.
  constexpr uint64_t kImm = 0xffff'ffff'0000'0000ULL;
  // Cannot fold into 64-bit instruction.
  TryRegRegInsnFolding<AddqRegReg,
                       AddqRegImm,
                       /* kExpectSuccess */ false>(/* is_64bit_mov_imm */ true, kImm);
  // But can fold if the upper bits are cleared out by MOVL.
  TryRegRegInsnFolding<AddqRegReg,
                       AddqRegImm,
                       /* kExpectSuccess */ true>(/* is_64bit_mov_imm */ false, kImm);

  for (bool is_64bit_mov_imm : {true, false}) {
    // Can fold into 32-bit instruction since the upper bits are not used.
    TryRegRegInsnFolding<AddlRegReg,
                         AddlRegImm,
                         /* kExpectSuccess */ true>(is_64bit_mov_imm, kImm);
  }
}

TEST(InsnFoldingTest, HardRegsAreSafe) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg, kNoSSA>(kMachineRegRAX, kMachineRegRDI, kMachineRegFLAGS);
  builder.Gen<Jump>(kNullGuestAddr);

  FoldInsns(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 2UL);
}

TEST_P(ReadFlagsVariantsTest, WriteFlagsErased) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg flag = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<AddqRegReg, kNoSSA>(vreg4, vreg5, flag);
  GenReadFlags(builder, vreg2, flag);
  builder.Gen<Copy>(vreg3, vreg2, 8);
  builder.Gen<WriteFlags, kNoSSA>(vreg3, flag);
  builder.Gen<Jump>(kNullGuestAddr);

  FoldInsns(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 4UL);

  auto insn_it = bb->insn_list().rbegin();
  ++insn_it;
  const berberis::MachineInsn* insn = *insn_it;

  EXPECT_EQ(kMachineOpCopy, insn->opcode());
}

TEST_P(ReadFlagsVariantsTest, FlagModifiedAfterReadFlags) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg flag = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  GenReadFlags(builder, vreg2, flag);
  builder.Gen<Copy>(vreg3, vreg2, 8);
  builder.Gen<AddqRegReg, kNoSSA>(vreg4, vreg5, flag);
  builder.Gen<WriteFlags, kNoSSA>(vreg3, flag);
  builder.Gen<Jump>(kNullGuestAddr);

  FoldInsns(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 5UL);
}

TEST_P(ReadFlagsVariantsTest, WriteFlagsNotDeletedBecauseDefinitionIsAfterUse) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg flag = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  GenReadFlags(builder, vreg2, flag);
  builder.Gen<Copy>(vreg3, vreg2, 8);
  builder.Gen<MovqRegImm>(vreg2, 3);
  builder.Gen<WriteFlags, kNoSSA>(vreg3, flag);
  builder.Gen<Jump>(kNullGuestAddr);

  FoldInsns(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 5UL);
}

TEST(InsnFoldingTest, FoldInsnsSmoke) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();

  MachineIRBuilder builder(&machine_ir);

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 2);
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg1, flags);
  builder.Gen<Jump>(kNullGuestAddr);

  bb->live_out().push_back(vreg2);
  bb->live_in().push_back(vreg2);

  FoldInsns(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 3UL);

  auto insn_it = bb->insn_list().begin();
  ++insn_it;
  berberis::MachineInsn* insn = *insn_it;

  EXPECT_EQ(insn->opcode(), kMachineOpAddqRegImm);
  EXPECT_EQ(vreg2, insn->RegAt(0));
  EXPECT_EQ(2UL, AsMachineInsnX86_64(insn)->imm());
}

using Cond = CodeEmitter::Condition;

void TestFoldCond(Cond input_cond, Cond expected_new_cond, LahfFlags expected_flags_mask) {
  Arena arena;
  MachineIR machine_ir(&arena);

  auto* bb = machine_ir.NewBasicBlock();
  MachineIRBuilder builder(&machine_ir);

  builder.StartBasicBlock(bb);
  builder.Gen<WriteFlags, kNoSSA>(kMachineRegRAX, kMachineRegFLAGS);
  builder.Gen<CondBranch>(input_cond, nullptr, nullptr, kMachineRegFLAGS);

  MachineReg flags_src = (*bb->insn_list().begin())->RegAt(0);

  FoldInsns(&machine_ir);

  EXPECT_EQ(bb->insn_list().size(), 2UL);

  auto insn_it = bb->insn_list().begin();
  const auto* insn = AsMachineInsnX86_64(*insn_it);
  EXPECT_EQ(insn->opcode(), kMachineOpTestwRegImm);
  EXPECT_EQ(flags_src, insn->RegAt(0));
  EXPECT_EQ(expected_flags_mask, static_cast<LahfFlags>(insn->imm()));
  MachineReg flags = insn->RegAt(1);

  const auto* branch = static_cast<const CondBranch*>(*(++insn_it));
  EXPECT_EQ(branch->opcode(), kMachineOpCondBranch);
  EXPECT_EQ(flags, branch->RegAt(0));
  EXPECT_EQ(expected_new_cond, branch->cond());
}

template <template <typename> typename MemoryAccessInsn,
          berberis::MachineOpcode MemoryAccessInsnOpcode>
void TryFoldScaleIntoMemWrite(int shift_amount, Assembler::ScaleFactor expected_scale_factor) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  auto* recovery_bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<Copy>(vreg1, vreg4, 8);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ShlqRegImm, kNoSSA>(vreg2, shift_amount, flags);
  builder.Gen<MemoryAccessInsn>({.base = vreg3, .index = vreg2, .disp = 12}, vreg4);
  builder.SetRecoveryPointAtLastInsn(recovery_bb);
  builder.SetRecoveryWithGuestPCAtLastInsn(42);

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;

  EXPECT_EQ(MemoryAccessInsnOpcode, folded_insn->opcode());
  EXPECT_EQ(vreg3, folded_insn->RegAt(0));
  EXPECT_EQ(vreg1, folded_insn->RegAt(1));
  EXPECT_EQ(vreg4, folded_insn->RegAt(2));
  EXPECT_EQ(12UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(expected_scale_factor, AsMachineInsnX86_64(folded_insn)->scale());
  EXPECT_EQ(recovery_bb, folded_insn->recovery_bb());
  EXPECT_EQ(42UL, folded_insn->recovery_pc());
}

template <template <typename> typename MemoryAccessInsn,
          berberis::MachineOpcode MemoryAccessInsnOpcode>
void TryFoldScaleIntoMemRead(int shift_amount, Assembler::ScaleFactor expected_scale_factor) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  auto* recovery_bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<Copy>(vreg1, vreg4, 8);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ShlqRegImm, kNoSSA>(vreg2, shift_amount, flags);
  builder.Gen<MemoryAccessInsn>(vreg4, {.base = vreg3, .index = vreg2, .disp = 12});
  builder.SetRecoveryPointAtLastInsn(recovery_bb);
  builder.SetRecoveryWithGuestPCAtLastInsn(42);

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;

  EXPECT_EQ(MemoryAccessInsnOpcode, folded_insn->opcode());
  EXPECT_EQ(vreg4, folded_insn->RegAt(0));
  EXPECT_EQ(vreg3, folded_insn->RegAt(1));
  EXPECT_EQ(vreg1, folded_insn->RegAt(2));
  EXPECT_EQ(12UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(expected_scale_factor, AsMachineInsnX86_64(folded_insn)->scale());
  EXPECT_EQ(recovery_bb, folded_insn->recovery_bb());
  EXPECT_EQ(42UL, folded_insn->recovery_pc());
}

TEST(InsnFoldingTest, ReplaceWriteFlagsWithTest) {
  TestFoldCond(Cond::kEqual, Cond::kNotEqual, LahfFlags::kZero);
  TestFoldCond(Cond::kNotEqual, Cond::kEqual, LahfFlags::kZero);
  TestFoldCond(Cond::kCarry, Cond::kNotEqual, LahfFlags::kCarry);
  TestFoldCond(Cond::kNotCarry, Cond::kEqual, LahfFlags::kCarry);
  TestFoldCond(Cond::kNegative, Cond::kNotEqual, LahfFlags::kNegative);
  TestFoldCond(Cond::kNotSign, Cond::kEqual, LahfFlags::kNegative);
  TestFoldCond(Cond::kOverflow, Cond::kNotEqual, LahfFlags::kOverflow);
  TestFoldCond(Cond::kNoOverflow, Cond::kEqual, LahfFlags::kOverflow);
}

TEST(InsnFoldingTest, CountTrailingZeroesFolding64) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();
  MachineReg vreg6 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 3);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ReverseBitsU64, kNoSSA>(vreg3, vreg2, vreg4, flags);
  builder.Gen<Copy>(vreg5, vreg3, 8);
  builder.Gen<CountLeadingZerosU64>(vreg6, vreg5, flags);

  berberis::MachineInsn* insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(insn->opcode(), kMachineOpCountTrailingZerosU64);
  EXPECT_EQ(insn->RegAt(0), vreg6);
  EXPECT_EQ(insn->RegAt(1), vreg1);
}

TEST(InsnFoldingTest, CountTrailingZeroesFolding64MBI) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();
  MachineReg vreg6 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 3);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ReverseBitsU64, kNoSSA>(vreg3, vreg2, vreg4, flags);
  builder.Gen<Copy>(vreg5, vreg3, 8);
  builder.Gen<LzcntqRegReg>(vreg6, vreg5, flags);

  berberis::MachineInsn* insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(insn->opcode(), kMachineOpTzcntqRegReg);
  EXPECT_EQ(insn->RegAt(0), vreg6);
  EXPECT_EQ(insn->RegAt(1), vreg1);
}

TEST(InsnFoldingTest, CountTrailingZeroesFolding32) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 3);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ReverseBitsU32, kNoSSA>(vreg3, vreg2, flags);
  builder.Gen<Copy>(vreg4, vreg3, 8);
  builder.Gen<CountLeadingZerosU32>(vreg5, vreg4, flags);

  berberis::MachineInsn* insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(insn->opcode(), kMachineOpCountTrailingZerosU32);
  EXPECT_EQ(insn->RegAt(0), vreg5);
  EXPECT_EQ(insn->RegAt(1), vreg1);
}

TEST(InsnFoldingTest, CountTrailingZeroesFolding32BMI) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 3);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ReverseBitsU32, kNoSSA>(vreg3, vreg2, flags);
  builder.Gen<Copy>(vreg4, vreg3, 8);
  builder.Gen<LzcntlRegReg>(vreg5, vreg4, flags);

  berberis::MachineInsn* insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(insn->opcode(), kMachineOpTzcntlRegReg);
  EXPECT_EQ(insn->RegAt(0), vreg5);
  EXPECT_EQ(insn->RegAt(1), vreg1);
}

TEST(InsnFoldingTest, CountTrailingZeroesFoldingCancelledIfArgNotAlive) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();
  MachineReg vreg6 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 3);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ReverseBitsU64, kNoSSA>(vreg3, vreg2, vreg4, flags);
  builder.Gen<MovqRegImm>(vreg1, 4);  // invalidates vreg1
  builder.Gen<Copy>(vreg5, vreg3, 8);
  builder.Gen<LzcntqRegReg>(vreg6, vreg5, flags);

  berberis::MachineInsn* insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(insn->opcode(), kMachineOpLzcntqRegReg);
}

TEST(InsnFoldingTest, FoldTwoImmediatesRegImmInsn32) {
  uint32_t imm = 0x1234'5678;
  TryTwoImmediatesRegImmInsnFolding<AndlRegImm, false>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm & uint32_t{0xf0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<OrlRegImm, false>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm | uint32_t{0xf0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<XorlRegImm, false>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm ^ uint32_t{0xf0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<AddlRegImm, false>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm + uint32_t{0xf0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<SublRegImm, false>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm - uint32_t{0xf0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<ShllRegImm, false>(imm, 10, imm << 10);
  TryTwoImmediatesRegImmInsnFolding<ShrlRegImm, false>(imm, 11, imm >> 11);
}

TEST(InsnFoldingTest, FoldTwoImmediatesRegImmInsn64) {
  uint64_t imm = 0x1234'5678'9abc'def0;
  TryTwoImmediatesRegImmInsnFolding<AndqRegImm, true>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm & uint64_t{0xffff'ffff'f0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<OrqRegImm, true>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm | uint64_t{0xffff'ffff'f0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<XorqRegImm, true>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm ^ uint64_t{0xffff'ffff'f0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<AddqRegImm, true>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm + uint64_t{0xffff'ffff'f0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<SubqRegImm, true>(
      imm, static_cast<int32_t>(0xf0f0'f0f0), imm - uint64_t{0xffff'ffff'f0f0'f0f0});
  TryTwoImmediatesRegImmInsnFolding<ShlqRegImm, true>(imm, 10, imm << 10);
  TryTwoImmediatesRegImmInsnFolding<ShrqRegImm, true>(imm, 11, imm >> 11);
}

TEST(InsnFoldingTest, FoldTwoImmediatesRegRegInsn32) {
  uint32_t imm1 = 0x1234'5678;
  uint32_t imm2 = 0xf0f0'f0f0;
  TryTwoImmediatesRegRegInsnFolding<AndlRegReg, false>(imm1, imm2, imm1 & imm2);
  TryTwoImmediatesRegRegInsnFolding<OrlRegReg, false>(imm1, imm2, imm1 | imm2);
  TryTwoImmediatesRegRegInsnFolding<XorlRegReg, false>(imm1, imm2, imm1 ^ imm2);
  TryTwoImmediatesRegRegInsnFolding<AddlRegReg, false>(imm1, imm2, imm1 + imm2);
  TryTwoImmediatesRegRegInsnFolding<SublRegReg, false>(imm1, imm2, imm1 - imm2);
  TryTwoImmediatesRegRegInsnFolding<ShllRegReg, false>(imm1, 10, imm1 << 10);
  TryTwoImmediatesRegRegInsnFolding<ShrlRegReg, false>(imm1, 11, imm1 >> 11);
}

TEST(InsnFoldingTest, FoldTwoImmediatesRegRegInsn64) {
  uint64_t imm1 = 0x1234'5678'9abc'def0;
  uint64_t imm2 = 0xf0f0'f0f0'f0f0'f0f0;
  TryTwoImmediatesRegRegInsnFolding<AndqRegReg, true>(imm1, imm2, imm1 & imm2);
  TryTwoImmediatesRegRegInsnFolding<OrqRegReg, true>(imm1, imm2, imm1 | imm2);
  TryTwoImmediatesRegRegInsnFolding<XorqRegReg, true>(imm1, imm2, imm1 ^ imm2);
  TryTwoImmediatesRegRegInsnFolding<AddqRegReg, true>(imm1, imm2, imm1 + imm2);
  TryTwoImmediatesRegRegInsnFolding<SubqRegReg, true>(imm1, imm2, imm1 - imm2);
  TryTwoImmediatesRegRegInsnFolding<ShlqRegReg, true>(imm1, 10, imm1 << 10);
  TryTwoImmediatesRegRegInsnFolding<ShrqRegReg, true>(imm1, 11, imm1 >> 11);
}

TEST(InsnFoldingTest, ContextAccessInfoGetsCorrectContextReadUsageValue) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg vreg5 = machine_ir.AllocVReg();
  MachineReg vreg6 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg1, flags);

  builder.Gen<Copy>(vreg3, vreg1, 8);
  builder.Gen<AddqRegReg, kNoSSA>(vreg3, vreg4, flags);

  builder.Gen<MovqRegImm>(vreg1, 5);
  builder.Gen<AddqRegReg, kNoSSA>(vreg4, vreg1, flags);

  builder.Gen<MovqRegOp>(vreg5, {.base = kMachineRegRAX, .disp = 6});
  builder.Gen<AddqRegReg, kNoSSA>(vreg6, vreg5, flags);

  ContextAccessInfo context_access_info(&machine_ir);
  context_access_info.Initialize(bb->insn_list());
  // Two insns use a register which contains value stored in kCPUStatePointer + 4, so
  // GetContextReadUsageCount(4) should return 2.
  EXPECT_EQ(context_access_info.GetContextReadUsageCount(4), uint32_t{2});  //
  // No memory accesses using disp = 5, so GetContextReadUsageCount should return 0.
  EXPECT_EQ(context_access_info.GetContextReadUsageCount(5), uint32_t{0});
  // The only memory accesses with disp = 6 uses base != kCPUStatePointer, so
  // GetContextReadUsageCount(6) should return 0.
  EXPECT_EQ(context_access_info.GetContextReadUsageCount(6), uint32_t{0});
}

TEST(InsnFoldingTest, FoldContextRead) {
  TryFoldContextReadIntoRegMemArithmetic<AddqRegReg, kMachineOpAddqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<AddlRegReg, kMachineOpAddlRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<XorqRegReg, kMachineOpXorqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<XorlRegReg, kMachineOpXorlRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<OrqRegReg, kMachineOpOrqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<OrlRegReg, kMachineOpOrlRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<SubqRegReg, kMachineOpSubqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<SublRegReg, kMachineOpSublRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<ImulqRegReg, kMachineOpImulqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<ImullRegReg, kMachineOpImullRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<CmpqRegReg, kMachineOpCmpqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<CmplRegReg, kMachineOpCmplRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<TestqRegReg, kMachineOpTestqMemBaseDispReg, false>();
  TryFoldContextReadIntoRegMemArithmetic<TestlRegReg, kMachineOpTestlMemBaseDispReg, false>();
  TryFoldContextReadIntoRegMemArithmetic<AndqRegReg, kMachineOpAndqRegMemBaseDisp, false>();
  TryFoldContextReadIntoRegMemArithmetic<AndlRegReg, kMachineOpAndlRegMemBaseDisp, false>();
  TryFoldContextReadIntoMemRegArithmetic<BtqRegReg, kMachineOpBtqMemBaseDispReg>();
  TryFoldContextReadIntoMemRegArithmetic<BtlRegReg, kMachineOpBtlMemBaseDispReg>();
  TryFoldContextReadIntoMemRegArithmetic<CmpqRegReg, kMachineOpCmpqMemBaseDispReg>();
  TryFoldContextReadIntoMemRegArithmetic<CmplRegReg, kMachineOpCmplMemBaseDispReg>();
  TryFoldContextReadIntoMemRegArithmetic<TestqRegReg, kMachineOpTestqMemBaseDispReg>();
  TryFoldContextReadIntoMemRegArithmetic<TestlRegReg, kMachineOpTestlMemBaseDispReg>();
  TryFoldContextReadIntoMemImmArithmetic<CmpqRegImm, kMachineOpCmpqMemBaseDispImm>();
  TryFoldContextReadIntoMemImmArithmetic<CmplRegImm, kMachineOpCmplMemBaseDispImm>();
  TryFoldContextReadIntoMemImmArithmetic<BtqRegImm, kMachineOpBtqMemBaseDispImm>();
  TryFoldContextReadIntoMemImmArithmetic<BtlRegImm, kMachineOpBtlMemBaseDispImm>();
  TryFoldContextReadIntoMemImmArithmetic<TestqRegImm, kMachineOpTestqMemBaseDispImm>();
  TryFoldContextReadIntoMemImmArithmetic<TestlRegImm, kMachineOpTestlMemBaseDispImm>();
}

TEST(InsnFoldingTest, FoldXMMContextRead) {
  TryFoldContextReadIntoRegMemArithmetic<PandXRegXReg, kMachineOpPandXRegMemBaseDisp, true>();
  TryFoldContextReadIntoRegMemArithmetic<PadddXRegXReg, kMachineOpPadddXRegMemBaseDisp, true>();
  TryFoldContextReadIntoRegMemArithmetic<PsubdXRegXReg, kMachineOpPsubdXRegMemBaseDisp, true>();
  TryFoldContextReadIntoRegMemArithmetic<PorXRegXReg, kMachineOpPorXRegMemBaseDisp, true>();
  TryFoldContextReadIntoRegMemArithmetic<PxorXRegXReg, kMachineOpPxorXRegMemBaseDisp, true>();
}

TEST(InsnFoldingTest, FoldContextReadAndImmediate) {
  TryFoldContextReadAndImmediateMemImmArithmetic<CmpqRegReg, kMachineOpCmpqMemBaseDispImm>();
  TryFoldContextReadAndImmediateMemImmArithmetic<CmplRegReg, kMachineOpCmplMemBaseDispImm>();
  TryFoldContextReadAndImmediateMemImmArithmetic<TestqRegReg, kMachineOpTestqMemBaseDispImm>();
  TryFoldContextReadAndImmediateMemImmArithmetic<TestlRegReg, kMachineOpTestlMemBaseDispImm>();
}

TEST(InsnFoldingTest, ReadContextFoldingCancelledIfIncreasesMemoryAccesses) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg1, flags);

  ContextAccessInfo context_access_info(&machine_ir);
  DefMap def_map(&machine_ir);
  InsnFolding insn_folder(def_map, context_access_info, &machine_ir);
  context_access_info.Initialize(bb->insn_list());
  def_map.Initialize();
  def_map.ProcessInsn(bb->insn_list().begin());
  auto insn_to_fold_it = std::prev(bb->insn_list().end());
  auto [folding_type, insn] = insn_folder.TryFoldContextReadForTesting(*insn_to_fold_it, 1);
  // Basic block has one usage of context read value, so we expect the optimization to occur.
  ASSERT_EQ(folding_type, FoldingType::kReplaceInsn);

  builder.Gen<Copy>(vreg3, vreg1, 8);
  builder.Gen<AddqRegReg, kNoSSA>(vreg4, vreg3, flags);

  context_access_info.Initialize(bb->insn_list());
  std::tie(folding_type, insn) = insn_folder.TryFoldContextReadForTesting(*insn_to_fold_it, 1);
  // Basic block has two usages of context read value, so we do not expect the optimization to
  // occur.
  ASSERT_EQ(folding_type, FoldingType::kImpossible);
}

TEST(InsnFoldingTest, ReadContextFoldingWorksWithNonContextWriteInsnBetweenContextReadAndValueUse) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<AddqRegReg, kNoSSA>(vreg3, vreg2, flags);
  builder.Gen<AddqRegReg, kNoSSA>(vreg4, vreg1, flags);

  berberis::MachineInsn* folded_insn = *FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  EXPECT_EQ(kMachineOpAddqRegMemBaseDisp, folded_insn->opcode());
  EXPECT_EQ(vreg4, folded_insn->RegAt(0));
  EXPECT_EQ(kCPUStatePointer, folded_insn->RegAt(1));
  EXPECT_EQ(4UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(flags, folded_insn->RegAt(2));
}

TEST(InsnFoldingTest,
     ReadContextFoldingDoesntWorkWithContextWriteInsnBetweenContextReadAndValueUse) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<MovqOpReg>({.base = kCPUStatePointer, .disp = 12}, vreg2);
  builder.Gen<AddqRegReg, kNoSSA>(vreg3, vreg1, flags);

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;
  EXPECT_EQ(kMachineOpAddqRegReg, folded_insn->opcode());
  EXPECT_EQ(vreg3, folded_insn->RegAt(0));
  EXPECT_EQ(vreg1, folded_insn->RegAt(1));
  EXPECT_EQ(flags, folded_insn->RegAt(2));
}

TEST(InsnFoldingTest, InsnFoldingExecutionMakesIsCPUStatePutInvalid) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 5);

  berberis::MachineInsn* insn = *bb->insn_list().begin();
  ASSERT_FALSE(machine_ir.IsCPUStatePut(insn));

  FoldInsns(&machine_ir);

  ASSERT_DEATH(EXPECT_FALSE(machine_ir.IsCPUStatePut(insn)),
               "IsCPUStatePut called after insn folding.");
}

TEST(InsnFoldingTest, InsnFoldingExecutionMakesIsCPUStateGetInvalid) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegImm>(vreg1, 5);

  berberis::MachineInsn* insn = *bb->insn_list().begin();
  ASSERT_FALSE(machine_ir.IsCPUStatePut(insn));

  FoldInsns(&machine_ir);

  ASSERT_DEATH(EXPECT_FALSE(machine_ir.IsCPUStateGet(insn)),
               "IsCPUStateGet called after insn folding.");
}

TEST(InsnFoldingTest, SwapRegOperandsAndFoldContextRead) {
  TrySwapRegOperandsAndFoldContextReadArithmetic<AddqRegReg, kMachineOpAddqRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<AddlRegReg, kMachineOpAddlRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<AndqRegReg, kMachineOpAndqRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<AndlRegReg, kMachineOpAndlRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<ImulqRegReg, kMachineOpImulqRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<ImullRegReg, kMachineOpImullRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<OrqRegReg, kMachineOpOrqRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<OrlRegReg, kMachineOpOrlRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<XorqRegReg, kMachineOpXorqRegMemBaseDisp>();
  TrySwapRegOperandsAndFoldContextReadArithmetic<XorlRegReg, kMachineOpXorlRegMemBaseDisp>();
}

TEST(InsnFoldingTest, SwapRegOperandsAndFoldContextReadTwiceWithDifferentTempReg) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<MovqRegOp>(vreg1, {.base = kCPUStatePointer, .disp = 4});
  builder.Gen<AddqRegReg, kNoSSA>(vreg1, vreg2, flags);

  builder.Gen<MovqRegOp>(vreg3, {.base = kCPUStatePointer, .disp = 8});
  builder.Gen<AndqRegReg, kNoSSA>(vreg3, vreg4, flags);

  auto final_folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* fourth_folded_copy = *final_folded_insn_it;
  EXPECT_EQ(kMachineOpCopy, fourth_folded_copy->opcode());
  EXPECT_EQ(vreg3, fourth_folded_copy->RegAt(0));
  auto new_vreg2 = fourth_folded_copy->RegAt(1);
  EXPECT_FALSE(Contains(std::list{vreg1, vreg2, vreg3, vreg4, flags}, new_vreg2));

  berberis::MachineInsn* second_folded_op_insn = *std::prev(final_folded_insn_it);
  EXPECT_EQ(kMachineOpAndqRegMemBaseDisp, second_folded_op_insn->opcode());
  EXPECT_EQ(new_vreg2, second_folded_op_insn->RegAt(0));
  EXPECT_EQ(kCPUStatePointer, second_folded_op_insn->RegAt(1));
  EXPECT_EQ(8UL, AsMachineInsnX86_64(second_folded_op_insn)->disp());
  EXPECT_EQ(flags, second_folded_op_insn->RegAt(2));

  berberis::MachineInsn* third_folded_copy = *std::prev(final_folded_insn_it, 2);
  EXPECT_EQ(kMachineOpCopy, third_folded_copy->opcode());
  EXPECT_EQ(new_vreg2, third_folded_copy->RegAt(0));
  EXPECT_EQ(vreg4, third_folded_copy->RegAt(1));

  berberis::MachineInsn* second_folded_copy = *std::prev(final_folded_insn_it, 4);
  EXPECT_EQ(kMachineOpCopy, second_folded_copy->opcode());
  EXPECT_EQ(vreg1, second_folded_copy->RegAt(0));
  auto new_vreg1 = second_folded_copy->RegAt(1);
  EXPECT_FALSE(Contains(std::list{vreg1, vreg2, vreg3, vreg4, flags, new_vreg2}, new_vreg1));

  berberis::MachineInsn* first_folded_op_insn = *std::prev(final_folded_insn_it, 5);
  EXPECT_EQ(kMachineOpAddqRegMemBaseDisp, first_folded_op_insn->opcode());
  EXPECT_EQ(new_vreg1, first_folded_op_insn->RegAt(0));
  EXPECT_EQ(kCPUStatePointer, first_folded_op_insn->RegAt(1));
  EXPECT_EQ(4UL, AsMachineInsnX86_64(first_folded_op_insn)->disp());
  EXPECT_EQ(flags, first_folded_op_insn->RegAt(2));

  berberis::MachineInsn* first_folded_copy = *std::prev(final_folded_insn_it, 6);
  EXPECT_EQ(kMachineOpCopy, first_folded_copy->opcode());
  EXPECT_EQ(new_vreg1, first_folded_copy->RegAt(0));
  EXPECT_EQ(vreg2, first_folded_copy->RegAt(1));
}

TEST(InsnFoldingTest, FoldScaleIntoMemWrite) {
  TryFoldScaleIntoMemWrite<MovlOpReg, kMachineOpMovlMemBaseIndexDispReg>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemWrite<MovlOpReg, kMachineOpMovlMemBaseIndexDispReg>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemWrite<MovlOpReg, kMachineOpMovlMemBaseIndexDispReg>(
      3, Assembler::ScaleFactor::kTimesEight);
  TryFoldScaleIntoMemWrite<MovqOpReg, kMachineOpMovqMemBaseIndexDispReg>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemWrite<MovqOpReg, kMachineOpMovqMemBaseIndexDispReg>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemWrite<MovqOpReg, kMachineOpMovqMemBaseIndexDispReg>(
      3, Assembler::ScaleFactor::kTimesEight);
}

TEST(InsnFoldingTest, FoldScaleIntoMemRead) {
  TryFoldScaleIntoMemRead<MovlRegOp, kMachineOpMovlRegMemBaseIndexDisp>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemRead<MovlRegOp, kMachineOpMovlRegMemBaseIndexDisp>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemRead<MovlRegOp, kMachineOpMovlRegMemBaseIndexDisp>(
      3, Assembler::ScaleFactor::kTimesEight);
  TryFoldScaleIntoMemRead<MovqRegOp, kMachineOpMovqRegMemBaseIndexDisp>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemRead<MovqRegOp, kMachineOpMovqRegMemBaseIndexDisp>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemRead<MovqRegOp, kMachineOpMovqRegMemBaseIndexDisp>(
      3, Assembler::ScaleFactor::kTimesEight);
  TryFoldScaleIntoMemRead<MovzxwlRegOp, kMachineOpMovzxwlRegMemBaseIndexDisp>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemRead<MovzxwlRegOp, kMachineOpMovzxwlRegMemBaseIndexDisp>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemRead<MovzxwlRegOp, kMachineOpMovzxwlRegMemBaseIndexDisp>(
      3, Assembler::ScaleFactor::kTimesEight);
  TryFoldScaleIntoMemRead<MovsxwlRegOp, kMachineOpMovsxwlRegMemBaseIndexDisp>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemRead<MovsxwlRegOp, kMachineOpMovsxwlRegMemBaseIndexDisp>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemRead<MovsxwlRegOp, kMachineOpMovsxwlRegMemBaseIndexDisp>(
      3, Assembler::ScaleFactor::kTimesEight);
  TryFoldScaleIntoMemRead<MovsxlqRegOp, kMachineOpMovsxlqRegMemBaseIndexDisp>(
      1, Assembler::ScaleFactor::kTimesTwo);
  TryFoldScaleIntoMemRead<MovsxlqRegOp, kMachineOpMovsxlqRegMemBaseIndexDisp>(
      2, Assembler::ScaleFactor::kTimesFour);
  TryFoldScaleIntoMemRead<MovsxlqRegOp, kMachineOpMovsxlqRegMemBaseIndexDisp>(
      3, Assembler::ScaleFactor::kTimesEight);
}

TEST(InsnFoldingTest, FoldScaleIntoMemAccessCancelledIfShiftTooLarge) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();
  auto* recovery_bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg3 = machine_ir.AllocVReg();
  MachineReg vreg4 = machine_ir.AllocVReg();
  MachineReg flags = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<Copy>(vreg1, vreg4, 8);
  builder.Gen<Copy>(vreg2, vreg1, 8);
  builder.Gen<ShlqRegImm, kNoSSA>(vreg2, 4, flags);
  builder.Gen<MovqOpReg>({.base = vreg3, .index = vreg2, .disp = 12}, vreg4);
  builder.SetRecoveryPointAtLastInsn(recovery_bb);
  builder.SetRecoveryWithGuestPCAtLastInsn(42);

  MachineInsnList::iterator folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* folded_insn = *folded_insn_it;
  EXPECT_EQ(kMachineOpMovqMemBaseIndexDispReg, folded_insn->opcode());
  EXPECT_EQ(vreg3, folded_insn->RegAt(0));
  EXPECT_EQ(vreg2, folded_insn->RegAt(1));
  EXPECT_EQ(vreg4, folded_insn->RegAt(2));
  EXPECT_EQ(12UL, AsMachineInsnX86_64(folded_insn)->disp());
  EXPECT_EQ(Assembler::ScaleFactor::kTimesOne, AsMachineInsnX86_64(folded_insn)->scale());
  EXPECT_EQ(recovery_bb, folded_insn->recovery_bb());
  EXPECT_EQ(42UL, folded_insn->recovery_pc());
}

TEST(InsnFoldingTest, ReplaceWriteFlagsWithCmpRegRegInsn) {
  TryReplaceWriteFlagsWithCmpRegRegInsn<CmpqRegReg, kMachineOpCmpqRegReg>();
  TryReplaceWriteFlagsWithCmpRegRegInsn<CmplRegReg, kMachineOpCmplRegReg>();
}

TEST(InsnFoldingTest, ReplaceWriteFlagsWithCmpRegImmInsn) {
  TryReplaceWriteFlagsWithCmpRegImmInsn<CmpqRegImm, kMachineOpCmpqRegImm>();
  TryReplaceWriteFlagsWithCmpRegImmInsn<CmplRegImm, kMachineOpCmplRegImm>();
}

TEST(InsnFoldingTest, WriteFlagsNotReplacedByCmpRegImmIfRegArgumentChanged) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg_rax = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<CmplRegImm>(vreg1, 5, kMachineRegFLAGS);
  builder.Gen<ReadFlagsWithOverflow>(vreg_rax, kMachineRegFLAGS);
  builder.Gen<AddqRegReg, kNoSSA>(vreg1, vreg2, kMachineRegFLAGS);
  builder.Gen<WriteFlags, kNoSSA>(vreg_rax, kMachineRegFLAGS);

  MachineInsnList::iterator not_folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* not_folded_insn = *not_folded_insn_it;

  EXPECT_EQ(kMachineOpWriteFlags, not_folded_insn->opcode());
}

TEST(InsnFoldingTest, WriteFlagsNotReplacedByCmpRegRegIfFirstRegArgumentChanged) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg_rax = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<CmplRegReg>(vreg1, vreg2, kMachineRegFLAGS);
  builder.Gen<ReadFlagsWithOverflow>(vreg_rax, kMachineRegFLAGS);
  builder.Gen<AddqRegReg, kNoSSA>(vreg1, vreg2, kMachineRegFLAGS);
  builder.Gen<WriteFlags, kNoSSA>(vreg_rax, kMachineRegFLAGS);

  MachineInsnList::iterator not_folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* not_folded_insn = *not_folded_insn_it;

  EXPECT_EQ(kMachineOpWriteFlags, not_folded_insn->opcode());
}

TEST(InsnFoldingTest, WriteFlagsNotReplacedByCmpRegRegIfSecondRegArgumentChanged) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg_rax = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<CmplRegReg>(vreg1, vreg2, kMachineRegFLAGS);
  builder.Gen<ReadFlagsWithOverflow>(vreg_rax, kMachineRegFLAGS);
  builder.Gen<AddqRegReg, kNoSSA>(vreg2, vreg1, kMachineRegFLAGS);
  builder.Gen<WriteFlags, kNoSSA>(vreg_rax, kMachineRegFLAGS);

  MachineInsnList::iterator not_folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* not_folded_insn = *not_folded_insn_it;

  EXPECT_EQ(kMachineOpWriteFlags, not_folded_insn->opcode());
}

TEST(InsnFoldingTest, WriteFlagsNotReplacedWhenNoInsnBeforeReadFlags) {
  Arena arena;
  MachineIR machine_ir(&arena);

  MachineIRBuilder builder(&machine_ir);

  auto* bb = machine_ir.NewBasicBlock();

  MachineReg vreg1 = machine_ir.AllocVReg();
  MachineReg vreg2 = machine_ir.AllocVReg();
  MachineReg vreg_rax = machine_ir.AllocVReg();

  builder.StartBasicBlock(bb);
  builder.Gen<ReadFlagsWithOverflow>(vreg_rax, kMachineRegFLAGS);
  builder.Gen<AddqRegReg, kNoSSA>(vreg1, vreg2, kMachineRegFLAGS);
  builder.Gen<WriteFlags, kNoSSA>(vreg_rax, kMachineRegFLAGS);

  MachineInsnList::iterator not_folded_insn_it = FoldInsnsAndGetLastInsnIt(&machine_ir, bb);
  berberis::MachineInsn* not_folded_insn = *not_folded_insn_it;

  EXPECT_EQ(kMachineOpWriteFlags, not_folded_insn->opcode());
}

INSTANTIATE_READ_FLAGS_VARIANTS_TEST(InsnFoldingTest);

}  // namespace

}  // namespace berberis::x86_64
