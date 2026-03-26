/*
 * Copyright (C) 2019 The Android Open Source Project
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

// This file is meant to be included as part of a C++ file testing the
// functionality of an execution engine, such as ARM64 hardware, or
// a CPU emulation layer.
//
// You need to define two items before including this file:
//
// - TESTSUITE: A macro defining the testsuite name.
// - TestHarness: the class to execute one instruction. InsnTestHarnessArm64
// template specification is recommended.
//
// The following includes are assumed:
// #include <x86intrin.h>  // _mm_getcsr()
// #include "string.h"
// #include <bit>  // std::bit_cast
// #include <type_traits>  // std::is_same_v
// #include "berberis/base/bit_util.h"
// #include "berberis/guest_state/guest_addr.h"
// #include "berberis/guest_state/guest_state.h"
// #include "berberis/intrinsics/common/intrinsics_float.h"
//
// See examples at:
// interpreter/arm64/interpreter_arm64_tests.cc
// lite_translator_arm64/lite_translate_insn_exec_tests.cc

#if defined(__x86_64__)
class ScopedMxcsr {
 public:
  ScopedMxcsr(unsigned int csr) : prev_csr_{get()} { _mm_setcsr(csr); }
  ~ScopedMxcsr() { _mm_setcsr(prev_csr_); }
  static unsigned int get() { return _mm_getcsr(); }

 private:
  unsigned int prev_csr_;
};

TEST(TESTSUITE, ReadFpsr) {
  static constexpr uint64_t kQcBit = 1ULL << 27;

  static const uint32_t code[] = {
      0xd53b'4420,  // mrs x0, fpsr
  };

  TestHarness test(code, sizeof(code));

  // Part of the state is on host, so make sure it's in a predefined state.
  ScopedMxcsr scoped_mxcsr(0);
  test.state()->cpu.emulated_fpsr = kQcBit;
  test.state()->cpu.x[0] = 0;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], kQcBit);
}

TEST(TESTSUITE, WriteFpcr) {
  constexpr uint32_t kFpcrIxeBit = 1U << 12;
  static const uint32_t code[] = {
      0xd51b'4400,  // msr fpcr, x0
  };

  TestHarness test(code, sizeof(code));

  ScopedMxcsr scoped_mxcsr(_MM_MASK_INEXACT);
  test.state()->cpu.x[0] = kFpcrIxeBit;  // to be moved into FPCR
  test.state()->cpu.cached_fpcr = 0;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.cached_fpcr, kFpcrIxeBit) << "Should cache FPCR";
  EXPECT_EQ(scoped_mxcsr.get() & _MM_MASK_INEXACT, 0u) << "Should sync MXCSR";
}
#endif

TEST(TESTSUITE, MovImmFillZero) {
  static const uint32_t code[] = {
      0xd280'0808,  // movz x8, #0x40
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0x40ULL);
}

TEST(TESTSUITE, MovImmFillZeroAndNegate) {
  static const uint32_t code[] = {
      0x9280'0808,  // movn x8, #0x40
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], ~0x40ULL);
}

TEST(TESTSUITE, MovImm32NoSignExtend) {
  static const uint32_t code[] = {
      0x52b0'0000,  // mov w0, #0x8000'0000
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xdead'beefULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x8000'0000ULL);
}

TEST(TESTSUITE, AddImm) {
  static const uint32_t code[] = {
      0x9100'0400,  // add x0, x0, 1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());
  // EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64 + 1);
}

TEST(TESTSUITE, SubImm) {
  static const uint32_t code[] = {
      0xd100'0400,  // sub x0, x0, 1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64 - 1ULL);
}

TEST(TESTSUITE, AddShiftedImm) {
  static const uint32_t code[] = {
      0x9140'0400,  // add x0, x0, 1, lsl #12
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64 + (1ULL << 12));
}

TEST(TESTSUITE, SubImmSP) {
  static const uint32_t code[] = {
      0xd100'041f,  // sub sp, x0, 1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.sp, kTestValue64 - 1ULL);
}

TEST(TESTSUITE, AddRegister) {
  static const uint32_t code[] = {
      0x8b0b'0047,  // add x7, x2, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = kTestValue64;
  test.state()->cpu.x[11] = 42;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], kTestValue64 + 42);
}

TEST(TESTSUITE, AddRegister32) {
  static const uint32_t code[] = {
      0x0b0b'0047,  // add w7, w2, w11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 0xffff'ffff'0000'0001ULL;
  test.state()->cpu.x[11] = 0x7fff'ffff'ffff'ffd6ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], 0x0000'0000'ffff'ffd7ULL);
}

TEST(TESTSUITE, AddRegister32WithFlags) {
  static const uint32_t code[] = {
      0x2b0b'0047,  // adds w7, w2, w11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 0xffff'ffff'0000'0001LL;
  test.state()->cpu.x[11] = 0x7fff'ffff'0000'002aLL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], 43ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0000));
}

TEST(TESTSUITE, SubRegister) {
  static const uint32_t code[] = {
      0xcb0b'0047,  // sub x7, x2, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = kTestValue64;
  test.state()->cpu.x[11] = 42;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], kTestValue64 - 42);
}

TEST(TESTSUITE, CmpRegister) {
  static const uint32_t code[] = {
      0xeb0b'005f,  // subs xzr, x2, x11 <=> cmp x2, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 42;
  test.state()->cpu.x[11] = 21;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0000));
}

TEST(TESTSUITE, CallImm) {
  static const uint32_t code[] = {
      0x9400'0002,  // bl pc + 0x8
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
  EXPECT_EQ(test.state()->cpu.x[30], ToGuestAddr(code) + 4);
}

TEST(TESTSUITE, CallSignExtendImm) {
  static const uint32_t code[] = {
      0x97ff'ffff,  // bl pc - 0x4
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) - 4));
  EXPECT_EQ(test.state()->cpu.x[30], ToGuestAddr(code) + 4);
}

TEST(TESTSUITE, Return) {
  static const uint32_t code[] = {
      0xd65f'03c0,  // ret
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[30] = kTestAddr64;

  EXPECT_TRUE(test.RunBranch(kTestAddr64));
}

TEST(TESTSUITE, CallRegister) {
  static const uint32_t code[] = {
      0xd63f'0040,  // blr x2
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = kTestAddr64;

  EXPECT_TRUE(test.RunBranch(kTestAddr64));
  EXPECT_EQ(test.state()->cpu.x[30], ToGuestAddr(code) + 4);
}

// Some programs use it instead of ret.
TEST(TESTSUITE, CallLinkRegister) {
  static const uint32_t code[] = {
      0xd63f'03c0,  // blr x30
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[30] = kTestAddr64;

  EXPECT_TRUE(test.RunBranch(kTestAddr64));
  EXPECT_EQ(test.state()->cpu.x[30], ToGuestAddr(code) + 4);
}

TEST(TESTSUITE, AndImm) {
  static const uint32_t code[] = {
      0x927c'ec01,  // and x1, x0, #0xffff'ffff.fffffff0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'00ffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'0000'ffff'00f0ULL);
}

TEST(TESTSUITE, AndImmZeroFlag) {
  static const uint32_t code[] = {
      0xf240'1c62,  // ands x2, x3, #0xff
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0xdead'beef;
  test.state()->cpu.x[3] = 0xdd00ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, AndImmNegFlag) {
  static const uint32_t code[] = {
      0xf27f'f862,  // ands x2, x3, #0xffff'ffff'ffff'fffe
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0x0;
  test.state()->cpu.x[3] = 0xffff'0000'0000'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'0000'0000'0000ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));
}

TEST(TESTSUITE, AndImmNegFlag32a) {
  static const uint32_t code[] = {
      0x721f'7862,  // ands w2, w3, #0xffff'fffe
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0x0;
  test.state()->cpu.x[3] = 0xffff'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'0000ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));
}

TEST(TESTSUITE, AndImmNegFlag32b) {
  static const uint32_t code[] = {
      0x721f'7862,  // ands w2, w3, #0xffff'fffe
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0xbbaa'aabb;
  test.state()->cpu.x[3] = 0xffff'0000'0000'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, AndImmSP) {
  static const uint32_t code[] = {
      0x927c'ec1f,  // and sp, x0, #0xffff'ffff.fffffff0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'00ffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.sp, 0xffff'0000'ffff'00f0ULL);
}

TEST(TESTSUITE, OrImm) {
  static const uint32_t code[] = {
      0xb200'd848,  // orr x8, x2, #0x7f7f'7f7f.7f7f7f7f
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 0xffff'0000'ffff'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0xffff'7f7f'ffff'7f7fULL);
}

TEST(TESTSUITE, XorImm) {
  static const uint32_t code[] = {
      0xd24c'2821,  // eor x1, x1, #0x7ff0'0000.00000000
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x800f'0000'ffff'0000ULL);
}

TEST(TESTSUITE, AndImmAllFlagsClear) {
  static const uint32_t code[] = {
      0xf240'0c07,  // ands x7, x0, #0xf
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0008ULL;
  test.state()->cpu.x[7] = 0xffff'0000'ffff'0008ULL;
  test.SetNZCV(0b1111);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], 0x8ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0000));
}

TEST(TESTSUITE, AndImmZeroFlagSet) {
  static const uint32_t code[] = {
      0xf240'0c07,  // ands x7, x0, #0xf
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  test.SetNZCV(0b0000);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], 0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, AndImmNegativeFlagSet) {
  static const uint32_t code[] = {
      0xf245'8364,  // ands x4, x27, #0xf800'0000.0fffffff
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[27] = 0xffff'0000'ffff'0008ULL;
  test.SetNZCV(0b0000);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[4], 0xf800'0000'0fff'0008ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));
}

TEST(TESTSUITE, AndImm32) {
  static const uint32_t code[] = {
      0x1202'e401,  // and w1, w0, #0xcccc'cccc
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'00ffULL;
  test.state()->cpu.x[1] = 0xffff'0000'ffff'00ffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xcccc'00ccULL);
}

TEST(TESTSUITE, TestImm) {
  static const uint32_t code[] = {
      0xf250'3c1f,  // ands xzr, x0, #0xffff'0000'0000'0000 <=> tst x0, #0xffff'0000'0000'0000
  };

  TestHarness test(code, sizeof(code));

  test.state()->cpu.x[0] = 0x0fff'0000'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0000));

  test.Reset();

  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));

  test.Reset();

  test.state()->cpu.x[0] = 0x0000'bbbb'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, AndRegister) {
  static const uint32_t code[] = {
      0x8a01'02f7,  // and  x23, x23, x1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = kTestValue64;
  test.state()->cpu.x[23] = 0xff00'ff00'ff00'ff00ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[23], 0xff00'0000'ff00'0000ULL);
}

TEST(TESTSUITE, AndReg) {
  static const uint32_t code[] = {
      0x8a04'0062,  // and x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1111);
  test.state()->cpu.x[2] = 0x0;
  test.state()->cpu.x[3] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[4] = 0xff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xccULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, AndReg32) {
  static const uint32_t code[] = {
      0x0a04'0062,  // and w2, w3, w4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1111);
  test.state()->cpu.x[2] = 0xffffULL;
  test.state()->cpu.x[3] = 0xffff'eeee'1111'ccccULL;
  test.state()->cpu.x[4] = 0xaa00'cccc'3223'ffffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x1001'ccccULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, AndRegZeroFlag) {
  static const uint32_t code[] = {
      0x6a04'0062,  // ands w2, w3, w4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0x0;
  test.state()->cpu.x[3] = 0xaabb'0000;
  test.state()->cpu.x[4] = 0x0000'bbaa;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, AndRegNegFlag) {
  static const uint32_t code[] = {
      0xea04'0062,  // ands x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0x33;
  test.state()->cpu.x[3] = 0xffff'0000'0000'0000;
  test.state()->cpu.x[4] = 0xffff'0000'0000'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'0000'0000'0000ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));
}

TEST(TESTSUITE, AndRegNegFlag32) {
  static const uint32_t code[] = {
      0x6a04'0062,  // ands w2, w3, w4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0x33;
  test.state()->cpu.x[3] = 0xf'8000'0000;
  test.state()->cpu.x[4] = 0xf'8000'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x8000'0000ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));
}

TEST(TESTSUITE, TestRegister) {
  static const uint32_t code[] = {
      0xea04'007f,  // ands xzr, x3, x4 <=> tst x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[4] = 0xffff'0000'0000'0000ULL;

  test.state()->cpu.x[3] = 0x0fff'0000'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0000));

  test.Reset();

  test.state()->cpu.x[3] = 0xffff'0000'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));

  test.Reset();

  test.state()->cpu.x[3] = 0x0000'bbbb'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, TestInvertedRegister) {
  static const uint32_t code[] = {
      0xea24'007f,  // bics xzr, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[4] = ~0xffff'0000'0000'0000ULL;

  test.state()->cpu.x[3] = 0x0fff'0000'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0000));

  test.Reset();

  test.state()->cpu.x[3] = 0xffff'0000'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));

  test.Reset();

  test.state()->cpu.x[3] = 0x0000'bbbb'ffff'0000ULL;
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0100));
}

TEST(TESTSUITE, OrRegister) {
  static const uint32_t code[] = {
      0xaa01'02f7,  // or  x23, x23, x1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = kTestValue64;
  test.state()->cpu.x[23] = 0xff00'ff00'ff00'ff00ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[23], 0xffff'ff00'ffff'ff00ULL);
}

TEST(TESTSUITE, XorRegister) {
  static const uint32_t code[] = {
      0xca01'02f7,  // xor  x23, x23, x1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = kTestValue64;
  test.state()->cpu.x[23] = 0xff00'ff00'ff00'ff00ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[23], 0x00ff'ff00'00ff'ff00ULL);
}

TEST(TESTSUITE, OrrReg) {
  static const uint32_t code[] = {
      0xaa04'0062,  // orr x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1111);
  test.state()->cpu.x[2] = 0xbafeULL;
  test.state()->cpu.x[3] = 0xf0f0'f0f0'f0f0'f0f0ULL;
  test.state()->cpu.x[4] = 0x0f0f'0f0f'0f0f'0f0fULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffff'ffff'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, OrrReg32) {
  static const uint32_t code[] = {
      0x2a04'0062,  // orr w2, w3, w4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1111);
  test.state()->cpu.x[2] = 0xbafeULL;
  test.state()->cpu.x[3] = 0xf0f0'f0f0'f0f0'f0f0ULL;
  test.state()->cpu.x[4] = 0x0f0f'0f0f'0f0f'0f0fULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, EorReg) {
  static const uint32_t code[] = {
      0xca04'0062,  // eor x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 0x0;
  test.state()->cpu.x[3] = 0xffff'eeee'dddd'cccc;
  test.state()->cpu.x[4] = 0xffff'0000'ffff'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xeeee'2222'ccccULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, EorReg32) {
  static const uint32_t code[] = {
      0x4a04'0062,  // eor w2, w3, w4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 0xabcdULL;
  test.state()->cpu.x[3] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[4] = 0xffff'0000'ffff'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x2222'ccccULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, OrZeroRegFirstArgIsMovAlias) {
  static const uint32_t code[] = {
      0xaa00'03e5,  // orr x5, xzr, x0 <=> mov x5, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0xdead'beef;
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[5], kTestValue64);
}

TEST(TESTSUITE, OrZeroRegFirstArgIsMovAlias32) {
  static const uint32_t code[] = {
      0x2a00'03e5,  // orr w5, wzr, w0 <=> mov w5, w0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0xdead'beef;
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[5], 0xdddd'ccccULL);
}

TEST(TESTSUITE, OrZeroRegSecondArgIsMovAlias) {
  static const uint32_t code[] = {
      0xaa1f'0005,  // orr x5, x0, xzr <=> mov x5, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0xdead'beef;
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[5], kTestValue64);
}

TEST(TESTSUITE, OrZeroRegSecondArgIsMovAlias32) {
  static const uint32_t code[] = {
      0x2a1f'0005,  // orr w5, w0, wzr <=> mov w5, w0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0xdead'beef;
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[5], 0xdddd'ccccULL);
}

TEST(TESTSUITE, AndRegLSL64) {
  static const uint32_t code[] = {
      0x8a04'0862,  // and x2, x3, x4, LSL #2
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 0x0;
  test.state()->cpu.x[3] = 0xff00;
  test.state()->cpu.x[4] = 0xff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x300ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, AndRegASR64) {
  static const uint32_t code[] = {
      0x8a84'fc62,  // and x2, x3, x4, ASR #63
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1001);
  test.state()->cpu.x[2] = 0xbafe;
  test.state()->cpu.x[3] = 0xdead'beef'dead'beef;
  test.state()->cpu.x[4] = 0x8000'0000'0000'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xdead'beef'dead'beefULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));
}

TEST(TESTSUITE, AndRegASR32a) {
  static const uint32_t code[] = {
      0x0a84'0c62,  // and w2, w3, w4, ASR #3
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1111);
  test.state()->cpu.x[2] = 0xbafe;
  test.state()->cpu.x[3] = 0xbbbb'bbbb'cccc'cccc;
  test.state()->cpu.x[4] = 0x7'8000'0078;
  // w4 = 0b 0111 1000 0000 0000 0000 0000 0000 0111 1000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xc000'000cULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, AndRegASR32b) {
  static const uint32_t code[] = {
      0x0a84'0c62,  // and w2, w3, w4, ASR #3
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1111);
  test.state()->cpu.x[2] = 0xbafe;
  test.state()->cpu.x[3] = 0xffff'ffff'ffff'ffff;
  test.state()->cpu.x[4] = 0x8000'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xf000'0000ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, AndRegisterLogicalShiftRight) {
  static const uint32_t code[] = {
      0x8a44'090f,  // and x15, x8, x4, lsr #2
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xfULL;
  test.state()->cpu.x[4] = 4ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 1ULL);
}

TEST(TESTSUITE, AndRegisterLogicalShiftRight32) {
  static const uint32_t code[] = {
      0x0a44'090f,  // and w15, w8, w4, lsr #2
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'0000ULL;
  // Make sure not to mix in bits higher than 31.
  test.state()->cpu.x[4] = 0x3'fffc'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0x3fff'0000ULL);
}

TEST(TESTSUITE, AndRegisterRotateRight) {
  static const uint32_t code[] = {
      0x8ac9'0e61,  // and x1, x19, x9, ror #3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = -1ULL;
  test.state()->cpu.x[9] = 0x11ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x2000'0000'0000'0002ULL);
}

TEST(TESTSUITE, AndRegisterRotateRight32) {
  static const uint32_t code[] = {
      0x0ac9'0e61,  // and w1, w19, w9, ror #3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = -1ULL;
  test.state()->cpu.x[9] = 0x11ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x2000'0002ULL);
}

TEST(TESTSUITE, AndRegLSL32) {
  static const uint32_t code[] = {
      0x0a04'2062,  // and w2, w3, w4, LSL #8
  };
  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 0xbb;
  test.state()->cpu.x[3] = 0xff'0000'0000;
  test.state()->cpu.x[4] = 0xff00'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, AndRegLSR32) {
  static const uint32_t code[] = {
      0x0a04'4062,  // and w2, w3, w4, LSR #16
  };
  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 0xaabb'ccdd;
  test.state()->cpu.x[3] = 0xffff'0000;
  test.state()->cpu.x[4] = 0xff'ffff'0000;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, AndRegLSR64) {
  static const uint32_t code[] = {
      0x8a44'0462,  // and x2, x3, x4, LSR #1
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 0xdada;
  test.state()->cpu.x[3] = 0x8000'0000'0f0f'0f0f;
  test.state()->cpu.x[4] = 0xffff'ffff'ffff'ffff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xf0f'0f0fULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, ArithmeticShiftRight) {
  static const uint32_t code[] = {
      0x9ac0'2822,  //  asr x2, x1, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 4ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'f000'0fff'f000ULL);

  test.Reset();
  // Must be taken mod 64
  test.state()->cpu.x[0] = 64 + 8ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ff00'00ff'ff00ULL);
}

TEST(TESTSUITE, ArithmeticShiftRight32) {
  static const uint32_t code[] = {
      0x1ac0'2822,  //  asr w2, w1, w0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xfafa'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 4ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'f000ULL);

  test.Reset();
  // Must be taken mod 32
  test.state()->cpu.x[0] = 32 + 8ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ff00ULL);
}

TEST(TESTSUITE, LogicalShiftLeft) {
  static const uint32_t code[] = {
      0x9ac0'2022,  //  lsl x2, x1, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 4ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xfff0'000f'fff0'0000ULL);
}

TEST(TESTSUITE, LogicalShiftLeft32) {
  static const uint32_t code[] = {
      0x1ac0'2022,  //  lsl w2, w1, w0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'bbbb'ffff'0000ULL;
  test.state()->cpu.x[0] = 8ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xff00'0000ULL);
}

TEST(TESTSUITE, LogicalShiftRight) {
  static const uint32_t code[] = {
      0x9ac0'2422,  //  lsr x2, x1, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 4ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0fff'f000'0fff'f000ULL);
}

TEST(TESTSUITE, LogicalShiftRight32) {
  static const uint32_t code[] = {
      0x1ac0'2422,  //  lsr w2, w1, w0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 4ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0fff'f000ULL);
}

TEST(TESTSUITE, RotateRight) {
  static const uint32_t code[] = {
      0x9ac0'2c22,  //  ror x2, x1, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 24ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xff00'00ff'ff00'00ffULL);
}

TEST(TESTSUITE, RotateRight32) {
  static const uint32_t code[] = {
      0x1ac0'2c22,  //  ror w2, w1, w0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'000aULL;
  test.state()->cpu.x[0] = 12ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x00af'fff0ULL);
}

// Since semantics player only calls ShiftByImm if used together with add/logical, perform an
// addition with 0.
TEST(TESTSUITE, ArithmeticShiftRightImm) {
  static const uint32_t code[] = {
      0x8b80'4022,  //  add x2, x1, x0, asr #16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'aaaa'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffff'aaaa'ffffULL);
}

TEST(TESTSUITE, ArithmeticShiftRightImm32) {
  static const uint32_t code[] = {
      0x0b80'2022,  //  add w2, w1, w0, asr #8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'bbbb'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ff00U);
}

TEST(TESTSUITE, LogicalShiftLeftImm) {
  static const uint32_t code[] = {
      0x8b00'4022,  //  add x2, x1, x0, lsl #16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'aaaa'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xaaaa'ffff'0000'0000ULL);
}

TEST(TESTSUITE, LogicalShiftLeftImm32) {
  static const uint32_t code[] = {
      0x0b00'2022,  //  add w2, w1, w0, lsl #8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'bbbb'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xff00'0000ULL);
}

TEST(TESTSUITE, LogicalShiftRightImm) {
  static const uint32_t code[] = {
      0x8b40'4022,  //  add x2, x1, x0, lsr #16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'aaaa'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x0000'ffff'aaaa'ffffULL);
}

TEST(TESTSUITE, LogicalShiftRightImm32) {
  static const uint32_t code[] = {
      0x0b40'2022,  //  add w2, w1, w0, lsr #8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'bbbb'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x00ff'ff00U);
}

// Add and rotate is not supported, so we have to do bitwise OR with 0.
TEST(TESTSUITE, RotateRightImm) {
  static const uint32_t code[] = {
      0xaac0'6022,  //  orr x2, x1, x0, ror #24
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xff00'00ff'ff00'00ffULL);
}

TEST(TESTSUITE, RotateRightImm32) {
  static const uint32_t code[] = {
      0x2ac0'5022,  //  orr w2, w1, w0, ror #20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[1] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xf000'0fffULL);
}

TEST(TESTSUITE, ConditionalSelect) {
  static const uint32_t code[] = {
      0x9a80'0022,  // csel x2, x1, x0, eq
  };

  TestHarness test(code, sizeof(code));
  // 'eq' is true.
  test.SetNZCV(0b0100);
  test.state()->cpu.x[1] = 0xffffULL;
  test.state()->cpu.x[0] = 0xeeeeULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffffULL);

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xeeeeULL);
}

TEST(TESTSUITE, ConditionalSelectHigher) {
  static const uint32_t code[] = {
      0x9a80'8022,  // csel x2, x1, x0, hi
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0010);
  test.state()->cpu.x[1] = 0xffffULL;
  test.state()->cpu.x[0] = 0xeeeeULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffffULL);

  test.Reset();
  test.SetNZCV(0b1101);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xeeeeULL);

  test.Reset();
  test.SetNZCV(0b0001);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xeeeeULL);

  test.Reset();
  test.SetNZCV(0b1110);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xeeeeULL);
}

TEST(TESTSUITE, Csel0) {
  static const uint32_t code[] = {
      0x9a84'6062,  // csel x2, x3, x4, vs
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0001);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 1;
  test.state()->cpu.x[4] = 2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 1ULL);
}

TEST(TESTSUITE, Csel1) {
  static const uint32_t code[] = {
      0x9a84'0062,  // csel x2, x3, x4, eq
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 1;
  test.state()->cpu.x[4] = 2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 2ULL);
}

// Copied from interpreter tests
TEST(TESTSUITE, Csel2) {
  static const uint32_t code[] = {
      0x9a84'2062,  // csel x2, x3, x4, cs
  };

  TestHarness test(code, sizeof(code));

  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 1;
  test.state()->cpu.x[4] = 2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 2ULL);
}

TEST(TESTSUITE, Csel3) {
  static const uint32_t code[] = {
      0x9a84'e062,  // csel x2, x3, x4, al
  };

  TestHarness test(code, sizeof(code));

  test.SetNZCV(0b0010);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 1;
  test.state()->cpu.x[4] = 2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 1ULL);
}

TEST(TESTSUITE, Csel4) {
  static const uint32_t code[] = {
      0x9a84'5062,  // csel x2, x3, x4, pl
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 1;
  test.state()->cpu.x[4] = 2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 1ULL);
}

TEST(TESTSUITE, Csel5) {
  static const uint32_t code[] = {
      0x9a84'8062,  // csel x2, x3, x4, hi
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0110);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 0x1;
  test.state()->cpu.x[4] = 0x2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 2ULL);
}

TEST(TESTSUITE, Csel6) {
  static const uint32_t code[] = {
      0x9a84'a062,  // csel x2, x3, x4, ge
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0110);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 0x1;
  test.state()->cpu.x[4] = 0x2;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 1ULL);
}

TEST(TESTSUITE, ConditionalSelectIncrement) {
  static const uint32_t code[] = {
      0x9a80'0422,  // csinc x2, x1, x0, eq
  };

  TestHarness test(code, sizeof(code));
  // 'eq' is true.
  test.SetNZCV(0b0100);
  test.state()->cpu.x[1] = 0xffffULL;
  test.state()->cpu.x[0] = 0xeeeeULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffffULL);

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xeeefULL);
}

TEST(TESTSUITE, ConditionalSelectNegate) {
  static const uint32_t code[] = {
      0xda80'0022,  // csneg x2, x1, x0, eq
  };

  TestHarness test(code, sizeof(code));
  // 'eq' is true.
  test.SetNZCV(0b0100);
  test.state()->cpu.x[1] = 0xffffULL;
  test.state()->cpu.x[0] = 0xeeeeULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffffULL);

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffff'ffff'1111ULL);
}

TEST(TESTSUITE, CondBranchEq) {
  static const uint32_t code[] = {
      0x5400'0040,  // b.eq pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchCarry) {
  static const uint32_t code[] = {
      0x5400'0042,  // b.cs pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchNoCarry) {
  static const uint32_t code[] = {
      0x5400'0043,  // b.cc pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
}

TEST(TESTSUITE, CondBranchNegative) {
  static const uint32_t code[] = {
      0x5400'0044,  // b.mi pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchPositiveOrZero) {
  static const uint32_t code[] = {
      0x5400'0045,  // b.pl pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0001);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0110);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
}

TEST(TESTSUITE, CondBranchOverflow) {
  static const uint32_t code[] = {
      0x5400'0046,  // b.vs pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0001);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchNoOverflow) {
  static const uint32_t code[] = {
      0x5400'0047,  // b.vc pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0001);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
}

TEST(TESTSUITE, CondBranchUnsignedHigher) {
  static const uint32_t code[] = {
      0x5400'0048,  // b.hi pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b1000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0101);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchUnsignedLowerOrSame) {
  static const uint32_t code[] = {
      0x5400'0049,  // b.ls pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b1000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0101);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b1111);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
}

TEST(TESTSUITE, CondBranchSignedGreaterThanOrEqual) {
  static const uint32_t code[] = {
      0x5400'004a,  // b.ge pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  // negative == overflow
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b1001);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0001);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b1000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchSignedLowerThan) {
  static const uint32_t code[] = {
      0x5400'004b,  // b.lt pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  // negative != overflow
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b1001);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0111);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b1010);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
}

TEST(TESTSUITE, CondBranchSignedGreaterThan) {
  static const uint32_t code[] = {
      0x5400'004c,  // b.gt pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  // !zero && negative == overflow
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b1000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchSignedLowerThanOrEqual) {
  static const uint32_t code[] = {
      0x5400'004d,  // b.le pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  // zero || negative != overflow
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));

  test.Reset();
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0101);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b1110);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
}

TEST(TESTSUITE, CondBranchAlways) {
  static const uint32_t code[] = {
      0x5400'004e,  // b.al pc + 0x8
  };

  TestHarness test(code, sizeof(code));

  for (unsigned cond = 0; cond < 16; cond++) {
    test.Reset();
    test.SetNZCV(cond);
    EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
  }
}

TEST(TESTSUITE, CondBranchAlways2) {
  static const uint32_t code[] = {
      0x5400'004f,  // b.nv pc + 0x8
  };

  TestHarness test(code, sizeof(code));

  for (unsigned cond = 0; cond < 16; cond++) {
    test.Reset();
    test.SetNZCV(cond);
    EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));
  }
}

TEST(TESTSUITE, CondBranchNotEq) {
  static const uint32_t code[] = {
      0x5400'0041,  // b.ne pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CondBranchEqSignExtend) {
  static const uint32_t code[] = {
      0x54ff'ffe0,  // b.eq pc - 0x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) - 4));
}

TEST(TESTSUITE, BitfieldLogicalShiftRight) {
  static const uint32_t code[] = {
      0xd344'fc01,  // lsr x1, x0, #4 (alias for ubfm x1, x0, #4, #63)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0fff'f000'0fff'f000ULL);
}

TEST(TESTSUITE, BitfieldLogicalShiftRight32) {
  static const uint32_t code[] = {
      0x5304'7c01,  // lsr w1, w0, #4 (alias for ubfm w1, w0, #4, #31)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0fff'f000ULL);
}

TEST(TESTSUITE, BitfieldLogicalShiftLeft) {
  static const uint32_t code[] = {
      0xd37c'ec01,  // lsl x1, x0, #4 (alias for ubfm x1, x0, #60, #59)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xfff0'000f'fff0'0000ULL);
}

TEST(TESTSUITE, BitfieldLogicalShiftLeft32) {
  static const uint32_t code[] = {
      0x531c'6c01,  // lsl w1, w0, #4 (alias for ubfm w1, w0, #28, #27)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xfff0'0000ULL);
}

TEST(TESTSUITE, BitfieldClear) {
  static const uint32_t code[] = {
      0xb35c'6c0b,  // bfi x11, x0, #36, #28
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[11] = 0x0000'0000'0000'8000ULL;
  test.state()->cpu.x[0] = 0x0ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[11], 0x0000'0000'0000'8000ULL);
}

TEST(TESTSUITE, BitfieldLeftInsert) {
  static const uint32_t code[] = {
      0xb368'3c20,  // bfm x0, x1, #40, #15
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0x1101'0446'8232'5271ULL;
  test.state()->cpu.x[1] = 0x3895'2286'8478'abcdULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x1101'04ab'cd32'5271ULL);
}

TEST(TESTSUITE, BitfieldRightInsert) {
  static const uint32_t code[] = {
      0xb344'9c20,  // bfm x0, x1, #4, #39
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0x1668'0396'2657'9787ULL;
  test.state()->cpu.x[1] = 0x3276'5618'0937'7344ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x1668'0391'8093'7734ULL);
}

TEST(TESTSUITE, BitfieldInsert) {
  static const uint32_t code[] = {
      0xb348'0c01,  // bfi x1, x0, #56, #4 (alias for bfm x1, x0, #8, #3)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 0xdddd'cccc'bbbb'aaaaULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xfaff'0000'ffff'0000ULL);
}

TEST(TESTSUITE, BitfieldInsert32) {
  static const uint32_t code[] = {
      0x3308'0c01,  // bfi w1, w0, #24, #4 (alias for bfm 21, 20, #8, #3)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 0xdddd'cccc'bbbb'aaaaULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xfaff'0000ULL);
}

TEST(TESTSUITE, BitfieldExtractAndInsertAtLowEnd) {
  static const uint32_t code[] = {
      0xb348'3c01,  // bfxil x1, x0, #8, #8 (alias for bfm x1, x0, #8, #15)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 0xdddd'cccc'bbbb'1234ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'0000'ffff'0012ULL);
}

TEST(TESTSUITE, BitfieldExtractAndInsertAtLowEnd32) {
  static const uint32_t code[] = {
      0x3308'3c01,  // bfxil w1, w0, #8, #8 (alias for bfm w1, w0, #8, #15)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'0000'ffff'0000ULL;
  test.state()->cpu.x[0] = 0xdddd'cccc'bbbb'1234ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'0012ULL);
}

TEST(TESTSUITE, BitfieldArithmeticShiftRight) {
  static const uint32_t code[] = {
      0x9348'fc01,  // asr x1, x0, #8 (alias for sbfm x1, x0, #8, #63)
  };

  TestHarness test(code, sizeof(code));
  // Sign bit is set.
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'ff00'00ff'ff00ULL);

  test.Reset();
  // Sign bit is clear.
  test.state()->cpu.x[0] = 0x00ff'ff00'00ff'ff00ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0000'ffff'0000'ffffULL);
}

TEST(TESTSUITE, BitfieldArithmeticShiftRight32) {
  static const uint32_t code[] = {
      0x1308'7c01,  // asr w1, w0, #8 (alias for sbfm w1, w0, #8, #31)
  };

  TestHarness test(code, sizeof(code));
  // Sign bit is set.
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'ff00ULL);

  test.Reset();
  // Sign bit is clear.
  test.state()->cpu.x[0] = 0xffff'00ff'0000'ffffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffULL);
}

TEST(TESTSUITE, BitfieldSignedExtend) {
  static const uint32_t code[] = {
      0x9340'2c01,  // sbfx x1, x0, #0, #12 (alias for sbfm x1, x0, #0, #11)
  };

  TestHarness test(code, sizeof(code));
  // Sign bit is set.
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0f0fULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'ffff'ffff'ff0fULL);

  test.Reset();
  // Sign bit is clear.
  test.state()->cpu.x[0] = 0xffff'0000'ffff'00ffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0ffULL);
}

TEST(TESTSUITE, BitfieldSignedExtend32) {
  static const uint32_t code[] = {
      0x1300'2c01,  // sbfx w1, w0, #0, #12 (alias for sbfm w1, w0, #0, #11)
  };

  TestHarness test(code, sizeof(code));
  // Sign bit is set.
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0f0fULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xffff'ff0fULL);

  test.Reset();
  // Sign bit is clear.
  test.state()->cpu.x[0] = 0xffff'0000'ffff'00ffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0ffULL);
}

TEST(TESTSUITE, BitfieldInsertInZeroAndSignExtend) {
  static const uint32_t code[] = {
      0x9348'0c01,  // sbfiz x1, x0, #56, #4 (alias for sbfm x1, x0, #8, #3)
  };

  TestHarness test(code, sizeof(code));
  // Sign bit is set.
  test.state()->cpu.x[0] = 0xaULL;
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xfa00'0000'0000'0000ULL);

  test.Reset();
  // Sign bit is clear.
  test.state()->cpu.x[0] = 0x7ULL;
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0700'0000'0000'0000ULL);
}

TEST(TESTSUITE, BitfieldInsertInZeroAndSignExtend32) {
  static const uint32_t code[] = {
      0x1308'0c01,  // sbfiz w1, w0, #24, #4 (alias for sbfm 21, 20, #8, #3)
  };

  TestHarness test(code, sizeof(code));
  // Sign bit is set.
  test.state()->cpu.x[0] = 0xaULL;
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xfa00'0000ULL);

  test.Reset();
  // Sign bit is clear.
  test.state()->cpu.x[0] = 0x7ULL;
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0700'0000ULL);
}

TEST(TESTSUITE, BitfieldInsertInZero) {
  static const uint32_t code[] = {
      0xd348'0c01,  // ubfiz x1, x0, #56, #4 (alias for ubfm x1, x0, #8, #3)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xaULL;
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0a00'0000'0000'0000ULL);
}

TEST(TESTSUITE, BitfieldInsertInZero32) {
  static const uint32_t code[] = {
      0x5308'0c01,  // ubfiz w1, w0, #24, #4 (alias for ubfm w1, w0, #8, #3)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xaULL;
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x0a00'0000ULL);
}

TEST(TESTSUITE, BitfieldUnsignedExtend) {
  static const uint32_t code[] = {
      0xd340'2c01,  // ubfx x1, x0, #0, #12 (alias for ubfm x1, x0, #0, #11)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0f0fULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xf0fULL);
}

TEST(TESTSUITE, BitfieldUnsignedExtend32) {
  static const uint32_t code[] = {
      0x5300'2c01,  // ubfx w1, w0, #0, #12 (alias for ubfm w1, w0, #0, #11)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'0f0fULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xf0fULL);
}

TEST(TESTSUITE, AddImmCarry) {
  static const uint32_t code[] = {
      0xb100'0862,  // adds x2, x3, #2
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0xffff'ffff'ffff'fffeULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0110));
}

TEST(TESTSUITE, AddImmOverflow) {
  static const uint32_t code[] = {
      0xb100'0862,  // adds x2, x3, #2
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0x7fff'ffff'ffff'ffffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x8000'0000'0000'0001ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));
}

TEST(TESTSUITE, SubImmNotBorrow) {
  static const uint32_t code[] = {
      0xb100'0462,  // subs x2, x3, #-1
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0xffff'ffff'ffff'ffffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0ULL);
  // #-1 is interpreted as max<uint64>, so there is no carry (no borrow).
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0100));
}

TEST(TESTSUITE, SubImmCarry) {
  static const uint32_t code[] = {
      0xf100'0462,  // subs x2, x3, #1
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffff'ffff'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b1010));
}

TEST(TESTSUITE, SubImmOverflow) {
  static const uint32_t code[] = {
      0xf100'0462,  // subs x2, x3, #1
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0x8000'0000'0000'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x7fff'ffff'ffff'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0001));
}

TEST(TESTSUITE, CmpImmCarry) {
  static const uint32_t code[] = {
      0xf100'047f,  // subs xzr, x3, #1 <=> cmp x3, #1
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[3] = 0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b1010));
}

TEST(TESTSUITE, CmpImmOverflow) {
  static const uint32_t code[] = {
      0xb100'047f,  // subs xzr, x3, #-1 <=> cmp x3, #-1
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[3] = 0x7fff'ffff'ffff'ffffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));
}

TEST(TESTSUITE, CmpnImmCarry) {
  static const uint32_t code[] = {
      0xb100'087f,  // adds xzr, x3, #2 <=> cmpn x3, #2
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[3] = 0xffff'ffff'ffff'fffeULL;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0110));
}

TEST(TESTSUITE, CmpnImmOverflow) {
  static const uint32_t code[] = {
      0xb100'087f,  // adds xzr, x3, #2 <=> cmpn, x3, #2
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[3] = 0x7fff'ffff'ffff'ffffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));
}

TEST(TESTSUITE, AddImmLeftShift) {
  static const uint32_t code[] = {
      0x9143'c062,  // add x2, x3, #0xf0, lsl 12
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0xffff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xf'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, AddExtendedReg1) {
  static const uint32_t code[] = {
      0x8b24'd062,  // add x2, x3, w4, sxtw #4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0xf;
  test.state()->cpu.x[4] = 0xffff'ffff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffff'ffff'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, AddExtendedReg2) {
  static const uint32_t code[] = {
      0x8b24'4862,  // add x2, x3, w4, uxtw #2
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0x1;
  test.state()->cpu.x[4] = 0xffff'ffff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x3'ffff'fffdULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, AddShiftedRegister) {
  static const uint32_t code[] = {
      0x8b04'3062,  // add x2, x3, x4, lsl #12
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0xdef;
  test.state()->cpu.x[4] = 0xabc;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xab'cdefULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, SubExtendedRegister) {
  static const uint32_t code[] = {
      0xcb24'd062,  // sub x2, x3, w4, sxtw #4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0x0;
  test.state()->cpu.x[4] = 0xffff'ffff;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x10ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, CmpExtendedRegister) {
  static const uint32_t code[] = {
      0xeb2b'605f,  // subs xzr, x2, x11, sxtx <=> cmp x2, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 42;
  test.state()->cpu.x[11] = 21;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0000));
}

TEST(TESTSUITE, SubShiftedRegister) {
  static const uint32_t code[] = {
      0xcb84'2062,  // sub x2, x3, x4, asr #8
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0101);
  test.state()->cpu.x[2] = 7;
  test.state()->cpu.x[3] = 0x0;
  test.state()->cpu.x[4] = 0xffff'ffff'ffff'ff00;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x1ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, CmpShiftedRegister) {
  static const uint32_t code[] = {
      0xeb8b'005f,  // subs xzr, x2, x11, asr #0 <=> cmp x2, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 42;
  test.state()->cpu.x[11] = 21;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0000));
}

TEST(TESTSUITE, AddWithCarry) {
  static const uint32_t code[] = {
      0x9a00'0022,  // adc x2, x1, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;
  test.state()->cpu.x[1] = 42;
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], kTestValue64 + 42 + 1);

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], kTestValue64 + 42);
}

TEST(TESTSUITE, AddWithCarry2) {
  static const uint32_t code[] = {
      0xba04'0062,  // adc x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[3] = 0x7fff'ffff'ffff'ffff;
  test.state()->cpu.x[4] = 0;
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x8000'0000'0000'0000ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x7fff'ffff'ffff'ffffULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0000));
}

TEST(TESTSUITE, AddWithCarry32) {
  static const uint32_t code[] = {
      0x3a00'0022,  // adcs w2, w1, w0
  };

  TestHarness test(code, sizeof(code));
  // 64-bit vs 32-bit addition have opposite carry and overflow.
  test.state()->cpu.x[0] = 0xffff'eeee'7777'ccccULL;
  test.state()->cpu.x[1] = 0x4444'3333'2222'1111ULL;
  test.SetNZCV(0b0010);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x9999'dddeULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));

  test.Reset();
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x9999'ddddULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));
}

TEST(TESTSUITE, SubsWithCarry) {
  static const uint32_t code[] = {
      0xfa04'0062,  // sbcs x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 42;
  test.state()->cpu.x[4] = 41;
  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[2], 0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0110));
}

TEST(TESTSUITE, SubsWithCarry2) {
  static const uint32_t code[] = {
      0xfa04'0062,  // sbcs x2, x3, x4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0010);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 42;
  test.state()->cpu.x[4] = 42;
  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[2], 0ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0110));
}

TEST(TESTSUITE, SubsWithCarry32) {
  static const uint32_t code[] = {
      0x7a04'0062,  // sbcs w2, w3, w4
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b0000);
  test.state()->cpu.x[2] = 0;
  test.state()->cpu.x[3] = 0;
  test.state()->cpu.x[4] = 42;
  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffd5ULL);
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1000));
}

TEST(TESTSUITE, AddRegisterLogicalShiftLeft) {
  static const uint32_t code[] = {
      0x8b0c'0c00,  // add x0, x0, x12, lsl #3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;
  test.state()->cpu.x[12] = 1ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64 + 8);
}

TEST(TESTSUITE, AddRegisterLogicalShiftRight) {
  static const uint32_t code[] = {
      0x8b4c'0c00,  // add x0, x0, x12, lsr #3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;
  test.state()->cpu.x[12] = 0b1000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64 + 1);
}

TEST(TESTSUITE, AddRegisterArithmeticShiftRight) {
  static const uint32_t code[] = {
      0x8b8c'0c00,  // add x0, x0, x12, asr #3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;
  test.state()->cpu.x[12] = -8ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64 - 1);
}

TEST(TESTSUITE, AddRegister32ArithmeticShiftRight) {
  static const uint32_t code[] = {
      0x0b8c'0c00,  // add w0, w0, w12, asr #3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[12] = 0xffff'fff8ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xdddd'cccbULL);
}

TEST(TESTSUITE, AddRegisterUnsignedExtend8) {
  static const uint32_t code[] = {
      0x8b20'0022,  // add x2, x1, w0, uxtb
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'ccddULL);
}

TEST(TESTSUITE, AddRegisterUnsignedExtend16) {
  static const uint32_t code[] = {
      0x8b20'2022,  // add x2, x1, w0, uxth
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'ddddULL);
}

TEST(TESTSUITE, AddRegisterUnsignedExtend32) {
  static const uint32_t code[] = {
      0x8b20'4022,  // add x2, x1, w0, uxtw
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'ffff'ddddULL);
}

TEST(TESTSUITE, AddRegisterUnsignedExtend64) {
  static const uint32_t code[] = {
      0x8b20'6022,  // add x2, x1, x0, uxtx
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0x9999'8888'7777'6666ULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xdddd'bbbb'9999'7777ULL);
}

TEST(TESTSUITE, AddRegisterUnsignedExtend64AndShiftLeft) {
  static const uint32_t code[] = {
      0x8b20'7022,  // add x2, x1, x0, uxtx #4
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0x9999'8888'7777'6666ULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xdddc'bbba'9998'7776ULL);
}

TEST(TESTSUITE, AddRegisterSignedExtend8) {
  static const uint32_t code[] = {
      0x8b20'8022,  // add x2, x1, w0, sxtb
  };

  TestHarness test(code, sizeof(code));
  // negative extension
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'11ffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'cccbULL);

  // positive extension
  test.Reset();
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'ccddULL);
}

TEST(TESTSUITE, AddRegisterSignedExtend16) {
  static const uint32_t code[] = {
      0x8b20'a022,  // add x2, x1, w0, sxth
  };

  TestHarness test(code, sizeof(code));
  // negative extension
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'2222'ffffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'cccbULL);

  // positive extension
  test.Reset();
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'ddddULL);
}

TEST(TESTSUITE, AddRegisterSignedExtend32) {
  static const uint32_t code[] = {
      0x8b20'c022,  // add x2, x1, w0, sxtw
  };

  TestHarness test(code, sizeof(code));

  // negative extension
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'ffff'ffffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'cccbULL);

  // positive extension
  test.Reset();
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'ffff'ddddULL);
}

TEST(TESTSUITE, AddRegisterSignedExtend64) {
  static const uint32_t code[] = {
      0x8b20'e022,  // add x2, x1, w0, sxtx
  };

  TestHarness test(code, sizeof(code));

  // negative extension
  test.state()->cpu.x[1] = 0x9999'8888'7777'6666ULL;
  test.state()->cpu.x[0] = 0xffff'ffff'ffff'ffffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x9999'8888'7777'6665ULL);

  // positive extension
  test.Reset();
  test.state()->cpu.x[0] = 0x4444'3333'2222'1111ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xdddd'bbbb'9999'7777ULL);
}

TEST(TESTSUITE, AddRegisterSignedExtend32AndShiftLeft) {
  static const uint32_t code[] = {
      0x8b20'd022,  // add x2, x1, w0, sxtw #4
  };

  TestHarness test(code, sizeof(code));
  // Make sure we sign-extend before the shift (not after).
  test.state()->cpu.x[1] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[0] = 0x4444'3333'ffff'ffffULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'ccbcULL);
}

TEST(TESTSUITE, MulAdd) {
  static const uint32_t code[] = {
      0x9b00'5a60,  // madd x0, x19, x0, x22
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 2ULL;
  test.state()->cpu.x[0] = 0x1111'2222'3333'4444ULL;
  test.state()->cpu.x[22] = ~0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x2222'4444'6666'8887ULL);
}

TEST(TESTSUITE, MulAdd32) {
  static const uint32_t code[] = {
      0x1b00'5a60,  // madd w0, w19, w0, w22
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 2ULL;
  test.state()->cpu.x[0] = 0x1111'2222'3333'4444ULL;
  test.state()->cpu.x[22] = ~0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x6666'8887ULL);
}

TEST(TESTSUITE, UnsignedMulAddLow) {
  static const uint32_t code[] = {
      0x9ba0'0d04,  // umaddl x4, w8, w0, x3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 2ULL;
  test.state()->cpu.x[0] = 0x1111'2222'8000'4444ULL;
  test.state()->cpu.x[3] = ~0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[4], 0x1'0000'8887ULL);
}

TEST(TESTSUITE, SignedMulAddLow) {
  static const uint32_t code[] = {
      0x9b21'010f,  // smaddl x15, w8, w1, x0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 2ULL;
  test.state()->cpu.x[1] = 0x1111'2222'8000'4444ULL;
  test.state()->cpu.x[0] = ~0ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0xffff'ffff'0000'8887ULL);
}

TEST(TESTSUITE, MulSub) {
  static const uint32_t code[] = {
      0x9b00'da60,  // msub x0, x19, x0, x22
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x1111'2222'3333'4444ULL;
  test.state()->cpu.x[0] = 2ULL;
  test.state()->cpu.x[22] = 0x3333'6666'9999'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x1111'2222'3333'4444ULL);
}

TEST(TESTSUITE, MulSub32) {
  static const uint32_t code[] = {
      0x1b00'da60,  // msub w0, w19, w0, w22
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x1111'2222'3333'4444ULL;
  test.state()->cpu.x[0] = 2ULL;
  test.state()->cpu.x[22] = 0x3333'6666'9999'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x3333'4444ULL);
}

TEST(TESTSUITE, UnsignedMulSubLow) {
  static const uint32_t code[] = {
      0x9ba0'8d04,  // umsubl x4, w8, w0, x3
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 2ULL;
  test.state()->cpu.x[0] = 0x1111'2222'8000'4444ULL;
  test.state()->cpu.x[3] = 0x3333'6666'9999'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[4], 0x3333'6665'9999'4444ULL);
}

constexpr __uint128_t MakeUInt128(uint64_t low, uint64_t high) {
  return (static_cast<__uint128_t>(high) << 64) | static_cast<__uint128_t>(low);
}

template <bool is_simd>
struct RegFile {
  template <typename T>
  static T& GetRef(ThreadState* state, uint8_t reg);
};

template <>
struct RegFile<false> {
  static uint64_t& GetRef(ThreadState* state, uint8_t reg) { return state->cpu.x[reg]; }
};

template <>
struct RegFile<true> {
  static __uint128_t& GetRef(ThreadState* state, uint8_t reg) { return state->cpu.v[reg]; }
};

template <typename DataType,
          unsigned addr_reg,
          unsigned data_reg,
          int pre_increment = 0,
          bool is_simd = false,
          unsigned offset = 0>
void TestLoad(const uint32_t code[], size_t code_size) {
  static_assert(offset <= 15, "Offset too large");
  static_assert(addr_reg <= 31 && data_reg < 31, "Unallowed register");
  TestHarness test(code, code_size);

  alignas(16) uint8_t val[32] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
                                 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

  if (addr_reg == 31) {
    test.state()->cpu.sp = ToGuestAddr(&val[offset]) - pre_increment;
  } else {
    test.state()->cpu.x[addr_reg] = ToGuestAddr(&val[offset]) - pre_increment;
  }

  EXPECT_TRUE(test.Run());
  DataType value;
  memcpy(&value, &val[offset], sizeof(DataType));
  EXPECT_EQ(RegFile<is_simd>::GetRef(test.state(), data_reg), value);
}

TEST(TESTSUITE, LoadImm) {
  static const uint32_t code[] = {
      0xf940'0fe0,  // ldr  x0, [sp,#24]
  };
  TestLoad<uint64_t, 31, 0, 24>(code, sizeof(code));
}

TEST(TESTSUITE, Load32Imm) {
  static const uint32_t code[] = {
      0xb940'0282,  // ldr  w2, [x20]
  };
  TestLoad<uint32_t, 20, 2>(code, sizeof(code));
}

TEST(TESTSUITE, LoadImmSignExtend) {
  static const uint32_t code[] = {
      0x3887'b747,  // ldrsb x7, [x26],#123
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = 0xffULL;
  test.state()->cpu.x[26] = ToGuestAddr(&val);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], ~0ULL);
  EXPECT_EQ(test.state()->cpu.x[26], ToGuestAddr(&val) + 123);
}

TEST(TESTSUITE, Load16Imm) {
  static const uint32_t code[] = {
      0x7840'0460,  // ldrh w0, [w3], #0
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = 0xff'ffffULL;
  test.state()->cpu.x[3] = ToGuestAddr(&val);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffffULL);
  EXPECT_EQ(test.state()->cpu.x[3], ToGuestAddr(&val));
}

TEST(TESTSUITE, LoadImmSignExtend32) {
  static const uint32_t code[] = {
      0x38cb'8682,  // ldrsb w2, [x20],#184
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = 0xffULL;
  test.state()->cpu.x[20] = ToGuestAddr(&val);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'ffffULL);
  EXPECT_EQ(test.state()->cpu.x[20], ToGuestAddr(&val) + 184);
}

TEST(TESTSUITE, LoadImmSimd128) {
  static const uint32_t code[] = {
      0x3dc0'02c1,  // ldr  q1, [x22]
  };
  TestLoad<__uint128_t, 22, 1, 0, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadUnalignedImmSimd128) {
  static const uint32_t code[] = {
      0x3dc0'02c1,  // ldr  q1, [x22]
  };
  TestLoad<__uint128_t, 22, 1, 0, true, 1>(code, sizeof(code));
}

TEST(TESTSUITE, LoadImmSimd64) {
  static const uint32_t code[] = {
      0xfd40'43ec,  // ldr  d12, [sp,#128]
  };
  TestLoad<uint64_t, 31, 12, 128, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadImmSimd32) {
  static const uint32_t code[] = {
      0xbd40'83ec,  // ldr  s12, [sp,#128]
  };
  TestLoad<uint32_t, 31, 12, 128, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadImmSimd16) {
  static const uint32_t code[] = {
      0x7d41'03ec,  // ldr  h12, [sp,#128]
  };
  TestLoad<uint16_t, 31, 12, 128, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadImmSimd8) {
  static const uint32_t code[] = {
      0x3d42'03ec,  // ldr  b12, [sp,#128]
  };
  TestLoad<uint8_t, 31, 12, 128, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadLiteral) {
  static const uint32_t code[] = {
      0x5800'0020,  // ldr x0, pc + 4
      0xdddd'cccc,
      0xffff'eeee,
  };

  TestHarness test(code, 4);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'eeee'dddd'ccccULL);
}

TEST(TESTSUITE, LoadImmPostIndex) {
  static const uint32_t code[] = {
      0xf843'07f3,  // ldr x19, [sp],#48
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = kTestValue64;
  test.state()->cpu.sp = ToGuestAddr(&val);
  test.state()->cpu.x[19] = 0;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[19], kTestValue64);
  EXPECT_EQ(test.state()->cpu.sp, ToGuestAddr(&val) + 48);
}

TEST(TESTSUITE, LoadLiteral32) {
  static const uint32_t code[] = {
      0x1800'0020,  // ldr w0, pc + 4
      0xdddd'cccc,
      0xffff'eeee,
  };

  TestHarness test(code, 4);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xdddd'ccccULL);
}

TEST(TESTSUITE, LoadLiteral32SignExtend) {
  static const uint32_t code[] = {
      0x9800'0020,  // ldrsw w0, pc + 4
      0xdddd'cccc,
      0xffff'eeee,
  };

  TestHarness test(code, 4);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'ffff'dddd'ccccULL);
}

TEST(TESTSUITE, LoadRegisterOffset) {
  static const uint32_t code[] = {
      0xf875'6982,  // ldr x2, [x12,x21]
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = kTestValue64;
  test.state()->cpu.x[12] = ToGuestAddr(&val) - 24;
  test.state()->cpu.x[21] = 24;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], kTestValue64);
}

TEST(TESTSUITE, LoadExtendedRegisterOffset) {
  static const uint32_t code[] = {
      0xf86d'c902,  // ldr x2, [x8,w13,sxtw]
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = kTestValue64;
  test.state()->cpu.x[8] = ToGuestAddr(&val) + 0x1'0000ULL;
  test.state()->cpu.x[13] = 0xffff'0000ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], kTestValue64);
}

TEST(TESTSUITE, LoadShiftedRegisterOffset) {
  static const uint32_t code[] = {
      0xf863'5846,  // ldr x6, [x2,w3,uxtw #3]
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = kTestValue64;
  test.state()->cpu.x[2] = ToGuestAddr(&val) - 24;
  test.state()->cpu.x[3] = 3;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[6], kTestValue64);
}

template <typename DataType,
          unsigned addr_reg,
          unsigned data_reg,
          unsigned data_reg2,
          int pre_increment = 0,
          bool is_simd = false>
void TestLoadPair(const uint32_t code[], size_t code_size) {
  static_assert(addr_reg <= 31 && data_reg < 31 && data_reg2 < 31, "Unallowed register");
  TestHarness test(code, code_size);

  alignas(16) __uint128_t val = MakeUInt128(0x1111'2222'3333'4444ULL, 0xffff'eeee'dddd'ccccULL);
  if (addr_reg == 31) {
    test.state()->cpu.sp = ToGuestAddr(&val) - pre_increment;
  } else {
    test.state()->cpu.x[addr_reg] = ToGuestAddr(&val) - pre_increment;
  }

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(RegFile<is_simd>::GetRef(test.state(), data_reg), static_cast<DataType>(val));
  EXPECT_EQ(RegFile<is_simd>::GetRef(test.state(), data_reg2),
            static_cast<DataType>(val >> (sizeof(DataType) * CHAR_BIT)));
}

TEST(TESTSUITE, LoadPair) {
  static const uint32_t code[] = {
      0xa942'5bf5,  // ldp x21, x22, [sp,#32]
  };
  TestLoadPair<uint64_t, 31, 21, 22, 32>(code, sizeof(code));
}

TEST(TESTSUITE, Load32Pair) {
  static const uint32_t code[] = {
      0x2942'5bf5,  // ldp w21, w22, [sp,#16]
  };
  TestLoadPair<uint32_t, 31, 21, 22, 16>(code, sizeof(code));
}

TEST(TESTSUITE, LoadPairSimd64) {
  static const uint32_t code[] = {
      0x6d47'2fea,  // ldp d10, d11, [sp,#112]
  };
  TestLoadPair<uint64_t, 31, 10, 11, 112, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadPairSimd32) {
  static const uint32_t code[] = {
      0x2d4e'2fea,  // ldp s10, s11, [sp,#112]
  };
  TestLoadPair<uint32_t, 31, 10, 11, 112, true>(code, sizeof(code));
}

TEST(TESTSUITE, LoadWordPairWithSignExtend) {
  static const uint32_t code[] = {
      0x68c0'04ac,  // ldpsw x12, x1, [x5],#0
  };
  TestLoadPair<uint32_t, 5, 12, 1>(code, sizeof(code));
}

TEST(TESTSUITE, LoadNotTemporalPair) {
  static const uint32_t code[] = {
      0xa84e'80e2,  // ldnp x2, x0, [x7,#232]
  };
  TestLoadPair<uint64_t, 7, 2, 0, 232>(code, sizeof(code));
}

TEST(TESTSUITE, Load32NotTemporalPair) {
  static const uint32_t code[] = {
      0x2856'a452,  // ldnp w18, w9, [x2,#180]
  };
  TestLoadPair<uint32_t, 2, 18, 9, 180>(code, sizeof(code));
}

TEST(TESTSUITE, LoadUnscaledImmOffset) {
  static const uint32_t code[] = {
      0xf84d'318f,  // ldur x15, [x12,#211]
  };
  TestLoad<uint64_t, 12, 15, 211>(code, sizeof(code));
}

TEST(TESTSUITE, LoadUnprivileged) {
  static const uint32_t code[] = {
      0xf85a'98d5,  // ldtr x21, [x6,#-87]
  };
  TestLoad<uint64_t, 6, 21, -87>(code, sizeof(code));
}

TEST(TESTSUITE, PrefetchRegisterOffset) {
  static const uint32_t code[] = {
      0xf8a2'6820,  // prfm pldl1keep, [x1, x2]
  };
  TestHarness test(code, 4);
  uint64_t data = 0;
  test.state()->cpu.x[1] = ToGuestAddr(&data);
  test.state()->cpu.x[2] = 0;

  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, PrefetchImm) {
  static const uint32_t code[] = {
      0xf980'0020,  // prfm pldl1keep, [x1]
  };
  TestHarness test(code, 4);
  uint64_t data = 0;
  test.state()->cpu.x[1] = ToGuestAddr(&data);

  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, PrefetchLiteral) {
  static const uint32_t code[] = {
      0x9800'0020,  // prfm pc + 4
      0xdddd'cccc,
      0xffff'eeee,
  };

  TestHarness test(code, 4);

  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

template <typename DataType,
          unsigned addr_reg,
          unsigned data_reg,
          int pre_increment = 0,
          bool is_simd = false,
          unsigned offset = 0>
void TestStore(const uint32_t code[], size_t code_size) {
  static_assert(offset <= 16, "Offset too large");
  static_assert(addr_reg <= 31 && data_reg < 31, "Unallowed register");
  TestHarness test(code, code_size);

  alignas(16) uint8_t val_addr[32] = {0x00};

  if (addr_reg == 31) {
    test.state()->cpu.sp = ToGuestAddr(&val_addr[offset]) - pre_increment;
  } else {
    test.state()->cpu.x[addr_reg] = ToGuestAddr(&val_addr[offset]) - pre_increment;
  }

  auto& data_ref = RegFile<is_simd>::GetRef(test.state(), data_reg);
  data_ref = static_cast<std::remove_reference_t<decltype(data_ref)>>(
      MakeUInt128(0x1111'2222'3333'4444ULL, 0xffff'eeee'dddd'ccccULL));

  EXPECT_TRUE(test.Run());

  DataType val;
  memcpy(&val, &val_addr[offset], sizeof(DataType));
  EXPECT_EQ(static_cast<DataType>(RegFile<is_simd>::GetRef(test.state(), data_reg)), val);
}

template <typename DataType,
          unsigned addr_reg,
          unsigned data_reg,
          unsigned data_reg2,
          int pre_increment = 0,
          bool is_simd = false>
void TestStorePair(const uint32_t code[], size_t code_size) {
  static_assert(addr_reg <= 31 && data_reg < 31, "Unallowed register");
  TestHarness test(code, code_size);

  alignas(16) __uint128_t val = 0;
  static_assert((sizeof(DataType) * 2) <= sizeof(val), "Unallowed DataType");

  if (addr_reg == 31) {
    test.state()->cpu.sp = ToGuestAddr(&val) - pre_increment;
  } else {
    test.state()->cpu.x[addr_reg] = ToGuestAddr(&val) - pre_increment;
  }
  RegFile<is_simd>::GetRef(test.state(), data_reg) = kTestValue64;
  RegFile<is_simd>::GetRef(test.state(), data_reg2) = ~kTestValue64;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(static_cast<DataType>(RegFile<is_simd>::GetRef(test.state(), data_reg)),
            static_cast<DataType>(val));
  EXPECT_EQ(static_cast<DataType>(RegFile<is_simd>::GetRef(test.state(), data_reg2)),
            static_cast<DataType>(val >> (sizeof(DataType) * CHAR_BIT)));
}

TEST(TESTSUITE, StoreImm) {
  static const uint32_t code[] = {
      0xf900'0fe0,  // str  x0, [sp,#24]
  };
  TestStore<uint64_t, 31, 0, 24>(code, sizeof(code));
}

TEST(TESTSUITE, Store32Imm) {
  static const uint32_t code[] = {
      0xb900'4fa0,  // str w0, [x29,#76]
  };
  TestStore<uint32_t, 29, 0, 76>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmSimd128) {
  static const uint32_t code[] = {
      0x3d80'23ec,  // str  q12, [sp,#128]
  };
  TestStore<__uint128_t, 31, 12, 128, true>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmUnalignedSimd128) {
  static const uint32_t code[] = {
      0x3d80'23ec,  // str  q12, [sp,#128]
  };
  TestStore<__uint128_t, 31, 12, 128, true, 1>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmSimd64) {
  static const uint32_t code[] = {
      0xfd00'43ec,  // str  d12, [sp,#128]
  };
  TestStore<uint64_t, 31, 12, 128, true>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmSimd32) {
  static const uint32_t code[] = {
      0xbd00'bba8,  // str  s8, [x29,#184]
  };
  TestStore<uint32_t, 29, 8, 184, true>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmSimd16) {
  static const uint32_t code[] = {
      0x7d01'73a8,  // str  h8, [x29,#184]
  };
  TestStore<uint16_t, 29, 8, 184, true>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmSimd8) {
  static const uint32_t code[] = {
      0x3d02'e3a8,  // str  b8, [x29,#184]
  };
  TestStore<uint8_t, 29, 8, 184, true>(code, sizeof(code));
}

TEST(TESTSUITE, StoreImmPreIndex) {
  static const uint32_t code[] = {
      // Also tests that imm is correctly sign-extended.
      0xf81e'0ffe,  // str  x30, [sp,#-32]!
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = 0;
  test.state()->cpu.sp = ToGuestAddr(&val) + 32;
  test.state()->cpu.x[30] = kTestValue64;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(val, kTestValue64);
  EXPECT_EQ(test.state()->cpu.sp, ToGuestAddr(&val));
}

TEST(TESTSUITE, StorePair) {
  static const uint32_t code[] = {
      0xa902'5bf5,  // stp x21, x22, [sp,#32]
  };
  TestStorePair<uint64_t, 31, 21, 22, 32>(code, sizeof(code));
}

TEST(TESTSUITE, Store32Pair) {
  static const uint32_t code[] = {
      0x2902'5bf5,  // stp w21, w22, [sp,#16]
  };
  TestStorePair<uint32_t, 31, 21, 22, 16>(code, sizeof(code));
}

TEST(TESTSUITE, StorePairSimd64) {
  static const uint32_t code[] = {
      0x6d07'2fea,  // stp d10, d11, [sp,#112]
  };
  TestStorePair<uint64_t, 31, 10, 11, 112, true>(code, sizeof(code));
}

TEST(TESTSUITE, StorePairSimd32) {
  static const uint32_t code[] = {
      0x2d07'2fea,  // stp s10, s11, [sp,#56]
  };
  TestStorePair<uint32_t, 31, 10, 11, 56, true>(code, sizeof(code));
}

TEST(TESTSUITE, StoreRegisterOffset) {
  static const uint32_t code[] = {
      0xf82c'690e,  // str x14, [x8,x12]
  };

  TestHarness test(code, sizeof(code));

  uint64_t val = 0ULL;
  test.state()->cpu.x[14] = kTestValue64;
  test.state()->cpu.x[8] = ToGuestAddr(&val) - 24;
  test.state()->cpu.x[12] = 24;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(val, kTestValue64);
}

TEST(TESTSUITE, TestAndBranch) {
  static const uint32_t code[] = {
      0xb678'0042,  // tbz x2, #47, pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 0;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.state()->cpu.x[2] = 1ULL << 47;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, TestAndBranchSignExtend) {
  static const uint32_t code[] = {
      0xb67f'ffe2,  // tbz x2, #47, pc - 0x4
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 0;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) - 4));
}

TEST(TESTSUITE, TestInvertedAndBranch) {
  static const uint32_t code[] = {
      0xb778'0042,  // tbnz x2, #47, pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 1ULL << 47;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.state()->cpu.x[2] = 0;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CompareInvertedAndBranch) {
  static const uint32_t code[] = {
      0xb500'0045,  // cbnz x5, pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 1;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.state()->cpu.x[5] = 0;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CompareAndBranch) {
  static const uint32_t code[] = {
      0xb400'0045,  // cbz x5, pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.state()->cpu.x[5] = 1;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CompareAndBranch32) {
  static const uint32_t code[] = {
      0x3400'0045,  // cbz w5, pc + 0x8
  };

  TestHarness test(code, sizeof(code));
  // Make sure upper bits aren't taken into account in the comparison.
  test.state()->cpu.x[5] = 0xffff'eeeeULL << 32;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 8));

  test.Reset();
  test.state()->cpu.x[5] = 1;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) + 4));
}

TEST(TESTSUITE, CompareAndBranchSignExtend) {
  static const uint32_t code[] = {
      0xb4ff'ffe5,  // cbz x5, pc - 0x4
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0;
  EXPECT_TRUE(test.RunBranch(ToGuestAddr(code) - 4));
}

TEST(TESTSUITE, ConditionalCompareImm) {
  static const uint32_t code[] = {
      0xfa40'08af,  // ccmp x5, #0x0, (alt_nzcv)#0xf, eq
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = kTestValue64;
  // 'eq' is true.
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b1000));

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, ConditionalCompareImm32) {
  static const uint32_t code[] = {
      0x7a40'08af,  // ccmp w5, #0x0, (alt_nzcv)#0xf, eq
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[5] = 0xffff'eeee'0000'0000ULL;
  // 'eq' is true.
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0100));
}

TEST(TESTSUITE, ConditionalCompareImmCarryOverflow) {
  static const uint32_t code[] = {
      0xfa41'08a0,  // ccmp x5, #0x1, (alt_nzcv)#0x0, eq
  };

  TestHarness test(code, sizeof(code));
  // (min_uint - 1) -> carry and negative are set
  test.state()->cpu.x[5] = 0;
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b1010));

  test.Reset();
  // (min_int - 1) -> overflow is set
  test.state()->cpu.x[5] = (1ULL << 63);
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0001));
}

TEST(TESTSUITE, ConditionalCompareNegativeImmCarryOverflow) {
  static const uint32_t code[] = {
      0xba41'08a0,  // ccmn x5, #0x1, (alt_nzcv)#0x0, eq
  };

  TestHarness test(code, sizeof(code));
  // (max_uint + 1) -> carry and zero
  test.state()->cpu.x[5] = -1ULL;
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0110));

  test.Reset();
  // (max_int + 1) -> overflow and negative are set
  test.state()->cpu.x[5] = (1ULL << 63) - 1ULL;
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1001));
}

TEST(TESTSUITE, ConditionalCompareRegister) {
  static const uint32_t code[] = {
      0xfa43'012f,  // ccmp x9, x3, (alt_nzcv)#0xf, eq
  };

  TestHarness test(code, sizeof(code));
  // 'eq' is true.
  test.SetNZCV(0b0100);
  test.state()->cpu.x[9] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[3] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0100));

  test.Reset();
  // 'eq' is true.
  test.SetNZCV(0b0100);
  test.state()->cpu.x[9] = 0ULL;
  test.state()->cpu.x[3] = 1ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b1010));

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, CompareRegister) {
  static const uint32_t code[] = {
      0xeb0b'005f,  // cmp x2, x11 <=> subs xzr, x2, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 8;
  test.state()->cpu.x[11] = 42;
  test.state()->cpu.sp = 0xffffULL;

  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b1010));
  // SP hasn't changed.
  EXPECT_EQ(test.state()->cpu.sp, 0xffffULL);
}

TEST(TESTSUITE, WriteTls) {
  static const uint32_t code[] = {
      0xd51b'd040,  // msr tpidr_el0, x0
  };

  TestHarness test(code, sizeof(code));

  test.state()->cpu.x[0] = kTestValue64;
  test.state()->tls = ~kTestValue64;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->tls, kTestValue64);
}

TEST(TESTSUITE, ReadTls) {
  static const uint32_t code[] = {
      0xd53b'd040,  // mrs x0, tpidr_el0
  };

  TestHarness test(code, sizeof(code));

  test.state()->tls = kTestValue64;
  test.state()->cpu.x[0] = ~kTestValue64;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], kTestValue64);
}

TEST(TESTSUITE, UnsignedDiv) {
  static const uint32_t code[] = {
      0x9acb'0b0f,  // udiv x15, x24, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[24] = 0x2222'4444'6666'8888LL;
  test.state()->cpu.x[11] = 0x2ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0x1111'2222'3333'4444ULL);
}

TEST(TESTSUITE, UnsignedDiv32) {
  static const uint32_t code[] = {
      0x1acb'0b0f,  // udiv w15, w24, w11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[24] = 0x2222'4444'6666'8888LL;
  test.state()->cpu.x[11] = 0xffff'eeee'0000'0002ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0x3333'4444ULL);
}

TEST(TESTSUITE, ExtractRegByteFromByteOfSimd64) {
  static const uint32_t code[] = {
      0x0e0d'3c08,  // umov w8, v0.b[6]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0x11ULL);
}

TEST(TESTSUITE, ExtractRegByteFromByteOfSimd128) {
  static const uint32_t code[] = {
      0x0e19'3c08,  // umov w8, v0.b[12]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0xccULL);
}

TEST(TESTSUITE, ExtractReg2BytesFrom2BytesOfSimd64) {
  static const uint32_t code[] = {
      0x0e0a'3c08,  // umov w8, v0.h[2]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0xaaaaULL);
}

TEST(TESTSUITE, ExtractReg2BytesFrom2BytesOfSimd128) {
  static const uint32_t code[] = {
      0x0e1a'3c08,  // umov w8, v0.h[6]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0xccccULL);
}

TEST(TESTSUITE, ExtractReg4BytesFrom4BytesOfSimd64) {
  static const uint32_t code[] = {
      0x0e0c'3c08,  // umov w8, v0.s[1]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0x1111'aaaaULL);
}

TEST(TESTSUITE, ExtractReg4BytesFrom4BytesOfSimd128) {
  static const uint32_t code[] = {
      0x0e1c'3c08,  // umov w8, v0.s[3]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0x3333'ccccULL);
}

TEST(TESTSUITE, ExtractRegDoubleFromDoubleOfSimd128) {
  static const uint32_t code[] = {
      0x4e18'3c08,  // mov x8, v0.d[1]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0x3333'cccc'4444'ddddULL);
}

TEST(TESTSUITE, ExtractRegDoubleFromDoubleOfSimd64) {
  static const uint32_t code[] = {
      0x4e08'3c08,  // mov x8, v0.d[0]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], 0x1111'aaaa'2222'bbbbULL);
}

TEST(TESTSUITE, InsertRegIntoHighSimd128) {
  static const uint32_t code[] = {
      0x4e18'1d00,  // mov v0.d[1], x8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0xffff'eeee'dddd'ccccULL));
}

TEST(TESTSUITE, InsertRegIntoLowSimd64) {
  static const uint32_t code[] = {
      0x4e08'1d00,  // mov v0.d[0], x8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0xffff'eeee'dddd'ccccULL, 0x3333'cccc'4444'ddddULL));
}

TEST(TESTSUITE, InsertRegByteIntoByteOfSimd64) {
  static const uint32_t code[] = {
      0x4e07'1d00,  // mov v0.b[3], w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x1111'aaaa'cc22'bbbbULL, 0x3333'cccc'4444'ddddULL));
}

TEST(TESTSUITE, InsertRegByteIntoByteOfSimd128) {
  static const uint32_t code[] = {
      0x4e17'1d00,  // mov v0.b[11], w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'cc44'ddddULL));
}

TEST(TESTSUITE, InsertReg2BytesInto2BytesOfSimd64) {
  static const uint32_t code[] = {
      0x4e06'1d00,  // mov v0.h[1], w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x1111'aaaa'cccc'bbbbULL, 0x3333'cccc'4444'ddddULL));
}

TEST(TESTSUITE, InsertReg2BytesInto2BytesOfSimd128) {
  static const uint32_t code[] = {
      0x4e16'1d00,  // mov v0.h[5], w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'cccc'ddddULL));
}

TEST(TESTSUITE, InsertReg4BytesInto4BytesOfSimd64) {
  static const uint32_t code[] = {
      0x4e0c'1d00,  // mov v0.s[1], w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0xdddd'cccc'2222'bbbbULL, 0x3333'cccc'4444'ddddULL));
}

TEST(TESTSUITE, InsertReg4BytesInto4BytesOfSimd128) {
  static const uint32_t code[] = {
      0x4e1c'1d00,  // mov v0.s[3], w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'1111'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0xdddd'cccc'4444'ddddULL));
}

TEST(TESTSUITE, SetReg) {
  static const uint32_t code[] = {
      0x5280'0022,  // mov w2, #1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = 0x5555'5555'5555'5555;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0x1ULL);
}

TEST(TESTSUITE, MovImmDoNotFillZero) {
  static const uint32_t code[] = {
      0xf28a'cf00,  // movk x0, #0x5678
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'eeee'dddd'5678ULL);
}

TEST(TESTSUITE, MovImmDoNotFillZeroShift16) {
  static const uint32_t code[] = {
      0xf2a2'4680,  // movk x0, #0x1234, lsl #16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'eeee'1234'ccccULL);
}

TEST(TESTSUITE, MovImmDoNotFillZeroShift32) {
  static const uint32_t code[] = {
      0xf2ce'ca80,  // movk x0, #0x7654, lsl #32
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'7654'dddd'ccccULL);
}

TEST(TESTSUITE, MovImmDoNotFillZeroShift48) {
  static const uint32_t code[] = {
      0xf2e1'5300,  // movk x0, #0xa98, lsl #48
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x0a98'eeee'dddd'ccccULL);
}

TEST(TESTSUITE, MovImm32DoNotFillZeroShift16) {
  static const uint32_t code[] = {
      0x72a2'4680,  // movk w0, #0x1234, lsl #16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'eeee'dddd'ccccULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x1234'ccccULL);
}

TEST(TESTSUITE, Nop) {
  static const uint32_t code[] = {
      0xd503'201f,  // nop
  };

  TestHarness test(code, sizeof(code));
  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, OtherNops) {
  uint32_t base_nop = 0xd503'201f;  // crm::op2 == 0
  uint32_t nop;

  TestHarness test(&nop, sizeof(nop));

  for (uint32_t patch = 0b110; patch < (1 << 7); patch++) {
    nop = base_nop + (patch << 5);
    test.Reset();
    // Just expect that PC is incremented.
    EXPECT_TRUE(test.Run());
  }
}

TEST(TESTSUITE, Yield) {
  static const uint32_t code[] = {
      0xd503'203f,  // yield
  };

  TestHarness test(code, sizeof(code));
  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, Dmb) {
  static const uint32_t code[] = {
      0xd503'30bf,  // dmb #0x00
  };

  TestHarness test(code, sizeof(code));
  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, Dsb) {
  static const uint32_t code[] = {
      0xd503'309f,  // dsb #0x00
  };

  TestHarness test(code, sizeof(code));
  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, Isb) {
  static const uint32_t code[] = {
      0xd503'30df,  // isb #0x00
  };

  TestHarness test(code, sizeof(code));
  // Just expect that PC is incremented.
  EXPECT_TRUE(test.Run());
}

TEST(TESTSUITE, ReverseBits) {
  static const uint32_t code[] = {
      0xdac0'01c0,  // rbit x0, x14
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[14] = 0xffee'cc88'7733'1100ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x0088'ccee'1133'77ffULL);
}

TEST(TESTSUITE, ReverseBits32) {
  static const uint32_t code[] = {
      0x5ac0'01c0,  // rbit w0, w14
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[14] = 0xffee'cc88'7733'1100ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0x0088'cceeULL);
}

TEST(TESTSUITE, ReverseBytes) {
  static const uint32_t code[] = {
      0xdac0'0cc6,  // rev x6, x6
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[6] = 0x1122'3344'5566'7788ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[6], 0x8877'6655'4433'2211ULL);
}

TEST(TESTSUITE, ReverseBytesChunk32) {
  static const uint32_t code[] = {
      0xdac0'0ae1,  // rev32 x1, x23
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[23] = 0x1122'3344'5566'7788ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x4433'2211'8877'6655ULL);
}

TEST(TESTSUITE, ReverseBytesChunk16) {
  static const uint32_t code[] = {
      0xdac0'048e,  // rev16 x14, x4
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[4] = 0x1122'3344'5566'7788ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[14], 0x2211'4433'6655'8877ULL);
}

TEST(TESTSUITE, ReverseBytes32) {
  static const uint32_t code[] = {
      0x5ac0'0ae1,  // rev32 w1, w23
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[23] = 0x1122'3344'5566'7788ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0x8877'6655ULL);
}

TEST(TESTSUITE, ReverseBytes32Chunk16) {
  static const uint32_t code[] = {
      0x5ac0'048e,  // rev16 x14, x4
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[4] = 0x1122'3344'5566'7788ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[14], 0x6655'8877ULL);
}

TEST(TESTSUITE, CountLeadingZeros) {
  static const uint32_t code[] = {
      0xdac0'10cc,  // clz x12, x6
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[6] = 0x1ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[12], 63ULL);
}

TEST(TESTSUITE, CountLeadingZeros32) {
  static const uint32_t code[] = {
      0x5ac0'10cc,  // clz w12, w6
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[6] = 0x1ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[12], 31ULL);
}

TEST(TESTSUITE, CountLeadingSignBits) {
  static const uint32_t code[] = {
      0xdac0'14cc,  // cls x12, x6
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[6] = 0x1ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[12], 62ULL);

  test.Reset();
  test.state()->cpu.x[6] = 0xffff'0000'ffff'0000ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[12], 15ULL);
}

TEST(TESTSUITE, CountLeadingSignBits32) {
  static const uint32_t code[] = {
      0x5ac0'14cc,  // cls w12, w6
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[6] = 0x1ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[12], 30ULL);

  test.Reset();
  test.state()->cpu.x[6] = 0xffff'0000'ff00'ff00ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[12], 7ULL);
}

TEST(TESTSUITE, Crc32b) {
  static const uint32_t code[] = {
      0x1ad4'4263,  // crc32b  w3, w19, w20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0xf8ea'd90fULL);
}

TEST(TESTSUITE, Crc32h) {
  static const uint32_t code[] = {
      0x1ad4'4663,  // crc32h  w3, w19, w20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0x1994'dca8ULL);
}

TEST(TESTSUITE, Crc32w) {
  static const uint32_t code[] = {
      0x1ad4'4a63,  // crc32w  w3, w19, w20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0xe1e0'8fedULL);
}

TEST(TESTSUITE, Crc32x) {
  static const uint32_t code[] = {
      0x9ad4'4e63,  // crc32x  w3, w19, x20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0x133a'2268ULL);
}

TEST(TESTSUITE, Crc32cb) {
  static const uint32_t code[] = {
      0x1ad4'5263,  // crc32cb  w3, w19, w20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0x6c73'da1eULL);
}

TEST(TESTSUITE, Crc32ch) {
  static const uint32_t code[] = {
      0x1ad4'5663,  // crc32ch  w3, w19, w20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0x5650'2c49ULL);
}

TEST(TESTSUITE, Crc32cw) {
  static const uint32_t code[] = {
      0x1ad4'5a63,  // crc32cw  w3, w19, w20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0x09da'b9caULL);
}

TEST(TESTSUITE, Crc32cx) {
  static const uint32_t code[] = {
      0x9ad4'5e63,  // crc32cx  w3, w19, x20
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[19] = 0x8877'6655ULL;
  test.state()->cpu.x[20] = 0xffee'ddcc'bbaa'9988ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[3], 0xa7b1'3667ULL);
}

TEST(TESTSUITE, SignedDiv) {
  static const uint32_t code[] = {
      0x9acb'0f0f,  // sdiv x15, x24, x11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[24] = 0xffff'eeee'ffff'eeeeULL;
  test.state()->cpu.x[11] = 0x10ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0xffff'feee'efff'feefULL);

  test.Reset();

  // Check the overflow case.
  test.state()->cpu.x[24] = 0x8000'0000'0000'0000ULL;
  test.state()->cpu.x[11] = ~uint64_t{0};

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0x8000'0000'0000'0000ULL);
}

TEST(TESTSUITE, SignedDiv32) {
  static const uint32_t code[] = {
      0x1acb'0f0f,  // sdiv w15, w24, w11
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[24] = 0x7777'cccc'ffff'eeeeULL;
  test.state()->cpu.x[11] = 0x10ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0xffff'feefULL);

  test.Reset();

  // Check the overflow case.
  test.state()->cpu.x[24] = 0x7777'cccc'8000'0000ULL;
  test.state()->cpu.x[11] = uint64_t{~uint32_t{0}};

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[15], 0x8000'0000ULL);
}

TEST(TESTSUITE, UnsignedMulHigh) {
  static const uint32_t code[] = {
      0x9bd0'7c44,  // umulh x4, x2, x16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[2] = ~0ULL;
  test.state()->cpu.x[16] = 0x0000'0001'0000'0001ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[4], 0x1'0000'0000ULL);
}

TEST(TESTSUITE, SignedMulHigh) {
  static const uint32_t code[] = {
      0x9b41'7ea7,  // smulh x7, x21, x1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[21] = ~0ULL;
  test.state()->cpu.x[1] = 0x0000'0001'0000'0001ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[7], ~0ULL);
}

TEST(TESTSUITE, Extract) {
  static const uint32_t code[] = {
      0x93d2'6290,  // extr x16, x20, x18, #24
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[20] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[18] = 0xbbbb'aaaa'8888'2222ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[16], 0xddcc'ccbb'bbaa'aa88ULL);
}

TEST(TESTSUITE, Extract32) {
  static const uint32_t code[] = {
      0x1392'6290,  // extr x16, x20, x18, #24
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[20] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.x[18] = 0xbbbb'aaaa'8888'2222ULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[16], 0xddcc'cc88ULL);
}

TEST(TESTSUITE, MovBitMaskImm) {
  static const uint32_t code[] = {
      0xb200'c3eb,  // mov x11, #0x0101'0101.01010101 (aliased to orr x11 = xzr, imm)
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[11], 0x0101'0101'0101'0101ULL);
}

TEST(TESTSUITE, MovRegister) {
  static const uint32_t code[] = {
      0xaa00'03f7,  // mov x23 = x0 (aliased to orr x32 = xzr, x0)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = kTestValue64;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[23], kTestValue64);
}

TEST(TESTSUITE, MovRegisterToZeroRegister) {
  static const uint32_t code[] = {
      0xaa00'03ff,  // mov xzr = x0 (actually nop)
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xaaaaULL;
  test.state()->cpu.sp = 0xffffULL;

  EXPECT_TRUE(test.Run());
  // SP hasn't changed.
  EXPECT_EQ(test.state()->cpu.sp, 0xffffULL);
}

TEST(TESTSUITE, CompareImmDoesNotModifyStackPointer) {
  static const uint32_t code[] = {
      0xf100'041f,  // cmp x0, 1 <=> subs xzr, x0, 1
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 1;
  test.state()->cpu.sp = 0xffff;
  test.SetNZCV(0b0000);

  EXPECT_TRUE(test.Run());
  EXPECT_TRUE(GetZ(&test.state()->cpu));
  // SP hasn't changed.
  EXPECT_EQ(test.state()->cpu.sp, 0xffffULL);
}

TEST(TESTSUITE, TestImmDoesNotModifyStackPointer) {
  static const uint32_t code[] = {
      0xf27c'ec1f,  // tst x0, #0xffff'ffff'ffff'fff0 <=> ands xzr, x0, imm
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[0] = 0xffff'0000'ffff'00ffULL;
  test.state()->cpu.sp = kTestValue64;

  EXPECT_TRUE(test.Run());
  // SP hasn't changed.
  EXPECT_EQ(test.state()->cpu.sp, kTestValue64);
}

TEST(TESTSUITE, PcRelativeAddressingPageAligned) {
  static const uint32_t code[] = {
      0x9000'0001,  // adrp x1, align<4k>(pc) + 0x0
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], reinterpret_cast<uint64_t>(code) & ~0xfffULL);
}

TEST(TESTSUITE, PcRelativeAddressingPageAlignedSignExtend) {
  static const uint32_t code[] = {
      0x9080'0001,  // adrp x1, align<4k>(pc) + sign_extend_33_to_64(0x1'0000'0000)
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1],
            (reinterpret_cast<uint64_t>(code) & ~0xfffULL) + 0xffff'ffff'0000'0000ULL);
}

TEST(TESTSUITE, PcRelativeAddressing) {
  static const uint32_t code[] = {
      0x1000'0000,  // adr x0, pc + 0x0
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], reinterpret_cast<uint64_t>(code));
}

TEST(TESTSUITE, PcRelativeAddressingSignExtend) {
  static const uint32_t code[] = {
      0x1080'0000,  // adr x0, pc + sign_extend_21_to_64(0x10'0000)
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], reinterpret_cast<uint64_t>(code) + 0xffff'ffff'fff0'0000ULL);
}

TEST(TESTSUITE, WriteNZCV) {
  static const uint32_t code[] = {
      0xd51b'4205,  // msr nzcv, x5
  };

  TestHarness test(code, sizeof(code));

  test.SetNZCV(0b0000);
  test.state()->cpu.x[5] = 0b1010ULL << 28;
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1010));

  test.Reset();
  test.state()->cpu.x[5] = 0b0101ULL << 28;
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b0101));
}

TEST(TESTSUITE, ReadNZCV) {
  static const uint32_t code[] = {
      0xd53b'4205,  // mrs x5, nzcv
  };

  TestHarness test(code, sizeof(code));
  test.SetNZCV(0b1010);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[5], 0b1010ULL << 28);

  test.Reset();
  test.SetNZCV(0b0101);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[5], 0b0101ULL << 28);
}

TEST(TESTSUITE, ReadDataCacheZeroID) {
  static const uint32_t code[] = {
      0xd53b'00e3,  // mrs x3, dczid_el0
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  // Expect prohibited DC ZVA.
  EXPECT_EQ(test.state()->cpu.x[3], 1ULL << 4);
}

TEST(TESTSUITE, WriteFpsr) {
  static constexpr uint64_t kQcBit = 1ULL << 27;

  static const uint32_t code[] = {
      0xd51b'4420,  // msr fpsr, x0
  };

  TestHarness test(code, sizeof(code));

  test.state()->cpu.x[0] = kQcBit;
  test.state()->cpu.emulated_fpsr = 0;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.emulated_fpsr, kQcBit);
}

TEST(TESTSUITE, MovFpToInt64) {
  static const uint32_t code[] = {
      0x9e66'0142,  // fmov x2, d10
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.v[10] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[2], 0xffff'eeee'dddd'ccccULL);
}

TEST(TESTSUITE, MovFpToInt32) {
  static const uint32_t code[] = {
      0x1e26'0101,  // fmov w1, s8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.v[8] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[1], 0xdddd'ccccULL);
}

TEST(TESTSUITE, MovHighFp128ToInt64) {
  static const uint32_t code[] = {
      0x9eae'004a,  // fmov x10, v2.d[1]
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.v[2] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[10], 0x3333'cccc'4444'ddddULL);
}

TEST(TESTSUITE, MovIntToFp64) {
  static const uint32_t code[] = {
      0x9e67'0142,  // fmov d2, x10
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[10] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[2] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);
  EXPECT_TRUE(test.Run());
  // High part is zeroed.
  EXPECT_EQ(test.state()->cpu.v[2], MakeUInt128(0xffff'eeee'dddd'ccccULL, 0x0));
}

TEST(TESTSUITE, MovIntToFp32) {
  static const uint32_t code[] = {
      0x1e27'0101,  // fmov s1, w8
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[8] = 0xffff'eeee'dddd'ccccULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[1], 0xdddd'ccccULL);
}

TEST(TESTSUITE, MovInt64ToHighFp128) {
  static const uint32_t code[] = {
      0x9eaf'0142,  // fmov v2.d[1], x10
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[10] = 0xffff'eeee'dddd'ccccULL;
  test.state()->cpu.v[2] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  // Low part is preserved.
  EXPECT_EQ(test.state()->cpu.v[2],
            MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0xffff'eeee'dddd'ccccULL));
}

TEST(TESTSUITE, DuplicateRegIntoSimd128) {
  static const uint32_t code[] = {
      0x4e01'0c20,  // dup v0.16b, w1
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[1] = 0xffff'00abULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0xabab'abab'abab'ababULL, 0xabab'abab'abab'ababULL));
}

TEST(TESTSUITE, DuplicateRegIntoSimd64) {
  static const uint32_t code[] = {
      0x0e04'0d40,  // dup v0.2s, w10
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[10] = 0x1111'cccc'ffff'eeeeULL;
  test.state()->cpu.v[0] = MakeUInt128(0x1111'aaaa'2222'bbbbULL, 0x3333'cccc'4444'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0], MakeUInt128(0xffff'eeee'ffff'eeeeULL, 0));
}

TEST(TESTSUITE, MovImm128ToSimd128) {
  static const uint32_t code[] = {
      0x4f00'f5e0,  // fmov v0.4s, #3.8750e+00
  };
  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0],
            MakeUInt128(0x4078'0000'4078'0000ULL, 0x4078'0000'4078'0000ULL));
}

TEST(TESTSUITE, ReadFpcr) {
  static const uint32_t code[] = {
      0xd53b'4400,  // mrs x0, fpcr
  };
  TestHarness test(code, sizeof(code));

  // To be overwritten by the MRS instruction.
  test.state()->cpu.x[0] = 0xdead'beef'cafe'deedULL;

  EXPECT_TRUE(test.Run());

  // TODO(b/133794506): Verify that we can write a value to FPCR and read the
  // value back.  For now, we are just testing that the instruction doesn't
  // cause the interpreter to crash.
}

TEST(TESTSUITE, FunnelShiftRightU32) {
  static const uint32_t code[]{
      0x139a'435a,  // ror w26, w26, #16
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[26] = 0x0000'0000'ffff'0011ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[26], 0x0011'ffffULL);
}

TEST(TESTSUITE, FunnelShiftRightByZeroU32) {
  static const uint32_t code[]{
      0x139a'035a,  // ror w26, w26, #0
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.x[26] = 0x0000'0000'ffff'0011ULL;

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[26], 0x0000'0000'ffff'0011ULL);
}

TEST(TESTSUITE, FpConditionalCompare) {
  static const uint32_t code[] = {
      0x1e61'040f,  // fccmp d0, d1, #15, eq
  };

  TestHarness test(code, sizeof(code));
  test.state()->cpu.v[0] = std::bit_cast<uint64_t>(2.0);
  test.state()->cpu.v[1] = std::bit_cast<uint64_t>(1.0);
  // 'eq' is true.
  test.SetNZCV(0b0100);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCVAfterSub(0b0000));

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_NO_FATAL_FAILURE(test.CheckNZCV(0b1111));
}

TEST(TESTSUITE, FpConditionalSelect) {
  static const uint32_t code[] = {
      0x1e62'0c20,  // fcsel d0, d1, d2, eq
  };

  TestHarness test(code, sizeof(code));
  // 'eq' is true.
  test.SetNZCV(0b0100);
  test.state()->cpu.v[1] = 0xffffULL;
  test.state()->cpu.v[2] = 0xeeeeULL;
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0], 0xffffULL);

  test.Reset();
  // 'eq' is false.
  test.SetNZCV(0b0000);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0], 0xeeeeULL);
}

TEST(TESTSUITE, LoadAtomic) {
  static const uint32_t code[] = {
      0xc8df'fd09,  // ldar x9, [x8]
  };
  TestLoad<uint64_t, 8, 9>(code, sizeof(code));
}

TEST(TESTSUITE, LoadAtomic32) {
  static const uint32_t code[] = {
      0x88df'fd09,  // ldar w9, [x8]
  };
  TestLoad<uint32_t, 8, 9>(code, sizeof(code));
}

TEST(TESTSUITE, LoadHalfwordAtomic) {
  static const uint32_t code[] = {
      0x48df'fd09,  // ldarh w9, [x8]
  };
  TestLoad<uint16_t, 8, 9>(code, sizeof(code));
}

TEST(TESTSUITE, LoadByteAtomic) {
  static const uint32_t code[] = {
      0x08df'fd09,  // ldarb w9, [x8]
  };
  TestLoad<uint8_t, 8, 9>(code, sizeof(code));
}

TEST(TESTSUITE, StoreAtomic) {
  static const uint32_t code[] = {
      0xc89f'ff2e,  // stlr x14, [x25]
  };
  TestStore<uint64_t, 25, 14>(code, sizeof(code));
}

TEST(TESTSUITE, StoreAtomic32) {
  static const uint32_t code[] = {
      0x889f'ff2e,  // stlr w14, [x25]
  };
  TestStore<uint32_t, 25, 14>(code, sizeof(code));
}

TEST(TESTSUITE, StoreHalfwordAtomic) {
  static const uint32_t code[] = {
      0x489f'ff2e,  // stlrh w14, [x25]
  };
  TestStore<uint16_t, 25, 14>(code, sizeof(code));
}

TEST(TESTSUITE, StoreByteAtomic) {
  static const uint32_t code[] = {
      0x089f'ff2e,  // stlrb w14, [x25]
  };
  TestStore<uint8_t, 25, 14>(code, sizeof(code));
}

TEST(TESTSUITE, AtomicMonitorClear) {
  static const uint32_t code[] = {
      0xd503'3f5f,  // clrex
  };
  TestHarness test(code, sizeof(code));
  test.state()->cpu.reservation_address = ToGuestAddr(code);
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.reservation_address, kNullGuestAddr);
}

TEST(TESTSUITE, Movi64BitVector) {
  static const uint32_t code[] = {
      0x6f00'e400,  // movi v0.2D, #0  ; 64-bit vector
  };
  TestHarness test(code, sizeof(code));

  // To be overwritten by the MOVI instruction.
  test.state()->cpu.v[0] = MakeUInt128(0xdead'beef'cafe'1234ULL, 0x3141'5926'5358'9793ULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.v[0], MakeUInt128(0, 0));
}

// Unlike SMov, UMov always writes to 32-bit distination.
TEST(TESTSUITE, UMovVectorToRegister32) {
  static const uint32_t code[] = {
      0x0e15'3c20,  // umov w0, v1.b[10]
  };
  TestHarness test(code, sizeof(code));

  test.state()->cpu.v[1] = MakeUInt128(0x1111'2222'3333'4444ULL, 0xaaaa'bbbb'ccff'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffULL);
}

TEST(TESTSUITE, SMovVectorToRegister32) {
  static const uint32_t code[] = {
      0x0e15'2c20,  // smov w0, v1.b[10]
  };
  TestHarness test(code, sizeof(code));

  test.state()->cpu.v[1] = MakeUInt128(0x1111'2222'3333'4444ULL, 0xaaaa'bbbb'ccff'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'ffffULL);
}

TEST(TESTSUITE, SMovVectorToRegister64) {
  static const uint32_t code[] = {
      0x4e15'2c20,  // smov x0, v1.b[10]
  };
  TestHarness test(code, sizeof(code));

  test.state()->cpu.v[1] = MakeUInt128(0x1111'2222'3333'4444ULL, 0xaaaa'bbbb'ccff'ddddULL);

  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[0], 0xffff'ffff'ffff'ffffULL);
}

TEST(TESTSUITE, ReadCacheTypeRegister) {
  static const uint32_t code[] = {
      0xd53b'0028,  // mrs x8, ctr_el0
  };

  TestHarness test(code, sizeof(code));
  EXPECT_TRUE(test.Run());
  EXPECT_EQ(test.state()->cpu.x[8], kArm64CacheTypeRegisterValue);
}

TEST(TESTSUITE, ReadCounterTimerVirtualCount) {
  static const uint32_t code[] = {
      0xd53b'e043,  // mrs x3, cntvct_el0
  };

  TestHarness test(code, sizeof(code));

  EXPECT_TRUE(test.Run());
  uint64_t first_count = test.state()->cpu.x[3];
  EXPECT_GT(first_count, 0ULL);

  test.Reset();

  EXPECT_TRUE(test.Run());
  uint64_t second_count = test.state()->cpu.x[3];
  EXPECT_GT(second_count, first_count);
}

template <typename DataType>
void TestSwap(const uint32_t code[], size_t code_size, DataType mem_val, DataType src_val) {
  static_assert(sizeof(DataType) <= 8);
  static_assert(std::is_unsigned<DataType>::value, "Swap can only be used with unsigned types");

  constexpr uint32_t kAddrReg = 0, kSrcReg = 8, kTargetReg = 2;
  TestHarness test(code, code_size);

  alignas(16) uint8_t val[16];
  memset(val, 0x55, sizeof(val));
  DataType& typed_value = *reinterpret_cast<DataType*>(val);
  typed_value = mem_val;

  test.state()->cpu.x[kAddrReg] = ToGuestAddr(val);
  test.state()->cpu.x[kSrcReg] = src_val;
  test.state()->cpu.x[kTargetReg] = ~mem_val;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(typed_value, src_val);
  EXPECT_EQ(test.state()->cpu.x[kTargetReg], static_cast<uint64_t>(mem_val));

  // Check that we didn't change any extra values.
  int8_t filler[16];
  memset(filler, 0x55, sizeof(filler));
  EXPECT_EQ(memcmp(val + sizeof(DataType), filler, 16 - sizeof(DataType)), 0);
}

TEST(TESTSUITE, SwapByte) {
  static const uint32_t code[] = {
      0x3828'8002,  // swpb w8, w2, [x0]
  };

  TestSwap<uint8_t>(code, sizeof(code), 0xff, 0xf6);
  TestSwap<uint8_t>(code, sizeof(code), 0, 1);
}

TEST(TESTSUITE, SwapHalfword) {
  static const uint32_t code[] = {
      0x7828'8002,  // swpb w8, w2, [x0]
  };

  TestSwap<uint16_t>(code, sizeof(code), 0x126f, 0x77f6);
  TestSwap<uint16_t>(code, sizeof(code), 0xf26f, 0x77f6);
  TestSwap<uint16_t>(code, sizeof(code), 0, 1);
}

TEST(TESTSUITE, SwapWord) {
  static const uint32_t code[] = {
      0xb828'8002,  // swpb w8, w2, [x0]
  };

  TestSwap<uint32_t>(code, sizeof(code), 0xff01'126f, 0x01ff'77f6);
  TestSwap<uint32_t>(code, sizeof(code), 0, 1);
}

TEST(TESTSUITE, SwapDoubleword) {
  static const uint32_t code[] = {
      0xf828'8002,  // swpb x8, xw2, [x0]
  };

  TestSwap<uint64_t>(code, sizeof(code), 0xff01'126f'ff01'126fUL, 0x01ff'77f6'ff01'126fUL);
  TestSwap<uint64_t>(code, sizeof(code), 0, 1);
}

template <typename IntType>
IntType TestAtomicMemOp(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  const uint32_t addr_reg = 0;
  const uint32_t src_reg = 8;
  const uint32_t target_reg = 2;
  TestHarness test(code, code_size);

  // Allocate memory with buffer on each side to check that we don't
  // write to neighboring memory.
  alignas(sizeof(IntType)) uint8_t mem[sizeof(IntType) * 3];
  memset(mem, 0xff, sizeof(mem));
  IntType& typed_value = *reinterpret_cast<IntType*>(mem + sizeof(IntType));
  typed_value = mem_val;

  test.state()->cpu.x[addr_reg] = ToGuestAddr(mem + sizeof(IntType));
  test.state()->cpu.x[src_reg] = src_val;
  test.state()->cpu.x[target_reg] = ~mem_val;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.x[target_reg], static_cast<std::make_unsigned_t<IntType>>(mem_val));

  // Check that we didn't change any extra values.
  for (size_t i = 0; i < sizeof(mem); i++) {
    if (i < sizeof(IntType) || i >= 2 * sizeof(IntType)) {
      EXPECT_EQ(mem[i], 0xff);
    }
  }

  return typed_value;
}

template <typename IntType>
void TestLdAdd(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  static_assert(sizeof(IntType) <= 8);
  static_assert(std::is_unsigned<IntType>::value, "LdAdd can only be used with unsigned types");

  auto val = TestAtomicMemOp(code, code_size, mem_val, src_val);
  EXPECT_EQ(val, static_cast<IntType>(src_val + mem_val));
}

TEST(TESTSUITE, LdAddByte) {
  static const uint32_t code[] = {
      0x3828'0002,  // ldaddb w8, w2, [x0]
  };

  TestLdAdd<uint8_t>(code, sizeof(code), 0, 0);
  TestLdAdd<uint8_t>(code, sizeof(code), 0x0f, 5);
  TestLdAdd<uint8_t>(code, sizeof(code), 0xff, 0x12);
}

TEST(TESTSUITE, LdAddHalfword) {
  static const uint32_t code[] = {
      0x7828'0002,  // ldaddh w8, w2, [x0]
  };

  TestLdAdd<uint16_t>(code, sizeof(code), 0, 0);
  TestLdAdd<uint16_t>(code, sizeof(code), 0x0fff, 5);
  TestLdAdd<uint16_t>(code, sizeof(code), 0xffff, 0x12);
}

TEST(TESTSUITE, LdAddWord) {
  static const uint32_t code[] = {
      0xb828'0002,  // ldaddw w8, w2, [x0]
  };

  TestLdAdd<uint32_t>(code, sizeof(code), 0, 0);
  TestLdAdd<uint32_t>(code, sizeof(code), 0x0ff0'0fff, 5);
  TestLdAdd<uint32_t>(code, sizeof(code), 0xffff'ffff, 0x12);
}

TEST(TESTSUITE, LdAddDoubleword) {
  static const uint32_t code[] = {
      0xf828'0002,  // ldaddl x8, x2, [x0]
  };

  TestLdAdd<uint64_t>(code, sizeof(code), 0, 0);
  TestLdAdd<uint64_t>(code, sizeof(code), 0x0ff0'0fff'0ff0'0fff, 5);
  TestLdAdd<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x12);
}

template <typename IntType>
void TestLdClr(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  static_assert(sizeof(IntType) <= 8);
  static_assert(std::is_unsigned<IntType>::value, "LdClr can only be used with unsigned types");

  auto val = TestAtomicMemOp(code, code_size, mem_val, src_val);
  EXPECT_EQ(val, static_cast<IntType>(mem_val & ~src_val));
}

TEST(TESTSUITE, LdClrByte) {
  static const uint32_t code[] = {
      0x3828'1002,  // ldclrb w8, w2, [x0]
  };

  TestLdClr<uint8_t>(code, sizeof(code), 0, 0xff);
  TestLdClr<uint8_t>(code, sizeof(code), 0xff, 0x0f);
  TestLdClr<uint8_t>(code, sizeof(code), 0xca, 0x83);
  TestLdClr<uint8_t>(code, sizeof(code), 0xff, 0xff);
}

TEST(TESTSUITE, LdClrHalfword) {
  static const uint32_t code[] = {
      0x7828'1002,  // ldclrh w8, w2, [x0]
  };

  TestLdClr<uint16_t>(code, sizeof(code), 0, 0xffff);
  TestLdClr<uint16_t>(code, sizeof(code), 0xffff, 0x0ff0);
  TestLdClr<uint16_t>(code, sizeof(code), 0xcaac, 0x8383);
  TestLdClr<uint16_t>(code, sizeof(code), 0xffff, 0xffff);
}

TEST(TESTSUITE, LdClrWord) {
  static const uint32_t code[] = {
      0xb828'1002,  // ldclrw w8, w2, [x0]
  };

  TestLdClr<uint32_t>(code, sizeof(code), 0, 0xffff'ffff);
  TestLdClr<uint32_t>(code, sizeof(code), 0xffff'ffff, 0x0ff0'0ff0);
  TestLdClr<uint32_t>(code, sizeof(code), 0xcaac'acca, 0x8383'3883);
  TestLdClr<uint32_t>(code, sizeof(code), 0xffff'ffff, 0xffff'ffff);
}

TEST(TESTSUITE, LdClrDoubleword) {
  static const uint32_t code[] = {
      0xf828'1002,  // ldclrl x8, x2, [x0]
  };

  TestLdClr<uint64_t>(code, sizeof(code), 0, 0xffff'ffff'ffff'ffff);
  TestLdClr<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x0ff0'0ff0'0ff0'0ff0);
  TestLdClr<uint64_t>(code, sizeof(code), 0xcaac'acca'caac'acca, 0x8383'3883'8383'3883);
  TestLdClr<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0xffff'ffff'ffff'ffff);
}

template <typename IntType>
void TestLdEor(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  static_assert(sizeof(IntType) <= 8);
  static_assert(std::is_unsigned<IntType>::value, "LdEor can only be used with unsigned types");

  auto val = TestAtomicMemOp(code, code_size, mem_val, src_val);
  EXPECT_EQ(val, static_cast<IntType>(mem_val ^ src_val));
}

TEST(TESTSUITE, LdEorByte) {
  static const uint32_t code[] = {
      0x3828'2002,  // ldeorb w8, w2, [x0]
  };

  TestLdEor<uint8_t>(code, sizeof(code), 0, 0);
  TestLdEor<uint8_t>(code, sizeof(code), 0xff, 0x0f);
  TestLdEor<uint8_t>(code, sizeof(code), 0xca, 0x83);
  TestLdEor<uint8_t>(code, sizeof(code), 0xff, 0xff);
}

TEST(TESTSUITE, LdEorHalfword) {
  static const uint32_t code[] = {
      0x7828'2002,  // ldeorh w8, w2, [x0]
  };

  TestLdEor<uint16_t>(code, sizeof(code), 0, 0);
  TestLdEor<uint16_t>(code, sizeof(code), 0xffff, 0x0ff0);
  TestLdEor<uint16_t>(code, sizeof(code), 0xcaca, 0x8383);
  TestLdEor<uint16_t>(code, sizeof(code), 0xffff, 0xffff);
}

TEST(TESTSUITE, LdEorWord) {
  static const uint32_t code[] = {
      0xb828'2002,  // ldeorw w8, w2, [x0]
  };

  TestLdEor<uint32_t>(code, sizeof(code), 0, 0);
  TestLdEor<uint32_t>(code, sizeof(code), 0xffff'ffff, 0x0ff0'0ff0);
  TestLdEor<uint32_t>(code, sizeof(code), 0xcaca'caca, 0x8383'8383);
  TestLdEor<uint32_t>(code, sizeof(code), 0xffff'ffff, 0xffff'ffff);
}

TEST(TESTSUITE, LdEorDoubleword) {
  static const uint32_t code[] = {
      0xf828'2002,  // ldeorl x8, x2, [x0]
  };

  TestLdEor<uint64_t>(code, sizeof(code), 0, 0);
  TestLdEor<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x0ff0'0ff0'0ff0'0ff0);
  TestLdEor<uint64_t>(code, sizeof(code), 0xcaca'caca'caca'caca, 0x8383'8383'8383'8383);
  TestLdEor<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0xffff'ffff'ffff'ffff);
}

template <typename IntType>
void TestLdSet(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  static_assert(sizeof(IntType) <= 8);
  static_assert(std::is_unsigned<IntType>::value, "LdSet can only be used with unsigned types");

  auto val = TestAtomicMemOp(code, code_size, mem_val, src_val);
  EXPECT_EQ(val, static_cast<IntType>(src_val | mem_val));
}

TEST(TESTSUITE, LdSetByte) {
  static const uint32_t code[] = {
      0x3828'3002,  // ldeorb w8, w2, [x0]
  };

  TestLdSet<uint8_t>(code, sizeof(code), 0, 0);
  TestLdSet<uint8_t>(code, sizeof(code), 0, 0xff);
  TestLdSet<uint8_t>(code, sizeof(code), 0xca, 0x83);
  TestLdSet<uint8_t>(code, sizeof(code), 0xff, 0xff);
}

TEST(TESTSUITE, LdSetHalfword) {
  static const uint32_t code[] = {
      0x7828'3002,  // ldeorh w8, w2, [x0]
  };

  TestLdSet<uint16_t>(code, sizeof(code), 0, 0);
  TestLdSet<uint16_t>(code, sizeof(code), 0, 0xffff);
  TestLdSet<uint16_t>(code, sizeof(code), 0xcaca, 0x8383);
  TestLdSet<uint16_t>(code, sizeof(code), 0xffff, 0xffff);
}

TEST(TESTSUITE, LdSetWord) {
  static const uint32_t code[] = {
      0xb828'3002,  // ldeorw w8, w2, [x0]
  };

  TestLdSet<uint32_t>(code, sizeof(code), 0, 0);
  TestLdSet<uint32_t>(code, sizeof(code), 0, 0xffff'ffff);
  TestLdSet<uint32_t>(code, sizeof(code), 0xcaca'caca, 0x8383'8383);
  TestLdSet<uint32_t>(code, sizeof(code), 0xffff'ffff, 0xffff'ffff);
}

TEST(TESTSUITE, LdSetDoubleword) {
  static const uint32_t code[] = {
      0xf828'3002,  // ldeorl x8, x2, [x0]
  };

  TestLdSet<uint64_t>(code, sizeof(code), 0, 0);
  TestLdSet<uint64_t>(code, sizeof(code), 0, 0xffff'ffff'ffff'ffff);
  TestLdSet<uint64_t>(code, sizeof(code), 0xcaca'caca'caca'caca, 0x8383'8383'8383'8383);
  TestLdSet<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0xffff'ffff'ffff'ffff);
}

template <typename IntType>
void TestLdMax(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  static_assert(sizeof(IntType) <= 8);

  auto val = TestAtomicMemOp(code, code_size, mem_val, src_val);
  EXPECT_EQ(val, std::max(src_val, mem_val));
}

TEST(TESTSUITE, LdSMaxByte) {
  static const uint32_t code[] = {
      0x3828'4002,  // ldsmaxb w8, w2, [x0]
  };

  TestLdMax<int8_t>(code, sizeof(code), 0, 0);
  TestLdMax<int8_t>(code, sizeof(code), 0, 0xff);
  TestLdMax<int8_t>(code, sizeof(code), 0xca, 0x83);
  TestLdMax<int8_t>(code, sizeof(code), 0xff, 0x7f);
}

TEST(TESTSUITE, LdSMaxHalfword) {
  static const uint32_t code[] = {
      0x7828'4002,  // ldsmaxh w8, w2, [x0]
  };

  TestLdMax<int16_t>(code, sizeof(code), 0, 0);
  TestLdMax<int16_t>(code, sizeof(code), 0, 0xffff);
  TestLdMax<int16_t>(code, sizeof(code), 0xcaca, 0x8383);
  TestLdMax<int16_t>(code, sizeof(code), 0xffff, 0x7fff);
}

TEST(TESTSUITE, LdSMaxWord) {
  static const uint32_t code[] = {
      0xb828'4002,  // ldsmax w8, w2, [x0]
  };

  TestLdMax<int32_t>(code, sizeof(code), 0, 0);
  TestLdMax<int32_t>(code, sizeof(code), 0, 0xffff'ffff);
  TestLdMax<int32_t>(code, sizeof(code), 0xcaca'caca, 0x8383'8383);
  TestLdMax<int32_t>(code, sizeof(code), 0xffff'ffff, 0x7fff'ffff);
}

TEST(TESTSUITE, LdSMaxDoubleword) {
  static const uint32_t code[] = {
      0xf828'4002,  // ldsmaxl x8, x2, [x0]
  };

  TestLdMax<int64_t>(code, sizeof(code), 0, 0);
  TestLdMax<int64_t>(code, sizeof(code), 0, 0xffff'ffff'ffff'ffff);
  TestLdMax<int64_t>(code, sizeof(code), 0xcaca'caca'caca'caca, 0x8383'8383'8383'8383);
  TestLdMax<int64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x7fff'ffff'ffff'ffff);
}

template <typename IntType>
void TestLdMin(const uint32_t code[], size_t code_size, IntType mem_val, IntType src_val) {
  static_assert(sizeof(IntType) <= 8);

  auto val = TestAtomicMemOp(code, code_size, mem_val, src_val);
  EXPECT_EQ(val, std::min(src_val, mem_val));
}

TEST(TESTSUITE, LdSMinByte) {
  static const uint32_t code[] = {
      0x3828'5002,  // ldsminb w8, w2, [x0]
  };

  TestLdMin<int8_t>(code, sizeof(code), 0, 0);
  TestLdMin<int8_t>(code, sizeof(code), 0, 0xff);
  TestLdMin<int8_t>(code, sizeof(code), 0x4a, 0x43);
  TestLdMin<int8_t>(code, sizeof(code), 0xff, 0x7f);
}

TEST(TESTSUITE, LdSMinHalfword) {
  static const uint32_t code[] = {
      0x7828'5002,  // ldsminh w8, w2, [x0]
  };

  TestLdMin<int16_t>(code, sizeof(code), 0, 0);
  TestLdMin<int16_t>(code, sizeof(code), 0, 0xffff);
  TestLdMin<int16_t>(code, sizeof(code), 0x7aca, 0x7383);
  TestLdMin<int16_t>(code, sizeof(code), 0xffff, 0x7fff);
}

TEST(TESTSUITE, LdSMinWord) {
  static const uint32_t code[] = {
      0xb828'5002,  // ldsmin w8, w2, [x0]
  };

  TestLdMin<int32_t>(code, sizeof(code), 0, 0);
  TestLdMin<int32_t>(code, sizeof(code), 0, 0xffff'ffff);
  TestLdMin<int32_t>(code, sizeof(code), 0x7aca'caca, 0x7383'8383);
  TestLdMin<int32_t>(code, sizeof(code), 0xffff'ffff, 0x7fff'ffff);
}

TEST(TESTSUITE, LdSMinDoubleword) {
  static const uint32_t code[] = {
      0xf828'5002,  // ldsminl x8, x2, [x0]
  };

  TestLdMin<int64_t>(code, sizeof(code), 0, 0);
  TestLdMin<int64_t>(code, sizeof(code), 0, 0xffff'ffff'ffff'ffff);
  TestLdMin<int64_t>(code, sizeof(code), 0x7aca'caca'caca'caca, 0x7383'8383'8383'8383);
  TestLdMin<int64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x7fff'ffff'ffff'ffff);
}

TEST(TESTSUITE, LdUMaxByte) {
  static const uint32_t code[] = {
      0x3828'6002,  // ldumaxb w8, w2, [x0]
  };

  TestLdMax<uint8_t>(code, sizeof(code), 0, 0);
  TestLdMax<uint8_t>(code, sizeof(code), 0, 0xff);
  TestLdMax<uint8_t>(code, sizeof(code), 0xca, 0x83);
  TestLdMax<uint8_t>(code, sizeof(code), 0xff, 0x7f);
}

TEST(TESTSUITE, LdUMaxHalfword) {
  static const uint32_t code[] = {
      0x7828'6002,  // ldumaxh w8, w2, [x0]
  };

  TestLdMax<uint16_t>(code, sizeof(code), 0, 0);
  TestLdMax<uint16_t>(code, sizeof(code), 0, 0xffff);
  TestLdMax<uint16_t>(code, sizeof(code), 0xcaca, 0x8383);
  TestLdMax<uint16_t>(code, sizeof(code), 0xffff, 0x7fff);
}

TEST(TESTSUITE, LdUMaxWord) {
  static const uint32_t code[] = {
      0xb828'6002,  // ldumax w8, w2, [x0]
  };

  TestLdMax<uint32_t>(code, sizeof(code), 0, 0);
  TestLdMax<uint32_t>(code, sizeof(code), 0, 0xffff'ffff);
  TestLdMax<uint32_t>(code, sizeof(code), 0xcaca'caca, 0x8383'8383);
  TestLdMax<uint32_t>(code, sizeof(code), 0xffff'ffff, 0x7fff'ffff);
}

TEST(TESTSUITE, LdUMaxDoubleword) {
  static const uint32_t code[] = {
      0xf828'6002,  // ldumaxl x8, x2, [x0]
  };

  TestLdMax<uint64_t>(code, sizeof(code), 0, 0);
  TestLdMax<uint64_t>(code, sizeof(code), 0, 0xffff'ffff'ffff'ffff);
  TestLdMax<uint64_t>(code, sizeof(code), 0xcaca'caca'caca'caca, 0x8383'8383'8383'8383);
  TestLdMax<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x7fff'ffff'ffff'ffff);
}

TEST(TESTSUITE, LdUMinByte) {
  static const uint32_t code[] = {
      0x3828'7002,  // lduminb w8, w2, [x0]
  };

  TestLdMin<uint8_t>(code, sizeof(code), 0, 0);
  TestLdMin<uint8_t>(code, sizeof(code), 0, 0xff);
  TestLdMin<uint8_t>(code, sizeof(code), 0xca, 0x83);
  TestLdMin<uint8_t>(code, sizeof(code), 0xff, 0x7f);
}

TEST(TESTSUITE, LdUMinHalfword) {
  static const uint32_t code[] = {
      0x7828'7002,  // lduminh w8, w2, [x0]
  };

  TestLdMin<uint16_t>(code, sizeof(code), 0, 0);
  TestLdMin<uint16_t>(code, sizeof(code), 0, 0xffff);
  TestLdMin<uint16_t>(code, sizeof(code), 0xcaca, 0x8383);
  TestLdMin<uint16_t>(code, sizeof(code), 0xffff, 0x7fff);
}

TEST(TESTSUITE, LdUMinWord) {
  static const uint32_t code[] = {
      0xb828'7002,  // ldumin w8, w2, [x0]
  };

  TestLdMin<uint32_t>(code, sizeof(code), 0, 0);
  TestLdMin<uint32_t>(code, sizeof(code), 0, 0xffff'ffff);
  TestLdMin<uint32_t>(code, sizeof(code), 0xcaca'caca, 0x8383'8383);
  TestLdMin<uint32_t>(code, sizeof(code), 0xffff'ffff, 0x7fff'ffff);
}

TEST(TESTSUITE, LdUMinDoubleword) {
  static const uint32_t code[] = {
      0xf828'7002,  // lduminl x8, x2, [x0]
  };

  TestLdMin<uint64_t>(code, sizeof(code), 0, 0);
  TestLdMin<uint64_t>(code, sizeof(code), 0, 0xffff'ffff'ffff'ffff);
  TestLdMin<uint64_t>(code, sizeof(code), 0xcaca'caca'caca'caca, 0x8383'8383'8383'8383);
  TestLdMin<uint64_t>(code, sizeof(code), 0xffff'ffff'ffff'ffff, 0x7fff'ffff'ffff'ffff);
}

TEST(TESTSUITE, UndefinedInsn) {
  static const uint32_t code[] = {
      0x0000'0000,  // undefined insn
  };

  TestHarness test(code, sizeof(code));

  ASSERT_EXIT(test.Run(), testing::KilledBySignal(SIGILL), "");
}

template <typename DataType>
void TestCompareAndSwap(const uint32_t code[],
                        size_t code_size,
                        DataType mem_val,
                        DataType src_val,
                        DataType target_val) {
  static_assert(std::is_unsigned<DataType>::value, "DataType must be unsigned");
  static_assert(sizeof(DataType) <= 8);

  constexpr uint32_t kAddrReg = 0, kDataReg = 8, kDataReg2 = 2;
  TestHarness test(code, code_size);

  alignas(16) uint8_t val[16];
  memset(val, 0x55, sizeof(val));
  DataType& typed_value = *reinterpret_cast<DataType*>(val);
  typed_value = mem_val;

  test.state()->cpu.x[kAddrReg] = ToGuestAddr(val);
  test.state()->cpu.x[kDataReg] = src_val;
  test.state()->cpu.x[kDataReg2] = target_val;

  EXPECT_TRUE(test.Run());

  if (mem_val == src_val) {
    EXPECT_EQ(typed_value, target_val);
    EXPECT_EQ(test.state()->cpu.x[kDataReg], static_cast<uint64_t>(src_val));
  } else {
    EXPECT_EQ(typed_value, mem_val);
    EXPECT_EQ(test.state()->cpu.x[kDataReg], static_cast<uint64_t>(mem_val));
  }

  int8_t filler[16];
  memset(filler, 0x55, sizeof(filler));
  EXPECT_EQ(memcmp(val + sizeof(DataType), filler, 16 - sizeof(DataType)), 0);
}

TEST(TESTSUITE, CompareAndSwapByte) {
  static const uint32_t code[] = {
      0x08e8'7c02,  // casab w8, w2, [x0]
  };

  TestCompareAndSwap<uint8_t>(code, sizeof(code), 0xfe, 0xfe, 40);
  TestCompareAndSwap<uint8_t>(code, sizeof(code), 0xfe, 100, 30);
}

TEST(TESTSUITE, CompareAndSwapHalf) {
  static const uint32_t code[] = {
      0x48e8'7c02,  // casah w8, w2, [x0]
  };

  TestCompareAndSwap<uint16_t>(code, sizeof(code), 0xfedc, 0xfedc, 24200);
  TestCompareAndSwap<uint16_t>(code, sizeof(code), 0xfedc, 30000, 27500);
}

TEST(TESTSUITE, CompareAndSwapWord) {
  static const uint32_t code[] = {
      0x88e8'7c02,  // casa w8, w2, [x0]
  };

  TestCompareAndSwap<uint32_t>(code, sizeof(code), 0xfedc'ba98, 0xfedc'ba98, 400000);
  TestCompareAndSwap<uint32_t>(code, sizeof(code), 0xfedc'ba98, 100000, 5000000);
}

TEST(TESTSUITE, CompareAndSwapDoubleWord) {
  static const uint32_t code[] = {
      0xc8e8'7c02,  // casa x8, x2, [x0]
  };

  TestCompareAndSwap<uint64_t>(
      code, sizeof(code), 0xfedc'ba98'7654'3210LL, 0xfedc'ba98'7654'3210LL, 1LL << 43);
  TestCompareAndSwap<uint64_t>(code, sizeof(code), 0xfedc'ba98'7654'3210LL, 1LL << 39, 1LL << 50);
}

template <typename DataType, typename DataTypePair>
void TestCompareAndSwapPair(const uint32_t code[],
                            size_t code_size,
                            DataTypePair mem_val,
                            DataType src_val,
                            DataType src_val_pair,
                            DataType target_val,
                            DataType target_val_pair) {
  static_assert(sizeof(DataTypePair) <= 16);
  static_assert(sizeof(DataType) == sizeof(DataTypePair) / 2);

  constexpr uint32_t kAddrReg = 0, kDataReg = 4, kDataReg2 = 2;
  TestHarness test(code, code_size);

  alignas(16) uint8_t val[32];
  memset(val, 0x55, sizeof(val));
  DataTypePair& typed_value = *reinterpret_cast<DataTypePair*>(val);
  typed_value = mem_val;

  test.state()->cpu.x[kAddrReg] = ToGuestAddr(val);
  test.state()->cpu.x[kDataReg] = src_val;
  test.state()->cpu.x[kDataReg + 1] = src_val_pair;
  test.state()->cpu.x[kDataReg2] = target_val;
  test.state()->cpu.x[kDataReg2 + 1] = target_val_pair;

  EXPECT_TRUE(test.Run());

  uint8_t type_bits = (sizeof(DataType) * 8);
  DataTypePair src_merged =
      (static_cast<DataTypePair>(src_val_pair) << type_bits) | (static_cast<DataType>(src_val));
  DataTypePair target_merged = (static_cast<DataTypePair>(target_val_pair) << type_bits) |
                               (static_cast<DataType>(target_val));

  if (mem_val == src_merged) {
    EXPECT_EQ(typed_value, target_merged);
    EXPECT_EQ(test.state()->cpu.x[kDataReg], src_val);
    EXPECT_EQ(test.state()->cpu.x[kDataReg + 1], src_val_pair);
  } else {
    EXPECT_EQ(typed_value, mem_val);
    EXPECT_EQ(test.state()->cpu.x[kDataReg], static_cast<DataType>(mem_val));
    EXPECT_EQ(test.state()->cpu.x[kDataReg + 1], static_cast<DataType>(mem_val >> type_bits));
  }

  int8_t filler[32];
  memset(filler, 0x55, sizeof(filler));
  EXPECT_EQ(memcmp(val + sizeof(DataTypePair), filler, 32 - sizeof(DataTypePair)), 0);
}

TEST(TESTSUITE, CompareAndSwapPair32Bit) {
  static const uint32_t code[] = {
      0x0864'7c02,  // caspa w4, w5, w2, w3, [x0]
  };

  TestCompareAndSwapPair<uint32_t, uint64_t>(code,
                                             sizeof(code),
                                             0xfedc'fedc'aaaa'bbbb | 0xaaaa'aaaa,
                                             0xfedc'fedc,
                                             0xaaaa'aaaa,
                                             1LL << 30,
                                             1 << 20);
  TestCompareAndSwapPair<uint32_t, uint64_t>(
      code, sizeof(code), (1LL << 50) | (1 << 14), 1 << 13, 1 << 20, 1 << 25, 1 << 21);
}

TEST(TESTSUITE, CompareAndSwapPair64Bit) {
  static const uint32_t code[] = {
      0x4864'7c02,  // caspa x4, x5, x2, x3, [x0]
  };

  TestCompareAndSwapPair<uint64_t, __int128>(
      code,
      sizeof(code),
      MakeUInt128(0xfedc'fedc'fedc'fedc, 0xaaaa'aaaa'aaaa'aaaa),
      0xaaaa'aaaa'aaaa'aaaa,
      0xfedc'fedc'fedc'fedc,
      1LL << 60,
      1LL << 40);
  TestCompareAndSwapPair<uint64_t, __int128>(
      code,
      sizeof(code),
      MakeUInt128(0xfedc'fedc'fedc'fedc, 0xaaaa'aaaa'aaaa'aaaa),
      0xfedc'fedc'fedc'fedc,
      0xaaaa'aaaa'aaaa'aaaa,
      1LL << 39,
      1LL << 52);
}

enum BinaryFloatOperators {
  kAdd,
  kSub,
  kDiv,
  kMul,
};

template <typename FloatType>
void TestBinOpFloat(const uint32_t code[],
                    size_t code_size,
                    FloatType op1,
                    FloatType op2,
                    uint32_t result_reg,
                    BinaryFloatOperators op) {
  using IntType = std::conditional_t<std::is_same_v<FloatType, float>, uint32_t, uint64_t>;

  constexpr uint32_t kOp1Reg = 0, kOp2Reg = 1;
  TestHarness test(code, code_size);

  test.state()->cpu.v[kOp1Reg] = std::bit_cast<IntType>(op1);
  test.state()->cpu.v[kOp2Reg] = std::bit_cast<IntType>(op2);

  EXPECT_TRUE(test.Run());

  FloatType result;
  switch (op) {
    case kAdd:
      result = op1 + op2;
      break;
    case kSub:
      result = op1 - op2;
      break;
    case kDiv:
      result = op1 / op2;
      break;
    case kMul:
      result = op1 * op2;
      break;
  }

  EXPECT_EQ(test.state()->cpu.v[result_reg], std::bit_cast<IntType>(result));
}

TEST(TESTSUITE, AddFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'2802,  // fadd s2, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 357.838f, 515.883f, 2, kAdd);
}

TEST(TESTSUITE, AssignAddFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'2800,  // fadd s0, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 213.671f, 853.135f, 0, kAdd);
}

TEST(TESTSUITE, AddFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'2802,  // fadd d2, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 357681.8382416, 515.81233583, 2, kAdd);
}

TEST(TESTSUITE, AssignAddFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'2800,  // fadd d0, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 2139801.6271, 853910.1357801, 0, kAdd);
}

TEST(TESTSUITE, SubFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'3802,  // fsub s2, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 641.841f, 616.83f, 2, kSub);
}

TEST(TESTSUITE, AssignSubFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'3800,  // fsub s0, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 416.563f, 243.175f, 0, kSub);
}

TEST(TESTSUITE, SubFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'3802,  // fsub d2, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 780135.780135, 725.81233583, 2, kSub);
}

TEST(TESTSUITE, AssignSubFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'3800,  // fsub d0, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 5178051.513, 15781.518073, 0, kSub);
}

TEST(TESTSUITE, DivFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'1802,  // fdiv s2, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 746.841f, 125.83f, 2, kDiv);
}

TEST(TESTSUITE, AssignDivFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'1800,  // fdiv s0, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 135.563f, 243.512, 0, kDiv);
}

TEST(TESTSUITE, DivFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'1802,  // fdiv d2, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 780135.14661, 1441.81233583, 2, kDiv);
}

TEST(TESTSUITE, AssignDivFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'1800,  // fdiv d0, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 1345145.513, 15781.16461, 0, kDiv);
}

TEST(TESTSUITE, MulFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'0802,  // fmul s2, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 641.136, 764.83f, 2, kMul);
}

TEST(TESTSUITE, AssignMulFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'0800,  // fmul s0, s0, s1
  };

  TestBinOpFloat<float>(code, sizeof(code), 416.136, 555.175f, 0, kMul);
}

TEST(TESTSUITE, MulFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'0802,  // fmul d2, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 356363.780135, 123.81233583, 2, kMul);
}

TEST(TESTSUITE, AssignMulFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'0800,  // fmul d0, d0, d1
  };

  TestBinOpFloat<double>(code, sizeof(code), 353245.513, 15781.63563, 0, kMul);
}

enum FloatComparisonOperators {
  kLt,
  kGt,
  kLe,
  kGe,
  kEq,
  kNe,
};

template <typename FloatType>
void TestComparisonFloat(const uint32_t code[],
                         size_t code_size,
                         FloatType op1,
                         FloatType op2,
                         FloatComparisonOperators op) {
  using IntType = std::conditional_t<std::is_same_v<FloatType, float>, uint32_t, uint64_t>;

  constexpr uint32_t kOp1Reg = 0, kOp2Reg = 1;
  TestHarness test(code, code_size);

  test.state()->cpu.v[kOp1Reg] = std::bit_cast<IntType>(op1);
  test.state()->cpu.v[kOp2Reg] = std::bit_cast<IntType>(op2);
  test.SetNZCV(0b0000);

  EXPECT_TRUE(test.Run());

  uint16_t actual_nzcv = GetNZCV(&test.state()->cpu) & MakeNZCV(0b1111);
  uint16_t lt_nzcv = MakeNZCV(1, 0, 0, 0);
  uint16_t gt_nzcv = MakeNZCV(0, 0, 1, 0);
  uint16_t eq_nzcv = MakeNZCV(0, 1, 1, 0);
  switch (op) {
    case kLt:
      EXPECT_EQ(actual_nzcv, lt_nzcv);
      break;
    case kGt:
      EXPECT_EQ(actual_nzcv, gt_nzcv);
      break;
    case kLe:
      EXPECT_TRUE(actual_nzcv == lt_nzcv || actual_nzcv == eq_nzcv);
      break;
    case kGe:
      EXPECT_TRUE(actual_nzcv == gt_nzcv || actual_nzcv == eq_nzcv);
      break;
    case kEq:
      EXPECT_EQ(actual_nzcv, eq_nzcv);
      break;
    case kNe:
      EXPECT_TRUE(actual_nzcv == lt_nzcv || actual_nzcv == gt_nzcv);
      break;
  }
}

TEST(TESTSUITE, CmpFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'2000,  // fcmp s0, s1
  };

  TestComparisonFloat<float>(code, sizeof(code), 123.456f, 531.786f, kLt);
  TestComparisonFloat<float>(code, sizeof(code), 456.456f, 456.459f, kLt);
  TestComparisonFloat<float>(code, sizeof(code), 987.361f, 756.352f, kGt);
  TestComparisonFloat<float>(code, sizeof(code), 778.361f, 778.352f, kGt);
  TestComparisonFloat<float>(code, sizeof(code), 315.361f, 796.752f, kLe);
  TestComparisonFloat<float>(code, sizeof(code), 441.751f, 441.751f, kLe);
  TestComparisonFloat<float>(code, sizeof(code), 861.881f, 861.881f, kGe);
  TestComparisonFloat<float>(code, sizeof(code), 875.111f, 553.975f, kGe);
  TestComparisonFloat<float>(code, sizeof(code), 888.885f, 888.885f, kEq);
  TestComparisonFloat<float>(code, sizeof(code), 123.123f, 456.005f, kNe);
  TestComparisonFloat<float>(code, sizeof(code), 987.987f, 987.010f, kNe);
}

TEST(TESTSUITE, CmpFloat64bit) {
  static const uint32_t code[] = {
      0x1e61'2000,  // fcmp d0, d1
  };

  TestComparisonFloat<double>(code, sizeof(code), 345.511233, 15781.63563, kLt);
  TestComparisonFloat<double>(code, sizeof(code), 35454.511233, 35454.6353, kLt);
  TestComparisonFloat<double>(code, sizeof(code), 3414215.3413, 5113.631743, kGt);
  TestComparisonFloat<double>(code, sizeof(code), 123123.3413, 123123.34129, kGt);
  TestComparisonFloat<double>(code, sizeof(code), 836.153413, 9873.57843, kLe);
  TestComparisonFloat<double>(code, sizeof(code), 9999.1413, 9999.1413, kLe);
  TestComparisonFloat<double>(code, sizeof(code), 8888.1413, 8888.1413, kGe);
  TestComparisonFloat<double>(code, sizeof(code), 5141.178523, 864.146413, kGe);
  TestComparisonFloat<double>(code, sizeof(code), 5.135135135, 5.135135135, kEq);
  TestComparisonFloat<double>(code, sizeof(code), 7.135135135, 7.135135136, kNe);
  TestComparisonFloat<double>(code, sizeof(code), 5.888135, 5.135135135, kNe);
}

template <typename FloatType>
void TestNegationFloat(const uint32_t code[], size_t code_size, FloatType op) {
  using IntType = std::conditional_t<std::is_same_v<FloatType, float>, uint32_t, uint64_t>;

  constexpr uint32_t kOpReg = 0, kResultReg = 1;
  TestHarness test(code, code_size);

  test.state()->cpu.v[kOpReg] = std::bit_cast<IntType>(op);

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.v[kResultReg], std::bit_cast<IntType>(-op));
}

TEST(TESTSUITE, NegFloat32Bit) {
  static const uint32_t code[] = {
      0x1e21'4001,  // fneg s1, s0
  };

  TestNegationFloat<float>(code, sizeof(code), 123.123f);
  TestNegationFloat<float>(code, sizeof(code), -456.456f);
}

TEST(TESTSUITE, NegFloat64Bit) {
  static const uint32_t code[] = {
      0x1e61'4001,  // fneg d1, d0
  };

  TestNegationFloat<double>(code, sizeof(code), 123.123f);
  TestNegationFloat<double>(code, sizeof(code), -456.456f);
}

template <typename IntType>
void TestRoundingFloat(const uint32_t code[], size_t code_size, IntType op, IntType res) {
  static_assert(std::is_same_v<IntType, uint32_t> || std::is_same_v<IntType, uint64_t>);

  constexpr uint32_t kOpReg = 0, kResultReg = 1;
  TestHarness test(code, code_size);

  test.state()->cpu.v[kOpReg] = op;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.v[kResultReg], res);
}

// We take inputs that show the difference between rounding modes:
// Round to nearest (no tie): 123.25 vs 123.625
// Round to nearest (tie: away or to-even): 0.5, 1.5, -2.5, -3.5
// Toward zero: negative vs positive, e.g. -864.75 vs 123.625.
// To positive/negative infinity: all inputs
// For values too large to have a fractional part: 1e20, -1e20, 1e40, -1e40

TEST(TESTSUITE, RoundFloatTiesAway32Bit) {
  static const uint32_t code[] = {
      0x1e26'4001,  // frinta s1, s0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.25f), std::bit_cast<uint32_t>(123.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.625f), std::bit_cast<uint32_t>(124.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.75f), std::bit_cast<uint32_t>(-865.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.375f), std::bit_cast<uint32_t>(-864.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(0.5f), std::bit_cast<uint32_t>(1.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(2.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-2.5f), std::bit_cast<uint32_t>(-3.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-3.5f), std::bit_cast<uint32_t>(-4.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1e20f), std::bit_cast<uint32_t>(1e20f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-1e20f), std::bit_cast<uint32_t>(-1e20f));
}

TEST(TESTSUITE, RoundFloatMinusInf32Bit) {
  static const uint32_t code[] = {
      0x1e25'4001  // frintm s1, s0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.25f), std::bit_cast<uint32_t>(123.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.625f), std::bit_cast<uint32_t>(123.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.75f), std::bit_cast<uint32_t>(-865.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.375f), std::bit_cast<uint32_t>(-865.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(0.5f), std::bit_cast<uint32_t>(0.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(1.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-2.5f), std::bit_cast<uint32_t>(-3.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-3.5f), std::bit_cast<uint32_t>(-4.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1e20f), std::bit_cast<uint32_t>(1e20f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-1e20f), std::bit_cast<uint32_t>(-1e20f));
}

TEST(TESTSUITE, RoundFloatTiesEven32Bit) {
  static const uint32_t code[] = {
      0x1e24'4001  // frintn s1, s0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.25f), std::bit_cast<uint32_t>(123.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.625f), std::bit_cast<uint32_t>(124.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.75f), std::bit_cast<uint32_t>(-865.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.375f), std::bit_cast<uint32_t>(-864.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(0.5f), std::bit_cast<uint32_t>(0.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(2.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-2.5f), std::bit_cast<uint32_t>(-2.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-3.5f), std::bit_cast<uint32_t>(-4.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1e20f), std::bit_cast<uint32_t>(1e20f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-1e20f), std::bit_cast<uint32_t>(-1e20f));
}

TEST(TESTSUITE, RoundFloatPlusInf32Bit) {
  static const uint32_t code[] = {
      0x1e24'c001  // frintp s1, s0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.25f), std::bit_cast<uint32_t>(124.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.625f), std::bit_cast<uint32_t>(124.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.75f), std::bit_cast<uint32_t>(-864.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.375f), std::bit_cast<uint32_t>(-864.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(0.5f), std::bit_cast<uint32_t>(1.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(2.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-2.5f), std::bit_cast<uint32_t>(-2.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-3.5f), std::bit_cast<uint32_t>(-3.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1e20f), std::bit_cast<uint32_t>(1e20f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-1e20f), std::bit_cast<uint32_t>(-1e20f));
}

TEST(TESTSUITE, RoundFloatToZero32Bit) {
  static const uint32_t code[] = {
      0x1e25'c001  // frintz s1, s0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.25f), std::bit_cast<uint32_t>(123.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(123.625f), std::bit_cast<uint32_t>(123.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.75f), std::bit_cast<uint32_t>(-864.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-864.375f), std::bit_cast<uint32_t>(-864.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(0.5f), std::bit_cast<uint32_t>(0.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1.5f), std::bit_cast<uint32_t>(1.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-2.5f), std::bit_cast<uint32_t>(-2.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-3.5f), std::bit_cast<uint32_t>(-3.0f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(1e20f), std::bit_cast<uint32_t>(1e20f));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint32_t>(-1e20f), std::bit_cast<uint32_t>(-1e20f));
}

TEST(TESTSUITE, RoundFloatTiesAway64Bit) {
  static const uint32_t code[] = {
      0x1e66'4001  // frinta d1, d0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.5), std::bit_cast<uint64_t>(35455.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.25), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.125), std::bit_cast<uint64_t>(-5141.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.875), std::bit_cast<uint64_t>(-5142.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015130.5),
                      std::bit_cast<uint64_t>(-58015131.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015131.5),
                      std::bit_cast<uint64_t>(-58015132.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748108.5), std::bit_cast<uint64_t>(6748109.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748109.5), std::bit_cast<uint64_t>(6748110.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(1e40), std::bit_cast<uint64_t>(1e40));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-1e40), std::bit_cast<uint64_t>(-1e40));
}

TEST(TESTSUITE, RoundFloatMinusInf64Bit) {
  static const uint32_t code[] = {
      0x1e65'4001  // frintm d1, d0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.5), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.25), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.125), std::bit_cast<uint64_t>(-5142.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.875), std::bit_cast<uint64_t>(-5142.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015130.5),
                      std::bit_cast<uint64_t>(-58015131.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015131.5),
                      std::bit_cast<uint64_t>(-58015132.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748108.5), std::bit_cast<uint64_t>(6748108.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748109.5), std::bit_cast<uint64_t>(6748109.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(1e40), std::bit_cast<uint64_t>(1e40));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-1e40), std::bit_cast<uint64_t>(-1e40));
}

TEST(TESTSUITE, RoundFloatTiesEven64Bit) {
  static const uint32_t code[] = {
      0x1e64'4001  // frintn d1, d0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.5), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.25), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.125), std::bit_cast<uint64_t>(-5141.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.875), std::bit_cast<uint64_t>(-5142.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015130.5),
                      std::bit_cast<uint64_t>(-58015130.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015131.5),
                      std::bit_cast<uint64_t>(-58015132.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748108.5), std::bit_cast<uint64_t>(6748108.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748109.5), std::bit_cast<uint64_t>(6748110.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(1e40), std::bit_cast<uint64_t>(1e40));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-1e40), std::bit_cast<uint64_t>(-1e40));
}

TEST(TESTSUITE, RoundFloatPlusInf64Bit) {
  static const uint32_t code[] = {
      0x1e64'c001  // frintp d1, d0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.5), std::bit_cast<uint64_t>(35455.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.25), std::bit_cast<uint64_t>(35455.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.125), std::bit_cast<uint64_t>(-5141.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.875), std::bit_cast<uint64_t>(-5141.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015130.5),
                      std::bit_cast<uint64_t>(-58015130.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015131.5),
                      std::bit_cast<uint64_t>(-58015131.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748108.5), std::bit_cast<uint64_t>(6748109.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748109.5), std::bit_cast<uint64_t>(6748110.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(1e40), std::bit_cast<uint64_t>(1e40));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-1e40), std::bit_cast<uint64_t>(-1e40));
}

TEST(TESTSUITE, RoundFloatToZero64Bit) {
  static const uint32_t code[] = {
      0x1e65'c001  // frintz d1, d0
  };

  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.5), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(35454.25), std::bit_cast<uint64_t>(35454.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.125), std::bit_cast<uint64_t>(-5141.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-5141.875), std::bit_cast<uint64_t>(-5141.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015130.5),
                      std::bit_cast<uint64_t>(-58015130.0));
  TestRoundingFloat<>(code,
                      sizeof(code),
                      std::bit_cast<uint64_t>(-58015131.5),
                      std::bit_cast<uint64_t>(-58015131.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748108.5), std::bit_cast<uint64_t>(6748108.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(6748109.5), std::bit_cast<uint64_t>(6748109.0));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(1e40), std::bit_cast<uint64_t>(1e40));
  TestRoundingFloat<>(
      code, sizeof(code), std::bit_cast<uint64_t>(-1e40), std::bit_cast<uint64_t>(-1e40));
}

template <typename FloatTypeFrom, typename FloatTypeTo>
void TestConversionFptoFp(const uint32_t code[],
                          size_t code_size,
                          FloatTypeFrom op,
                          FloatTypeTo res) {
  using IntTypeFrom = std::conditional_t<
      std::is_same_v<FloatTypeFrom, _Float16>,
      uint16_t,
      std::conditional_t<std::is_same_v<FloatTypeFrom, float>, uint32_t, uint64_t>>;

  using IntTypeTo = std::conditional_t<
      std::is_same_v<FloatTypeTo, _Float16>,
      uint16_t,
      std::conditional_t<std::is_same_v<FloatTypeTo, float>, uint32_t, uint64_t>>;

  constexpr uint32_t kArgReg = 0, kResultReg = 1;
  TestHarness test(code, code_size);

  test.state()->cpu.v[kArgReg] = std::bit_cast<IntTypeFrom>(op);

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.v[kResultReg], std::bit_cast<IntTypeTo>(res));
}

// The test inputs check whether the correct signed/unsigned conversion is done and also whether
// floating values with/without decimals after the comma are converted correctly.

TEST(TESTSUITE, ConvertF32toF16) {
  static const uint32_t code[] = {
      0x1e23'c001  // fcvt h1, s0
  };

  TestConversionFptoFp<float, _Float16>(code, sizeof(code), 123.25f, _Float16{123.25});
  TestConversionFptoFp<float, _Float16>(code, sizeof(code), -431.5f, _Float16{-431.5});
  TestConversionFptoFp<float, _Float16>(code, sizeof(code), -3.5f, _Float16{-3.5});
  TestConversionFptoFp<float, _Float16>(code, sizeof(code), 20.0f, _Float16{20.0});
}

TEST(TESTSUITE, ConvertF64toF16) {
  static const uint32_t code[] = {
      0x1e63'c001  // fcvt h1, d0
  };

  TestConversionFptoFp<double, _Float16>(code, sizeof(code), 10.0, _Float16{10.0});
  TestConversionFptoFp<double, _Float16>(code, sizeof(code), -5.0, _Float16{-5.0});
  TestConversionFptoFp<double, _Float16>(code, sizeof(code), -4.5, _Float16{-4.5});
  TestConversionFptoFp<double, _Float16>(code, sizeof(code), 1.25, _Float16{1.25});
}

TEST(TESTSUITE, ConvertF16toF32) {
  static const uint32_t code[] = {
      0x1ee2'4001  // fcvt s1, h0
  };

  TestConversionFptoFp<_Float16, float>(code, sizeof(code), _Float16{4.0}, 4.0f);
  TestConversionFptoFp<_Float16, float>(code, sizeof(code), _Float16{-2.0}, -2.0f);
  TestConversionFptoFp<_Float16, float>(code, sizeof(code), _Float16{1.5}, 1.5f);
  TestConversionFptoFp<_Float16, float>(code, sizeof(code), _Float16{-0.125}, -0.125f);
}

TEST(TESTSUITE, ConvertF64toF32) {
  static const uint32_t code[] = {
      0x1e62'4001  // fcvt s1, d0
  };

  TestConversionFptoFp<double, float>(code, sizeof(code), 16748.625, 16748.625f);
  TestConversionFptoFp<double, float>(code, sizeof(code), 10135.0, 10135.0f);
  TestConversionFptoFp<double, float>(code, sizeof(code), -5135.0, -5135.0f);
  TestConversionFptoFp<double, float>(code, sizeof(code), -415.75, -415.75f);
}

TEST(TESTSUITE, ConvertF16toF64) {
  static const uint32_t code[] = {
      0x1ee2'c001  // fcvt d1, h0
  };

  TestConversionFptoFp<_Float16, double>(code, sizeof(code), _Float16{20.0}, 20.0);
  TestConversionFptoFp<_Float16, double>(code, sizeof(code), _Float16{-30.0}, -30.0);
  TestConversionFptoFp<_Float16, double>(code, sizeof(code), _Float16{14.25}, 14.25);
  TestConversionFptoFp<_Float16, double>(code, sizeof(code), _Float16{-56.875}, -56.875);
}

TEST(TESTSUITE, ConvertF32toF64) {
  static const uint32_t code[] = {
      0x1e22'c001  // fcvt d1, s0
  };

  TestConversionFptoFp<float, double>(code, sizeof(code), 513.625f, 513.625);
  TestConversionFptoFp<float, double>(code, sizeof(code), -2123.25f, -2123.25);
  TestConversionFptoFp<float, double>(code, sizeof(code), 1331.0f, 1331.0);
  TestConversionFptoFp<float, double>(code, sizeof(code), -51533.0f, -51533.0);
}

template <typename IntTypeFrom, typename FloatTypeTo>
void TestConversionInttoFp(const uint32_t code[],
                           size_t code_size,
                           IntTypeFrom op,
                           FloatTypeTo res) {
  static_assert(std::is_integral_v<IntTypeFrom> &&
                (sizeof(IntTypeFrom) == 4 || sizeof(IntTypeFrom) == 8));
  using IntTypeTo = std::conditional_t<std::is_same_v<FloatTypeTo, float>, uint32_t, uint64_t>;

  constexpr uint32_t kOpReg = 0, kResultReg = 1;
  TestHarness test(code, code_size);

  test.state()->cpu.x[kOpReg] = op;

  EXPECT_TRUE(test.Run());

  EXPECT_EQ(test.state()->cpu.v[kResultReg], std::bit_cast<IntTypeTo>(res));
}

// The test inputs check whether the correct signed/unsigned conversion is done and in the case of
// 64bit integers to 32bit floats there is an extra input that checks clipping.

TEST(TESTSUITE, ConvertI32toF32) {
  static const uint32_t code[] = {
      0x1e22'0001  // scvtf s1, w0
  };

  TestConversionInttoFp<int32_t, float>(code, sizeof(code), 51748, 51748.0f);
  TestConversionInttoFp<int32_t, float>(code, sizeof(code), -145, -145.0f);
}

TEST(TESTSUITE, ConvertU32toF32) {
  static const uint32_t code[] = {
      0x1e23'0001  // ucvtf s1, w0
  };

  TestConversionInttoFp<uint32_t, float>(code, sizeof(code), 715715, 715715.0f);
  TestConversionInttoFp<uint32_t, float>(code, sizeof(code), uint32_t{1U} << 31, 2147483648.0f);
}

TEST(TESTSUITE, ConvertI64toF32) {
  static const uint32_t code[] = {
      0x9e22'0001  // scvtf s1, x0
  };

  TestConversionInttoFp<int64_t, float>(code, sizeof(code), 417891, 417891.0f);
  TestConversionInttoFp<int64_t, float>(code, sizeof(code), -148147, -148147.0f);
}

TEST(TESTSUITE, ConvertU64toF32) {
  static const uint32_t code[] = {
      0x9e23'0001  // ucvtf s1, x0
  };

  TestConversionInttoFp<uint64_t, float>(code, sizeof(code), 6146416ULL, 6146416.0f);
  TestConversionInttoFp<uint64_t, float>(
      code, sizeof(code), uint64_t{1ULL} << 63, 9223372036854775808.0f);
}

TEST(TESTSUITE, ConvertI32toF64) {
  static const uint32_t code[] = {
      0x1e62'0001  // scvtf d1, w0
  };

  TestConversionInttoFp<int32_t, double>(code, sizeof(code), 5138751, 5138751.0);
  TestConversionInttoFp<int32_t, double>(code, sizeof(code), -1578041875, -1578041875.0);
}

TEST(TESTSUITE, ConvertU32toF64) {
  static const uint32_t code[] = {
      0x1e63'0001  // ucvtf d1, w0
  };

  TestConversionInttoFp<uint32_t, double>(code, sizeof(code), 635632U, 635632.0);
  TestConversionInttoFp<uint32_t, double>(code, sizeof(code), uint32_t{1U} << 31, 2147483648.0);
}

TEST(TESTSUITE, ConvertI64toF64) {
  static const uint32_t code[] = {
      0x9e62'0001  // scvtf d1, x0
  };

  TestConversionInttoFp<int64_t, double>(code, sizeof(code), 145708415780414LL, 145708415780414.0);
  TestConversionInttoFp<int64_t, double>(code, sizeof(code), -653899658398LL, -653899658398.0);
}

TEST(TESTSUITE, ConvertU64toF64) {
  static const uint32_t code[] = {
      0x9e63'0001  // ucvtf d1, x0
  };

  TestConversionInttoFp<uint64_t, double>(code, sizeof(code), 135785143541ULL, 135785143541.0);
  TestConversionInttoFp<uint64_t, double>(
      code, sizeof(code), uint64_t{1ULL} << 63, 9223372036854775808.0);
}
