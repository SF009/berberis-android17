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

#include "berberis/base/tuple_processing.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "berberis/base/checks.h"
#include "berberis/base/type_traits.h"

namespace berberis {

namespace {

using Float16 = WrappedFloatType<_Float16>;
using Float32 = WrappedFloatType<float>;
using Float64 = WrappedFloatType<double>;

// Type checks.

static_assert(kMetaType<int>.IsIntegral());
static_assert(!kMetaType<float>.IsIntegral());
static_assert(!kMetaType<int>.IsFloatingPoint());
static_assert(kMetaType<float>.IsFloatingPoint());

// Type conversions.

static_assert(std::is_same_v<Type<kMetaType<int>.AddConst().AddLvalueReference()>, const int&>);
static_assert(std::is_same_v<Type<kMetaType<float>.AddConst().AddPointer().AddVolatile()>,
                             const float* volatile>);

// Non-trivial checks (more than one type involed).
// All types from the std are permitted to be nothrow-swappable (even if for some it's not
// required). Instead of trying to see which standard class in swappable but not nothrow-swappable
// it's better to create class that does what we want 100% guaranteed.
struct SwappableButNotNothrowSwappable {
  SwappableButNotNothrowSwappable& operator=(const SwappableButNotNothrowSwappable&) {
    return *this;
  }  // Copyable
  SwappableButNotNothrowSwappable(SwappableButNotNothrowSwappable&&) noexcept(false) {
  }  // Throwing move
  SwappableButNotNothrowSwappable& operator=(SwappableButNotNothrowSwappable&&) noexcept(false) {
    return *this;
  }
};

static_assert(
    std::same_as<
        Type<kMetaType<std::add_lvalue_reference_t<int>>.CommonReference<
            std::add_lvalue_reference_t<int>,
            std::add_lvalue_reference_t<int>&,
            std::add_lvalue_reference_t<int>&&
            /*,std::add_lvalue_reference_t<int>const*/
            /*,std::add_lvalue_reference_t<int>const&*/
        >()>,
        int&
    >
);
static_assert(std::same_as<Type<kMetaType<std::add_lvalue_reference_t<int>>.CommonReference(
                               kMetaType<std::add_lvalue_reference_t<int>>,
                               kMetaType<std::add_lvalue_reference_t<int>&>,
                               kMetaType<std::add_lvalue_reference_t<int>&&>
                               /*,kMetaType<std::add_lvalue_reference_t<int>const>*/
                               /*,kMetaType<std::add_lvalue_reference_t<int>const&>*/
                               )>,
                           int&>);
static_assert(
    std::same_as<Type<kMetaType<std::add_lvalue_reference_t<int>>.CommonReference(
                     kTupleMetaTypes<std::tuple<std::add_lvalue_reference_t<int>,
                                                std::add_lvalue_reference_t<int>&,
                                                std::add_lvalue_reference_t<int>&&
                                                /*,std::add_lvalue_reference_t<int>const*/
                                                /*,std::add_lvalue_reference_t<int>const&*/
                                                >>)>,
                 int&>);
static_assert(std::same_as<Type<kMetaType<char>.CommonType<short, int, float>()>, float>);
static_assert(std::same_as<
              Type<kMetaType<char>.CommonType(kMetaType<short>, kMetaType<int>, kMetaType<float>)>,
              float>);
static_assert(
    std::same_as<Type<kMetaType<char>.CommonType(kTupleMetaTypes<std::tuple<short, int, float>>)>,
                 float>);
static_assert(!kMetaType<std::true_type>.Conjunction<std::true_type, std::false_type>());
static_assert(!kMetaType<std::true_type>.Conjunction(kMetaType<std::true_type>,
                                                     kMetaType<std::false_type>));
static_assert(!kMetaType<std::true_type>.Conjunction(
    kTupleMetaTypes<std::tuple<std::true_type, std::false_type>>));
static_assert(kMetaType<std::true_type>.Disjunction<std::true_type, std::false_type>());
static_assert(kMetaType<std::true_type>.Disjunction(kMetaType<std::true_type>,
                                                    kMetaType<std::false_type>));
static_assert(kMetaType<std::true_type>.Disjunction(
    kTupleMetaTypes<std::tuple<std::true_type, std::false_type>>));
static_assert(
    std::same_as<Type<kMetaType<double (*)(double, double)>.InvokeResult<int, int>()>, double>);
static_assert(
    std::same_as<
        Type<kMetaType<double (*)(double, double)>.InvokeResult(kMetaType<int>, kMetaType<int>)>,
        double>);
static_assert(std::same_as<Type<kMetaType<double (*)(double, double)>.InvokeResult(
                               kTupleMetaTypes<std::tuple<int, int>>)>,
                           double>);
static_assert(kMetaType<std::string&>.IsAssignable<const char*>());
static_assert(kMetaType<std::string&>.IsAssignable(kMetaType<const char*>));
static_assert(kMetaType<std::ostream>.IsBaseOf<std::iostream>());
static_assert(kMetaType<std::ostream>.IsBaseOf(kMetaType<std::iostream>));
static_assert(kMetaType<const char*>.IsConvertible<std::string>());
static_assert(kMetaType<const char*>.IsConvertible(kMetaType<std::string>));
static_assert(kMetaType<double (*)(double, double)>.IsInvocable<int, int>());
static_assert(kMetaType<double (*)(double, double)>.IsInvocable(kMetaType<int>, kMetaType<int>));
static_assert(
    kMetaType<double (*)(double, double)>.IsInvocable(kTupleMetaTypes<std::tuple<int, int>>));
static_assert(kMetaType<void>.IsInvocableR<double (*)(double, double), int, int>());
static_assert(kMetaType<void>.IsInvocableR(kMetaType<double (*)(double, double)>,
                                           kMetaType<int>,
                                           kMetaType<int>));
static_assert(kMetaType<void>.IsInvocableR(kMetaType<double (*)(double, double)>,
                                           kTupleMetaTypes<std::tuple<int, int>>));
static_assert(kMetaType<void>.IsInvocableR(
    kTupleMetaTypes<std::tuple<double (*)(double, double), int, int>>));
static_assert(!kMetaType<std::string&>.IsNothrowAssignable<const char*>());
static_assert(!kMetaType<std::string&>.IsNothrowAssignable(kMetaType<const char*>));
static_assert(kMetaType<std::vector<int>&>.IsNothrowAssignable<std::vector<int>&&>());
static_assert(kMetaType<std::vector<int>&>.IsNothrowAssignable(kMetaType<std::vector<int>&&>));
static_assert(!kMetaType<const char*>.IsNothrowConvertible<std::string>());
static_assert(!kMetaType<const char*>.IsNothrowConvertible(kMetaType<std::string>));
static_assert(kMetaType<std::ostream*>.IsNothrowConvertible<std::ios*>());
static_assert(kMetaType<std::ostream*>.IsNothrowConvertible(kMetaType<std::ios*>));
static_assert(!kMetaType<double (*)(double, double)>.IsNothrowInvocable<int, int>());
static_assert(!kMetaType<double (*)(double, double)>.IsNothrowInvocable(kMetaType<int>,
                                                                        kMetaType<int>));
static_assert(!kMetaType<double (*)(double, double)>.IsNothrowInvocable(
    kTupleMetaTypes<std::tuple<int, int>>));
static_assert(kMetaType<double (*)(double, double) noexcept>.IsNothrowInvocable<int, int>());
static_assert(kMetaType<double (*)(double, double) noexcept>.IsNothrowInvocable(kMetaType<int>,
                                                                                kMetaType<int>));
static_assert(kMetaType<double (*)(double, double) noexcept>.IsNothrowInvocable(
    kTupleMetaTypes<std::tuple<int, int>>));
static_assert(!kMetaType<void>.IsNothrowInvocableR<double (*)(double, double), int, int>());
static_assert(!kMetaType<void>.IsNothrowInvocableR(kMetaType<double (*)(double, double)>,
                                                   kMetaType<int>,
                                                   kMetaType<int>));
static_assert(!kMetaType<void>.IsNothrowInvocableR(kMetaType<double (*)(double, double)>,
                                                   kTupleMetaTypes<std::tuple<int, int>>));
static_assert(!kMetaType<void>.IsNothrowInvocableR(
    kTupleMetaTypes<std::tuple<double (*)(double, double), int, int>>));
static_assert(kMetaType<void>.IsNothrowInvocableR<double (*)(double, double) noexcept, int, int>());
static_assert(kMetaType<void>.IsNothrowInvocableR(kMetaType<double (*)(double, double) noexcept>,
                                                  kMetaType<int>,
                                                  kMetaType<int>));
static_assert(kMetaType<void>.IsNothrowInvocableR(kMetaType<double (*)(double, double) noexcept>,
                                                  kTupleMetaTypes<std::tuple<int, int>>));
static_assert(kMetaType<void>.IsNothrowInvocableR(
    kTupleMetaTypes<std::tuple<double (*)(double, double) noexcept, int, int>>));
static_assert(kMetaType<bool&>.IsNothrowSwappableWith<std::vector<bool>::reference>());
static_assert(kMetaType<bool&>.IsNothrowSwappableWith(kMetaType<std::vector<bool>::reference>));
static_assert(!kMetaType<SwappableButNotNothrowSwappable&>
                  .IsNothrowSwappableWith<SwappableButNotNothrowSwappable&>());
static_assert(!kMetaType<SwappableButNotNothrowSwappable&>.IsNothrowSwappableWith(
    kMetaType<SwappableButNotNothrowSwappable&>));
static_assert(kMetaType<int>.IsSame<int>());
static_assert(kMetaType<int>.IsSame(kMetaType<int>));
static_assert(kMetaType<SwappableButNotNothrowSwappable&>
                  .IsSwappableWith<SwappableButNotNothrowSwappable&>());
static_assert(kMetaType<SwappableButNotNothrowSwappable&>.IsSwappableWith(
    kMetaType<SwappableButNotNothrowSwappable&>));
static_assert(!kMetaType<std::vector<int>&>.IsTriviallyAssignable<std::vector<int>&&>());
static_assert(!kMetaType<std::vector<int>&>.IsTriviallyAssignable(kMetaType<std::vector<int>&&>));
static_assert(kMetaType<int&>.IsTriviallyAssignable<double>());
static_assert(kMetaType<int&>.IsTriviallyAssignable(kMetaType<double>));
static_assert(std::is_same_v<Type<kMetaType<int>.Void<void, void, void>()>, void>);
static_assert(
    std::is_same_v<Type<kMetaType<int>.Void(kMetaType<void>, kMetaType<void>, kMetaType<void>)>,
                   void>);
static_assert(
    std::is_same_v<Type<kMetaType<int>.Void(kTupleMetaTypes<std::tuple<char, short, int>>)>, void>);

// Altered operations.

static_assert(kMetaType<_Float16>.IsFloatingPoint());
static_assert(kMetaType<float>.IsFloatingPoint());
static_assert(kMetaType<double>.IsFloatingPoint());
static_assert(!kMetaType<int32_t>.IsFloatingPoint());
static_assert(!kMetaType<uint32_t>.IsFloatingPoint());
static_assert(!kMetaType<void>.IsFloatingPoint());
static_assert(!kMetaType<void*>.IsFloatingPoint());
static_assert(kMetaType<Float16>.IsFloatingPoint());
static_assert(kMetaType<Float32>.IsFloatingPoint());
static_assert(kMetaType<Float64>.IsFloatingPoint());
static_assert(!kMetaType<Int32>.IsFloatingPoint());
static_assert(!kMetaType<RawInt32>.IsFloatingPoint());
static_assert(!kMetaType<SatInt32>.IsFloatingPoint());
static_assert(!kMetaType<SatUInt32>.IsFloatingPoint());
static_assert(!kMetaType<UInt32>.IsFloatingPoint());

static_assert(!kMetaType<_Float16>.IsIntegral());
static_assert(!kMetaType<float>.IsIntegral());
static_assert(!kMetaType<double>.IsIntegral());
static_assert(kMetaType<int32_t>.IsIntegral());
static_assert(kMetaType<uint32_t>.IsIntegral());
static_assert(!kMetaType<void>.IsIntegral());
static_assert(!kMetaType<void*>.IsIntegral());
static_assert(!kMetaType<Float16>.IsIntegral());
static_assert(!kMetaType<Float32>.IsIntegral());
static_assert(!kMetaType<Float64>.IsIntegral());
static_assert(kMetaType<Int32>.IsIntegral());
static_assert(kMetaType<RawInt32>.IsIntegral());
static_assert(kMetaType<SatInt32>.IsIntegral());
static_assert(kMetaType<SatUInt32>.IsIntegral());
static_assert(kMetaType<UInt32>.IsIntegral());

static_assert(!kMetaType<_Float16>.IsRawInt());
static_assert(!kMetaType<float>.IsRawInt());
static_assert(!kMetaType<double>.IsRawInt());
static_assert(!kMetaType<int32_t>.IsRawInt());
static_assert(!kMetaType<uint32_t>.IsRawInt());
static_assert(!kMetaType<void>.IsRawInt());
static_assert(!kMetaType<void*>.IsRawInt());
static_assert(!kMetaType<Float16>.IsRawInt());
static_assert(!kMetaType<Float32>.IsRawInt());
static_assert(!kMetaType<Float64>.IsRawInt());
static_assert(!kMetaType<Int32>.IsRawInt());
static_assert(kMetaType<RawInt32>.IsRawInt());
static_assert(!kMetaType<SatInt32>.IsRawInt());
static_assert(!kMetaType<SatUInt32>.IsRawInt());
static_assert(!kMetaType<UInt32>.IsRawInt());

static_assert(!kMetaType<_Float16>.IsSaturatingInt());
static_assert(!kMetaType<float>.IsSaturatingInt());
static_assert(!kMetaType<double>.IsSaturatingInt());
static_assert(!kMetaType<int32_t>.IsSaturatingInt());
static_assert(!kMetaType<uint32_t>.IsSaturatingInt());
static_assert(!kMetaType<void>.IsSaturatingInt());
static_assert(!kMetaType<void*>.IsSaturatingInt());
static_assert(!kMetaType<Float16>.IsSaturatingInt());
static_assert(!kMetaType<Float32>.IsSaturatingInt());
static_assert(!kMetaType<Float64>.IsSaturatingInt());
static_assert(!kMetaType<Int32>.IsSaturatingInt());
static_assert(!kMetaType<RawInt32>.IsSaturatingInt());
static_assert(kMetaType<SatInt32>.IsSaturatingInt());
static_assert(kMetaType<SatUInt32>.IsSaturatingInt());
static_assert(!kMetaType<UInt32>.IsSaturatingInt());

static_assert(kMetaType<_Float16>.IsSigned());
static_assert(kMetaType<float>.IsSigned());
static_assert(kMetaType<double>.IsSigned());
static_assert(kMetaType<int32_t>.IsSigned());
static_assert(!kMetaType<uint32_t>.IsSigned());
static_assert(kMetaType<Float16>.IsSigned());
static_assert(kMetaType<Float32>.IsSigned());
static_assert(kMetaType<Float64>.IsSigned());
static_assert(kMetaType<Int32>.IsSigned());
static_assert(!kMetaType<RawInt32>.IsSigned());
static_assert(kMetaType<SatInt32>.IsSigned());
static_assert(!kMetaType<SatUInt32>.IsSigned());
static_assert(!kMetaType<UInt32>.IsSigned());

static_assert(!kMetaType<_Float16>.IsUnsigned());
static_assert(!kMetaType<float>.IsUnsigned());
static_assert(!kMetaType<double>.IsUnsigned());
static_assert(!kMetaType<int32_t>.IsUnsigned());
static_assert(kMetaType<uint32_t>.IsUnsigned());
static_assert(!kMetaType<Float16>.IsUnsigned());
static_assert(!kMetaType<Float32>.IsUnsigned());
static_assert(!kMetaType<Float64>.IsUnsigned());
static_assert(!kMetaType<Int32>.IsUnsigned());
static_assert(kMetaType<RawInt32>.IsUnsigned());
static_assert(!kMetaType<SatInt32>.IsUnsigned());
static_assert(kMetaType<SatUInt32>.IsUnsigned());
static_assert(kMetaType<UInt32>.IsUnsigned());

static_assert(!kMetaType<_Float16>.IsWrappedFloat());
static_assert(!kMetaType<float>.IsWrappedFloat());
static_assert(!kMetaType<double>.IsWrappedFloat());
static_assert(!kMetaType<int32_t>.IsWrappedFloat());
static_assert(!kMetaType<uint32_t>.IsWrappedFloat());
static_assert(!kMetaType<void>.IsWrappedFloat());
static_assert(!kMetaType<void*>.IsWrappedFloat());
static_assert(kMetaType<Float16>.IsWrappedFloat());
static_assert(kMetaType<Float32>.IsWrappedFloat());
static_assert(kMetaType<Float64>.IsWrappedFloat());
static_assert(!kMetaType<Int32>.IsWrappedFloat());
static_assert(!kMetaType<RawInt32>.IsWrappedFloat());
static_assert(!kMetaType<SatInt32>.IsWrappedFloat());
static_assert(!kMetaType<SatUInt32>.IsWrappedFloat());
static_assert(!kMetaType<UInt32>.IsWrappedFloat());

static_assert(!kMetaType<_Float16>.IsWrappingInt());
static_assert(!kMetaType<float>.IsWrappingInt());
static_assert(!kMetaType<double>.IsWrappingInt());
static_assert(!kMetaType<int32_t>.IsWrappingInt());
static_assert(!kMetaType<uint32_t>.IsWrappingInt());
static_assert(!kMetaType<void>.IsWrappingInt());
static_assert(!kMetaType<void*>.IsWrappingInt());
static_assert(!kMetaType<Float16>.IsWrappingInt());
static_assert(!kMetaType<Float32>.IsWrappingInt());
static_assert(!kMetaType<Float64>.IsWrappingInt());
static_assert(kMetaType<Int32>.IsWrappingInt());
static_assert(!kMetaType<RawInt32>.IsWrappingInt());
static_assert(!kMetaType<SatInt32>.IsWrappingInt());
static_assert(!kMetaType<SatUInt32>.IsWrappingInt());
static_assert(kMetaType<UInt32>.IsWrappingInt());

static_assert(std::is_same_v<Type<kMetaType<int32_t>.MakeSigned()>, int32_t>);
static_assert(std::is_same_v<Type<kMetaType<uint32_t>.MakeSigned()>, int32_t>);
static_assert(std::is_same_v<Type<kMetaType<Int32>.MakeSigned()>, Int32>);
static_assert(std::is_same_v<Type<kMetaType<SatInt32>.MakeSigned()>, SatInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatUInt32>.MakeSigned()>, SatInt32>);
static_assert(std::is_same_v<Type<kMetaType<UInt32>.MakeSigned()>, Int32>);

static_assert(std::is_same_v<Type<kMetaType<int32_t>.MakeUnsigned()>, uint32_t>);
static_assert(std::is_same_v<Type<kMetaType<uint32_t>.MakeUnsigned()>, uint32_t>);
static_assert(std::is_same_v<Type<kMetaType<Int32>.MakeUnsigned()>, UInt32>);
static_assert(std::is_same_v<Type<kMetaType<RawInt32>.MakeUnsigned()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatInt32>.MakeUnsigned()>, SatUInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatUInt32>.MakeUnsigned()>, SatUInt32>);
static_assert(std::is_same_v<Type<kMetaType<UInt32>.MakeUnsigned()>, UInt32>);

static_assert(std::is_same_v<Type<kMetaType<int32_t>.Raw()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<uint32_t>.Raw()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<Int32>.Raw()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<RawInt32>.Raw()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatInt32>.Raw()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatUInt32>.Raw()>, RawInt32>);
static_assert(std::is_same_v<Type<kMetaType<UInt32>.Raw()>, RawInt32>);

static_assert(std::is_same_v<Type<kMetaType<int32_t>.Saturating()>, SatInt32>);
static_assert(std::is_same_v<Type<kMetaType<uint32_t>.Saturating()>, SatUInt32>);
static_assert(std::is_same_v<Type<kMetaType<Int32>.Saturating()>, SatInt32>);
static_assert(std::is_same_v<Type<kMetaType<RawInt32>.Saturating()>, SatUInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatInt32>.Saturating()>, SatInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatUInt32>.Saturating()>, SatUInt32>);
static_assert(std::is_same_v<Type<kMetaType<UInt32>.Saturating()>, SatUInt32>);

enum Foo : int;

static_assert(std::is_same_v<Type<kMetaType<Foo>.UnderlyingType()>, int>);
static_assert(std::is_same_v<Type<kMetaType<Float16>.UnderlyingType()>, _Float16>);
static_assert(std::is_same_v<Type<kMetaType<Float32>.UnderlyingType()>, float>);
static_assert(std::is_same_v<Type<kMetaType<Float64>.UnderlyingType()>, double>);
static_assert(std::is_same_v<Type<kMetaType<Int32>.UnderlyingType()>, int32_t>);
static_assert(std::is_same_v<Type<kMetaType<RawInt32>.UnderlyingType()>, uint32_t>);
static_assert(std::is_same_v<Type<kMetaType<SatInt32>.UnderlyingType()>, int32_t>);
static_assert(std::is_same_v<Type<kMetaType<SatUInt32>.UnderlyingType()>, uint32_t>);
static_assert(std::is_same_v<Type<kMetaType<UInt32>.UnderlyingType()>, uint32_t>);

static_assert(std::is_same_v<Type<kMetaType<_Float16>.Wrapped()>, Float16>);
static_assert(std::is_same_v<Type<kMetaType<float>.Wrapped()>, Float32>);
static_assert(std::is_same_v<Type<kMetaType<double>.Wrapped()>, Float64>);
static_assert(std::is_same_v<Type<kMetaType<Float16>.Wrapped()>, Float16>);
static_assert(std::is_same_v<Type<kMetaType<Float32>.Wrapped()>, Float32>);
static_assert(std::is_same_v<Type<kMetaType<Float64>.Wrapped()>, Float64>);

static_assert(std::is_same_v<Type<kMetaType<int32_t>.Wrapping()>, Int32>);
static_assert(std::is_same_v<Type<kMetaType<uint32_t>.Wrapping()>, UInt32>);
static_assert(std::is_same_v<Type<kMetaType<Int32>.Wrapping()>, Int32>);
static_assert(std::is_same_v<Type<kMetaType<RawInt32>.Wrapping()>, UInt32>);
static_assert(std::is_same_v<Type<kMetaType<SatInt32>.Wrapping()>, Int32>);
static_assert(std::is_same_v<Type<kMetaType<SatUInt32>.Wrapping()>, UInt32>);
static_assert(std::is_same_v<Type<kMetaType<UInt32>.Wrapping()>, UInt32>);

// Note: we want to have references in out tests to produce results with references and also
// verify that types of the results include references, not straight `int`s, this is somewhat
// tricky and the main reason to use static_assert based tests here: dangling reference is in UB
// in normal tests, but a compile-time error in static_assert based tests.

constexpr int kForEachInt1 = 1;
constexpr int kForEachInt2 = 2;

constexpr std::pair<const int&, char> kForEachPairIn{kForEachInt1, 'A'};
constexpr std::tuple<const int&, char> kForEachTupleIn{kForEachInt1, 'A'};

template <typename T>
[[__nodiscard__]] inline constexpr std::remove_cvref_t<T>&& MoveToNonConst(T&& t) noexcept {
  return const_cast<std::remove_cvref_t<T>&&>(t);
}

#ifdef _MSC_VER
#define FOR_EACH_TUPLE_TYPE_CAPTURE &kForEachTupleTypeIn
#else
#define FOR_EACH_TUPLE_TYPE_CAPTURE
#endif

template <template <typename...> typename TupleType>
constexpr bool TestFunc() {
  constexpr auto& kForEachTupleTypeIn = []() -> auto& {
    if constexpr (std::is_same_v<TupleType<char, char>, std::pair<char, char>>) {
      return kForEachPairIn;
    } else {
      return kForEachTupleIn;
    }
  }();

  static_assert(TypesToTypes::All<const TupleType<char, char>,
                                  []<typename T>() { return std::is_same_v<T, const char>; }>{});
  static_assert(!TypesToTypes::All<const TupleType<char, int&>,
                                   []<typename T>() { return std::is_same_v<T, const char>; }>{});
  static_assert(!TypesToTypes::All<const TupleType<float, int&>,
                                   []<typename T>() { return std::is_same_v<T, const char>; }>{});

  static_assert(TypesToTypes::All<const TupleType<char, char>&,
                                  []<typename T>() { return std::is_same_v<T, const char&>; }>{});
  static_assert(!TypesToTypes::All<const TupleType<char, int&>&,
                                   []<typename T>() { return std::is_same_v<T, const char&>; }>{});
  static_assert(!TypesToTypes::All<const TupleType<float, int&>&,
                                   []<typename T>() { return std::is_same_v<T, const char&>; }>{});

  static_assert(TypesToTypes::All<const TupleType<char, char>&&,
                                  []<typename T>() { return std::is_same_v<T, const char&&>; }>{});
  static_assert(!TypesToTypes::All<const TupleType<char, int&>&&,
                                   []<typename T>() { return std::is_same_v<T, const char&&>; }>{});
  static_assert(!TypesToTypes::All<const TupleType<float, int&>&&,
                                   []<typename T>() { return std::is_same_v<T, const char&&>; }>{});

  static_assert(TypesToTypes::All<TupleType<char, char>,
                                  []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(!TypesToTypes::All<TupleType<char, int&>,
                                   []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(!TypesToTypes::All<TupleType<float, int&>,
                                   []<typename T>() { return std::is_same_v<T, char>; }>{});

  static_assert(TypesToTypes::All<TupleType<char, char>&,
                                  []<typename T>() { return std::is_same_v<T, char&>; }>{});
  static_assert(!TypesToTypes::All<TupleType<char, int&>&,
                                   []<typename T>() { return std::is_same_v<T, char&>; }>{});
  static_assert(!TypesToTypes::All<TupleType<float, int&>&,
                                   []<typename T>() { return std::is_same_v<T, char&>; }>{});

  static_assert(TypesToTypes::All<TupleType<char, char>&&,
                                  []<typename T>() { return std::is_same_v<T, char&&>; }>{});
  static_assert(!TypesToTypes::All<TupleType<char, int&>&&,
                                   []<typename T>() { return std::is_same_v<T, char&&>; }>{});
  static_assert(!TypesToTypes::All<TupleType<float, int&>&&,
                                   []<typename T>() { return std::is_same_v<T, char&&>; }>{});

  static_assert(TypesToTypes::Any<const TupleType<char, char>,
                                  []<typename T>() { return std::is_same_v<T, const char>; }>{});
  static_assert(TypesToTypes::Any<const TupleType<char, int&>,
                                  []<typename T>() { return std::is_same_v<T, const char>; }>{});
  static_assert(!TypesToTypes::Any<const TupleType<float, int&>,
                                   []<typename T>() { return std::is_same_v<T, const char>; }>{});

  static_assert(TypesToTypes::Any<const TupleType<char, char>&,
                                  []<typename T>() { return std::is_same_v<T, const char&>; }>{});
  static_assert(TypesToTypes::Any<const TupleType<char, int&>&,
                                  []<typename T>() { return std::is_same_v<T, const char&>; }>{});
  static_assert(!TypesToTypes::Any<const TupleType<float, int&>&,
                                   []<typename T>() { return std::is_same_v<T, const char&>; }>{});

  static_assert(TypesToTypes::Any<const TupleType<char, char>&&,
                                  []<typename T>() { return std::is_same_v<T, const char&&>; }>{});
  static_assert(TypesToTypes::Any<const TupleType<char, int&>&&,
                                  []<typename T>() { return std::is_same_v<T, const char&&>; }>{});
  static_assert(!TypesToTypes::Any<const TupleType<float, int&>&&,
                                   []<typename T>() { return std::is_same_v<T, const char&&>; }>{});

  static_assert(TypesToTypes::Any<TupleType<char, char>,
                                  []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(TypesToTypes::Any<TupleType<char, int&>,
                                  []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(!TypesToTypes::Any<TupleType<float, int&>,
                                   []<typename T>() { return std::is_same_v<T, char>; }>{});

  static_assert(TypesToTypes::Any<TupleType<char, char>&,
                                  []<typename T>() { return std::is_same_v<T, char&>; }>{});
  static_assert(TypesToTypes::Any<TupleType<char, int&>&,
                                  []<typename T>() { return std::is_same_v<T, char&>; }>{});
  static_assert(!TypesToTypes::Any<TupleType<float, int&>&,
                                   []<typename T>() { return std::is_same_v<T, char&>; }>{});

  static_assert(TypesToTypes::Any<TupleType<char, char>&&,
                                  []<typename T>() { return std::is_same_v<T, char&&>; }>{});
  static_assert(TypesToTypes::Any<TupleType<char, int&>&&,
                                  []<typename T>() { return std::is_same_v<T, char&&>; }>{});
  static_assert(!TypesToTypes::Any<TupleType<float, int&>&&,
                                   []<typename T>() { return std::is_same_v<T, char&&>; }>{});

  static_assert(!TypesToTypes::Contains<const TupleType<char&, char&>, const char>{});
  static_assert(TypesToTypes::Contains<const TupleType<char&, char>, const char>{});
  static_assert(TypesToTypes::Contains<const TupleType<char, char&>, const char>{});
  static_assert(TypesToTypes::Contains<const TupleType<char, char>, const char>{});

  static_assert(!TypesToTypes::Contains<const TupleType<char&, char&>&, const char&>{});
  static_assert(TypesToTypes::Contains<const TupleType<char&, char>&, const char&>{});
  static_assert(TypesToTypes::Contains<const TupleType<char, char&>&, const char&>{});
  static_assert(TypesToTypes::Contains<const TupleType<char, char>&, const char&>{});

  static_assert(!TypesToTypes::Contains<const TupleType<char&, char&>&&, const char&&>{});
  static_assert(TypesToTypes::Contains<const TupleType<char&, char>&&, const char&&>{});
  static_assert(TypesToTypes::Contains<const TupleType<char, char&>&&, const char&&>{});
  static_assert(TypesToTypes::Contains<const TupleType<char, char>&&, const char&&>{});

  static_assert(!TypesToValues::Contains<TupleType<const char, const char>, char>());
  static_assert(TypesToValues::Contains<TupleType<const char, char>, char>());
  static_assert(TypesToValues::Contains<TupleType<char, const char>, char>());
  static_assert(TypesToValues::Contains<TupleType<char, char>, char>());

  static_assert(!TypesToTypes::Contains<TupleType<char&, char&>, char>{});
  static_assert(TypesToTypes::Contains<TupleType<char&, char>, char>{});
  static_assert(TypesToTypes::Contains<TupleType<char, char&>, char>{});
  static_assert(TypesToTypes::Contains<TupleType<char, char>, char>{});

  static_assert(TypesToTypes::Contains<TupleType<char&&, char&&>, char&&>{});
  static_assert(TypesToTypes::Contains<TupleType<char&&, char>, char&&>{});
  static_assert(TypesToTypes::Contains<TupleType<char, char&&>, char&&>{});
  static_assert(!TypesToTypes::Contains<TupleType<char, char>, char&&>{});

  static_assert(!TypesToTypes::Contains<TupleType<const char, const char>&, char&>{});
  static_assert(TypesToTypes::Contains<TupleType<const char, char>&, char&>{});
  static_assert(TypesToTypes::Contains<TupleType<char, const char>&, char&>{});
  static_assert(TypesToTypes::Contains<TupleType<char, char>&, char&>{});

  static_assert(!TypesToValues::Contains<TupleType<const char, const char>&&, char&&>());
  static_assert(TypesToValues::Contains<TupleType<const char, char>&&, char&&>());
  static_assert(TypesToValues::Contains<TupleType<char, const char>&&, char&&>());
  static_assert(TypesToValues::Contains<TupleType<char, char>&&, char&&>());

  // Combination of rvalue references and lvalue references lead to lvalue references.
  static_assert(!TypesToTypes::Contains<TupleType<char&&, char&&>&, char&&>{});
  static_assert(TypesToTypes::Contains<TupleType<char&&, char&&>&, char&>{});
  static_assert(TypesToTypes::Contains<TupleType<char&&, char>&, char&>{});
  static_assert(TypesToTypes::Contains<TupleType<char&, char&>&&, char&>{});

  static_assert(TypesToTypes::Count<const TupleType<char&, char&>, const char>{} == 0);
  static_assert(TypesToTypes::Count<const TupleType<char&, char>, const char>{} == 1);
  static_assert(TypesToTypes::Count<const TupleType<char, char&>, const char>{} == 1);
  static_assert(TypesToTypes::Count<const TupleType<char, char>, const char>{} == 2);

  static_assert(TypesToTypes::Count<const TupleType<char&, char&>&, const char&>{} == 0);
  static_assert(TypesToTypes::Count<const TupleType<char&, char>&, const char&>{} == 1);
  static_assert(TypesToTypes::Count<const TupleType<char, char&>&, const char&>{} == 1);
  static_assert(TypesToTypes::Count<const TupleType<char, char>&, const char&>{} == 2);

  static_assert(TypesToTypes::Count<const TupleType<char&, char&>&&, const char&&>{} == 0);
  static_assert(TypesToTypes::Count<const TupleType<char&, char>&&, const char&&>{} == 1);
  static_assert(TypesToTypes::Count<const TupleType<char, char&>&&, const char&&>{} == 1);
  static_assert(TypesToTypes::Count<const TupleType<char, char>&&, const char&&>{} == 2);

  static_assert(TypesToValues::Count<TupleType<const char, const char>, char>() == 0);
  static_assert(TypesToValues::Count<TupleType<const char, char>, char>() == 1);
  static_assert(TypesToValues::Count<TupleType<char, const char>, char>() == 1);
  static_assert(TypesToValues::Count<TupleType<char, char>, char>() == 2);

  static_assert(TypesToTypes::Count<TupleType<char&, char&>, char>{} == 0);
  static_assert(TypesToTypes::Count<TupleType<char&, char>, char>{} == 1);
  static_assert(TypesToTypes::Count<TupleType<char, char&>, char>{} == 1);
  static_assert(TypesToTypes::Count<TupleType<char, char>, char>{} == 2);

  static_assert(TypesToTypes::Count<TupleType<char&&, char&&>, char&&>{} == 2);
  static_assert(TypesToTypes::Count<TupleType<char&&, char>, char&&>{} == 1);
  static_assert(TypesToTypes::Count<TupleType<char, char&&>, char&&>{} == 1);
  static_assert(TypesToTypes::Count<TupleType<char, char>, char&&>{} == 0);

  static_assert(TypesToTypes::Count<TupleType<const char, const char>&, char&>{} == 0);
  static_assert(TypesToTypes::Count<TupleType<const char, char>&, char&>{} == 1);
  static_assert(TypesToTypes::Count<TupleType<char, const char>&, char&>{} == 1);
  static_assert(TypesToTypes::Count<TupleType<char, char>&, char&>{} == 2);

  static_assert(TypesToValues::Count<TupleType<const char, const char>&&, char&&>() == 0);
  static_assert(TypesToValues::Count<TupleType<const char, char>&&, char&&>() == 1);
  static_assert(TypesToValues::Count<TupleType<char, const char>&&, char&&>() == 1);
  static_assert(TypesToValues::Count<TupleType<char, char>&&, char&&>() == 2);

  // Combination of rvalue references and lvalue references lead to lvalue references.
  static_assert(TypesToTypes::Count<TupleType<char&&, char&&>&, char&&>{} == 0);
  static_assert(TypesToTypes::Count<TupleType<char&&, char&&>&, char&>{} == 2);
  static_assert(TypesToTypes::Count<TupleType<char&&, char>&, char&>{} == 2);
  static_assert(TypesToTypes::Count<TupleType<char&, char&>&&, char&>{} == 2);

  static_assert(TypesToTypes::CountIf<TupleType<int, int>,
                                      []<typename T>() { return sizeof(T) < sizeof(int); }>{} == 0);
  static_assert(TypesToTypes::CountIf<TupleType<char, int>,
                                      []<typename T>() { return sizeof(T) < sizeof(int); }>{} == 1);
  static_assert(TypesToTypes::CountIf<TupleType<int, char>,
                                      []<typename T>() { return sizeof(T) < sizeof(int); }>{} == 1);
  static_assert(TypesToTypes::CountIf<TupleType<char, char>,
                                      []<typename T>() { return sizeof(T) < sizeof(int); }>{} == 2);

  static_assert(std::is_same_v<TypesToTypes::Enumerate<const TupleType<char, int&>>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, const char>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(std::is_same_v<TypesToTypes::Enumerate<const TupleType<char, int&>&>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, const char&>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(std::is_same_v<TypesToTypes::Enumerate<const TupleType<char, int&>&&>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, const char&&>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(std::is_same_v<TypesToTypes::Enumerate<TupleType<char, int&>>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, char>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(std::is_same_v<TypesToTypes::Enumerate<TupleType<char, int&>&>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, char&>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(std::is_same_v<TypesToTypes::Enumerate<TupleType<char, int&>&&>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, char&&>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(
      std::is_same_v<
          TypesToTypes::Enumerate<const TupleType<char, int&>,
                                  const TupleType<char, int&>&,
                                  const TupleType<char, int&>&&,
                                  TupleType<char, int&>,
                                  TupleType<char, int&>&,
                                  TupleType<char, int&>&&>,
          std::tuple<std::tuple<MetaValue<std::size_t{0}>,
                                const char,
                                const char&,
                                const char&&,
                                char,
                                char&,
                                char&&>,
                     std::tuple<MetaValue<std::size_t{1}>, int&, int&, int&, int&, int&, int&>>>);

  static_assert(std::is_same_v<TypesToTypes::EnumerateShortest<const TupleType<char, int&>,
                                                               const TupleType<char, int&>&,
                                                               const TupleType<char, int&>&&,
                                                               TupleType<char, int&>,
                                                               TupleType<char, int&>&,
                                                               TupleType<char, int&>&&,
                                                               std::array<double, 1>&>,
                               std::tuple<std::tuple<MetaValue<std::size_t{0}>,
                                                     const char,
                                                     const char&,
                                                     const char&&,
                                                     char,
                                                     char&,
                                                     char&&,
                                                     double&>>>);

  static_assert(std::is_same_v<
                TypesToTypes::Filter<const TupleType<char, int&>,
                                     []<typename T>() { return std::is_same_v<T, const char>; }>,
                std::tuple<const char>>);

  static_assert(std::is_same_v<
                TypesToTypes::Filter<const TupleType<char, int&>&,
                                     []<typename T>() { return std::is_same_v<T, const char&>; }>,
                std::tuple<const char&>>);

  static_assert(std::is_same_v<
                TypesToTypes::Filter<const TupleType<char, int&>&&,
                                     []<typename T>() { return std::is_same_v<T, const char&&>; }>,
                std::tuple<const char&&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Filter<TupleType<char, int&>,
                                          []<typename T>() { return std::is_same_v<T, char>; }>,
                     std::tuple<char>>);

  static_assert(
      std::is_same_v<TypesToTypes::Filter<TupleType<char, int&>&,
                                          []<typename T>() { return std::is_same_v<T, char&>; }>,
                     std::tuple<char&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Filter<TupleType<char, int&>&&,
                                          []<typename T>() { return std::is_same_v<T, char&&>; }>,
                     std::tuple<char&&>>);

  // Test mapping by type.
  static_assert(
      std::is_same_v<TypesToTypes::FlatMap<const TupleType<char, int&>,
                                           []<typename T>() -> decltype(auto) {
                                             if constexpr (std::is_same_v<T, const char>) {
                                               return kMetaTypes<float, float>;
                                             } else {
                                               return kMetaTypes<T>;
                                             }
                                           }>,
                     std::tuple<float, float, int&>>);

  static_assert(
      std::is_same_v<TypesToTypes::FlatMap<const TupleType<char, int&>&,
                                           []<typename T>() -> decltype(auto) {
                                             if constexpr (std::is_same_v<T, const char&>) {
                                               return kMetaTypes<float, float>;
                                             } else {
                                               return kMetaTypes<T>;
                                             }
                                           }>,
                     std::tuple<float, float, int&>>);

  static_assert(
      std::is_same_v<TypesToTypes::FlatMap<const TupleType<char, int&>&&,
                                           []<typename T>() -> decltype(auto) {
                                             if constexpr (std::is_same_v<T, const char&&>) {
                                               return kMetaTypes<float, float>;
                                             } else {
                                               return kMetaTypes<T>;
                                             }
                                           }>,
                     std::tuple<float, float, int&>>);

  static_assert(
      std::is_same_v<TypesToTypes::FlatMap<TupleType<char, int&>,
                                           []<typename T>() -> decltype(auto) {
                                             if constexpr (std::is_same_v<T, char>) {
                                               return kMetaTypes<float, float>;
                                             } else {
                                               return kMetaTypes<T>;
                                             }
                                           }>,
                     std::tuple<float, float, int&>>);

  static_assert(std::is_same_v<TypesToTypes::FlatMap<TupleType<char, int&>&,
                                                     []<typename T>() -> decltype(auto) {
                                                       if constexpr (std::is_same_v<T, char&>) {
                                                         return kMetaTypes<float, float>;
                                                       } else {
                                                         return kMetaTypes<T>;
                                                       }
                                                     }>,
                               std::tuple<float, float, int&>>);

  static_assert(std::is_same_v<TypesToTypes::FlatMap<TupleType<char, int&>&&,
                                                     []<typename T>() -> decltype(auto) {
                                                       if constexpr (std::is_same_v<T, char&&>) {
                                                         return kMetaTypes<float, float>;
                                                       } else {
                                                         return kMetaTypes<T>;
                                                       }
                                                     }>,
                               std::tuple<float, float, int&>>);

  // Test mapping by value.
  static_assert(std::is_same_v<TypesToTypes::Map<const TupleType<char, const int&>,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, const char>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Map<const TupleType<char, const int&>&,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, const char&>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Map<const TupleType<char, const int&>&&,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, const char&&>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Map<TupleType<char, const int&>,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, char>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Map<TupleType<char, const int&>&,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, char&>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Map<TupleType<char, const int&>&&,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, char&&>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char&>, const char>,
                               std::tuple<>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<const char, const char>, char&>,
                               std::tuple<>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<const char, char&>, char&>,
                               std::tuple<char&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, const char>, char&>,
                               std::tuple<char&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char&>, char&>,
                               std::tuple<char&, char&>>);

  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char&>&, const char&>,
                               std::tuple<>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char, char&>&, const char&>,
                               std::tuple<const char&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char>&, const char&>,
                               std::tuple<const char&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char, char>&, const char&>,
                               std::tuple<const char&, const char&>>);

  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char&>&&, const char&>,
                               std::tuple<>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char, char&>&&, const char&&>,
                               std::tuple<const char&&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char>&&, const char&&>,
                               std::tuple<const char&&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<const TupleType<char, char>&&, const char&&>,
                               std::tuple<const char&&, const char&&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<const char, const char>, char>, std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<const char, char>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, const char>, char>, std::tuple<char>>);

  static_assert(
      std::is_same_v<TypesToTypes::Retain<const TupleType<char&, char&>, char>, std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char&, char>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, char&>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, char>, char>, std::tuple<char, char>>);

  static_assert(std::is_same_v<TypesToTypes::Retain<TupleType<const char&, const char&>&, char&>,
                               std::tuple<>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<TupleType<const char&, char>&, char&>,
                               std::tuple<char&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<TupleType<char, const char&>&, char&>,
                               std::tuple<char&>>);
  static_assert(std::is_same_v<TypesToTypes::Retain<TupleType<char, char>&, char&>,
                               std::tuple<char&, char&>>);

  static_assert(std::is_same_v<TypesToTypes::Retain<TupleType<char&, char&>, char>, std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char&, char>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, char&>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, char>, char>, std::tuple<char, char>>);

  static_assert(std::is_same_v<TypesToTypes::Retain<TupleType<char&, char&>, char>, std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char&, char>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, char&>, char>, std::tuple<char>>);
  static_assert(
      std::is_same_v<TypesToTypes::Retain<TupleType<char, char>, char>, std::tuple<char, char>>);

  static_assert(std::is_same_v<TypesToTypes::RetainIfNot<TupleType<const char, const char>, char>,
                               std::tuple<const char, const char>>);
  static_assert(std::is_same_v<TypesToTypes::RetainIfNot<TupleType<const char, char>, char>,
                               std::tuple<const char>>);
  static_assert(std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char, const char>, char>,
                               std::tuple<const char>>);
  static_assert(
      std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char, char>, char>, std::tuple<>>);

  static_assert(std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char&, char&>, char>,
                               std::tuple<char&, char&>>);
  static_assert(
      std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char&, char>, char>, std::tuple<char&>>);
  static_assert(
      std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char, char&>, char>, std::tuple<char&>>);
  static_assert(
      std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char, char>, char>, std::tuple<>>);

  static_assert(std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char&, char>, void>,
                               std::tuple<char&, char>>);
  static_assert(std::is_same_v<TypesToTypes::RetainIfNot<TupleType<char, char&>, void>,
                               std::tuple<char, char&>>);

  static_assert(std::is_same_v<TypesToTypes::Skip<const TupleType<char, const int>, 1>,
                               std::tuple<const int>>);

  static_assert(std::is_same_v<TypesToTypes::Skip<const TupleType<char, const int>&, 1>,
                               std::tuple<const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Skip<const TupleType<char, const int&&>, 1>,
                               std::tuple<const int&&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Skip<TupleType<char, const int>, 1>, std::tuple<const int>>);

  static_assert(
      std::is_same_v<TypesToTypes::Skip<TupleType<char, const int&>, 1>, std::tuple<const int&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Skip<TupleType<char, const int&&>, 1>, std::tuple<const int&&>>);

  static_assert(std::is_same_v<TypesToTypes::SkipWhile<const TupleType<char, const int>,
                                                       []<typename T>() { return true; }>,
                               std::tuple<>>);
  static_assert(std::is_same_v<
                TypesToTypes::SkipWhile<const TupleType<char, const int>,
                                        []<typename T>() { return std::is_same_v<T, const char>; }>,
                std::tuple<const int>>);
  static_assert(std::is_same_v<
                TypesToTypes::SkipWhile<const TupleType<char, const int>,
                                        []<typename T>() { return std::is_same_v<T, const int>; }>,
                std::tuple<const char, const int>>);

  static_assert(std::is_same_v<TypesToTypes::SkipWhile<const TupleType<char, const int>&,
                                                       []<typename T>() { return true; }>,
                               std::tuple<>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<const TupleType<char, const int>&,
                                  []<typename T>() { return std::is_same_v<T, const char&>; }>,
          std::tuple<const int&>>);
  static_assert(std::is_same_v<
                TypesToTypes::SkipWhile<const TupleType<char, const int>&,
                                        []<typename T>() { return std::is_same_v<T, const int&>; }>,
                std::tuple<const char&, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::SkipWhile<const TupleType<char, const int>&&,
                                                       []<typename T>() { return true; }>,
                               std::tuple<>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<const TupleType<char, const int>&&,
                                  []<typename T>() { return std::is_same_v<T, const char&&>; }>,
          std::tuple<const int&&>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<const TupleType<char, const int>&&,
                                  []<typename T>() { return std::is_same_v<T, const int&&>; }>,
          std::tuple<const char&&, const int&&>>);

  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<TupleType<char, const int>, []<typename T>() { return true; }>,
          std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::SkipWhile<TupleType<char, const int>,
                                             []<typename T>() { return std::is_same_v<T, char>; }>,
                     std::tuple<const int>>);
  static_assert(std::is_same_v<
                TypesToTypes::SkipWhile<TupleType<char, const int>,
                                        []<typename T>() { return std::is_same_v<T, const int>; }>,
                std::tuple<char, const int>>);

  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<TupleType<char, const int>&, []<typename T>() { return true; }>,
          std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::SkipWhile<TupleType<char, const int>&,
                                             []<typename T>() { return std::is_same_v<T, char&>; }>,
                     std::tuple<const int&>>);
  static_assert(std::is_same_v<
                TypesToTypes::SkipWhile<TupleType<char, const int>&,
                                        []<typename T>() { return std::is_same_v<T, const int&>; }>,
                std::tuple<char&, const int&>>);

  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<TupleType<char, const int>&&, []<typename T>() { return true; }>,
          std::tuple<>>);
  static_assert(std::is_same_v<
                TypesToTypes::SkipWhile<TupleType<char, const int>&&,
                                        []<typename T>() { return std::is_same_v<T, char&&>; }>,
                std::tuple<const int&&>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::SkipWhile<TupleType<char, const int>&&,
                                  []<typename T>() { return std::is_same_v<T, const int&&>; }>,
          std::tuple<char&&, const int&&>>);

  static_assert(std::is_same_v<TypesToTypes::Take<const TupleType<char, const int>, 1>,
                               std::tuple<const char>>);

  static_assert(std::is_same_v<TypesToTypes::Take<const TupleType<char, const int>&, 1>,
                               std::tuple<const char&>>);

  static_assert(std::is_same_v<TypesToTypes::Take<const TupleType<char, const int>&&, 1>,
                               std::tuple<const char&&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Take<TupleType<char, const int>, 1>, std::tuple<char>>);

  static_assert(
      std::is_same_v<TypesToTypes::Take<TupleType<char, const int>&, 1>, std::tuple<char&>>);

  static_assert(
      std::is_same_v<TypesToTypes::Take<TupleType<char, const int>&&, 1>, std::tuple<char&&>>);

  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<const TupleType<char, const int>,
                                        []<typename T>() { return std::is_same_v<T, const int>; }>,
                std::tuple<>>);
  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<const TupleType<char, const int>,
                                        []<typename T>() { return std::is_same_v<T, const char>; }>,
                std::tuple<const char>>);
  static_assert(std::is_same_v<TypesToTypes::TakeWhile<const TupleType<char, const int>,
                                                       []<typename T>() { return true; }>,
                               std::tuple<const char, const int>>);

  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<const TupleType<char, const int>&,
                                        []<typename T>() { return std::is_same_v<T, const int>; }>,
                std::tuple<>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::TakeWhile<const TupleType<char, const int>&,
                                  []<typename T>() { return std::is_same_v<T, const char&>; }>,
          std::tuple<const char&>>);
  static_assert(std::is_same_v<TypesToTypes::TakeWhile<const TupleType<char, const int>&,
                                                       []<typename T>() { return true; }>,
                               std::tuple<const char&, const int&>>);

  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<const TupleType<char, const int>&&,
                                        []<typename T>() { return std::is_same_v<T, const int>; }>,
                std::tuple<>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::TakeWhile<const TupleType<char, const int>&&,
                                  []<typename T>() { return std::is_same_v<T, const char&&>; }>,
          std::tuple<const char&&>>);
  static_assert(std::is_same_v<TypesToTypes::TakeWhile<const TupleType<char, const int>&&,
                                                       []<typename T>() { return true; }>,
                               std::tuple<const char&&, const int&&>>);

  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<TupleType<char, const int>,
                                        []<typename T>() { return std::is_same_v<T, const int>; }>,
                std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::TakeWhile<TupleType<char, const int>,
                                             []<typename T>() { return std::is_same_v<T, char>; }>,
                     std::tuple<char>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::TakeWhile<TupleType<char, const int>, []<typename T>() { return true; }>,
          std::tuple<char, const int>>);

  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<TupleType<char, const int>&,
                                        []<typename T>() { return std::is_same_v<T, const int&>; }>,
                std::tuple<>>);
  static_assert(
      std::is_same_v<TypesToTypes::TakeWhile<TupleType<char, const int>&,
                                             []<typename T>() { return std::is_same_v<T, char&>; }>,
                     std::tuple<char&>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::TakeWhile<TupleType<char, const int>&, []<typename T>() { return true; }>,
          std::tuple<char&, const int&>>);

