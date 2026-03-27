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

#ifndef BERBERIS_BASE_INT_H_
#define BERBERIS_BASE_INT_H_

#include <bit>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>

namespace berberis {

template <typename BaseType>
class Raw;

template <typename BaseType>
class Saturating;

template <typename BaseType>
class Wrapping;

template <typename BaseType>
class WrappedFloatType;

template <typename T>
struct TypeTraits;

// Raw integers.  Used to carry payload, which may be be EXPLICITLY converted to Saturating
// integer, Wrapping integer, or WrappedFloatType.
//
// 𝐃𝐨𝐞𝐬𝐧'𝐭 suppopt any actual operations, arithmetic, etc.
// Use bitcast or convert to one of three types listed above!

template <typename Base>
class Raw {
 public:
  using BaseType = Base;

  static_assert(std::is_integral_v<BaseType>);
  static_assert(!std::is_signed_v<BaseType>);

  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(IntType) == sizeof(BaseType)>>
  [[nodiscard]] constexpr operator IntType() const {
    return static_cast<IntType>(value);
  }
  template <typename FloatType,
            typename = std::enable_if_t<!std::numeric_limits<FloatType>::is_exact &&
                                        sizeof(BaseType) == sizeof(FloatType)>>
  [[nodiscard]] constexpr operator WrappedFloatType<FloatType>() const {
    // Can't use bit_cast here because of IA32 ABI!
    WrappedFloatType<FloatType> result;
    memcpy(&result, &value, sizeof(BaseType));
    return result;
  }
  template <typename IntType,
            typename = std::enable_if_t<
                std::is_integral_v<IntType> && sizeof(BaseType) == sizeof(IntType) &&
                !std::is_signed_v<IntType> && !std::is_same_v<IntType, BaseType>>>
  [[nodiscard]] constexpr operator Raw<IntType>() const {
    return {static_cast<IntType>(value)};
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Saturating<IntType>() const {
    return {static_cast<IntType>(value)};
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Wrapping<IntType>() const {
    return {static_cast<IntType>(value)};
  }

  friend auto constexpr BitCastToFloat(Raw src) { return src.Float(); }
  friend auto constexpr BitCastToRaw(Raw src) { return src; }
  friend auto constexpr BitCastToSaturating(Raw src) { return src.Saturating(); }
  friend auto constexpr BitCastToSigned(Raw src) { return src.Signed(); }
  friend auto constexpr BitCastToUnsigned(Raw src) { return src.Unsigned(); }
  friend auto constexpr BitCastToWrapping(Raw src) { return src.Wrapping(); }
  [[nodiscard]] constexpr auto Float() const {
    return std::bit_cast<typename TypeTraits<BaseType>::Float>(value);
  }
  template <typename ResultType>
  [[nodiscard]] auto constexpr MaybeTruncateTo() const
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) <= sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(value)};
  }
  template <typename ResultType>
  friend auto constexpr MaybeTruncateTo(Raw src)
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) <= sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(src.value)};
  }
  [[nodiscard]] constexpr auto Saturating() const { return berberis::Saturating{value}; }
  [[nodiscard]] auto constexpr Signed() const {
    return berberis::Wrapping{static_cast<std::make_signed_t<BaseType>>(value)};
  }
  template <typename ResultType>
  [[nodiscard]] auto constexpr TruncateTo() const
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) < sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(value)};
  }
  template <typename ResultType>
  friend auto constexpr TruncateTo(Raw src)
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) < sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(src.value)};
  }
  [[nodiscard]] auto constexpr Unsigned() const { return berberis::Wrapping{value}; }
  [[nodiscard]] constexpr auto Wrapping() const { return berberis::Wrapping{value}; }

  [[nodiscard]] friend constexpr bool operator==(Raw lhs, Raw rhs) {
    return lhs.value == rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator!=(Raw lhs, Raw rhs) {
    return lhs.value != rhs.value;
  }

  BaseType value = 0;
};

