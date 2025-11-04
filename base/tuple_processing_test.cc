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

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>

#include "berberis/base/checks.h"
#include "berberis/base/tuple_processing.h"

namespace berberis {

namespace {

// Note: we want to have references in out tests to produce results with references and also
// verify that types of the results include references, not straight `int`s, this is somewhat
// tricky and the main reason to use static_assert based tests here: dangling reference is in UB
// in normal tests, but a compile-time error in static_assert based tests.

static constexpr int kMapInt1 = 1;
static constexpr int kMapInt2 = 2;

template <template <typename, typename> typename TupleType>
constexpr bool TestFunc() {
  static_assert(std::is_same_v<typename TupleTypes<std::tuple<char, int&>>::Enumerate,
                               std::tuple<std::tuple<Value<std::size_t{0}>, char>,
                                          std::tuple<Value<std::size_t{1}>, int&>>>);

  // Test mapping by type.
  static_assert(
      std::is_same_v<
          typename TupleTypes<std::tuple<char, int&>, Value<[]<typename T>() -> decltype(auto) {
                                if constexpr (std::is_same_v<T, char>) {
                                  return static_cast<std::tuple<float>*>(nullptr);
                                } else {
                                  return static_cast<std::tuple<T>*>(nullptr);
                                }
                              }>>::Map,
          std::tuple<float, int&>>);

  // Test mapping by value.
  static_assert(std::is_same_v<typename TupleTypes<std::tuple<char, const int&>,
                                                   Value<[]<typename T>(T t) -> decltype(auto) {
                                                     if constexpr (std::is_same_v<T, char>) {
                                                       return float{0.0};
                                                     } else {
                                                       return std::forward<T>(t);
                                                     }
                                                   }>,
                                                   void>::Map,
                               std::tuple<float, const int&>>);

  static_assert(
      std::is_same_v<typename TupleTypes<std::tuple<char, int&>, std::tuple<long&, float>>::Zip,
                     std::tuple<std::tuple<char, long&>, std::tuple<int&, float>>>);

  static_assert(TupleValues::Enumerate(std::tuple{'a', 42}) ==
                std::tuple{std::tuple{0, 'a'}, std::tuple{1, 42}});

  constexpr std::tuple<const int&, char> kMapTupleIn{kMapInt1, 'A'};

  // Test Map types to values.
  constexpr std::tuple<const int&, float> kMapTupleOut1{kMapInt2, float{42.42}};
  constexpr auto MapResult1 = TupleValues::Map<std::tuple<const int&, char>>(
      []<typename T>(const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kMapInt1);
        CHECK_EQ(&kExtraArg2, &kMapInt2);
        if constexpr (std::is_same_v<T, char>) {
          return float{42.42};
        } else {
          return std::forward<T>(kMapInt2);
        }
      },
      kMapInt1,
      kMapInt2);
  static_assert(MapResult1 == kMapTupleOut1);
  static_assert(std::is_same_v<decltype(MapResult1), const std::tuple<const int&, float>>);
  // Test Map types to values with a temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut2{kMapInt2, float{43.42}};
  constexpr auto MapResult2 = TupleValues::MapWithTemporary<std::tuple<const int&, char>, int>(
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kMapInt1);
        CHECK_EQ(&kExtraArg2, &kMapInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(kMapInt2);
        }
      },
      kMapInt1,
      kMapInt2);
  static_assert(MapResult2 == kMapTupleOut2);
  static_assert(std::is_same_v<decltype(MapResult2), const std::tuple<const int&, float>>);
  // Test Map types to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut3{kMapInt2, float{85.42}};
  constexpr auto MapResult3 = TupleValues::MapWithTemporary<std::tuple<const int&, char>>(
      /* idx = */ 42,
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kMapInt1);
        CHECK_EQ(&kExtraArg2, &kMapInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(kMapInt2);
        }
      },
      kMapInt1,
      kMapInt2);
  static_assert(MapResult3 == kMapTupleOut3);
  static_assert(std::is_same_v<decltype(MapResult3), const std::tuple<const int&, float>>);
  // Test Map values to values.
  constexpr std::tuple<const int&, float> kMapTupleOut4{kMapInt1, float{42.42}};
  constexpr auto MapResult4 = TupleValues::Map(
      kMapTupleIn,
      []<typename T>(T t, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kMapInt1);
        CHECK_EQ(&kExtraArg2, &kMapInt2);
        if constexpr (std::is_same_v<T, char>) {
          return float{42.42};
        } else {
          return std::forward<T>(t);
        }
      },
      kMapInt1,
      kMapInt2);
  static_assert(MapResult4 == kMapTupleOut4);
  static_assert(std::is_same_v<decltype(MapResult4), const std::tuple<const int&, float>>);
  // Test Map values to values with a temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut5{kMapInt1, float{43.42}};
  constexpr auto MapResult5 = TupleValues::MapWithTemporary<int>(
      kMapTupleIn,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kMapInt1);
        CHECK_EQ(&kExtraArg2, &kMapInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(t);
        }
      },
      kMapInt1,
      kMapInt2);
  static_assert(MapResult5 == kMapTupleOut5);
  static_assert(std::is_same_v<decltype(MapResult5), const std::tuple<const int&, float>>);
  // Test Map values to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut6{kMapInt1, float{85.42}};
  constexpr auto MapResult6 = TupleValues::MapWithTemporary(
      kMapTupleIn,
      /* idx = */ 42,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kMapInt1);
        CHECK_EQ(&kExtraArg2, &kMapInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(t);
        }
      },
      kMapInt1,
      kMapInt2);
  static_assert(MapResult6 == kMapTupleOut6);
  static_assert(std::is_same_v<decltype(MapResult6), const std::tuple<const int&, float>>);

  static_assert(TupleValues::Zip(std::tuple{'a', 42}, std::tuple{1ULL, 3.00}) ==
                std::tuple{std::tuple{'a', 1ULL}, std::tuple{42, 3.00}});

  return true;
}

static_assert(TestFunc<std::pair>());
static_assert(TestFunc<std::tuple>());

}  // namespace

}  // namespace berberis