  static_assert(
      std::is_same_v<
          TypesToTypes::TakeWhile<TupleType<char, const int>&&,
                                  []<typename T>() { return std::is_same_v<T, const int&&>; }>,
          std::tuple<>>);
  static_assert(std::is_same_v<
                TypesToTypes::TakeWhile<TupleType<char, const int>&&,
                                        []<typename T>() { return std::is_same_v<T, char&&>; }>,
                std::tuple<char&&>>);
  static_assert(
      std::is_same_v<
          TypesToTypes::TakeWhile<TupleType<char, const int>&&, []<typename T>() { return true; }>,
          std::tuple<char&&, const int&&>>);

  // Note that array of references is illegal thus all references are removed in transformation.
  // But you may add or remove const in many ways.
  static_assert(std::is_same_v<TypesToTypes::ToArray<TupleType<char, char>>, std::array<char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<TupleType<char, char&>>, std::array<char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<TupleType<char&, char>>, std::array<char, 2>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<TupleType<char, char&&>>, std::array<char, 2>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<TupleType<char&&, char>>, std::array<char, 2>>);

  static_assert(std::is_same_v<TypesToTypes::ToArray<const TupleType<char, char>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const TupleType<char, char&>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const TupleType<char&, char>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const TupleType<char, char&&>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const TupleType<char&&, char>>,
                               std::array<const char, 2>>);