// Saturating and wrapping integers.
//   1. Never trigger UB, even in case of overflow.
//   2. Only support mixed types when both are of the same type (e.g. SatInt8 and SatInt16 or
//      Int8 and Int64 are allowed, but SatInt8 and Int8 are forbidden and Int32 and Uint32
//      require explicit casting, too).
//   3. Results are performed after type expansion.

template <typename Base>
class Saturating {
 public:
  using BaseType = Base;
  using SignedType = Saturating<std::make_signed_t<BaseType>>;
  using UnsignedType = Saturating<std::make_unsigned_t<BaseType>>;
  static constexpr bool kIsSigned = std::is_signed_v<BaseType>;

  static_assert(std::is_integral_v<BaseType>);

  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        ((sizeof(BaseType) < sizeof(IntType) &&
                                          (std::is_signed_v<IntType> ||
                                           kIsSigned == std::is_signed_v<IntType>)) ||
                                         sizeof(IntType) == sizeof(BaseType))>>
  [[nodiscard]] constexpr operator IntType() const {
    return static_cast<IntType>(value);
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Raw<IntType>() const {
    return {static_cast<IntType>(value)};
  }
  template <typename IntType,
            typename = std::enable_if_t<
                std::is_integral_v<IntType> && sizeof(BaseType) <= sizeof(IntType) &&
                std::is_signed_v<IntType> == kIsSigned && !std::is_same_v<IntType, BaseType>>>
  [[nodiscard]] constexpr operator Saturating<IntType>() const {
    return {static_cast<IntType>(value)};
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Wrapping<IntType>() const {
    return {static_cast<IntType>(value)};
  }

  friend auto constexpr BitCastToFloat(Saturating src) { return src.Float(); }
  friend auto constexpr BitCastToRaw(Saturating src) { return src.Raw(); }
  friend auto constexpr BitCastToSaturating(Saturating src) { return src; }
  friend auto constexpr BitCastToSigned(Saturating src) { return src.Signed(); }
  friend auto constexpr BitCastToUnsigned(Saturating src) { return src.Unsigned(); }
  friend auto constexpr BitCastToWrapping(Saturating src) { return src.Wrapping(); }
  [[nodiscard]] constexpr auto Float() const {
    return std::bit_cast<typename TypeTraits<BaseType>::Float>(value);
  }
  template <typename ResultType>
  [[nodiscard]] auto constexpr MaybeTruncateTo() const
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) <= sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(value)};
  }
  template <typename ResultType>
  friend auto constexpr MaybeTruncateTo(Saturating src)
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) <= sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(src.value)};
  }
  [[nodiscard]] constexpr auto Narrow() const {
    if constexpr (Saturating<BaseType>::kIsSigned) {
      if (value < std::numeric_limits<typename TypeTraits<BaseType>::Narrow>::min()) {
        return Saturating<typename TypeTraits<BaseType>::Narrow>{
            std::numeric_limits<typename TypeTraits<BaseType>::Narrow>::min()};
      }
    }
    if (value > std::numeric_limits<typename TypeTraits<BaseType>::Narrow>::max()) {
      return Saturating<typename TypeTraits<BaseType>::Narrow>{
          std::numeric_limits<typename TypeTraits<BaseType>::Narrow>::max()};
    }
    return Saturating<typename TypeTraits<BaseType>::Narrow>{
        static_cast<typename TypeTraits<BaseType>::Narrow>(value)};
  }
  friend constexpr auto Narrow(Saturating source) { return source.Narrow(); }
  [[nodiscard]] constexpr auto Raw() const {
    return berberis::Raw{static_cast<std::make_unsigned_t<BaseType>>(value)};
  }
  [[nodiscard]] constexpr auto Signed() const {
    return static_cast<Saturating<std::make_signed_t<BaseType>>>(value);
  }
  template <typename ResultType>
  [[nodiscard]] auto constexpr TruncateTo() const
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) < sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(value)};
  }
  template <typename ResultType>
  friend auto constexpr TruncateTo(Saturating src)
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) < sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(src.value)};
  }
  [[nodiscard]] constexpr auto Unsigned() const {
    return static_cast<Saturating<std::make_unsigned_t<BaseType>>>(value);
  }
  [[nodiscard]] constexpr auto Wide() const {
    return Saturating<typename TypeTraits<BaseType>::Wide>{value};
  }
  [[nodiscard]] constexpr auto Widen() const {
    return Saturating<typename TypeTraits<BaseType>::Wide>{value};
  }
  friend constexpr auto Widen(Saturating source) { return source.Widen(); }
  [[nodiscard]] constexpr auto Wrapping() const {
    return static_cast<berberis::Wrapping<BaseType>>(value);
  }

  [[nodiscard]] friend constexpr bool operator==(Saturating lhs, Saturating rhs) {
    return lhs.value == rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator!=(Saturating lhs, Saturating rhs) {
    return lhs.value != rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator<(Saturating lhs, Saturating rhs) {
    return lhs.value < rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator<=(Saturating lhs, Saturating rhs) {
    return lhs.value <= rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator>(Saturating lhs, Saturating rhs) {
    return lhs.value > rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator>=(Saturating lhs, Saturating rhs) {
    return lhs.value >= rhs.value;
  }
  friend constexpr Saturating& operator+=(Saturating& lhs, Saturating rhs) {
    lhs = lhs + rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr std::tuple<Saturating, bool> Add(Saturating lhs, Saturating rhs) {
    BaseType result;
    bool overflow = __builtin_add_overflow(lhs.value, rhs.value, &result);
    if (overflow) {
      if constexpr (kIsSigned) {
        if (result < 0) {
          result = std::numeric_limits<BaseType>::max();
        } else {
          result = std::numeric_limits<BaseType>::min();
        }
      } else {
        result = std::numeric_limits<BaseType>::max();
      }
    }
    return {{result}, overflow};
  }
  [[nodiscard]] friend constexpr Saturating operator+(Saturating lhs, Saturating rhs) {
    return std::get<0>(Add(lhs, rhs));
  }
  friend constexpr Saturating& operator-=(Saturating& lhs, Saturating rhs) {
    lhs = lhs - rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr std::tuple<Saturating, bool> Neg(Saturating lhs) {
    if constexpr (kIsSigned) {
      if (lhs.value == std::numeric_limits<BaseType>::min()) {
        return {std::numeric_limits<BaseType>::max(), true};
      }
      return {{-lhs.value}, false};
    }
    return {{0}, lhs != 0};
  }
  [[nodiscard]] friend constexpr Saturating operator-(Saturating lhs) {
    return std::get<0>(Neg(lhs));
  }
  [[nodiscard]] friend constexpr std::tuple<Saturating, bool> Sub(Saturating lhs, Saturating rhs) {
    BaseType result;
    bool overflow = __builtin_sub_overflow(lhs.value, rhs.value, &result);
    if (overflow) {
      if constexpr (kIsSigned) {
        if (result < 0) {
          result = std::numeric_limits<BaseType>::max();
        } else {
          result = std::numeric_limits<BaseType>::min();
        }
      } else {
        result = 0;
      }
    }
    return {{result}, overflow};
  }
  [[nodiscard]] friend constexpr Saturating operator-(Saturating lhs, Saturating rhs) {
    return std::get<0>(Sub(lhs, rhs));
  }
  friend constexpr Saturating& operator*=(Saturating& lhs, Saturating rhs) {
    lhs = lhs * rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr std::tuple<Saturating, bool> Mul(Saturating lhs, Saturating rhs) {
    BaseType result;
    bool overflow = __builtin_mul_overflow(lhs.value, rhs.value, &result);
    if (overflow) {
      if constexpr (kIsSigned) {
        if (lhs.value < 0 != rhs.value < 0) {
          result = std::numeric_limits<BaseType>::min();
        } else {
          result = std::numeric_limits<BaseType>::max();
        }
      } else {
        result = std::numeric_limits<BaseType>::max();
      }
    }
    return {{result}, overflow};
  }
  [[nodiscard]] friend constexpr Saturating operator*(Saturating lhs, Saturating rhs) {
    return std::get<0>(Mul(lhs, rhs));
  }
  friend constexpr Saturating& operator/=(Saturating& lhs, Saturating rhs) {
    lhs = lhs / rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr std::tuple<Saturating, bool> Div(Saturating lhs, Saturating rhs) {
    if constexpr (kIsSigned) {
      if (lhs.value == std::numeric_limits<BaseType>::min() && rhs.value == -1) {
        return {{std::numeric_limits<BaseType>::max()}, true};
      }
    }
    return {{BaseType(lhs.value / rhs.value)}, false};
  }
  [[nodiscard]] friend constexpr Saturating operator/(Saturating lhs, Saturating rhs) {
    return std::get<0>(Div(lhs, rhs));
  }
  friend constexpr Saturating& operator%=(Saturating& lhs, Saturating rhs) {
    lhs = lhs % rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr std::tuple<Saturating, bool> Rem(Saturating lhs, Saturating rhs) {
    if constexpr (kIsSigned) {
      if (lhs.value == std::numeric_limits<BaseType>::min() && rhs.value == -1) {
        return {{1}, true};
      }
    }
    return {{BaseType(lhs.value % rhs.value)}, false};
  }
  [[nodiscard]] friend constexpr Saturating operator%(Saturating lhs, Saturating rhs) {
    return std::get<0>(Rem(lhs, rhs));
  }
  BaseType value = 0;
};

template <typename Base>
class Wrapping {
 public:
  using BaseType = Base;
  using SignedType = Wrapping<std::make_signed_t<BaseType>>;
  using UnsignedType = Wrapping<std::make_unsigned_t<BaseType>>;
  static constexpr bool kIsSigned = std::is_signed_v<BaseType>;

  static_assert(std::is_integral_v<BaseType>);

  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        ((sizeof(BaseType) < sizeof(IntType) &&
                                          (std::is_signed_v<IntType> ||
                                           kIsSigned == std::is_signed_v<IntType>)) ||
                                         sizeof(IntType) == sizeof(BaseType))>>
  [[nodiscard]] constexpr operator IntType() const {
    return static_cast<IntType>(value);
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Raw<IntType>() const {
    return {static_cast<IntType>(value)};
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Saturating<IntType>() const {
    return {static_cast<IntType>(value)};
  }
  template <typename IntType,
            typename = std::enable_if_t<
                std::is_integral_v<IntType> && sizeof(BaseType) <= sizeof(IntType) &&
                std::is_signed_v<IntType> == kIsSigned && !std::is_same_v<IntType, BaseType>>>
  [[nodiscard]] constexpr operator Wrapping<IntType>() const {
    return {static_cast<IntType>(value)};
  }

  friend auto constexpr BitCastToFloat(Wrapping src) { return src.Float(); }
  friend auto constexpr BitCastToRaw(Wrapping src) { return src.Raw(); }
  friend auto constexpr BitCastToSaturating(Wrapping src) { return src.Saturating(); }
  friend auto constexpr BitCastToSigned(Wrapping src) { return src.Signed(); }
  friend auto constexpr BitCastToUnsigned(Wrapping src) { return src.Unsigned(); }
  friend auto constexpr BitCastToWrapping(Wrapping src) { return src; }
  [[nodiscard]] constexpr auto Float() const {
    return std::bit_cast<typename TypeTraits<BaseType>::Float>(value);
  }
  template <typename ResultType>
  [[nodiscard]] auto constexpr MaybeTruncateTo() const
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) <= sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(value)};
  }
  template <typename ResultType>
  friend auto constexpr MaybeTruncateTo(Wrapping src)
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) <= sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(src.value)};
  }
  [[nodiscard]] constexpr auto Narrow() const {
    return Wrapping<typename TypeTraits<BaseType>::Narrow>{
        static_cast<typename TypeTraits<BaseType>::Narrow>(value)};
  }
  friend constexpr auto Narrow(Wrapping source) { return source.Narrow(); }
  // While `Narrow` returns value reduced to smaller data type there are centain algorithms
  // which require the top half, too (most ofhen in the context of widening multiplication
  // where top half of the product is produced).
  // `NarrowTopHalf` returns top half of the value narrowed down to smaller type (overflow is not
  // possible in that case).
  [[nodiscard]] constexpr auto NarrowTopHalf() const {
    return Wrapping<typename TypeTraits<BaseType>::Narrow>{
        static_cast<typename TypeTraits<BaseType>::Narrow>(
            value >> (sizeof(typename TypeTraits<BaseType>::Narrow) * CHAR_BIT))};
  }
  friend constexpr auto NarrowTopHalf(Wrapping source) { return source.NarrowTopHalf(); }
  [[nodiscard]] constexpr auto Raw() const {
    return static_cast<berberis::Raw<std::make_unsigned_t<BaseType>>>(value);
  }
  [[nodiscard]] constexpr auto Saturating() const {
    return static_cast<berberis::Saturating<BaseType>>(value);
  }
  [[nodiscard]] constexpr auto Signed() const {
    return static_cast<Wrapping<std::make_signed_t<BaseType>>>(value);
  }
  template <typename ResultType>
  [[nodiscard]] auto constexpr TruncateTo() const
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) < sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(value)};
  }
  template <typename ResultType>
  friend auto constexpr TruncateTo(Wrapping src)
      -> std::enable_if_t<sizeof(typename ResultType::BaseType) < sizeof(BaseType), ResultType> {
    return ResultType{static_cast<ResultType::BaseType>(src.value)};
  }
  [[nodiscard]] constexpr auto Unsigned() const {
    return static_cast<Wrapping<std::make_unsigned_t<BaseType>>>(value);
  }
  [[nodiscard]] constexpr auto Wide() const {
    return Wrapping<typename TypeTraits<BaseType>::Wide>{value};
  }
  [[nodiscard]] constexpr auto Widen() const {
    return Wrapping<typename TypeTraits<BaseType>::Wide>{value};
  }
  friend constexpr auto Widen(Wrapping source) { return source.Widen(); }

  [[nodiscard]] friend constexpr bool operator==(Wrapping lhs, Wrapping rhs) {
    return lhs.value == rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator!=(Wrapping lhs, Wrapping rhs) {
    return lhs.value != rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator<(Wrapping lhs, Wrapping rhs) {
    return lhs.value < rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator<=(Wrapping lhs, Wrapping rhs) {
    return lhs.value <= rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator>(Wrapping lhs, Wrapping rhs) {
    return lhs.value > rhs.value;
  }
  [[nodiscard]] friend constexpr bool operator>=(Wrapping lhs, Wrapping rhs) {
    return lhs.value >= rhs.value;
  }
  // Note:
  //   1. We use __builtin_xxx_overflow instead of simple +, -, or * operators because
  //      __builtin_xxx_overflow produces well-defined result in case of overflow while
  //      +, -, * are triggering undefined behavior conditions.
  //   2. All operator xxx= are implemented in terms of opernator xxx
  friend constexpr Wrapping& operator+=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs + rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator+(Wrapping lhs, Wrapping rhs) {
    BaseType result;
    __builtin_add_overflow(lhs.value, rhs.value, &result);
    return {result};
  }
  friend constexpr Wrapping& operator-=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs - rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator-(Wrapping lhs) {
    BaseType result;
    __builtin_sub_overflow(BaseType{0}, lhs.value, &result);
    return {result};
  }
  [[nodiscard]] friend constexpr Wrapping operator-(Wrapping lhs, Wrapping rhs) {
    BaseType result;
    __builtin_sub_overflow(lhs.value, rhs.value, &result);
    return {result};
  }
  friend constexpr Wrapping& operator*=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs * rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator*(Wrapping lhs, Wrapping rhs) {
    BaseType result;
    __builtin_mul_overflow(lhs.value, rhs.value, &result);
    return {result};
  }
  friend constexpr Wrapping& operator/=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs / rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator/(Wrapping lhs, Wrapping rhs) {
    if constexpr (kIsSigned) {
      if (lhs.value == std::numeric_limits<BaseType>::min() && rhs.value == -1) {
        return {std::numeric_limits<BaseType>::min()};
      }
    }
    return {BaseType(lhs.value / rhs.value)};
  }
  friend constexpr Wrapping& operator%=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs % rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator%(Wrapping lhs, Wrapping rhs) {
    if constexpr (kIsSigned) {
      if (lhs.value == std::numeric_limits<BaseType>::min() && rhs.value == -1) {
        return {0};
      }
    }
    return {BaseType(lhs.value % rhs.value)};
  }
  friend constexpr Wrapping& operator<<=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs << rhs;
    return lhs;
  }
  template <typename IntType>
  [[nodiscard]] friend constexpr Wrapping operator<<(Wrapping lhs, Wrapping<IntType> rhs) {
    return {BaseType(lhs.value << (rhs.value & (sizeof(BaseType) * CHAR_BIT - 1)))};
  }
  friend constexpr Wrapping& operator>>=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs >> rhs;
    return lhs;
  }
  template <typename IntType>
  [[nodiscard]] friend constexpr Wrapping operator>>(Wrapping lhs, Wrapping<IntType> rhs) {
    return {BaseType(lhs.value >> (rhs.value & (sizeof(BaseType) * CHAR_BIT - 1)))};
  }
  friend constexpr Wrapping& operator&=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs & rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator&(Wrapping lhs, Wrapping rhs) {
    return {BaseType(lhs.value & rhs.value)};
  }
  friend constexpr Wrapping& operator|=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs | rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator|(Wrapping lhs, Wrapping rhs) {
    return {BaseType(lhs.value | rhs.value)};
  }
  friend constexpr Wrapping& operator^=(Wrapping& lhs, Wrapping rhs) {
    lhs = lhs ^ rhs;
    return lhs;
  }
  [[nodiscard]] friend constexpr Wrapping operator^(Wrapping lhs, Wrapping rhs) {
    return {BaseType(lhs.value ^ rhs.value)};
  }
  [[nodiscard]] friend constexpr Wrapping operator~(Wrapping lhs) { return {BaseType(~lhs.value)}; }
  BaseType value = 0;
};

using RawInt8 = Raw<uint8_t>;
using RawInt16 = Raw<uint16_t>;
using RawInt32 = Raw<uint32_t>;
using RawInt64 = Raw<uint64_t>;
#if defined(__LP64__)
using RawInt128 = Raw<unsigned __int128>;
#endif

using SatInt8 = Saturating<int8_t>;
using SatUInt8 = Saturating<uint8_t>;
using SatInt16 = Saturating<int16_t>;
using SatUInt16 = Saturating<uint16_t>;
using SatInt32 = Saturating<int32_t>;
using SatUInt32 = Saturating<uint32_t>;
using SatInt64 = Saturating<int64_t>;
using SatUInt64 = Saturating<uint64_t>;
#if defined(__LP64__)
using SatInt128 = Saturating<__int128>;
using SatUInt128 = Saturating<unsigned __int128>;
#endif

using Int8 = Wrapping<int8_t>;
using UInt8 = Wrapping<uint8_t>;
using Int16 = Wrapping<int16_t>;
using UInt16 = Wrapping<uint16_t>;
using Int32 = Wrapping<int32_t>;
using UInt32 = Wrapping<uint32_t>;
using Int64 = Wrapping<int64_t>;
using UInt64 = Wrapping<uint64_t>;
using IntPtr = Wrapping<intptr_t>;
using UIntPtr = Wrapping<uintptr_t>;
#if defined(__LP64__)
using Int128 = Wrapping<__int128>;
using UInt128 = Wrapping<unsigned __int128>;
#endif

}  // namespace berberis

#endif  // BERBERIS_BASE_INT_H_
