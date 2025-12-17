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

#ifndef BERBERIS_BASE_FLOAT_H_
#define BERBERIS_BASE_FLOAT_H_

// We couldn't safely pass arguments using "raw" float and double on X86 because of peculiarities
// of psABI (sometimes floating point registers are used by guest programs to pass integer value and
// certain integers when converted to fp80 type and back become corrupted).
//
// To make sure that we wouldn't use it to return value by accident we would wrap them and use class
// which makes such mistakes unlikely on x86.
//
// It's safe to pass "raw" values as float and double on modern ABI (RISC-V UABI, x86-64 psABI, etc)
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

namespace berberis {

template <typename BaseType>
class Raw;

template <typename T>
struct TypeTraits;

enum class FPInfo { kNaN, kInfinite, kNormal, kSubnormal, kZero };

template <typename BaseType>
class WrappedFloatType {
 public:
  constexpr WrappedFloatType() = default;
  explicit constexpr WrappedFloatType(BaseType value) : value_(value) {}
  constexpr WrappedFloatType(const WrappedFloatType& other) = default;
  constexpr WrappedFloatType(WrappedFloatType&& other) noexcept = default;
  WrappedFloatType& operator=(const WrappedFloatType& other) = default;
  WrappedFloatType& operator=(WrappedFloatType&& other) noexcept = default;
  ~WrappedFloatType() = default;
  auto Int() const {
    // Can't use bit_cast here because of IA32 ABI!
    Raw<std::make_unsigned_t<typename TypeTraits<WrappedFloatType>::Int>> result;
    memcpy(&result, &value_, sizeof(BaseType));
    return result;
  }
  template <typename IntType,
            typename = std::enable_if_t<std::is_integral_v<IntType> &&
                                        sizeof(BaseType) == sizeof(IntType)>>
  [[nodiscard]] constexpr operator Raw<IntType>() const {
    // Can't use bit_cast here because of IA32 ABI!
    Raw<IntType> result;
    memcpy(&result, &value_, sizeof(BaseType));
    return result;
  }
  [[nodiscard]] constexpr auto Narrow() const {
    return typename TypeTraits<WrappedFloatType>::Narrow(value_);
  }
  [[nodiscard]] constexpr auto Wide() const {
    return typename TypeTraits<WrappedFloatType>::Wide(value_);
  }
  [[nodiscard]] constexpr auto Widen() const {
    return typename TypeTraits<WrappedFloatType>::Wide(value_);
  }
  explicit constexpr operator int16_t() const { return value_; }
  explicit constexpr operator uint16_t() const { return value_; }
  explicit constexpr operator int32_t() const { return value_; }
  explicit constexpr operator uint32_t() const { return value_; }
  explicit constexpr operator int64_t() const { return value_; }
  explicit constexpr operator uint64_t() const { return value_; }
  explicit constexpr operator WrappedFloatType<_Float16>() const {
    return WrappedFloatType<_Float16>(value_);
  }
  explicit constexpr operator WrappedFloatType<float>() const {
    return WrappedFloatType<float>(value_);
  }
  explicit constexpr operator WrappedFloatType<double>() const {
    return WrappedFloatType<double>(value_);
  }
#if defined(__i386__) || defined(__x86_64__)
  explicit constexpr operator long double() const { return value_; }
#endif

 private:
  static_assert(!std::numeric_limits<BaseType>::is_exact,
                "WrappedFloatType should only be used with float types!");
  BaseType value_;
};

// Note: Float8 is uninhabited and variables of such type couldn't be created. Nonetheless it's
// useful for implementation of certain instructions. For example vfwcvt.f.x.v RISC-V instruction
// converts from Int8 (single-width int) to Float16 (double-width float) if titular element size if
// Float8. Because such instruction never actually accesses elements of Float8 type it's perfectly
// valid and allowed (if CPU supports Float16).
class Float8PhonyType;  // This class doesn't exist but we may use it in template arguments.
using Float8 = WrappedFloatType<Float8PhonyType>;  // Ditto.
using Float16 = WrappedFloatType<_Float16>;
using Float32 = WrappedFloatType<float>;
using Float64 = WrappedFloatType<double>;

}  // namespace berberis

#endif  // BERBERIS_BASE_FLOAT_H_
