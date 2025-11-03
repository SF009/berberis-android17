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

#ifndef BERBERIS_BASE_STRING_LITERAL_H_
#define BERBERIS_BASE_STRING_LITERAL_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <tuple>

#include "berberis/base/checks.h"

namespace berberis {

// Note: we use that type as argument of template which means that “all base classes and non-static
// data members should be public and non-mutable”.
//
// Note: C style array is presumed to have '\0' at the end because it's present in string literals.
//       std::array is presumed to only carry string without closing '\0' because it's easier to
//       produce it when string is the result of some constexpr-processing.
template <size_t N>
struct StringLiteral {
  constexpr StringLiteral(const std::array<char, N - 1>& str) {
    std::copy_n(std::begin(str), N - 1, std::begin(value));
    value[N - 1] = '\0';
  }
  constexpr StringLiteral(const char (&str)[N]) {
    std::copy_n(str, N, std::begin(value));
    CHECK_EQ(value[N - 1], '\0');
  }
  constexpr operator const char*() const { return std::begin(value); }

  std::array<char, N> value;
};

template <size_t N>
StringLiteral(const char (&str)[N]) -> StringLiteral<N>;
template <size_t N>
StringLiteral(const std::array<char, N>&) -> StringLiteral<N + 1>;

// Helper that produces names of template arguments.
template <typename kName>
constexpr auto GetTemplateName() {
  constexpr auto kFunctionName = __PRETTY_FUNCTION__;
  constexpr auto kFunctionNameLength = std::char_traits<char>::length(kFunctionName);
  static_assert(kFunctionName[kFunctionNameLength - 1] == ']');
  struct NameInfo {
    std::size_t start;
    std::size_t length;
  };
  constexpr auto kPrefixCheck = [](const char* const kPrefix,
                                   const char* const kSuffix) -> NameInfo {
    const auto kPrefixLen = std::char_traits<char>::length(kPrefix);
    const auto kSuffixLen = std::char_traits<char>::length(kSuffix);
    if (kPrefixLen >= kFunctionNameLength || kSuffixLen >= kFunctionNameLength) {
      return {};
    }
    if (std::char_traits<char>::compare(kFunctionName, kPrefix, kPrefixLen) == 0 &&
        std::char_traits<char>::compare(
            kFunctionName + kFunctionNameLength - kSuffixLen, kSuffix, kSuffixLen) == 0) {
      return {kPrefixLen, kFunctionNameLength - kPrefixLen - kSuffixLen};
    }
    return {};
  };
  constexpr NameInfo kInfo = [&kPrefixCheck] {
    // GCC, address of function.
    NameInfo info =
        kPrefixCheck("auto berberis::GetTemplateName() [kName = berberis::MetaValue<&", ">]");
    if (info.length == 0) {
      // GCC, non-function MetaValue.
      info = kPrefixCheck("auto berberis::GetTemplateName() [kName = berberis::MetaValue<", ">]");
    }
    if (info.length == 0) {
      // GCC, type.
      info = kPrefixCheck("auto berberis::GetTemplateName() [kName = ", "]");
    }
    if (info.length == 0) {
      // Clang, non-function or function MetaValue.
      info =
          kPrefixCheck("constexpr auto berberis::GetTemplateName() [with kName = MetaValue<", ">]");
    }
    if (info.length == 0) {
      // Clang, type.
      info = kPrefixCheck("constexpr auto berberis::GetTemplateName() [with kName = ", "]");
    }
    return info;
  }();
  if constexpr (kInfo.length) {
    std::array<char, kInfo.length> result;
    std::char_traits<char>::copy(std::begin(result), kFunctionName + kInfo.start, kInfo.length);
    return result;
  } else {
    std::array<char, kFunctionNameLength> result;
    std::char_traits<char>::copy(std::begin(result), kFunctionName, kFunctionNameLength);
    return result;
  }
}

template <typename kName>
inline constexpr auto kGetTemplateName = GetTemplateName<kName>();

template <typename TupleType>
constexpr auto ToArray(TupleType&& tuple) {
  return std::apply(
      []<typename... Arg>(Arg&&... arg) { return std::array{std::forward<Arg>(arg)...}; },
      std::forward<TupleType>(tuple));
}

}  // namespace berberis

#endif  // BERBERIS_BASE_STRING_LITERAL_H_
