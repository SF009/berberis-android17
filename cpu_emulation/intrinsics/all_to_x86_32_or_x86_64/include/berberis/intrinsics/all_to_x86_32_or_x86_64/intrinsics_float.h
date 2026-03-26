/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ALL_TO_X86_32_OR_x86_64_BERBERIS_INTRINSICS_INTRINSICS_FLOAT_H_
#define ALL_TO_X86_32_OR_x86_64_BERBERIS_INTRINSICS_INTRINSICS_FLOAT_H_

#include <bit>
#include <cmath>

#include "berberis/base/checks.h"
#include "berberis/intrinsics/common/intrinsics_float.h"  // Float32/Float64
#include "berberis/intrinsics/guest_rounding_modes.h"     // FE_HOSTROUND/FE_TIESAWAY

namespace berberis::intrinsics {

// Note that on x86-32 it's not safe to return float or double from function, even if that function
// is bit_cast! In addition to that we couldn't execute expressions that may use rounding modes in
// C++ because clang may “optimize” code by exchanging them with functions that change the rounding
// mode!

#define MAKE_BINARY_OPERATOR(guest_name, operator_name, assignment_name)        \
                                                                                \
  inline Float32 operator operator_name(const Float32& v1, const Float32& v2) { \
    float src1, src2;                                                           \
    static_assert(sizeof(src1) == sizeof(v1));                                  \
    memcpy(&src1, &v1, sizeof(v1));                                             \
    static_assert(sizeof(src2) == sizeof(v2));                                  \
    memcpy(&src2, &v2, sizeof(v2));                                             \
    asm volatile(#guest_name "ss %2,%0" : "=x"(src1) : "0"(src1), "x"(src2));   \
    return Float32{src1};                                                       \
  }                                                                             \
                                                                                \
  inline Float32& operator assignment_name(Float32& v1, const Float32& v2) {    \
    float src1, src2;                                                           \
    static_assert(sizeof(src1) == sizeof(v1));                                  \
    memcpy(&src1, &v1, sizeof(v1));                                             \
    static_assert(sizeof(src2) == sizeof(v2));                                  \
    memcpy(&src2, &v2, sizeof(v2));                                             \
    asm volatile(#guest_name "ss %2,%0" : "=x"(src1) : "0"(src1), "x"(src2));   \
    memcpy(&v1, &src1, sizeof(v1));                                             \
    return v1;                                                                  \
  }                                                                             \
                                                                                \
  inline Float64 operator operator_name(const Float64& v1, const Float64& v2) { \
    double src1, src2;                                                          \
    static_assert(sizeof(src1) == sizeof(v1));                                  \
    memcpy(&src1, &v1, sizeof(v1));                                             \
    static_assert(sizeof(src2) == sizeof(v2));                                  \
    memcpy(&src2, &v2, sizeof(v2));                                             \
    asm volatile(#guest_name "sd %2,%0" : "=x"(src1) : "0"(src1), "x"(src2));   \
    return Float64{src1};                                                       \
  }                                                                             \
                                                                                \
  inline Float64& operator assignment_name(Float64& v1, const Float64& v2) {    \
    double src1, src2;                                                          \
    static_assert(sizeof(src1) == sizeof(v1));                                  \
    memcpy(&src1, &v1, sizeof(v1));                                             \
    static_assert(sizeof(src2) == sizeof(v2));                                  \
    memcpy(&src2, &v2, sizeof(v2));                                             \
    asm volatile(#guest_name "sd %2,%0" : "=x"(src1) : "0"(src1), "x"(src2));   \
    memcpy(&v1, &src1, sizeof(v1));                                             \
    return v1;                                                                  \
  }

MAKE_BINARY_OPERATOR(add, +, +=)
MAKE_BINARY_OPERATOR(sub, -, -=)
MAKE_BINARY_OPERATOR(mul, *, *=)
MAKE_BINARY_OPERATOR(div, /, /=)

#undef MAKE_BINARY_OPERATOR

#define MAKE_BINARY_OPERATOR(guest_name, operands, check_name, operator_name) \
                                                                              \
  inline bool operator operator_name(const Float32& v1, const Float32& v2) {  \
    float src1, src2;                                                         \
    static_assert(sizeof(src1) == sizeof(v1));                                \
    memcpy(&src1, &v1, sizeof(v1));                                           \
    static_assert(sizeof(src2) == sizeof(v2));                                \
    memcpy(&src2, &v2, sizeof(v2));                                           \
    bool result;                                                              \
    asm volatile(#guest_name "ss " operands "\n " #check_name " %0"           \
                 : "=q"(result)                                               \
                 : "x"(src1), "x"(src2)                                       \
                 : "cc");                                                     \
    return result;                                                            \
  }                                                                           \
                                                                              \
  inline bool operator operator_name(const Float64& v1, const Float64& v2) {  \
    double src1, src2;                                                        \
    static_assert(sizeof(src1) == sizeof(v1));                                \
    memcpy(&src1, &v1, sizeof(v1));                                           \
    static_assert(sizeof(src2) == sizeof(v2));                                \
    memcpy(&src2, &v2, sizeof(v2));                                           \
    bool result;                                                              \
    asm volatile(#guest_name "sd " operands "\n " #check_name " %0"           \
                 : "=q"(result)                                               \
                 : "x"(src1), "x"(src2)                                       \
                 : "cc");                                                     \
    return result;                                                            \
  }

MAKE_BINARY_OPERATOR(ucomi, "%1,%2", seta, <)
MAKE_BINARY_OPERATOR(ucomi, "%2,%1", seta, >)
MAKE_BINARY_OPERATOR(ucomi, "%1,%2", setnb, <=)
MAKE_BINARY_OPERATOR(ucomi, "%2,%1", setnb, >=)

#undef MAKE_BINARY_OPERATOR

#define MAKE_BINARY_OPERATOR(guest_name, operator_name)                         \
                                                                                \
  inline bool operator operator_name(const Float32& v1, const Float32& v2) {    \
    float src1, src2;                                                           \
    static_assert(sizeof(src1) == sizeof(v1));                                  \
    memcpy(&src1, &v1, sizeof(v1));                                             \
    static_assert(sizeof(src2) == sizeof(v2));                                  \
    memcpy(&src2, &v2, sizeof(v2));                                             \
    float result;                                                               \
    asm volatile(#guest_name "ss %2,%0" : "=x"(result) : "0"(src1), "x"(src2)); \
    return std::bit_cast<uint32_t, float>(result) & 0x1;                        \
  }                                                                             \
                                                                                \
  inline bool operator operator_name(const Float64& v1, const Float64& v2) {    \
    double src1, src2;                                                          \
    static_assert(sizeof(src1) == sizeof(v1));                                  \
    memcpy(&src1, &v1, sizeof(v1));                                             \
    static_assert(sizeof(src2) == sizeof(v2));                                  \
    memcpy(&src2, &v2, sizeof(v2));                                             \
    double result;                                                              \
    asm volatile(#guest_name "sd %2,%0" : "=x"(result) : "0"(src1), "x"(src2)); \
    return std::bit_cast<uint64_t, double>(result) & 0x1;                       \
  }

MAKE_BINARY_OPERATOR(cmpeq, ==)
MAKE_BINARY_OPERATOR(cmpneq, !=)

#undef MAKE_BINARY_OPERATOR

// It's NOT safe to use ANY functions which return float or double.  That's because IA32 ABI uses
// x87 stack to pass arguments (and does that even with -mfpmath=sse) and NaN float and
// double values would be corrupted if pushed on it.

inline Float32 Negative(const Float32& v) {
  float result;
  static_assert(sizeof(result) == sizeof(v));
  memcpy(&result, &v, sizeof(v));
  uint64_t sign_bit = 0x8000'0000U;
  // TODO(b/120563432): Simple -v.value_ doesn't work after a clang update.
  asm volatile("pxor %2, %0" : "=x"(result) : "0"(result), "x"(sign_bit));
  return Float32{result};
}

inline Float64 Negative(const Float64& v) {
  // TODO(b/120563432): Simple -v.value_ doesn't work after a clang update.
  double result;
  static_assert(sizeof(result) == sizeof(v));
  memcpy(&result, &v, sizeof(v));
  uint64_t sign_bit = 0x8000'0000'0000'0000ULL;
  asm volatile("pxor %2, %0" : "=x"(result) : "0"(result), "x"(sign_bit));
  return Float64{result};
}

inline Float32 FPRound(const Float32& value, int round_control);
inline Float64 FPRound(const Float64& value, int round_control);

template <typename FloatType>
inline WrappedFloatType<FloatType> FPRoundTiesAwayPositive(WrappedFloatType<FloatType> value) {
  // Since x86 does not support this rounding mode exactly, we must manually handle the
  // tie-aways (from ±x.5).
  WrappedFloatType<FloatType> value_rounded_up = FPRound(value, FE_UPWARD);
  // Check if value has fraction of exactly 0.5.
  // Note that this check can produce spurious true and/or false results for numbers that are too
  // large to have fraction parts. We don't care because for such numbers all three possible FPRound
  // calls above and below produce the exact same result (which is the same as original value).
  if (value == value_rounded_up - WrappedFloatType<FloatType>{0.5f}) {
    // If value is positive then FE_TIESAWAY acts as FE_UPWARD.
    return value_rounded_up;
  }
  // Otherwise FE_TIESAWAY acts as FE_TONEAREST.
  return FPRound(value, FE_TONEAREST);
}

inline Float32 FPRound(const Float32& value, int round_control) {
  float result;
  static_assert(sizeof(result) == sizeof(value));
  memcpy(&result, &value, sizeof(value));
  switch (round_control) {
    case FE_HOSTROUND:
      asm volatile("roundss $4,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_TONEAREST:
      asm volatile("roundss $0,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_DOWNWARD:
      asm volatile("roundss $1,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_UPWARD:
      asm volatile("roundss $2,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_TOWARDZERO:
      asm volatile("roundss $3,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_TIESAWAY:
      return CopySignBit(FPRoundTiesAwayPositive(Absolute(value)), value);
    default:
      FATAL("Internal error: unknown round_control in FPRound!");
  }
  return Float32{result};
}

inline Float64 FPRound(const Float64& value, int round_control) {
  double result;
  static_assert(sizeof(result) == sizeof(value));
  memcpy(&result, &value, sizeof(value));
  switch (round_control) {
    case FE_HOSTROUND:
      asm volatile("roundsd $4,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_TONEAREST:
      asm volatile("roundsd $0,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_DOWNWARD:
      asm volatile("roundsd $1,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_UPWARD:
      asm volatile("roundsd $2,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_TOWARDZERO:
      asm volatile("roundsd $3,%1,%0" : "=x"(result) : "x"(result));
      break;
    case FE_TIESAWAY:
      return CopySignBit(FPRoundTiesAwayPositive(Absolute(value)), value);
    default:
      FATAL("Internal error: unknown round_control in FPRound!");
  }
  return Float64{result};
}

}  // namespace berberis::intrinsics

#endif  // ALL_TO_X86_32_OR_x86_64_BERBERIS_INTRINSICS_INTRINSICS_FLOAT_H_
