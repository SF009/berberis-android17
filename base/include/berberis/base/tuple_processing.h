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

template <typename... Types, auto Lambda>
class TupleTypes<std::tuple<Types...>, Value<Lambda>> {
 public:
  // The lambda is required to return a pointer to a tuple. This is a workaround to allow passing
  // type information through a value template parameter (Lambda). The type we are interested in is
  // the first element of the tuple pointed to by the lambda's return value. That way we may return
  // types like non-const references (that would be otherwise impossible to return from the lambda
  // in constant context).
  using Map = std::tuple<std::tuple_element_t<
      0,
      std::remove_pointer_t<decltype((Lambda.template operator()<Types>()))>>...>;
};

template <typename... Types, auto Lambda>
class TupleTypes<std::tuple<Types...>, Value<Lambda>, void> {
 public:
  using Map = std::tuple<decltype(Lambda.template operator()<Types>(std::declval<Types>()))...>;
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
  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr void Apply(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<ExtraLambdaArgTypes...>(std::forward<Lambda>(lambda),
                                        static_cast<TupleType*>(nullptr),
                                        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr void ApplyWithTemporary(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    ApplyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr void ApplyWithTemporary(TemporaryType tmp,
                                           Lambda&& lambda,
                                           ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType,
            typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr OutputType Generate(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    OutputType result;
    ApplyHelper<OutputType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename TupleType,
            typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr OutputType Generate(OutputType result,
                                       Lambda&& lambda,
                                       ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<OutputType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename TupleType,
            typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr OutputType GenerateWithTemporary(Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    OutputType result;
    TemporaryType tmp{};
    ApplyHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename TupleType,
            typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr OutputType GenerateWithTemporary(OutputType result,
                                                    TemporaryType tmp,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr decltype(auto) Map(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<ExtraLambdaArgTypes...>(std::forward<Lambda>(lambda),
                                             static_cast<TupleType*>(nullptr),
                                             std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(TemporaryType tmp,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        static_cast<TupleType*>(nullptr),
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

 private:
  template <typename... ExtraLambdaArgTypes, typename... Types, typename Lambda>
  static constexpr void ApplyHelper(Lambda&& lambda,
                                    std::tuple<Types...>*,
                                    ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template operator()<Types>(std::forward<ExtraLambdaArgTypes>(extra_types)...), ...);
  }

  template <typename... ExtraLambdaArgTypes, typename... Types, typename Lambda>
  static constexpr decltype(auto) MapHelper(Lambda&& lambda,
                                            std::tuple<Types...>*,
                                            ExtraLambdaArgTypes&&... extra_types) {
    // Note: we need to specify the type of tuple here explicitly, because otherwise type deduction
    // would produce `int` where we want `const int&`.
    return std::tuple<decltype(lambda.template operator()<Types>(
        std::forward<ExtraLambdaArgTypes>(extra_types)...))...> {
      lambda.template operator()<Types>(std::forward<ExtraLambdaArgTypes>(extra_types)...)...
    };
  }

 public:
  template <typename... ExtraLambdaArgTypes, typename... Types, typename Lambda>
  static constexpr void Apply(std::tuple<Types...> tuple,
                              Lambda&& lambda,
                              ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<ExtraLambdaArgTypes...>(tuple,
                                        std::forward<Lambda>(lambda),
                                        std::make_index_sequence<sizeof...(Types)>{},
                                        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr void ApplyWithTemporary(std::tuple<Types...> tuple,
                                           Lambda&& lambda,
                                           ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    ApplyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr void ApplyWithTemporary(std::tuple<Types...> tuple,
                                           TemporaryType tmp,
                                           Lambda&& lambda,
                                           ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... Types>
  static constexpr TupleTypes<std::tuple<Types...>>::Enumerate Enumerate(
      std::tuple<Types...> tuple) {
    return Zip(typename TupleTypes<std::tuple<Types...>>::Indexes{}, tuple);
  }

  template <typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr OutputType Generate(std::tuple<Types...> tuple,
                                       Lambda&& lambda,
                                       ExtraLambdaArgTypes&&... extra_types) {
    // Note: result is explicitly default initialized, not value initialized.
    // This helps to catch mistakes in costexpr context, because an attempt to assign uninitialized
    // result to constexpr variable would fail. Use the next form if you need initialization.
    OutputType result;
    ApplyHelper<OutputType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr OutputType Generate(std::tuple<Types...> tuple,
                                       OutputType result,
                                       Lambda&& lambda,
                                       ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<OutputType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr OutputType GenerateWithTemporary(std::tuple<Types...> tuple,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    // Note: result is explicitly default initialized, not value initialized.
    // This helps to catch mistakes in costexpr context, because an attempt to assign uninitialized
    // result to constexpr variable would fail. Use the next form if you need initialization.
    OutputType result;
    TemporaryType tmp{};
    ApplyHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr OutputType GenerateWithTemporary(std::tuple<Types...> tuple,
                                                    OutputType result,
                                                    TemporaryType tmp,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }

  template <typename... ExtraLambdaArgTypes, typename... Types, typename Lambda>
  static constexpr decltype(auto) Map(std::tuple<Types...> tuple,
                                      Lambda&& lambda,
                                      ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<ExtraLambdaArgTypes...>(tuple,
                                             std::forward<Lambda>(lambda),
                                             std::make_index_sequence<sizeof...(Types)>{},
                                             std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(std::tuple<Types...> tuple,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename... Types,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(std::tuple<Types...> tuple,
                                                   TemporaryType tmp,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<sizeof...(Types)>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... TypesLeft, typename... TypesRight>
  static constexpr TupleTypes<std::tuple<TypesLeft...>, std::tuple<TypesRight...>>::Zip Zip(
      std::tuple<TypesLeft...> tuple_left,
      std::tuple<TypesRight...> tuple_right) {
    return ZipHelper(tuple_left, tuple_right, std::make_index_sequence<sizeof...(TypesLeft)>{});
  }

 private:
  template <typename... ExtraLambdaArgTypes, typename... Types, std::size_t... Is, typename Lambda>
  static constexpr void ApplyHelper(std::tuple<Types...> tuple,
                                    Lambda&& lambda,
                                    std::index_sequence<Is...>,
                                    ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template operator()<Types>(std::get<Is>(tuple),
                                       std::forward<ExtraLambdaArgTypes>(extra_types)...),
     ...);
  }

  template <typename... ExtraLambdaArgTypes, typename... Types, std::size_t... Is, typename Lambda>
  static constexpr decltype(auto) MapHelper(std::tuple<Types...> tuple,
                                            Lambda&& lambda,
                                            std::index_sequence<Is...>,
                                            ExtraLambdaArgTypes&&... extra_types) {
    // Note: we need to specify the type of tuple here explicitly, because otherwise type deduction
    // would produce `int` where we want `const int&`.
    return std::tuple<decltype(lambda.template operator()<Types>(
        std::get<Is>(tuple), std::forward<ExtraLambdaArgTypes>(extra_types)...))...> {
      lambda.template operator()<Types>(std::get<Is>(tuple),
                                        std::forward<ExtraLambdaArgTypes>(extra_types)...)...
    };
  }

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
