/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef BERBERIS_BASE_BIT_UTIL_H_
#define BERBERIS_BASE_BIT_UTIL_H_

#include <bit>
#include <climits>
#include <cstdint>
#include <type_traits>

#include "berberis/base/checks.h"
#include "berberis/base/dependent_false.h"

namespace berberis {

template <typename BaseType>
class Raw;

template <typename BaseType>
class Saturating;

template <typename BaseType>
class Wrapping;

template <typename T>
constexpr bool IsPowerOf2(T x) {
  static_assert(std::is_integral_v<T>, "IsPowerOf2: T must be integral");
  CHECK_NE(x, 0);
  return (x & (x - 1)) == 0;
}

template <typename T>
constexpr bool IsPowerOf2(Raw<T> x) {
  return IsPowerOf2(x.value);
}

template <typename T>
constexpr bool IsPowerOf2(Saturating<T> x) {
  return IsPowerOf2(x.value);
}

template <typename T>
constexpr bool IsPowerOf2(Wrapping<T> x) {
  return IsPowerOf2(x.value);
}

template <size_t kAlign, typename T>
constexpr T AlignDown(T x) {
  static_assert(std::is_integral_v<T>);
  static_assert(IsPowerOf2(kAlign));
  static_assert(static_cast<T>(kAlign) > 0);
  return x & ~(kAlign - 1);
}

template <typename T>
constexpr T AlignDown(T x, size_t align) {
  static_assert(std::is_integral_v<T>, "AlignDown: T must be integral");
  CHECK(IsPowerOf2(align));
  return x & ~(align - 1);
}

template <size_t kAlign, typename T>
constexpr Raw<T> AlignDown(Raw<T> x) {
  return {AlignDown<kAlign>(x.value)};
}

template <size_t kAlign, typename T>
constexpr Saturating<T> AlignDown(Saturating<T> x) {
  return {AlignDown<kAlign>(x.value)};
}

template <size_t kAlign, typename T>
constexpr Wrapping<T> AlignDown(Wrapping<T> x) {
  return {AlignDown<kAlign>(x.value)};
}

// Helper to align pointers.
template <size_t kAlign, typename T>
constexpr T* AlignDown(T* p) {
  return std::bit_cast<T*>(AlignDown<kAlign>(std::bit_cast<uintptr_t>(p)));
}

template <typename T>
constexpr T* AlignDown(T* p, size_t align) {
  return std::bit_cast<T*>(AlignDown(std::bit_cast<uintptr_t>(p), align));
}

template <size_t kAlign, typename T>
constexpr T AlignUp(T x) {
  return AlignDown<kAlign>(x + kAlign - 1);
}

template <typename T>
constexpr T AlignUp(T x, size_t align) {
  return AlignDown(x + align - 1, align);
}

template <size_t kAlign, typename T>
constexpr Raw<T> AlignUp(Raw<T> x) {
  return {AlignUp<kAlign>(x.value)};
}

template <size_t kAlign, typename T>
constexpr Saturating<T> AlignUp(Saturating<T> x) {
  return {AlignUp<kAlign>(x.value)};
}

template <size_t kAlign, typename T>
constexpr Wrapping<T> AlignUp(Wrapping<T> x) {
  return {AlignUp<kAlign>(x.value)};
}

// Helper to align pointers.
template <size_t kAlign, typename T>
constexpr T* AlignUp(T* p) {
  return std::bit_cast<T*>(AlignUp<kAlign>(std::bit_cast<uintptr_t>(p)));
}

template <typename T>
constexpr T* AlignUp(T* p, size_t align) {
  return std::bit_cast<T*>(AlignUp(std::bit_cast<uintptr_t>(p), align));
}

template <size_t kAlign, typename T>
constexpr bool IsAligned(T x) {
  return AlignDown<kAlign>(x) == x;
}

template <typename T>
constexpr bool IsAligned(T x, size_t align) {
  return AlignDown(x, align) == x;
}

template <size_t kAlign, typename T>
constexpr bool IsAligned(Raw<T> x) {
  return IsAligned<kAlign>(x.value);
}

template <size_t kAlign, typename T>
constexpr bool IsAligned(Saturating<T> x) {
  return IsAligned<kAlign>(x.value);
}

template <size_t kAlign, typename T>
constexpr bool IsAligned(Wrapping<T> x) {
  return IsAligned<kAlign>(x.value);
}

// Helper to align pointers.
template <size_t kAlign, typename T>
constexpr bool IsAligned(T* p, size_t align) {
  return IsAligned<kAlign>(std::bit_cast<uintptr_t>(p), align);
}

template <typename T>
constexpr bool IsAligned(T* p, size_t align) {
  return IsAligned(std::bit_cast<uintptr_t>(p), align);
}

template <typename T>
constexpr T BitUtilLog2(T x) {
  static_assert(std::is_integral_v<T>, "Log2: T must be integral");
  CHECK(IsPowerOf2(x));
  // TODO(b/260725458): Use std::countr_zero after C++20 becomes available
  return __builtin_ctz(x);
}

// Signextend bits from size to the corresponding signed type of sizeof(Type) size.
// If the result of this function is assigned to a wider signed type it'll automatically
// sign-extend.
template <unsigned size, typename Type>
static auto SignExtend(const Type val) {
  static_assert(std::is_integral_v<Type>, "Only integral types are supported");
  static_assert(size > 0 && size < (sizeof(Type) * CHAR_BIT), "Invalid size value");
  using SignedType = std::make_signed_t<Type>;
  struct {
    SignedType val : size;
  } holder = {.val = static_cast<SignedType>(val)};
  // Compiler takes care of sign-extension of the field with the specified bit-length.
  return static_cast<SignedType>(holder.val);
}

// Verify that argument value fits into a target.
template <typename ResultType, typename ArgumentType>
inline bool IsInRange(ArgumentType x) {
  // Note: conversion from wider integer type into narrow integer type is always
  // defined.  Conversion to unsigned produces well-defined result while conversion
  // to signed type produces implementation-defined result but in both cases value
  // is guaranteed to be unchanged if it can be represented in the destination type
  // and is *some* valid value if it's unrepesentable.
  //
  // Quote from the standard (including "note" in the standard):
  //   If the destination type is unsigned, the resulting value is the least unsigned
  // integer congruent to the source integer (modulo 2ⁿ where n is the number of bits
  // used to represent the unsigned type). [ Note: In a two’s complement representation,
  // this conversion is conceptual and there is no change in the bit pattern (if there
  // is no truncation). — end note ]
  //   If the destination type is signed, the value is unchanged if it can be represented
  // in the destination type; otherwise, the value is implementation-defined.

  return static_cast<ResultType>(x) == x;
}

template <typename T>
[[nodiscard]] constexpr T CountRZero(T x) {
  // We couldn't use C++20 std::countr_zero yet ( http://b/318678905 ) for __uint128_t .
  // Switch to std::popcount when/if that bug would be fixed.
  static_assert(!std::is_signed_v<T>);
  if constexpr (sizeof(T) == 16) {
    if (static_cast<uint64_t>(x) == 0) {
      return __builtin_ctzll(x >> 64) + 64;
    }
    return __builtin_ctzll(x);
  } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
    return __builtin_ctzll(x);
  } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
    return __builtin_ctz(x);
  } else {
    static_assert(kDependentTypeFalse<T>);
  }
}