  static_assert(
      std::is_same_v<TypesToTypes::ToArray<char, TupleType<char, char>>, std::array<char, 2>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<char, TupleType<char, char&>>, std::array<char, 2>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<char, TupleType<char&, char>>, std::array<char, 2>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<char, TupleType<char, char&&>>, std::array<char, 2>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<char, TupleType<char&&, char>>, std::array<char, 2>>);

  static_assert(std::is_same_v<TypesToTypes::ToArray<const char, TupleType<char, char>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const char, TupleType<char, char&>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const char, TupleType<char&, char>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const char, TupleType<char, char&&>>,
                               std::array<const char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const char, TupleType<char&&, char>>,
                               std::array<const char, 2>>);

  static_assert(std::is_same_v<TypesToTypes::ToArray<char, TupleType<const char, const char>>,
                               std::array<char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<char, TupleType<const char, const char&>>,
                               std::array<char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<char, TupleType<const char&, const char>>,
                               std::array<char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<char, TupleType<const char, const char&&>>,
                               std::array<char, 2>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<char, TupleType<const char&&, const char>>,
                               std::array<char, 2>>);

  // Conner cases.
  static_assert(std::is_same_v<TypesToTypes::ToArray<char, std::tuple<>>, std::array<char, 0>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<const char, std::tuple<>>, std::array<const char, 0>>);
  static_assert(
      std::is_same_v<TypesToTypes::ToArray<char, std::array<const int, 0>>, std::array<char, 0>>);
  static_assert(std::is_same_v<TypesToTypes::ToArray<const char, std::array<double, 0>>,
                               std::array<const char, 0>>);

