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

#include "berberis/intrinsics/all_to_x86_32_or_x86_64/intrinsics_bindings.h"
#include "berberis/intrinsics/all_to_x86_32_or_x86_64/intrinsics_float.h"
#include "berberis/intrinsics/all_to_x86_32_or_x86_64/verifier_assembler_x86_32_and_x86_64.h"

namespace berberis {

namespace {

template <typename Assembler>
class MacroAssembler : public Assembler {
 public:
  using MacroAssemblers = std::tuple<MacroAssembler<Assembler>,
                                     typename Assembler::BaseAssembler,
                                     typename Assembler::FinalAssembler>;
  template <typename... Args>
  constexpr explicit MacroAssembler(Args&&... args) : Assembler(std::forward<Args>(args)...) {}

#define IMPORT_ASSEMBLER_FUNCTIONS
#include "berberis/assembler/gen_assembler_x86_common-using-inl.h"
#undef IMPORT_ASSEMBLER_FUNCTIONS

#define DEFINE_MACRO_ASSEMBLER_GENERIC_FUNCTIONS
#include "berberis/intrinsics/all_to_x86_32_or_x86_64/macro_assembler-inl.h"
#undef DEFINE_MACRO_ASSEMBLER_GENERIC_FUNCTIONS

  // dst: USE_DEF, src1: USE
  constexpr void SSE3Intrinsic(XMMRegister dst, XMMRegister src1) { Haddpd(dst, src1); }

  // dst: DEF_EARLY_CLOBBER, src1: USE_DEF, src2: USE, flags: DEF
  constexpr void LinearRegisterIntrinsic(Register dst, Register src1, Register src2) {
    Addl(src1, src2);  // Writes to FLAGS
    Movl(dst, src1);
    Addl(dst, src2);
  }

  // dst: DEF_EARLY_CLOBBER, src1: USE, src2: USE
  constexpr void LinearXMMRegisterIntrinsic(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    Pmov(dst, src1);
    Pmov(dst, src2);
  }

  // dst: DEF, src1: USE
  constexpr void InfinitelyLoopingIntrinsicWithDef(Register dst, Register src1) {
    Label* l1 = MakeLabel();
    Movl(dst, src1);
    Bind(l1);
    Jmp(*l1);
  }

  // dst: DEF_EARLY_CLOBBER, src1: USE
  constexpr void InfinitelyLoopingIntrinsicWithDefEarlyClobber(Register dst, Register src1) {
    Label* l1 = MakeLabel();
    Cmpl(src1, src1);
    Bind(l1);
    Movl(dst, src1);
    Jcc(Assembler::Condition::kZero, *l1);
  }

  // dst: DEF, src1: USE, flags: DEF
  constexpr void ForwardJumpingIntrinsicWithDef(Register dst, Register src1) {
    Label* l1 = MakeLabel();
    Label* l2 = MakeLabel();
    Label* done = MakeLabel();

    Movl(dst, src1);

    Jcc(Assembler::Condition::kZero, *l1);
    Jcc(Assembler::Condition::kZero, *l2);

    Addl(dst, dst);
    Jmp(*done);

    Bind(l1);
    Addl(dst, dst);
    Jmp(*done);

    Bind(l2);
    Addl(dst, dst);
    Jmp(*done);

    Bind(done);
  }

  // dst: DEF_EARLY_CLOBBER, src1: USE, flags: DEF
  constexpr void ForwardJumpingIntrinsicWithDefEarlyClobber(Register dst, Register src1) {
    Label* l1 = MakeLabel();
    Label* l2 = MakeLabel();
    Label* done = MakeLabel();

    Movl(dst, src1);

    Jcc(Assembler::Condition::kZero, *l1);
    // Taking jump to l2 is the invalid path.
    Jcc(Assembler::Condition::kZero, *l2);

    Addl(dst, dst);
    Jmp(*done);

    Bind(l1);
    Addl(dst, dst);
    Jmp(*done);

    Bind(l2);
    Addl(dst, src1);
    Jmp(*done);

    Bind(done);
  }