template <typename T>
[[nodiscard]] constexpr Raw<T> CountRZero(Raw<T> x) {
  return {CountRZero(x.value)};
}

template <typename T>
[[nodiscard]] constexpr Saturating<T> CountRZero(Saturating<T> x) {
  return {CountRZero(x.value)};
}

template <typename T>
[[nodiscard]] constexpr Wrapping<T> CountRZero(Wrapping<T> x) {
  return {CountRZero(x.value)};
}

template <typename T>
[[nodiscard]] constexpr T Popcount(T x) {
  // We couldn't use C++20 std::popcount yet ( http://b/318678905 ) for __uint128_t .
  // Switch to std::popcount when/if that bug would be fixed.
  static_assert(!std::is_signed_v<T>);
  if constexpr (sizeof(T) == 16) {
    return __builtin_popcountll(x) + __builtin_popcountll(x >> 64);
  } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
    return __builtin_popcountll(x);
  } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
    return __builtin_popcount(x);
  } else {
    static_assert(kDependentTypeFalse<T>);
  }
}

template <typename T>
[[nodiscard]] constexpr Raw<T> Popcount(Raw<T> x) {
  return {Popcount(x.value)};
}

template <typename T>
[[nodiscard]] constexpr Saturating<T> Popcount(Saturating<T> x) {
  return {Popcount(x.value)};
}

template <typename T>
[[nodiscard]] constexpr Wrapping<T> Popcount(Wrapping<T> x) {
  return {Popcount(x.value)};
}

}  // namespace berberis

#endif  // BERBERIS_BASE_BIT_UTIL_H_