  static_assert(std::is_same_v<TypesToTypes::Zip<TupleType<char, int&>>,
                               std::tuple<std::tuple<char>, std::tuple<int&>>>);

  static_assert(
      std::is_same_v<TypesToTypes::Zip<const TupleType<char, int&>, const std::array<long, 2>>,
                     std::tuple<std::pair<const char, const long>, std::pair<int&, const long>>>);

  static_assert(std::is_same_v<
                TypesToTypes::Zip<const TupleType<char, int&>&, const std::array<long, 2>&>,
                std::tuple<std::pair<const char&, const long&>, std::pair<int&, const long&>>>);

  static_assert(std::is_same_v<
                TypesToTypes::Zip<const TupleType<char, int&>&&, const std::array<long, 2>&&>,
                std::tuple<std::pair<const char&&, const long&&>, std::pair<int&, const long&&>>>);

  static_assert(std::is_same_v<TypesToTypes::Zip<TupleType<char, int&>, std::array<long, 2>>,
                               std::tuple<std::pair<char, long>, std::pair<int&, long>>>);

  static_assert(std::is_same_v<TypesToTypes::Zip<TupleType<char, int&>&, std::array<long, 2>&>,
                               std::tuple<std::pair<char&, long&>, std::pair<int&, long&>>>);

