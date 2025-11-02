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

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

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

// TupleTypes provides type-level metaprogramming utilities for std::tuple.
// It uses template specializations to perform operations on the types within a tuple.
template <typename... TupleTupеs>
class TupleTypes;

template <typename... Types>
class TupleTypes<std::tuple<Types...>> {
  template <std::size_t... Is>
  static constexpr std::tuple<Value<Is>...> IndexedHelper(std::index_sequence<Is...>);

 public:
  using Indexes =
      decltype(IndexedHelper(std::declval<std::make_index_sequence<sizeof...(Types)>>()));
  using Enumerate = TupleTypes<Indexes, std::tuple<Types...>>::Zip;
};

template <typename... TypesLeft, typename... TypesRight>
class TupleTypes<std::tuple<TypesLeft...>, std::tuple<TypesRight...>> {
 public:
  using Zip = std::tuple<std::tuple<TypesLeft, TypesRight>...>;
};

// TupleValues provides value-level functional-style operations on std::tuple instances or types.
// These methods often take a lambda to apply to each element of the tuple.
// Note: it's also possible to pass the type of tuple and get back values. In fact that's a common
// way of using this class. The important part is that it generates values and not types.
class TupleValues {
 public:
  template <typename... Types>
  static constexpr TupleTypes<std::tuple<Types...>>::Enumerate Enumerate(
      std::tuple<Types...> tuple) {
    return Zip(typename TupleTypes<std::tuple<Types...>>::Indexes{}, tuple);
  }
  template <typename... TypesLeft, typename... TypesRight>
  static constexpr TupleTypes<std::tuple<TypesLeft...>, std::tuple<TypesRight...>>::Zip Zip(
      std::tuple<TypesLeft...> tuple_left,
      std::tuple<TypesRight...> tuple_right) {
    return ZipHelper(tuple_left, tuple_right, std::make_index_sequence<sizeof...(TypesLeft)>{});
  }

 private:
  template <typename... TypesLeft, typename... TypesRight, std::size_t... Is>
  static constexpr TupleTypes<std::tuple<TypesLeft...>, std::tuple<TypesRight...>>::Zip ZipHelper(
      std::tuple<TypesLeft...> tuple_left,
      std::tuple<TypesRight...> tuple_right,
      std::index_sequence<Is...>) {
    using ZipType = TupleTypes<std::tuple<TypesLeft...>, std::tuple<TypesRight...>>::Zip;
    return ZipType{
        std::tuple<TypesLeft, TypesRight>{std::get<Is>(tuple_left), std::get<Is>(tuple_right)}...};
  }
};

}  // namespace berberis

#endif  // BERBERIS_BASE_TUPLE_PROCESSING_H_
