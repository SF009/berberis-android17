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

#ifndef BERBERIS_INTRINSICS_COMMON_CONSTANTS_POOL_H_
#define BERBERIS_INTRINSICS_COMMON_CONSTANTS_POOL_H_

#include <bit>
#include <cstdint>

#include <type_traits>

namespace berberis {

namespace constants_pool {

#if defined(__i386__) || defined(__x86_64__)
using ConstPoolAddrType = int32_t;
#else
using ConstPoolAddrType = intptr_t;
#endif

// Vector constants, that is: constants are repeated to fill 128bit SIMD register.
template <auto kValue_, typename = void>
struct VectorConst {};

template <auto kValue_>
inline const int32_t& kVectorConst = VectorConst<kValue_>::kValue;

template <auto kValue_>
struct VectorConst<kValue_,
                   std::enable_if_t<std::is_unsigned_v<std::remove_cvref_t<decltype(kValue_)>>>> {
  static constexpr const ConstPoolAddrType& kValue =
      kVectorConst<static_cast<std::make_signed_t<std::remove_cvref_t<decltype(kValue_)>>>(
          kValue_)>;
};

template <float kValue_>
struct VectorConst<kValue_> {
  static constexpr const ConstPoolAddrType& kValue = kVectorConst<std::bit_cast<int32_t>(kValue_)>;
};

template <double kValue_>
struct VectorConst<kValue_> {
  static constexpr const ConstPoolAddrType& kValue = kVectorConst<std::bit_cast<int64_t>(kValue_)>;
};

}  // namespace constants_pool

namespace constants_offsets {

// constants_offsets namespace includes compile-time versions of constants used in macro assembler
// functions. This allows the static verifier assembler to use static versions of the macro-
// assembly functions.
using ConstPoolAddrType = constants_pool::ConstPoolAddrType;

template <const int32_t* kConstantAddr>
class ConstantAccessor {
 public:
  constexpr operator ConstPoolAddrType() const {
    if (std::is_constant_evaluated()) {
      return 0;
    } else {
      return *kConstantAddr;
    }
  }
};

template <const auto kValue_>
class TypeConstantAccessor {
 public:
  constexpr operator ConstPoolAddrType() const {
    if (std::is_constant_evaluated()) {
      return 0;
    } else {
      return *kValue_;
    }
  }
};

template <const auto kValue_>
class VectorConstantAccessor {
 public:
  constexpr operator ConstPoolAddrType() const {
    if (std::is_constant_evaluated()) {
      return 0;
    } else {
      return constants_pool::VectorConst<kValue_>::kValue;
    }
  }
};

}  // namespace constants_offsets

}  // namespace berberis

#endif  // BERBERIS_INTRINSICS_COMMON_CONSTANTS_POOL_H_