  static_assert(std::is_same_v<TypesToTypes::Zip<TupleType<char, int&>&&, std::array<long, 2>&&>,
                               std::tuple<std::pair<char&&, long&&>, std::pair<int&, long&&>>>);

  static_assert(
      std::is_same_v<
          TypesToTypes::Zip<TupleType<char, int&>, std::array<long, 2>, TupleType<long&, short>>,
          std::tuple<std::tuple<char, long, long&>, std::tuple<int&, long, short>>>);

  static_assert(std::is_same_v<TypesToTypes::ZipShortest<TupleType<char, int&>>,
                               std::tuple<std::tuple<char>, std::tuple<int&>>>);

  static_assert(
      std::is_same_v<TypesToTypes::ZipShortest<TupleType<char, int&>, std::array<long, 2>>,
                     std::tuple<std::pair<char, long>, std::pair<int&, long>>>);

  static_assert(
      std::is_same_v<TypesToTypes::ZipShortest<TupleType<char, int&>,
                                               std::array<long, 2>,
                                               TupleType<long&, short>>,
                     std::tuple<std::tuple<char, long, long&>, std::tuple<int&, long, short>>>);

  // Test All, Any, and CountIf types to values.
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::All<std::tuple<char, const int&>>(
        []<typename T>(int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return !result;
  }());
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::Any<std::tuple<char, const int&>>(
        []<typename T>(int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::CountIf<std::tuple<char, const int&>>(
        []<typename T>(int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }() == 1);
  // Test All, Any and CountIf types to values with a temporary.
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::AllWithTemporary<std::tuple<char, const int&>, int>(
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return !result;
  }());
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::AnyWithTemporary<std::tuple<char, const int&>, int>(
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::CountIfWithTemporary<std::tuple<char, const int&>, int>(
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }() == 1);
  // Test All, Any, and CountIF types to values with an explicitly initialized temporary.
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::AllWithTemporary<std::tuple<char, const int&>>(
        /* idx = */ 42,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return !result;
  }());
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::AnyWithTemporary<std::tuple<char, const int&>>(
        /* idx = */ 42,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::CountIfWithTemporary<std::tuple<char, const int&>>(
        /* idx = */
        42,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }() == 1);
  // Test All, Any and CountIf values to values.
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::All(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(T&&, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return !result;
  }());
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::Any(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(T&&, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::CountIf(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(T&&, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }() == 1);
  // Test All, Any and CountIf values to values with a temporary.
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::AllWithTemporary<int>(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(T&&, int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return !result;
  }());
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::AnyWithTemporary<int>(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(T&&, int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::CountIfWithTemporary<int>(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(T&&, int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }() == 1);
  // Test All, Any, and Count values to values with an explicitly initialized temporary.
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::AllWithTemporary(
        MoveToNonConst(kForEachTupleTypeIn),
        /* idx = */ 42,
        []<typename T>(T&&, int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return !result;
  }());
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::AnyWithTemporary(
        MoveToNonConst(kForEachTupleTypeIn),
        /* idx = */ 42,
        []<typename T>(T&&, int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          return std::is_same_v<T, const int&>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  static_assert([FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = ValuesToValues::CountIfWithTemporary(
        MoveToNonConst(kForEachTupleTypeIn),
        /* idx = */ 42,
        []<typename T>(T&&, int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          return std::is_same_v<T, char>;
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }() == 1);

  static_assert(ValuesToValues::Contains<const int&>(kForEachTupleTypeIn));
  static_assert(ValuesToValues::Contains<const int&>(MoveToNonConst(kForEachTupleTypeIn)));

  static_assert(!ValuesToValues::Contains<const float&>(kForEachTupleTypeIn));
  static_assert(!ValuesToValues::Contains<const float&>(MoveToNonConst(kForEachTupleTypeIn)));

  static_assert(ValuesToValues::Contains(kForEachTupleTypeIn, 'A'));
  static_assert(ValuesToValues::Contains(MoveToNonConst(kForEachTupleTypeIn), 'A'));

  static_assert(!ValuesToValues::Contains(TupleType{"A", 'B'}, 'A'));
  static_assert(ValuesToValues::Contains(TupleType{"B", 'A'}, 'A'));

  static_assert(ValuesToValues::Count<const int&>(kForEachTupleTypeIn) == 1);
  static_assert(ValuesToValues::Count<const int&>(MoveToNonConst(kForEachTupleTypeIn)) == 1);

  static_assert(ValuesToValues::Count<const float&>(kForEachTupleTypeIn) == 0);
  static_assert(ValuesToValues::Count<const float&>(MoveToNonConst(kForEachTupleTypeIn)) == 0);

  static_assert(ValuesToValues::Count(kForEachTupleTypeIn, 'A') == 1);
  static_assert(ValuesToValues::Count(MoveToNonConst(kForEachTupleTypeIn), 'A') == 1);

  static_assert(ValuesToValues::Count(TupleType{"A", 'B'}, 'A') == 0);
  static_assert(ValuesToValues::Count(TupleType{"B", 'A'}, 'A') == 1);

  // Enumerate for values.
#if (!defined(__clang__) && !defined(__GNUC__)) || (defined(__clang__) && __clang_major__ > 19) || \
    (defined(__GNUC__) && __GNUC__ > 14)
  static_assert(ValuesToValues::Enumerate(TupleType{'a', 42}) ==
                std::tuple{std::pair{kMeta<0>, 'a'}, std::pair{kMeta<1>, 42}});
  static_assert(ValuesToValues::Enumerate(TupleType{'a', 42}, TupleType{true, 1ULL}) ==
                std::tuple{std::tuple{kMeta<0>, 'a', true}, std::tuple{kMeta<1>, 42, 1ULL}});
  static_assert(ValuesToValues::EnumerateShortest(
                    TupleType{'a', 42}, TupleType{true, 1ULL}, std::array{1.0}) ==
                std::tuple<std::tuple<MetaValue<0>, char, bool, double>>{
                    std::tuple{kMeta<0>, 'a', true, 1.0}});
#else
  constexpr auto kEnumerateResult1 = ValuesToValues::Enumerate(TupleType{'a', 42});
  static_assert(std::get<0>(std::get<0>(kEnumerateResult1)) == kMeta<0>);
  static_assert(std::get<1>(std::get<0>(kEnumerateResult1)) == 'a');
  static_assert(std::get<0>(std::get<1>(kEnumerateResult1)) == kMeta<1>);
  static_assert(std::get<1>(std::get<1>(kEnumerateResult1)) == 42);
  constexpr auto kEnumerateResult2 =
      ValuesToValues::Enumerate(TupleType{'a', 42}, TupleType{true, 1ULL});
  static_assert(std::get<0>(std::get<0>(kEnumerateResult2)) == kMeta<0>);
  static_assert(std::get<1>(std::get<0>(kEnumerateResult2)) == 'a');
  static_assert(std::get<2>(std::get<0>(kEnumerateResult2)) == true);
  static_assert(std::get<0>(std::get<1>(kEnumerateResult2)) == kMeta<1>);
  static_assert(std::get<1>(std::get<1>(kEnumerateResult2)) == 42);
  static_assert(std::get<2>(std::get<1>(kEnumerateResult2)) == 1ULL);
  constexpr auto kEnumerateResult3 =
      ValuesToValues::EnumerateShortest(TupleType{'a', 42}, TupleType{true, 1ULL}, std::array{1.0});
  static_assert(std::get<0>(std::get<0>(kEnumerateResult3)) == kMeta<0>);
  static_assert(std::get<1>(std::get<0>(kEnumerateResult3)) == 'a');
  static_assert(std::get<2>(std::get<0>(kEnumerateResult3)) == true);
  static_assert(std::get<3>(std::get<0>(kEnumerateResult3)) == 1.0);
#endif
  static_assert(std::is_same_v<decltype(ValuesToValues::Enumerate(TupleType{'a', 42})),
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, char>,
                                          std::pair<MetaValue<std::size_t{1}>, int>>>);
  static_assert(
      std::is_same_v<decltype(ValuesToValues::Enumerate(TupleType{'a', 42}, TupleType{true, 1ULL})),
                     std::tuple<std::tuple<MetaValue<std::size_t{0}>, char, bool>,
                                std::tuple<MetaValue<std::size_t{1}>, int, unsigned long long>>>);
  static_assert(
      std::is_same_v<decltype(ValuesToValues::EnumerateShortest(
                         TupleType{'a', 42}, TupleType{true, 1ULL}, std::array{1.0})),
                     std::tuple<std::tuple<MetaValue<std::size_t{0}>, char, bool, double>>>);

  // Test FlatMap values to values.
  constexpr std::tuple<const int&> kFilterTupleOut1{kForEachInt1};
  constexpr auto kFilterResult1 = [FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    auto result = ValuesToValues::Filter(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(int& extra_arg1, int& extra_arg2) -> decltype(auto) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          if constexpr (std::is_same_v<T, const int&>) {
            return kMeta<true>;
          } else {
            return kMeta<false>;
          }
        },
        extra_arg1,
        extra_arg2);
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }();
  static_assert(kFilterResult1 == kFilterTupleOut1);
  static_assert(std::is_same_v<decltype(kFilterResult1), const std::tuple<const int&>>);
  // Test FlatMap values to values with a temporary.
  constexpr std::tuple<const int&> kFilterTupleOut2{kForEachInt1};
  constexpr auto kFilterResult2 = [FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    auto result = ValuesToValues::FilterWithTemporary<int>(
        MoveToNonConst(kForEachTupleTypeIn),
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) -> decltype(auto) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          if constexpr (std::is_same_v<T, const int&>) {
            return kMeta<true>;
          } else {
            return kMeta<false>;
          }
        },
        extra_arg1,
        extra_arg2);
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }();
  static_assert(kFilterResult2 == kFilterTupleOut2);
  static_assert(std::is_same_v<decltype(kFilterResult2), const std::tuple<const int&>>);
  // Test FlatMap values to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&> kFilterTupleOut3{kForEachInt1};
  constexpr auto kFilterResult3 = [FOR_EACH_TUPLE_TYPE_CAPTURE] {
    int extra_arg1 = 0, extra_arg2 = 0;
    auto result = ValuesToValues::FilterWithTemporary(
        MoveToNonConst(kForEachTupleTypeIn),
        /* idx = */ 42,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) -> decltype(auto) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          if constexpr (std::is_same_v<T, const int&>) {
            return kMeta<true>;
          } else {
            return kMeta<false>;
          }
        },
        extra_arg1,
        extra_arg2);
    CHECK_EQ(extra_arg1, 2);
    CHECK_EQ(extra_arg2, -2);
    return result;
  }();
  static_assert(kFilterResult3 == kFilterTupleOut3);
  static_assert(std::is_same_v<decltype(kFilterResult3), const std::tuple<const int&>>);

  // Test FlatMap types to values.
  constexpr std::tuple<const int&, float, char> kFlatMapTupleOut1{kForEachInt2, float{42.42}, 'A'};
  constexpr auto kFlatMapResult1 = TypesToValues::FlatMap<TupleType<const int&, char>>(
      []<typename T>(const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return std::tuple{float{42.42}, 'A'};
        } else {
          return std::tuple<T>(kForEachInt2);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kFlatMapResult1 == kFlatMapTupleOut1);
  static_assert(
      std::is_same_v<decltype(kFlatMapResult1), const std::tuple<const int&, float, char>>);
  // Test FlatMap types to values with a temporary.
  constexpr std::tuple<const int&, float, char> kFlatMapTupleOut2{kForEachInt2, float{43.42}, 'B'};
  constexpr auto kFlatMapResult2 =
      TypesToValues::FlatMapWithTemporary<TupleType<const int&, char>, int>(
          []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
            CHECK_EQ(&kExtraArg1, &kForEachInt1);
            CHECK_EQ(&kExtraArg2, &kForEachInt2);
            if constexpr (std::is_same_v<T, char>) {
              return std::tuple{static_cast<float>(42.42 + idx), static_cast<char>('A' + idx)};
            } else {
              idx++;
              return std::tuple<T>(kForEachInt2);
            }
          },
          kForEachInt1,
          kForEachInt2);
  static_assert(kFlatMapResult2 == kFlatMapTupleOut2);
  static_assert(
      std::is_same_v<decltype(kFlatMapResult2), const std::tuple<const int&, float, char>>);
  // Test FlatMap types to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float, char> kFlatMapTupleOut3{kForEachInt2, float{85.42}, 'l'};
  constexpr auto kFlatMapResult3 = TypesToValues::FlatMapWithTemporary<TupleType<const int&, char>>(
      /* idx = */ 42,
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return std::tuple{static_cast<float>(42.42 + idx), static_cast<char>('A' + idx)};
        } else {
          idx++;
          return std::tuple<T>(kForEachInt2);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kFlatMapResult3 == kFlatMapTupleOut3);
  static_assert(
      std::is_same_v<decltype(kFlatMapResult3), const std::tuple<const int&, float, char>>);
  // Test FlatMap values to values.
  constexpr std::tuple<const int&, float, char> kFlatMapTupleOut4{kForEachInt1, float{42.42}, 'A'};
  constexpr auto kFlatMapResult4 = ValuesToValues::FlatMap(
      MoveToNonConst(kForEachTupleTypeIn),
      []<typename T>(T&& t, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return std::tuple{float{42.42}, 'A'};
        } else {
          return std::tuple<T>{t};
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kFlatMapResult4 == kFlatMapTupleOut4);
  static_assert(
      std::is_same_v<decltype(kFlatMapResult4), const std::tuple<const int&, float, char>>);
  // Test FlatMap values to values with a temporary.
  constexpr std::tuple<const int&, float, char> kFlatMapTupleOut5{kForEachInt1, float{43.42}, 'B'};
  constexpr auto kFlatMapResult5 = ValuesToValues::FlatMapWithTemporary<int>(
      MoveToNonConst(kForEachTupleTypeIn),
      []<typename T>(
          T&& t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return std::tuple{static_cast<float>(42.42 + idx), static_cast<char>('A' + idx)};
        } else {
          idx++;
          return std::tuple<T>{t};
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kFlatMapResult5 == kFlatMapTupleOut5);
  static_assert(
      std::is_same_v<decltype(kFlatMapResult5), const std::tuple<const int&, float, char>>);
  // Test FlatMap values to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float, char> kFlatMapTupleOut6{kForEachInt1, float{85.42}, 'l'};
  constexpr auto kFlatMapResult6 = ValuesToValues::FlatMapWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      /* idx = */ 42,
      []<typename T>(
          T&& t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return std::tuple{static_cast<float>(42.42 + idx), char('A' + idx)};
        } else {
          idx++;
          return std::tuple<T>{t};
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kFlatMapResult6 == kFlatMapTupleOut6);
  static_assert(
      std::is_same_v<decltype(kFlatMapResult6), const std::tuple<const int&, float, char>>);

  // Test ForEach to types.
  constexpr auto kForEachResult1 = [] {
    std::array<char, 2> result;
    TypesToValues::ForEach<TypesToTypes::Enumerate<TupleType<const int&, char>>>(
        [&result]<typename T>(const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kForEachInt1);
          CHECK_EQ(&kExtraArg2, &kForEachInt2);
          constexpr std::size_t kIdx = std::tuple_element_t<0, T>{};
          if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
            result[kIdx] = 42;
          } else {
            result[kIdx] = 'X';
          }
        },
        kForEachInt1,
        kForEachInt2);
    return result;
  }();
  static_assert(kForEachResult1 == std::array<char, 2>{42, 'X'});
  // Test ForEach to types with a temporary.
  constexpr auto kForEachResult2 = [] {
    std::array<char, 2> result;
    TypesToValues::ForEachWithTemporary<TypesToTypes::Enumerate<TupleType<const int&, char>>, int>(
        [&result]<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kForEachInt1);
          CHECK_EQ(&kExtraArg2, &kForEachInt2);
          constexpr std::size_t kIdx = std::tuple_element_t<0, T>{};
          if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
            result[kIdx] = 42 + idx++;
          } else {
            result[kIdx] = 'X' + idx;
          }
        },
        kForEachInt1,
        kForEachInt2);
    return result;
  }();
  static_assert(kForEachResult2 == std::array<char, 2>{42, 'Y'});
  // Test ForEach to types with an explicitly initialized temporary.
  constexpr auto kForEachResult3 = [] {
    std::array<char, 2> result;
    TypesToValues::ForEachWithTemporary<TypesToTypes::Enumerate<TupleType<const int&, char>>>(
        /* idx = */ 42,
        [&result]<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kForEachInt1);
          CHECK_EQ(&kExtraArg2, &kForEachInt2);
          constexpr std::size_t kIdx = std::tuple_element_t<0, T>{};
          if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
            result[kIdx] = 42 + idx++;
          } else {
            result[kIdx] = 'A' + idx;
          }
        },
        kForEachInt1,
        kForEachInt2);
    return result;
  }();
  static_assert(kForEachResult3 == std::array<char, 2>{84, 'l'});
  // Test ForEach to values.
  constexpr auto kForEachResult4 = [FOR_EACH_TUPLE_TYPE_CAPTURE] {
    std::array<char, 2> result;
    ValuesToValues::ForEach(
        ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
        [&result]<typename T>(T t, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kForEachInt1);
          CHECK_EQ(&kExtraArg2, &kForEachInt2);
          auto [idx, elem] = t;
          if constexpr (std::is_same_v<decltype(elem), const int&>) {
            result[idx] = 42;
          } else {
            result[idx] = elem;
          }
        },
        kForEachInt1,
        kForEachInt2);
    return result;
  }();
  static_assert(kForEachResult4 == std::array<char, 2>{42, 'A'});
  // Test ForEach to values with a temporary.
  constexpr auto kForEachResult5 = [FOR_EACH_TUPLE_TYPE_CAPTURE] {
    std::array<char, 2> result;
    ValuesToValues::ForEachWithTemporary<int>(
        ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
        [&result]<typename T>(T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kForEachInt1);
          CHECK_EQ(&kExtraArg2, &kForEachInt2);
          auto [id, elem] = t;
          if constexpr (std::is_same_v<decltype(elem), const int&>) {
            result[id] = 42 + idx++;
          } else {
            result[id] = elem + idx;
          }
        },
        kForEachInt1,
        kForEachInt2);
    return result;
  }();
  static_assert(kForEachResult5 == std::array<char, 2>{42, 'B'});
  // Test ForEach to values with an explicitly initialized temporary.
  constexpr auto kForEachResult6 = [FOR_EACH_TUPLE_TYPE_CAPTURE] {
    std::array<char, 2> result;
    ValuesToValues::ForEachWithTemporary(
        ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
        /* idx = */ 42,
        [&result]<typename T>(T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kForEachInt1);
          CHECK_EQ(&kExtraArg2, &kForEachInt2);
          auto [id, elem] = t;
          if constexpr (std::is_same_v<decltype(elem), const int&>) {
            result[id] = 42 + idx++;
          } else {
            result[id] = elem + idx;
          }
        },
        kForEachInt1,
        kForEachInt2);
    return result;
  }();
  static_assert(kForEachResult6 == std::array<char, 2>{84, 'l'});

  // Test Map types to values.
  constexpr std::tuple<const int&, float> kMapTupleOut1{kForEachInt2, float{42.42}};
  constexpr auto kMapResult1 = TypesToValues::Map<TupleType<const int&, char>>(
      []<typename T>(const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return float{42.42};
        } else {
          return std::forward<T>(kForEachInt2);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kMapResult1 == kMapTupleOut1);
  static_assert(std::is_same_v<decltype(kMapResult1), const std::tuple<const int&, float>>);
  // Test Map types to values with a temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut2{kForEachInt2, float{43.42}};
  constexpr auto kMapResult2 = TypesToValues::MapWithTemporary<TupleType<const int&, char>, int>(
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(kForEachInt2);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kMapResult2 == kMapTupleOut2);
  static_assert(std::is_same_v<decltype(kMapResult2), const std::tuple<const int&, float>>);
  // Test Map types to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut3{kForEachInt2, float{85.42}};
  constexpr auto kMapResult3 = TypesToValues::MapWithTemporary<TupleType<const int&, char>>(
      /* idx = */ 42,
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(kForEachInt2);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kMapResult3 == kMapTupleOut3);
  static_assert(std::is_same_v<decltype(kMapResult3), const std::tuple<const int&, float>>);
  // Test Map values to values.
  constexpr std::tuple<const int&, float> kMapTupleOut4{kForEachInt1, float{42.42}};
  constexpr auto kMapResult4 = ValuesToValues::Map(
      MoveToNonConst(kForEachTupleTypeIn),
      []<typename T>(T&& t, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return float{42.42};
        } else {
          return std::forward<T>(t);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kMapResult4 == kMapTupleOut4);
  static_assert(std::is_same_v<decltype(kMapResult4), const std::tuple<const int&, float>>);
  // Test Map values to values with a temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut5{kForEachInt1, float{43.42}};
  constexpr auto kMapResult5 = ValuesToValues::MapWithTemporary<int>(
      MoveToNonConst(kForEachTupleTypeIn),
      []<typename T>(
          T&& t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(t);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kMapResult5 == kMapTupleOut5);
  static_assert(std::is_same_v<decltype(kMapResult5), const std::tuple<const int&, float>>);
  // Test Map values to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut6{kForEachInt1, float{85.42}};
  constexpr auto kMapResult6 = ValuesToValues::MapWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      /* idx = */ 42,
      []<typename T>(
          T&& t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(t);
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kMapResult6 == kMapTupleOut6);
  static_assert(std::is_same_v<decltype(kMapResult6), const std::tuple<const int&, float>>);

  // Test Produce from types.
  constexpr auto kProduceResult1 =
      TypesToValues::Produce<TypesToTypes::Enumerate<TupleType<char, const int&>>,
                             std::array<char, 2>>(
          []<typename T>(
              std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kForEachInt1);
            CHECK_EQ(&kExtraArg2, &kForEachInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] = 42;
            } else {
              result[kid] = 'X';
            }
          },
          kForEachInt1,
          kForEachInt2);
  static_assert(kProduceResult1 == std::array<char, 2>{'X', 42});
  // Test Produce from types with explicitly initialized output.
  constexpr auto kProduceResult2 =
      TypesToValues::Produce<TypesToTypes::Enumerate<TupleType<char, const int&>>>(
          /* result = */ std::array<char, 2>{1, 2},
          []<typename T>(
              std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kForEachInt1);
            CHECK_EQ(&kExtraArg2, &kForEachInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] += 42;
            } else {
              result[kid] += 'X';
            }
          },
          kForEachInt1,
          kForEachInt2);
  static_assert(kProduceResult2 == std::array<char, 2>{'Y', 44});
  // Test Produce from types with a temporary.
  constexpr auto kProduceResult3 =
      TypesToValues::ProduceWithTemporary<TypesToTypes::Enumerate<TupleType<char, const int&>>,
                                          std::array<char, 2>,
                                          int>(
          []<typename T>(
              std::array<char, 2>& result, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kForEachInt1);
            CHECK_EQ(&kExtraArg2, &kForEachInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] = 42 + idx;
            } else {
              result[kid] = 'X' + idx++;
            }
          },
          kForEachInt1,
          kForEachInt2);
  static_assert(kProduceResult3 == std::array<char, 2>{'X', 43});
  // Test Produce from types with an explicitly initialized output and temporary.
  constexpr auto kProduceResult4 =
      TypesToValues::ProduceWithTemporary<TypesToTypes::Enumerate<TupleType<char, const int&>>>(
          /* result = */ std::array<char, 2>{1, 2},
          /* idx = */ 42,
          []<typename T>(
              std::array<char, 2>& result, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kForEachInt1);
            CHECK_EQ(&kExtraArg2, &kForEachInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] += 42 + idx;
            } else {
              result[kid] += 'A' + idx++;
            }
          },
          kForEachInt1,
          kForEachInt2);
  static_assert(kProduceResult4 == std::array<char, 2>{'l', 87});
  // Test Produce from values.
  constexpr auto kProduceResult5 = ValuesToValues::Produce<std::array<char, 2>>(
      ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
      []<typename T>(
          T t, std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] = 42;
        } else {
          result[id] = elem;
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kProduceResult5 == std::array<char, 2>{42, 'A'});
  // Test Produce from values.
  constexpr auto kProduceResult6 = ValuesToValues::Produce(
      ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
      /* result = */ std::array<char, 2>{1, 2},
      []<typename T>(
          T t, std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] += 42;
        } else {
          result[id] += elem;
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kProduceResult6 == std::array<char, 2>{43, 'C'});
  // Test Produce from values with a temporary.
  constexpr auto kProduceResult7 = ValuesToValues::ProduceWithTemporary<std::array<char, 2>, int>(
      ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
      []<typename T>(T t,
                     std::array<char, 2>& result,
                     int& idx,
                     const int& kExtraArg1,
                     const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] = 42 + idx++;
        } else {
          result[id] = elem + idx;
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kProduceResult7 == std::array<char, 2>{42, 'B'});
  // Test Produce from values with an explicitly initialized output and temporary.
  constexpr auto kProduceResult8 = ValuesToValues::ProduceWithTemporary(
      ValuesToValues::Enumerate(MoveToNonConst(kForEachTupleTypeIn)),
      /* result = */ std::array<char, 2>{1, 2},
      /* int = */ 42,
      []<typename T>(T t,
                     std::array<char, 2>& result,
                     int& idx,
                     const int& kExtraArg1,
                     const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kForEachInt1);
        CHECK_EQ(&kExtraArg2, &kForEachInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] += 42 + idx++;
        } else {
          result[id] += elem + idx;
        }
      },
      kForEachInt1,
      kForEachInt2);
  static_assert(kProduceResult8 == std::array<char, 2>{85, 'n'});

  static_assert(ValuesToValues::Retain<const int&>(kForEachTupleTypeIn) ==
                std::tuple<const int&>{kForEachInt1});
  static_assert(ValuesToValues::RetainIfNot<const int&>(kForEachTupleTypeIn) ==
                std::tuple<char>{'A'});

  static_assert(ValuesToValues::Retain<const int&>(MoveToNonConst(kForEachTupleTypeIn)) ==
                std::tuple<const int&>{kForEachInt1});
  static_assert(ValuesToValues::RetainIfNot<const int&>(MoveToNonConst(kForEachTupleTypeIn)) ==
                std::tuple<char>{'A'});

  // Skip 1 element.
  constexpr auto kSkipResult1 = ValuesToValues::Skip<1>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult1 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult1), const std::tuple<char>>);
  constexpr auto kSkipResult2 = ValuesToValues::Skip(MoveToNonConst(kForEachTupleTypeIn), kMeta<1>);
  static_assert(kSkipResult2 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult2), const std::tuple<char>>);
  // Skip everything.
  constexpr auto kSkipResult3 =
      ValuesToValues::SkipWhile<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      },
                                &kForEachInt1,
                                &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult3 == std::tuple{});
  static_assert(std::is_same_v<decltype(kSkipResult3), const std::tuple<>>);
  constexpr auto kSkipResult4 =
      ValuesToValues::SkipWhile(MoveToNonConst(kForEachTupleTypeIn),
                                kMeta<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
                                  CHECK_EQ(kExtraArg1, &kForEachInt1);
                                  CHECK_EQ(kExtraArg2, &kForEachInt2);
                                  return true;
                                }>,
                                kMeta<&kForEachInt1>,
                                kMeta<&kForEachInt2>);
  static_assert(kSkipResult4 == std::tuple{});
  static_assert(std::is_same_v<decltype(kSkipResult4), const std::tuple<>>);
  constexpr auto kSkipResult5 = ValuesToValues::SkipWhileWithTemporary<
      std::size_t,
      []<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult5 == std::tuple{});
  static_assert(std::is_same_v<decltype(kSkipResult5), const std::tuple<>>);
  constexpr auto kSkipResult6 = ValuesToValues::SkipWhileWithTemporary<std::size_t>(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<[]<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kSkipResult6 == std::tuple{});
  static_assert(std::is_same_v<decltype(kSkipResult6), const std::tuple<>>);
  constexpr auto kSkipResult7 = ValuesToValues::SkipWhileWithTemporary<
      42,
      []<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult7 == std::tuple{});
  static_assert(std::is_same_v<decltype(kSkipResult7), const std::tuple<>>);
  constexpr auto kSkipResult8 = ValuesToValues::SkipWhileWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<42>,
      kMeta<[]<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kSkipResult8 == std::tuple{});
  static_assert(std::is_same_v<decltype(kSkipResult8), const std::tuple<>>);
  // Skip const int& (first one).
  constexpr auto kSkipResult9 =
      ValuesToValues::SkipWhile<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      },
                                &kForEachInt1,
                                &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult9 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult9), const std::tuple<char>>);
  constexpr auto kSkipResult10 =
      ValuesToValues::SkipWhile(MoveToNonConst(kForEachTupleTypeIn),
                                kMeta<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
                                  CHECK_EQ(kExtraArg1, &kForEachInt1);
                                  CHECK_EQ(kExtraArg2, &kForEachInt2);
                                  return std::is_same_v<T, const int&>;
                                }>,
                                kMeta<&kForEachInt1>,
                                kMeta<&kForEachInt2>);
  static_assert(kSkipResult10 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult10), const std::tuple<char>>);
  constexpr auto kSkipResult11 = ValuesToValues::SkipWhileWithTemporary<
      42,
      []<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult11 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult11), const std::tuple<char>>);
  constexpr auto kSkipResult12 = ValuesToValues::SkipWhileWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<42>,
      kMeta<[]<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kSkipResult12 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult12), const std::tuple<char>>);
  constexpr auto kSkipResult13 = ValuesToValues::SkipWhileWithTemporary<
      std::size_t,
      []<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult13 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult13), const std::tuple<char>>);
  constexpr auto kSkipResult14 = ValuesToValues::SkipWhileWithTemporary<std::size_t>(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<[]<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kSkipResult14 == std::tuple{'A'});
  static_assert(std::is_same_v<decltype(kSkipResult14), const std::tuple<char>>);
  // Skip char (none, first one doesn't match).
  constexpr auto kSkipResult15 =
      ValuesToValues::SkipWhile<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      },
                                &kForEachInt1,
                                &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult15 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kSkipResult15), const std::tuple<const int&, char>>);
  constexpr auto kSkipResult16 =
      ValuesToValues::SkipWhile(MoveToNonConst(kForEachTupleTypeIn),
                                kMeta<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
                                  CHECK_EQ(kExtraArg1, &kForEachInt1);
                                  CHECK_EQ(kExtraArg2, &kForEachInt2);
                                  return std::is_same_v<T, char>;
                                }>,
                                kMeta<&kForEachInt1>,
                                kMeta<&kForEachInt2>);
  static_assert(kSkipResult16 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kSkipResult16), const std::tuple<const int&, char>>);
  constexpr auto kSkipResult17 = ValuesToValues::SkipWhileWithTemporary<
      std::size_t,
      []<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult17 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kSkipResult17), const std::tuple<const int&, char>>);
  constexpr auto kSkipResult18 = ValuesToValues::SkipWhileWithTemporary<std::size_t>(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<[]<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kSkipResult18 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kSkipResult18), const std::tuple<const int&, char>>);
  constexpr auto kSkipResult19 = ValuesToValues::SkipWhileWithTemporary<
      42,
      []<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kSkipResult19 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kSkipResult19), const std::tuple<const int&, char>>);
  constexpr auto kSkipResult20 = ValuesToValues::SkipWhileWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<42>,
      kMeta<[]<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kSkipResult20 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kSkipResult20), const std::tuple<const int&, char>>);

  // Take 1 element.
  constexpr auto kTakeResult1 = ValuesToValues::Take<1>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult1 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult1), const std::tuple<const int&>>);
  constexpr auto kTakeResult2 = ValuesToValues::Take(MoveToNonConst(kForEachTupleTypeIn), kMeta<1>);
  static_assert(kTakeResult2 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult2), const std::tuple<const int&>>);
  // Take char (none, first one doesn't match).
  constexpr auto kTakeResult3 =
      ValuesToValues::TakeWhile<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      },
                                &kForEachInt1,
                                &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult3 == std::tuple{});
  static_assert(std::is_same_v<decltype(kTakeResult3), const std::tuple<>>);
  constexpr auto kTakeResult4 =
      ValuesToValues::TakeWhile(MoveToNonConst(kForEachTupleTypeIn),
                                kMeta<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
                                  CHECK_EQ(kExtraArg1, &kForEachInt1);
                                  CHECK_EQ(kExtraArg2, &kForEachInt2);
                                  return std::is_same_v<T, char>;
                                }>,
                                kMeta<&kForEachInt1>,
                                kMeta<&kForEachInt2>);
  static_assert(kTakeResult4 == std::tuple{});
  static_assert(std::is_same_v<decltype(kTakeResult4), const std::tuple<>>);
  constexpr auto kTakeResult5 = ValuesToValues::TakeWhileWithTemporary<
      std::size_t,
      []<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult5 == std::tuple{});
  static_assert(std::is_same_v<decltype(kTakeResult5), const std::tuple<>>);
  constexpr auto kTakeResult6 = ValuesToValues::TakeWhileWithTemporary<std::size_t>(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<[]<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kTakeResult6 == std::tuple{});
  static_assert(std::is_same_v<decltype(kTakeResult6), const std::tuple<>>);
  constexpr auto kTakeResult7 = ValuesToValues::TakeWhileWithTemporary<
      42,
      []<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult7 == std::tuple{});
  static_assert(std::is_same_v<decltype(kTakeResult7), const std::tuple<>>);
  constexpr auto kTakeResult8 = ValuesToValues::TakeWhileWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<42>,
      kMeta<[]<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, char>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kTakeResult8 == std::tuple{});
  static_assert(std::is_same_v<decltype(kTakeResult8), const std::tuple<>>);
  // Take const int& (first one).
  constexpr auto kTakeResult9 =
      ValuesToValues::TakeWhile<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      },
                                &kForEachInt1,
                                &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult9 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult9), const std::tuple<const int&>>);
  constexpr auto kTakeResult10 =
      ValuesToValues::TakeWhile(MoveToNonConst(kForEachTupleTypeIn),
                                kMeta<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
                                  CHECK_EQ(kExtraArg1, &kForEachInt1);
                                  CHECK_EQ(kExtraArg2, &kForEachInt2);
                                  return std::is_same_v<T, const int&>;
                                }>,
                                kMeta<&kForEachInt1>,
                                kMeta<&kForEachInt2>);
  static_assert(kTakeResult10 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult10), const std::tuple<const int&>>);
  constexpr auto kTakeResult11 = ValuesToValues::TakeWhileWithTemporary<
      std::size_t,
      []<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult11 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult11), const std::tuple<const int&>>);
  constexpr auto kTakeResult12 = ValuesToValues::TakeWhileWithTemporary<std::size_t>(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<[]<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kTakeResult12 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult12), const std::tuple<const int&>>);
  constexpr auto kTakeResult13 = ValuesToValues::TakeWhileWithTemporary<
      42,
      []<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult13 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult13), const std::tuple<const int&>>);
  constexpr auto kTakeResult14 = ValuesToValues::TakeWhileWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<42>,
      kMeta<[]<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return std::is_same_v<T, const int&>;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kTakeResult14 == std::tuple{kForEachInt1});
  static_assert(std::is_same_v<decltype(kTakeResult14), const std::tuple<const int&>>);
  // Take everything.
  constexpr auto kTakeResult15 =
      ValuesToValues::TakeWhile<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      },
                                &kForEachInt1,
                                &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult15 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kTakeResult15), const std::tuple<const int&, char>>);
  constexpr auto kTakeResult16 =
      ValuesToValues::TakeWhile(MoveToNonConst(kForEachTupleTypeIn),
                                kMeta<[]<typename T>(const int* kExtraArg1, const int* kExtraArg2) {
                                  CHECK_EQ(kExtraArg1, &kForEachInt1);
                                  CHECK_EQ(kExtraArg2, &kForEachInt2);
                                  return true;
                                }>,
                                kMeta<&kForEachInt1>,
                                kMeta<&kForEachInt2>);
  static_assert(kTakeResult16 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kTakeResult16), const std::tuple<const int&, char>>);
  constexpr auto kTakeResult17 = ValuesToValues::TakeWhileWithTemporary<
      std::size_t,
      []<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult17 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kTakeResult17), const std::tuple<const int&, char>>);
  constexpr auto kTakeResult18 = ValuesToValues::TakeWhileWithTemporary<std::size_t>(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<[]<typename T>(std::size_t& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T), count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kTakeResult18 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kTakeResult18), const std::tuple<const int&, char>>);
  constexpr auto kTakeResult19 = ValuesToValues::TakeWhileWithTemporary<
      42,
      []<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      },
      &kForEachInt1,
      &kForEachInt2>(MoveToNonConst(kForEachTupleTypeIn));
  static_assert(kTakeResult19 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kTakeResult19), const std::tuple<const int&, char>>);
  constexpr auto kTakeResult20 = ValuesToValues::TakeWhileWithTemporary(
      MoveToNonConst(kForEachTupleTypeIn),
      kMeta<42>,
      kMeta<[]<typename T>(int& count, const int* kExtraArg1, const int* kExtraArg2) {
        CHECK_EQ(1 / sizeof(T) + 42, count++);
        CHECK_EQ(kExtraArg1, &kForEachInt1);
        CHECK_EQ(kExtraArg2, &kForEachInt2);
        return true;
      }>,
      kMeta<&kForEachInt1>,
      kMeta<&kForEachInt2>);
  static_assert(kTakeResult20 == std::tuple{kForEachInt1, 'A'});
  static_assert(std::is_same_v<decltype(kTakeResult20), const std::tuple<const int&, char>>);

  static_assert(ValuesToValues::ToArray(TupleType{'A', 'B'}) == std::array{'A', 'B'});
  static_assert(ValuesToValues::ToArray<char>(TupleType{'A', 'B'}) == std::array{'A', 'B'});
  static_assert(ValuesToValues::ToArray<const char>(TupleType{'A', 'B'}) ==
                std::array<const char, 2>{'A', 'B'});

  // Corner cases.
  static_assert(ValuesToValues::ToArray<char>(std::tuple{}) == std::array<char, 0>{});
  static_assert(ValuesToValues::ToArray<char>(std::array<const int, 0>{}) == std::array<char, 0>{});

  static_assert(ValuesToValues::Zip(TupleType{'a', 3.00}) ==
                std::tuple{std::tuple{'a'}, std::tuple{3.00}});
  static_assert(ValuesToValues::Zip(std::array{2, 42}, TupleType{'a', 3.00}) ==
                std::tuple{std::pair{2, 'a'}, std::pair{42, 3.00}});
  static_assert(
      ValuesToValues::Zip(std::array{2, 42}, TupleType{'a', 3.00}, TupleType{true, 1ULL}) ==
      std::tuple{std::tuple{2, 'a', true}, std::tuple{42, 3.00, 1ULL}});

  // Ensure that we cna Zip kMetaTypes and values.
  constexpr auto kMetaTypesProcessingResult = ValuesToValues::Map(
      ValuesToValues::Zip(kMetaTypes<char, int>, std::tuple{72, 'A'}),
      []<typename TypeAndValue>(TypeAndValue&& type_and_value) {
        return static_cast<std::remove_reference_t<std::tuple_element_t<0, TypeAndValue>>::Type>(
            std::get<1>(type_and_value));
      });
  static_assert(kMetaTypesProcessingResult == std::tuple{'H', 65});
  static_assert(std::is_same_v<decltype(kMetaTypesProcessingResult), const std::tuple<char, int>>);

  // Ensure that we can Zip kTupleMetaTypes and values.
  constexpr auto kTupleMetaTypesProcessingResult = ValuesToValues::Map(
      ValuesToValues::Zip(kTupleMetaTypes<TupleType<char, int>>, std::tuple{72, 'A'}),
      []<typename TypeAndValue>(TypeAndValue&& type_and_value) {
        return static_cast<std::remove_reference_t<std::tuple_element_t<0, TypeAndValue>>::Type>(
            std::get<1>(type_and_value));
      });
  static_assert(kTupleMetaTypesProcessingResult == std::tuple{'H', 65});
  static_assert(
      std::is_same_v<decltype(kTupleMetaTypesProcessingResult), const std::tuple<char, int>>);

  // Note: attempt to call Zip and put result into constexpr result variable works on clang and
  // msvc, but not on gcc.
  constexpr TupleType<char, double> zip_tuple1 = {'a', 3.00};
  constexpr TupleType<bool, unsigned long long> zip_tuple2 = {true, 1ULL};
  static_assert(ValuesToValues::Zip(zip_tuple1) == std::tuple{std::tuple{'a'}, std::tuple{3.00}});
  static_assert(std::is_same_v<decltype(ValuesToValues::Zip(zip_tuple1)),
                               std::tuple<std::tuple<const char&>, std::tuple<const double&>>>);
  static_assert(
      ValuesToValues::Zip(std::array{2, 42}, zip_tuple1) ==
      std::tuple{std::pair<int, const char&>{2, 'a'}, std::pair<int, const double&>{42, 3.00}});
  static_assert(
      std::is_same_v<decltype(ValuesToValues::Zip(std::array{2, 42}, zip_tuple1)),
                     std::tuple<std::pair<int, const char&>, std::pair<int, const double&>>>);
  static_assert(ValuesToValues::Zip(std::array{2, 42}, zip_tuple1, zip_tuple2) ==
                std::tuple{std::tuple{2, 'a', true}, std::tuple{42, 3.00, 1ULL}});
  static_assert(
      std::is_same_v<decltype(ValuesToValues::Zip(std::array{2, 42}, zip_tuple1, zip_tuple2)),
                     std::tuple<std::tuple<int, const char&, const bool&>,
                                std::tuple<int, const double&, const unsigned long long&>>>);

  static_assert(ValuesToValues::ZipShortest(TupleType{'a', 3.00}) ==
                std::tuple{std::tuple{'a'}, std::tuple{3.00}});
  static_assert(ValuesToValues::ZipShortest(std::array{2}, TupleType{'a', 3.00}) ==
                std::tuple<std::pair<int, char>>{std::pair{2, 'a'}});
  static_assert(
      ValuesToValues::ZipShortest(std::array{2}, TupleType{'a', 3.00}, TupleType{true, 1ULL}) ==
      std::tuple<std::tuple<int, char, bool>>{std::tuple{2, 'a', true}});

  // Note: attempt to call Zip and put result into constexpr result variable works on clang and
  // msvc, but not on gcc.
  static_assert(ValuesToValues::ZipShortest(zip_tuple1) ==
                std::tuple{std::tuple{'a'}, std::tuple{3.00}});
  static_assert(std::is_same_v<decltype(ValuesToValues::ZipShortest(zip_tuple1)),
                               std::tuple<std::tuple<const char&>, std::tuple<const double&>>>);
  static_assert(ValuesToValues::ZipShortest(std::array{2}, zip_tuple1) ==
                std::tuple<std::pair<int, const char&>>{std::pair<int, const char&>{2, 'a'}});
  static_assert(std::is_same_v<decltype(ValuesToValues::ZipShortest(std::array{2}, zip_tuple1)),
                               std::tuple<std::pair<int, const char&>>>);
  static_assert(ValuesToValues::ZipShortest(std::array{2}, zip_tuple1, zip_tuple2) ==
                std::tuple<std::tuple<int, const char&, const bool&>>{std::tuple{2, 'a', true}});
  static_assert(
      std::is_same_v<decltype(ValuesToValues::ZipShortest(std::array{2}, zip_tuple1, zip_tuple2)),
                     std::tuple<std::tuple<int, const char&, const bool&>>>);

  return true;
}

// Note that it's almost impossible to create a function that accepts both pair and tuple but
// doesn't accept array. We are testign array with Zip, because it's critically important in many
// cases, but rely on testing for two tuple types for other functions.
static_assert(TestFunc<std::pair>());
static_assert(TestFunc<std::tuple>());

constexpr auto kMetaValuesInput1 = std::array{1, 2};
static_assert(std::is_same_v<ValuesToTypes::MetaValues<&kMetaValuesInput1>,
                             std::tuple<MetaValue<1>, MetaValue<2>>>);

constexpr auto kMetaValuesInput2 = std::pair{1, 2};
static_assert(std::is_same_v<ValuesToTypes::MetaValues<&kMetaValuesInput2>,
                             std::tuple<MetaValue<1>, MetaValue<2>>>);

constexpr auto kMetaValuesInput3 = std::tuple{1, 2};
static_assert(std::is_same_v<ValuesToTypes::MetaValues<&kMetaValuesInput3>,
                             std::tuple<MetaValue<1>, MetaValue<2>>>);

}  // namespace

}  // namespace berberis
