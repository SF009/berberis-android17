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

#ifndef BERBERIS_BASE_LIMITS_H_
#define BERBERIS_BASE_LIMITS_H_

#include <limits>

#include "berberis/base/float.h"

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

#endif  // BERBERIS_BASE_LIMITS_H_
