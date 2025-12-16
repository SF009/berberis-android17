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

#ifndef BERBERIS_INTRINSICS_COMMON_INTRINSICS_FLOAT_H_
#define BERBERIS_INTRINSICS_COMMON_INTRINSICS_FLOAT_H_

// We couldn't safely pass arguments using "raw" float and double on X86 because of peculiarities
// of psABI (sometimes floating point registers are used by guest programs to pass integer value and
// certain integers when converted to fp80 type and back become corrupted).
//
// To make sure that we wouldn't use it to return value by accident we would wrap them and use class
// which makes such mistakes unlikely on x86.
//
// It's safe to pass "raw" values as float and double on modern ABI (RISC-V UABI, x86-64 psABI, etc).
//
// NOTE: That type must be layout-compatible with underlying type thus it must ONLY have one field
// value_ inside.
//
// NOTE: It's perfectly safe to do bit_cast<uint32_t>(Float32) or bit_cast<Float64>(uint64_t).
// Yet it's NOT safe to do bit_cast<float>(Float32) or bit_cast<Float64>(double). This is because
// bit_cast itself is just a regular function and is affected by that psABI issue as well.
//
// If you need to convert between float/double and Float32/Float64 then you have to use memcpy
// and couldn't use any helper function which would receive or return raw float or double value.

#include <stdint.h>

#include <cmath>
#include <limits>

#include "berberis/base/bit_util.h"
#include "berberis/base/float.h"

namespace berberis::intrinsics {

// It's NOT safe to use ANY functions which return raw float or double.  That's because IA32 ABI
// uses x87 stack to pass arguments (even with -mfpmath=sse) which clobbers NaN values.
//
// Builtins do NOT use the official calling conventions but are instead embedded in the function -
// even if all optimizations are disabled. Therefore, it's safe to use builtins here if on x86 host
// this file is compiled with SSE enforced for FP calculations, which is always the case for us.
// Clang uses SSE whenever possible by default. For GCC we need to specify -msse2 and -mfpmath=sse.

inline Float32 CopySignBit(const Float32& v1, const Float32& v2) {
  float src1, src2;
  static_assert(sizeof(src1) == sizeof(v1));
  memcpy(&src1, &v1, sizeof(v1));
  static_assert(sizeof(src2) == sizeof(v2));
  memcpy(&src2, &v2, sizeof(v2));
  return Float32{__builtin_copysignf(src1, src2)};
}

inline Float64 CopySignBit(const Float64& v1, const Float64& v2) {
  double src1, src2;
  static_assert(sizeof(src1) == sizeof(v1));
  memcpy(&src1, &v1, sizeof(v1));
  static_assert(sizeof(src2) == sizeof(v2));
  memcpy(&src2, &v2, sizeof(v2));
  return Float64{__builtin_copysign(src1, src2)};
}

inline Float32 Absolute(const Float32& v) {
  float src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return Float32{__builtin_fabsf(src)};
}

inline Float64 Absolute(const Float64& v) {
  double src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return Float64{__builtin_fabs(src)};
}

inline FPInfo FPClassify(const Float32& v) {
  float src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return static_cast<FPInfo>(__builtin_fpclassify(static_cast<int>(FPInfo::kNaN),
                                                  static_cast<int>(FPInfo::kInfinite),
                                                  static_cast<int>(FPInfo::kNormal),
                                                  static_cast<int>(FPInfo::kSubnormal),
                                                  static_cast<int>(FPInfo::kZero),
                                                  src));
}

inline FPInfo FPClassify(const Float64& v) {
  double src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return static_cast<FPInfo>(__builtin_fpclassify(static_cast<int>(FPInfo::kNaN),
                                                  static_cast<int>(FPInfo::kInfinite),
                                                  static_cast<int>(FPInfo::kNormal),
                                                  static_cast<int>(FPInfo::kSubnormal),
                                                  static_cast<int>(FPInfo::kZero),
                                                  src));
}

inline int IsNan(const Float32& v) {
  float src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return __builtin_isnan(src);
}

inline int IsNan(const Float64& v) {
  double src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return __builtin_isnan(src);
}

inline int SignBit(const Float32& v) {
  float src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return __builtin_signbitf(src);
}

inline int SignBit(const Float64& v) {
  double src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return __builtin_signbit(src);
}

inline Float32 Sqrt(const Float32& v) {
  float src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return Float32{__builtin_sqrtf(src)};
}

inline Float64 Sqrt(const Float64& v) {
  double src;
  static_assert(sizeof(src) == sizeof(v));
  memcpy(&src, &v, sizeof(v));
  return Float64{__builtin_sqrt(src)};
}

// x*y + z
inline Float32 MulAdd(const Float32& v1, const Float32& v2, const Float32& v3) {
  float src1, src2, src3;
  static_assert(sizeof(src1) == sizeof(v1));
  memcpy(&src1, &v1, sizeof(v1));
  static_assert(sizeof(src2) == sizeof(v2));
  memcpy(&src2, &v2, sizeof(v2));
  static_assert(sizeof(src3) == sizeof(v3));
  memcpy(&src3, &v3, sizeof(v3));
  return Float32{fmaf(src1, src2, src3)};
}

inline Float64 MulAdd(const Float64& v1, const Float64& v2, const Float64& v3) {
  double src1, src2, src3;
  static_assert(sizeof(src1) == sizeof(v1));
  memcpy(&src1, &v1, sizeof(v1));
  static_assert(sizeof(src2) == sizeof(v2));
  memcpy(&src2, &v2, sizeof(v2));
  static_assert(sizeof(src3) == sizeof(v3));
  memcpy(&src3, &v3, sizeof(v3));
  return Float64{fma(src1, src2, src3)};
}

}  // namespace berberis::intrinsics

