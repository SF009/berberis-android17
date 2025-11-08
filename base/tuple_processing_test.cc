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
#include <utility>

#include "berberis/base/checks.h"
#include "berberis/base/tuple_processing.h"

namespace berberis {

namespace {

// Note: we want to have references in out tests to produce results with references and also
// verify that types of the results include references, not straight `int`s, this is somewhat
// tricky and the main reason to use static_assert based tests here: dangling reference is in UB
// in normal tests, but a compile-time error in static_assert based tests.

constexpr int kForEachInt1 = 1;
constexpr int kForEachInt2 = 2;

template <template <typename, typename> typename TupleType>
constexpr bool TestFunc() {
  static_assert(TypesToTypes::All<TupleType<char, char>,
                                  []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(!TypesToTypes::All<TupleType<char, int&>,
                                   []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(!TypesToTypes::All<TupleType<float, int&>,
                                   []<typename T>() { return std::is_same_v<T, char>; }>{});

  static_assert(TypesToTypes::Any<TupleType<char, char>,
                                  []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(TypesToTypes::Any<TupleType<char, int&>,
                                  []<typename T>() { return std::is_same_v<T, char>; }>{});
  static_assert(!TypesToTypes::Any<TupleType<float, int&>,
                                   []<typename T>() { return std::is_same_v<T, char>; }>{});

  static_assert(std::is_same_v<TypesToTypes::Enumerate<TupleType<char, int&>>,
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, char>,
                                          std::pair<MetaValue<std::size_t{1}>, int&>>>);

  static_assert(
      std::is_same_v<TypesToTypes::Filter<TupleType<char, int&>,
                                          []<typename T>() { return std::is_same_v<T, char>; }>,
                     std::tuple<char>>);

  // Test mapping by type.
  static_assert(
      std::is_same_v<TypesToTypes::FlatMap<TupleType<char, int&>,
                                           []<typename T>() -> decltype(auto) {
                                             if constexpr (std::is_same_v<T, char>) {
                                               return static_cast<std::tuple<float, float>*>(
                                                   nullptr);
                                             } else {
                                               return static_cast<std::tuple<T>*>(nullptr);
                                             }
                                           }>,
                     std::tuple<float, float, int&>>);

  // Test mapping by value.
  static_assert(std::is_same_v<TypesToTypes::Map<TupleType<char, const int&>,
                                                 []<typename T>(T t) -> decltype(auto) {
                                                   if constexpr (std::is_same_v<T, char>) {
                                                     return float{0.0};
                                                   } else {
                                                     return std::forward<T>(t);
                                                   }
                                                 }>,
                               std::tuple<float, const int&>>);

  static_assert(std::is_same_v<TypesToTypes::Zip<TupleType<char, int&>, std::array<long, 2>>,
                               std::tuple<std::pair<char, long>, std::pair<int&, long>>>);

  constexpr std::tuple<const int&, char> kForEachTupleIn{kForEachInt1, 'A'};

  // Test All and Any types to values.
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::All<std::tuple<char, const int&>>(
        []<typename T>(int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          if constexpr (std::is_same_v<T, const int&>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
          }
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
          if constexpr (std::is_same_v<T, char>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
          }
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  // Test All and Any types to values with a temporary.
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::AllWithTemporary<std::tuple<char, const int&>, int>(
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          if constexpr (std::is_same_v<T, const int&>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
          }
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
          if constexpr (std::is_same_v<T, char>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
          }
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());
  // Test All and Any types to values with an explicitly initialized temporary.
  static_assert([] {
    int extra_arg1 = 0, extra_arg2 = 0;
    bool result = TypesToValues::AllWithTemporary<std::tuple<char, const int&>>(
        /* idx = */ 42,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          if constexpr (std::is_same_v<T, const int&>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
          }
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
          if constexpr (std::is_same_v<T, char>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
          }
        },
        extra_arg1,
        extra_arg2);
    // Short-circuit logic.
    CHECK_EQ(extra_arg1, 1);
    CHECK_EQ(extra_arg2, -1);
    return result;
  }());

  // Enumerate for values.
  static_assert(ValuesToValues::Enumerate(TupleType{'a', 42}) ==
                std::tuple{std::pair{MetaValue<0>{}, 'a'}, std::pair{MetaValue<1>{}, 42}});
  static_assert(std::is_same_v<decltype(ValuesToValues::Enumerate(TupleType{'a', 42})),
                               std::tuple<std::pair<MetaValue<std::size_t{0}>, char>,
                                          std::pair<MetaValue<std::size_t{1}>, int>>>);

  // Test FlatMap values to values.
  constexpr std::tuple<const int&> kFilterTupleOut1{kForEachInt1};
  constexpr auto kFilterResult1 = [&kForEachTupleIn] {
    int extra_arg1 = 0, extra_arg2 = 0;
    auto result = ValuesToValues::Filter(
        kForEachTupleIn,
        []<typename T>(int& extra_arg1, int& extra_arg2) -> decltype(auto) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          if constexpr (std::is_same_v<T, const int&>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
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
  constexpr auto kFilterResult2 = [&kForEachTupleIn] {
    int extra_arg1 = 0, extra_arg2 = 0;
    auto result = ValuesToValues::FilterWithTemporary<int>(
        kForEachTupleIn,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) -> decltype(auto) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, extra_arg1);
          if constexpr (std::is_same_v<T, const int&>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
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
  constexpr auto kFilterResult3 = [&kForEachTupleIn] {
    int extra_arg1 = 0, extra_arg2 = 0;
    auto result = ValuesToValues::FilterWithTemporary(
        kForEachTupleIn,
        /* idx = */ 42,
        []<typename T>(int& idx, int& extra_arg1, int& extra_arg2) -> decltype(auto) {
          extra_arg1 += 1;
          extra_arg2 -= 1;
          CHECK_EQ(++idx, 42 + extra_arg1);
          if constexpr (std::is_same_v<T, const int&>) {
            return MetaValue<true>{};
          } else {
            return MetaValue<false>{};
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
      kForEachTupleIn,
      []<typename T>(T t, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
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
      kForEachTupleIn,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
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
      kForEachTupleIn,
      /* idx = */ 42,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
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
  constexpr auto kForEachResult4 = [&kForEachTupleIn] {
    std::array<char, 2> result;
    ValuesToValues::ForEach(
        ValuesToValues::Enumerate(kForEachTupleIn),
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
  constexpr auto kForEachResult5 = [&kForEachTupleIn] {
    std::array<char, 2> result;
    ValuesToValues::ForEachWithTemporary<int>(
        ValuesToValues::Enumerate(kForEachTupleIn),
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
  constexpr auto kForEachResult6 = [&kForEachTupleIn] {
    std::array<char, 2> result;
    ValuesToValues::ForEachWithTemporary(
        ValuesToValues::Enumerate(kForEachTupleIn),
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
      kForEachTupleIn,
      []<typename T>(T t, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
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
      kForEachTupleIn,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
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
      kForEachTupleIn,
      /* idx = */ 42,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
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
      ValuesToValues::Enumerate(kForEachTupleIn),
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
      ValuesToValues::Enumerate(kForEachTupleIn),
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
      ValuesToValues::Enumerate(kForEachTupleIn),
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
      ValuesToValues::Enumerate(kForEachTupleIn),
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

  static_assert(ValuesToValues::Zip(std::array{2, 42}, TupleType{'a', 3.00}) ==
                std::tuple{std::pair{2, 'a'}, std::pair{42, 3.00}});

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