  // dst: DEF_EARLY_CLOBBER, src1: USE
  constexpr void LoopingIntrinsicWithDefEarlyClobber(XMMRegister dst, XMMRegister src1) {
    Label* l1 = MakeLabel();
    Label* out = MakeLabel();

    Bind(l1);
    Jcc(Assembler::Condition::kZero, *out);
    Pxor(dst, dst);
    Jmp(*l1);

    Bind(out);
    Pmov(dst, src1);
  }
};

class VerifierAssembler : public x86_32_and_x86_64::VerifierAssembler<VerifierAssembler> {
 public:
  using BaseAssembler = x86_32_and_x86_64::VerifierAssembler<VerifierAssembler>;
  using FinalAssembler = VerifierAssembler;

  constexpr VerifierAssembler() : BaseAssembler() {}

 private:
  VerifierAssembler(const VerifierAssembler&) = delete;
  VerifierAssembler(VerifierAssembler&&) = delete;
  void operator=(const VerifierAssembler&) = delete;
  void operator=(VerifierAssembler&&) = delete;
  using DerivedAssemblerType = VerifierAssembler;

  friend BaseAssembler;
};

template <typename AsmCallInfo>
constexpr void VerifyIntrinsic() {
  int register_numbers[std::tuple_size_v<typename AsmCallInfo::Bindings> == 0
                           ? 1
                           : std::tuple_size_v<typename AsmCallInfo::Bindings>];
  AssignRegisterNumbers<AsmCallInfo>(register_numbers);
  MacroAssembler<VerifierAssembler> as;
  CallVerifierAssembler<AsmCallInfo, MacroAssembler<VerifierAssembler>>(&as, register_numbers);
  // Verify CPU vendor and SSE restrictions.
  as.CheckCPUIDRestriction<typename AsmCallInfo::CPUIDRestriction>();

  // Verify that intrinsic's bindings correctly states that intrinsic uses/doesn't use FLAGS
  // register.
  bool expect_flags = false;
  CheckIntrinsicHasFlagsBinding<AsmCallInfo>(expect_flags);
  as.CheckFlagsBinding(expect_flags);
  as.CheckAppropriateDefEarlyClobbers();
  as.CheckLabelsAreBound();
  as.CheckNonLinearIntrinsicsUseDefRegisters();
}

static constexpr const char kBindingName[] = "TestInstruction";
static constexpr const char kBindingMnemo[] = "TEST_0";

using MacroAssemblers = MacroAssembler<VerifierAssembler>::MacroAssemblers;

TEST(VerifierAssembler, TestCorrectCPUID) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::SSE3Intrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::HasSSE3,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<SIMD128Register, SIMD128Register>,
      std::tuple<SIMD128Register>,
      std::tuple<InOutArg<0, 0, intrinsics::bindings::XmmReg, intrinsics::bindings::Def>,
                 InArg<1, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>>>;

  VerifyIntrinsic<AsmCallInfo>();
}

TEST(VerifierAssembler, TestIncorrectCPUID) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::SSE3Intrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<SIMD128Register, SIMD128Register>,
      std::tuple<SIMD128Register>,
      std::tuple<InOutArg<0, 0, intrinsics::bindings::XmmReg, intrinsics::bindings::Def>,
                 InArg<1, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>>>;

  ASSERT_DEATH(VerifyIntrinsic<AsmCallInfo>(), "error: expect_sse3 != need_sse3");
}

TEST(VerifierAssembler, TestFlagsIntrinsicWithNoFlagsBinding) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LinearRegisterIntrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t, uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<
          OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::DefEarlyClobber>,
          InOutArg<1, 1, intrinsics::bindings::GeneralReg32, intrinsics::bindings::UseDef>,
          InArg<2, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>>>;

  ASSERT_DEATH(VerifyIntrinsic<AsmCallInfo>(), "error: expect_flags != defines_flags");
}

TEST(VerifierAssembler, TestNoFlagsIntrinsicWithFlagsBinding) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LinearXMMRegisterIntrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<SIMD128Register, SIMD128Register>,
      std::tuple<SIMD128Register>,
      std::tuple<OutArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::DefEarlyClobber>,
                 InArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>,
                 InArg<1, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>,
                 TmpArg<intrinsics::bindings::FLAGS, intrinsics::bindings::Def>>>;

  ASSERT_DEATH(VerifyIntrinsic<AsmCallInfo>(), "error: expect_flags != defines_flags");
}