namespace std {

template <typename BaseType>
class numeric_limits<berberis::WrappedFloatType<BaseType>> {
 public:
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = true;
  static constexpr bool is_integer = false;
  static constexpr bool is_exact = false;
  static constexpr bool has_infinity = true;
  static constexpr bool has_quiet_NaN = std::numeric_limits<BaseType>::has_quiet_NaN;
  static constexpr bool has_signaling_NaN = std::numeric_limits<BaseType>::has_signaling_NaN;
  static constexpr std::float_denorm_style has_denorm = std::numeric_limits<BaseType>::has_denorm;
  static constexpr bool has_denorm_loss = std::numeric_limits<BaseType>::has_denorm_loss;
  static constexpr std::float_round_style round_style = std::numeric_limits<BaseType>::round_style;
  static constexpr bool is_iec559 = std::numeric_limits<BaseType>::is_iec559;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = false;
  static constexpr int digits = std::numeric_limits<BaseType>::digits;
  static constexpr int digits10 = std::numeric_limits<BaseType>::digits10;
  static constexpr int max_digits10 = std::numeric_limits<BaseType>::max_digits10;
  static constexpr int radix = std::numeric_limits<BaseType>::radix;
  static constexpr int min_exponent = std::numeric_limits<BaseType>::min_exponent;
  static constexpr int min_exponent10 = std::numeric_limits<BaseType>::min_exponent10;
  static constexpr int max_exponent = std::numeric_limits<BaseType>::max_exponent;
  static constexpr int max_exponent10 = std::numeric_limits<BaseType>::max_exponent10;
  static constexpr bool traps = std::numeric_limits<BaseType>::traps;
  static constexpr bool tinyness_before = std::numeric_limits<BaseType>::tinyness_before;
  static constexpr berberis::WrappedFloatType<BaseType> min() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::min());
  }
  static constexpr berberis::WrappedFloatType<BaseType> lowest() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::lowest());
  }
  static constexpr berberis::WrappedFloatType<BaseType> max() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::max());
  }
  static constexpr berberis::WrappedFloatType<BaseType> epsilon() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::epsilon());
  }
  static constexpr berberis::WrappedFloatType<BaseType> round_error() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::round_error());
  }
  static constexpr berberis::WrappedFloatType<BaseType> infinity() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::infinity());
  }
  static constexpr berberis::WrappedFloatType<BaseType> quiet_NaN() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::quiet_NaN());
  }
  static constexpr berberis::WrappedFloatType<BaseType> signaling_NaN() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::signaling_NaN());
  }
  static constexpr berberis::WrappedFloatType<BaseType> denorm_min() {
    return berberis::WrappedFloatType<BaseType>(std::numeric_limits<BaseType>::denorm_min());
  }
};

}  // namespace std

#endif  // BERBERIS_INTRINSICS_COMMON_INTRINSICS_FLOAT_H_
