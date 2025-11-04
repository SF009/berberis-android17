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

constexpr int kApplyInt1 = 1;
constexpr int kApplyInt2 = 2;
constexpr std::tuple<const int&, char> kApplyTupleIn{kApplyInt1, 'A'};

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

  // Test Apply to types.
  constexpr auto ApplyResult1 = [] {
    std::array<char, 2> result;
    TupleValues::Apply<TupleTypes<std::tuple<const int&, char>>::Enumerate>(
        [&result]<typename T>(const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kApplyInt1);
          CHECK_EQ(&kExtraArg2, &kApplyInt2);
          constexpr std::size_t kIdx = std::tuple_element_t<0, T>{};
          if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
            result[kIdx] = 42;
          } else {
            result[kIdx] = 'X';
          }
        },
        kApplyInt1,
        kApplyInt2);
    return result;
  }();
  static_assert(ApplyResult1 == std::array<char, 2>{42, 'X'});
  // Test Apply to types with a temporary.
  constexpr auto ApplyResult2 = [] {
    std::array<char, 2> result;
    TupleValues::ApplyWithTemporary<TupleTypes<std::tuple<const int&, char>>::Enumerate, int>(
        [&result]<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kApplyInt1);
          CHECK_EQ(&kExtraArg2, &kApplyInt2);
          constexpr std::size_t kIdx = std::tuple_element_t<0, T>{};
          if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
            result[kIdx] = 42 + idx++;
          } else {
            result[kIdx] = 'X' + idx;
          }
        },
        kApplyInt1,
        kApplyInt2);
    return result;
  }();
  static_assert(ApplyResult2 == std::array<char, 2>{42, 'Y'});
  // Test Apply to types with an explicitly initialized temporary.
  constexpr auto ApplyResult3 = [] {
    std::array<char, 2> result;
    TupleValues::ApplyWithTemporary<TupleTypes<std::tuple<const int&, char>>::Enumerate>(
        /* idx = */ 42,
        [&result]<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kApplyInt1);
          CHECK_EQ(&kExtraArg2, &kApplyInt2);
          constexpr std::size_t kIdx = std::tuple_element_t<0, T>{};
          if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
            result[kIdx] = 42 + idx++;
          } else {
            result[kIdx] = 'A' + idx;
          }
        },
        kApplyInt1,
        kApplyInt2);
    return result;
  }();
  static_assert(ApplyResult3 == std::array<char, 2>{84, 'l'});
  // Test Apply to values.
  constexpr auto ApplyResult4 = [] {
    std::array<char, 2> result;
    TupleValues::Apply(
        TupleValues::Enumerate(kApplyTupleIn),
        [&result]<typename T>(T t, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kApplyInt1);
          CHECK_EQ(&kExtraArg2, &kApplyInt2);
          auto [idx, elem] = t;
          if constexpr (std::is_same_v<decltype(elem), const int&>) {
            result[idx] = 42;
          } else {
            result[idx] = elem;
          }
        },
        kApplyInt1,
        kApplyInt2);
    return result;
  }();
  static_assert(ApplyResult4 == std::array<char, 2>{42, 'A'});
  // Test Apply to values with a temporary.
  constexpr auto ApplyResult5 = [] {
    std::array<char, 2> result;
    TupleValues::ApplyWithTemporary<int>(
        TupleValues::Enumerate(kApplyTupleIn),
        [&result]<typename T>(T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kApplyInt1);
          CHECK_EQ(&kExtraArg2, &kApplyInt2);
          auto [id, elem] = t;
          if constexpr (std::is_same_v<decltype(elem), const int&>) {
            result[id] = 42 + idx++;
          } else {
            result[id] = elem + idx;
          }
        },
        kApplyInt1,
        kApplyInt2);
    return result;
  }();
  static_assert(ApplyResult5 == std::array<char, 2>{42, 'B'});
  // Test Apply to values with an explicitly initialized temporary.
  constexpr auto ApplyResult6 = [] {
    std::array<char, 2> result;
    TupleValues::ApplyWithTemporary(
        TupleValues::Enumerate(kApplyTupleIn),
        /* idx = */ 42,
        [&result]<typename T>(T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
          CHECK_EQ(&kExtraArg1, &kApplyInt1);
          CHECK_EQ(&kExtraArg2, &kApplyInt2);
          auto [id, elem] = t;
          if constexpr (std::is_same_v<decltype(elem), const int&>) {
            result[id] = 42 + idx++;
          } else {
            result[id] = elem + idx;
          }
        },
        kApplyInt1,
        kApplyInt2);
    return result;
  }();
  static_assert(ApplyResult6 == std::array<char, 2>{84, 'l'});

  static_assert(TupleValues::Enumerate(std::tuple{'a', 42}) ==
                std::tuple{std::tuple{MetaValue<0>{}, 'a'}, std::tuple{MetaValue<1>{}, 42}});
  static_assert(std::is_same_v<decltype(TupleValues::Enumerate(std::tuple{'a', 42})),
                               std::tuple<std::tuple<MetaValue<std::size_t{0}>, char>,
                                          std::tuple<MetaValue<std::size_t{1}>, int>>>);

  // Test Generate from types.
  constexpr auto GenerateResult1 =
      TupleValues::Generate<TupleTypes<std::tuple<char, const int&>>::Enumerate,
                            std::array<char, 2>>(
          []<typename T>(
              std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kApplyInt1);
            CHECK_EQ(&kExtraArg2, &kApplyInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] = 42;
            } else {
              result[kid] = 'X';
            }
          },
          kApplyInt1,
          kApplyInt2);
  static_assert(GenerateResult1 == std::array<char, 2>{'X', 42});
  // Test Generate from types with explicitly initialized output.
  constexpr auto GenerateResult2 =
      TupleValues::Generate<TupleTypes<std::tuple<char, const int&>>::Enumerate>(
          /* result = */ std::array<char, 2>{1, 2},
          []<typename T>(
              std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kApplyInt1);
            CHECK_EQ(&kExtraArg2, &kApplyInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] += 42;
            } else {
              result[kid] += 'X';
            }
          },
          kApplyInt1,
          kApplyInt2);
  static_assert(GenerateResult2 == std::array<char, 2>{'Y', 44});
  // Test Generate from types with a temporary.
  constexpr auto GenerateResult3 =
      TupleValues::GenerateWithTemporary<TupleTypes<std::tuple<char, const int&>>::Enumerate,
                                         std::array<char, 2>,
                                         int>(
          []<typename T>(
              std::array<char, 2>& result, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kApplyInt1);
            CHECK_EQ(&kExtraArg2, &kApplyInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] = 42 + idx;
            } else {
              result[kid] = 'X' + idx++;
            }
          },
          kApplyInt1,
          kApplyInt2);
  static_assert(GenerateResult3 == std::array<char, 2>{'X', 43});
  // Test Generate from types with an explicitly initialized output and temporary.
  constexpr auto GenerateResult4 =
      TupleValues::GenerateWithTemporary<TupleTypes<std::tuple<char, const int&>>::Enumerate>(
          /* result = */ std::array<char, 2>{1, 2},
          /* idx = */ 42,
          []<typename T>(
              std::array<char, 2>& result, int& idx, const int& kExtraArg1, const int& kExtraArg2) {
            CHECK_EQ(&kExtraArg1, &kApplyInt1);
            CHECK_EQ(&kExtraArg2, &kApplyInt2);
            constexpr std::size_t kid = std::tuple_element_t<0, T>{};
            if constexpr (std::is_same_v<std::tuple_element_t<1, T>, const int&>) {
              result[kid] += 42 + idx;
            } else {
              result[kid] += 'A' + idx++;
            }
          },
          kApplyInt1,
          kApplyInt2);
  static_assert(GenerateResult4 == std::array<char, 2>{'l', 87});
  // Test Generate from values.
  constexpr auto GenerateResult5 = TupleValues::Generate<std::array<char, 2>>(
      TupleValues::Enumerate(kApplyTupleIn),
      []<typename T>(
          T t, std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] = 42;
        } else {
          result[id] = elem;
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(GenerateResult5 == std::array<char, 2>{42, 'A'});
  // Test Generate from values.
  constexpr auto GenerateResult6 = TupleValues::Generate(
      TupleValues::Enumerate(kApplyTupleIn),
      /* result = */ std::array<char, 2>{1, 2},
      []<typename T>(
          T t, std::array<char, 2>& result, const int& kExtraArg1, const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] += 42;
        } else {
          result[id] += elem;
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(GenerateResult6 == std::array<char, 2>{43, 'C'});
  // Test Generate from values with a temporary.
  constexpr auto GenerateResult7 = TupleValues::GenerateWithTemporary<std::array<char, 2>, int>(
      TupleValues::Enumerate(kApplyTupleIn),
      []<typename T>(T t,
                     std::array<char, 2>& result,
                     int& idx,
                     const int& kExtraArg1,
                     const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] = 42 + idx++;
        } else {
          result[id] = elem + idx;
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(GenerateResult7 == std::array<char, 2>{42, 'B'});
  // Test Generate from values with an explicitly initialized output and temporary.
  constexpr auto GenerateResult8 = TupleValues::GenerateWithTemporary(
      TupleValues::Enumerate(kApplyTupleIn),
      /* result = */ std::array<char, 2>{1, 2},
      /* int = */ 42,
      []<typename T>(T t,
                     std::array<char, 2>& result,
                     int& idx,
                     const int& kExtraArg1,
                     const int& kExtraArg2) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        auto [id, elem] = t;
        if constexpr (std::is_same_v<decltype(elem), const int&>) {
          result[id] += 42 + idx++;
        } else {
          result[id] += elem + idx;
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(GenerateResult8 == std::array<char, 2>{85, 'n'});

  // Test Map types to values.
  constexpr std::tuple<const int&, float> kMapTupleOut1{kApplyInt2, float{42.42}};
  constexpr auto MapResult1 = TupleValues::Map<std::tuple<const int&, char>>(
      []<typename T>(const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        if constexpr (std::is_same_v<T, char>) {
          return float{42.42};
        } else {
          return std::forward<T>(kApplyInt2);
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(MapResult1 == kMapTupleOut1);
  static_assert(std::is_same_v<decltype(MapResult1), const std::tuple<const int&, float>>);
  // Test Map types to values with a temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut2{kApplyInt2, float{43.42}};
  constexpr auto MapResult2 = TupleValues::MapWithTemporary<std::tuple<const int&, char>, int>(
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(kApplyInt2);
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(MapResult2 == kMapTupleOut2);
  static_assert(std::is_same_v<decltype(MapResult2), const std::tuple<const int&, float>>);
  // Test Map types to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut3{kApplyInt2, float{85.42}};
  constexpr auto MapResult3 = TupleValues::MapWithTemporary<std::tuple<const int&, char>>(
      /* idx = */ 42,
      []<typename T>(int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(kApplyInt2);
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(MapResult3 == kMapTupleOut3);
  static_assert(std::is_same_v<decltype(MapResult3), const std::tuple<const int&, float>>);
  // Test Map values to values.
  constexpr std::tuple<const int&, float> kMapTupleOut4{kApplyInt1, float{42.42}};
  constexpr auto MapResult4 = TupleValues::Map(
      kApplyTupleIn,
      []<typename T>(T t, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        if constexpr (std::is_same_v<T, char>) {
          return float{42.42};
        } else {
          return std::forward<T>(t);
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(MapResult4 == kMapTupleOut4);
  static_assert(std::is_same_v<decltype(MapResult4), const std::tuple<const int&, float>>);
  // Test Map values to values with a temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut5{kApplyInt1, float{43.42}};
  constexpr auto MapResult5 = TupleValues::MapWithTemporary<int>(
      kApplyTupleIn,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(t);
        }
      },
      kApplyInt1,
      kApplyInt2);
  static_assert(MapResult5 == kMapTupleOut5);
  static_assert(std::is_same_v<decltype(MapResult5), const std::tuple<const int&, float>>);
  // Test Map values to values with an explicitly initialized temporary.
  constexpr std::tuple<const int&, float> kMapTupleOut6{kApplyInt1, float{85.42}};
  constexpr auto MapResult6 = TupleValues::MapWithTemporary(
      kApplyTupleIn,
      /* idx = */ 42,
      []<typename T>(
          T t, int& idx, const int& kExtraArg1, const int& kExtraArg2) -> decltype(auto) {
        CHECK_EQ(&kExtraArg1, &kApplyInt1);
        CHECK_EQ(&kExtraArg2, &kApplyInt2);
        if constexpr (std::is_same_v<T, char>) {
          return static_cast<float>(42.42 + idx);
        } else {
          idx++;
          return std::forward<T>(t);
        }
      },
      kApplyInt1,
      kApplyInt2);
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