TEST(VerifierAssembler, TestValidRegisterUseDef) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LinearRegisterIntrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t, uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<
          OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::DefEarlyClobber>,
          InOutArg<1, 1, intrinsics::bindings::GeneralReg32, intrinsics::bindings::UseDef>,
          InArg<2, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>,
          TmpArg<intrinsics::bindings::FLAGS, intrinsics::bindings::Def>>>;

  VerifyIntrinsic<AsmCallInfo>();
}

TEST(VerifierAssembler, TestInvalidRegisterUseDef) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LinearRegisterIntrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t, uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Def>,
                 InOutArg<1, 1, intrinsics::bindings::GeneralReg32, intrinsics::bindings::UseDef>,
                 InArg<2, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>,
                 TmpArg<intrinsics::bindings::FLAGS, intrinsics::bindings::Def>>>;

  ASSERT_DEATH(
      VerifyIntrinsic<AsmCallInfo>(),
      "error: intrinsic used a 'use' general register after writing to a 'def' general register");
}

TEST(VerifierAssembler, TestValidXMMRegisterUseDef) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LinearXMMRegisterIntrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<SIMD128Register, SIMD128Register>,
      std::tuple<SIMD128Register>,
      std::tuple<OutArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::DefEarlyClobber>,
                 InArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>,
                 InArg<1, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>>>;

  VerifyIntrinsic<AsmCallInfo>();
}

TEST(VerifierAssembler, TestInvalidXMMRegisterUseDef) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LinearXMMRegisterIntrinsic,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<SIMD128Register, SIMD128Register>,
      std::tuple<SIMD128Register>,
      std::tuple<OutArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::Def>,
                 InArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>,
                 InArg<1, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>>>;

  ASSERT_DEATH(VerifyIntrinsic<AsmCallInfo>(),
               "error: intrinsic used a 'use' xmm register after writing to a 'def' xmm register");
}

TEST(VerifierAssembler, TestValidInfinitelyLoopingValidIntrinsic) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::InfinitelyLoopingIntrinsicWithDef,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Def>,
                 InArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>>>;

  VerifyIntrinsic<AsmCallInfo>();
}

TEST(VerifierAssembler, TestInvalidInfinitelyLoopingIntrinsic) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::InfinitelyLoopingIntrinsicWithDefEarlyClobber,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Def>,
                 InArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>,
                 TmpArg<intrinsics::bindings::FLAGS, intrinsics::bindings::Def>>>;

  ASSERT_DEATH(
      VerifyIntrinsic<AsmCallInfo>(),
      "error: intrinsic used a 'use' general register after writing to a 'def' general register");
}

TEST(VerifierAssembler, TestValidForwardJumpingIntrinsic) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::ForwardJumpingIntrinsicWithDef,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Def>,
                 InArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>,
                 TmpArg<intrinsics::bindings::FLAGS, intrinsics::bindings::Def>>>;

  VerifyIntrinsic<AsmCallInfo>();
}

TEST(VerifierAssembler, TestInvalidForwardJumpingIntrinsic) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::ForwardJumpingIntrinsicWithDefEarlyClobber,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<uint32_t>,
      std::tuple<uint32_t>,
      std::tuple<OutArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Def>,
                 InArg<0, intrinsics::bindings::GeneralReg32, intrinsics::bindings::Use>,
                 TmpArg<intrinsics::bindings::FLAGS, intrinsics::bindings::Def>>>;

  ASSERT_DEATH(
      VerifyIntrinsic<AsmCallInfo>(),
      "error: intrinsic used a 'use' general register after writing to a 'def' general register");
}

TEST(VerifierAssembler, TestInvalidLoopingIntrinsic) {
  using AsmCallInfo = intrinsics::bindings::AsmCallInfo<
      kBindingName,
      &std::tuple_element_t<0, MacroAssemblers>::LoopingIntrinsicWithDefEarlyClobber,
      kBindingMnemo,
      nullptr,
      intrinsics::bindings::NoCPUIDRestriction,
      intrinsics::bindings::NoNansOperation,
      false,
      std::tuple<SIMD128Register>,
      std::tuple<SIMD128Register>,
      std::tuple<OutArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::Def>,
                 InArg<0, intrinsics::bindings::XmmReg, intrinsics::bindings::Use>>>;

  ASSERT_DEATH(VerifyIntrinsic<AsmCallInfo>(),
               "error: intrinsic used a 'use' xmm register after writing to a 'def' xmm register");
}

}  // namespace

}  // namespace berberis
