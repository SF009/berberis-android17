/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef BERBERIS_BASE_TUPLE_PROCESSING_H_
#define BERBERIS_BASE_TUPLE_PROCESSING_H_

#include <tuple>
#include <type_traits>

namespace berberis {

// Value that's passed as an argument of a function or lambda cannot be constexpr. But if it's
// passed as part of argument type then it's different.
// MetaValue class is empty, but carries the required information in its type.
// It can also be automatically converted into the value of the specified type when needed.
// That way we can pass an argument into a template as a normal, non-template argument.
template <auto kValueParam>
class MetaValue {
 public:
  using ValueType = std::remove_cvref_t<decltype(kValueParam)>;
  static constexpr auto kValue = kValueParam;
  constexpr operator ValueType() const { return kValue; }
};

#pragma push_macro("DEFINE_VALUE_OPERATOR")
#undef DEFINE_VALUE_OPERATOR
#define DEFINE_VALUE_OPERATOR(operator_name)                                             \
  template <auto kValueParam1, auto kValueParam2>                                        \
  constexpr MetaValue<(kValueParam1 operator_name kValueParam2)> operator operator_name( \
      MetaValue<kValueParam1>, MetaValue<kValueParam2>) {                                \
    return {};                                                                           \
  }

DEFINE_VALUE_OPERATOR(+)
DEFINE_VALUE_OPERATOR(-)
DEFINE_VALUE_OPERATOR(*)
DEFINE_VALUE_OPERATOR(/)
DEFINE_VALUE_OPERATOR(<<)
DEFINE_VALUE_OPERATOR(>>)
DEFINE_VALUE_OPERATOR(==)
DEFINE_VALUE_OPERATOR(!=)
DEFINE_VALUE_OPERATOR(>)
DEFINE_VALUE_OPERATOR(<)
DEFINE_VALUE_OPERATOR(<=)
DEFINE_VALUE_OPERATOR(>=)
DEFINE_VALUE_OPERATOR(&&)
DEFINE_VALUE_OPERATOR(||)
DEFINE_VALUE_OPERATOR(&)
DEFINE_VALUE_OPERATOR(|)
DEFINE_VALUE_OPERATOR(^)

#pragma pop_macro("DEFINE_VALUE_OPERATOR")

template <auto kValueParam>
using Value = MetaValue<kValueParam>;

}  // namespace berberis

#endif  // BERBERIS_BASE_TUPLE_PROCESSING_H_
