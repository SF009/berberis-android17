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

// TypesToTypes provides type-level metaprogramming utilities for std::tuple (with support for
// std::array if <array> include is present). It uses template specializations to perform operations
// on the types within a tuple.
class TypesToTypes {
 private:
  template <typename Type, auto kLambda>
  class FlatMapHelper;
  template <typename Type>
  class IndexedHelper;
  template <typename Type, auto kLambda>
  class MapHelper;
  template <typename TypeLeft, typename TypeRight>
  class ZipHelper;

 public:
  template <typename Type>
  using Indexes = decltype(IndexedHelper<Type>::Indexes(
      std::declval<std::make_index_sequence<std::tuple_size_v<Type>>>()));

  template <typename TypeLeft, typename TypeRight>
  using Zip = ZipHelper<TypeLeft, TypeRight>::Result;

  template <typename Type>
  using Enumerate = Zip<Indexes<Type>, Type>;

  // Applies a type-level lambda to each type in the input |Type| tuple.
  // The lambda is expected to return a tuple-like type for each input type.
  // All the resulting tuples are then concatenated ("flattened") into a single tuple.
  // Example:
  //   FlatMap<std::tuple<int, bool>, []<typename>(){
  //     return static_cast<std::tuple<float, double>>(nullptr;
  //   }>
  // results in std::tuple<float, double, float, double>.
  // The use of pointer to tuple allows us to return types that couldn't be constructed
  // in lambda, e.g. references.
  template <typename Type, auto kLambda>
  using FlatMap = FlatMapHelper<Type, kLambda>::Result;

  // Applies a type-level lambda to each type in the input |Type| tuple.
  // The lambda is expected to return a single type for each input type.
  // The result is a new tuple with the same number of elements, where each element's type
  // is the result of the lambda applied to the corresponding input type.
  // Example:
  //   Map<std::tuple<int, bool>, []<typename>(){ return float{}; }>
  // results in std::tuple<float, float>.
  // The need to return type simplifies the code if type can be constructed, but makes it
  // impossible to return certain types (e.g. references).
  template <typename Type, auto kLambda>
  using Map = MapHelper<Type, kLambda>::Result;

 private:
  template <typename Type>
  class IndexedHelper {
   public:
    template <std::size_t... Is>
    static constexpr std::tuple<MetaValue<Is>...> Indexes(std::index_sequence<Is...>);
  };

  template <typename... Types, auto kLambda>
  class FlatMapHelper<std::tuple<Types...>, kLambda> {
   public:
    using Result = decltype(std::tuple_cat(*kLambda.template operator()<Types>()...));
  };
  template <typename Type, auto kLambda>
  class FlatMapHelper {
   public:
    // Note: tuple_cap here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = FlatMapHelper<decltype(std::tuple_cat(std::declval<Type>())), kLambda>::Result;
  };

  template <typename... Types, auto kLambda>
  class MapHelper<std::tuple<Types...>, kLambda> {
   public:
    using Result =
        std::tuple<decltype(kLambda.template operator()<Types>(std::declval<Types>()))...>;
  };
  template <typename Type, auto kLambda>
  class MapHelper {
   public:
    // Note: tuple_cap here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = MapHelper<decltype(std::tuple_cat(std::declval<Type>())), kLambda>::Result;
  };

  template <typename... TypesLeft, typename... TypesRight>
  class ZipHelper<std::tuple<TypesLeft...>, std::tuple<TypesRight...>> {
   public:
    using Result = std::tuple<std::pair<TypesLeft, TypesRight>...>;
  };
  template <typename TypeLeft, typename TypeRight>
  class ZipHelper {
   public:
    // Note: tuple_cap here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = ZipHelper<decltype(std::tuple_cat(std::declval<TypeLeft>())),
                             decltype(std::tuple_cat(std::declval<TypeRight>()))>::Result;
  };
};

// TypesToValues provides value-level functional-style operations on std::tuple types. It also
// supports std::array if <array> header is included. These methods often take a lambda to apply to
// each element of the tuple.
class TypesToValues {
 public:
  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr void Apply(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr void ApplyWithTemporary(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    ApplyHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
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
    ApplyHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType,
            typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr OutputType Generate(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    OutputType result;
    ApplyHelper<TupleType, OutputType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
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
    ApplyHelper<TupleType, OutputType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
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
    ApplyHelper<TupleType, OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
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
    ApplyHelper<TupleType, OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr decltype(auto) Map(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return MapHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
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
    return MapHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

 private:
  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda, std::size_t... Is>
  static constexpr void ApplyHelper(Lambda&& lambda,
                                    std::index_sequence<Is...>,
                                    ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
         std::forward<ExtraLambdaArgTypes>(extra_types)...),
     ...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda, std::size_t... Is>
  static constexpr decltype(auto) MapHelper(Lambda&& lambda,
                                            std::index_sequence<Is...>,
                                            ExtraLambdaArgTypes&&... extra_types) {
    // Note: we need to specify the type of tuple here explicitly, because otherwise type deduction
    // would produce `int` where we want `const int&`.
    return std::tuple<decltype(lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
        std::forward<ExtraLambdaArgTypes>(extra_types)...))...> {
      lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
          std::forward<ExtraLambdaArgTypes>(extra_types)...)...
    };
  }
};

// ValuesToTypes provides type-level metaprogramming utilities for std::tuple (with support for
// std::array if <array> include is present). It uses template specializations to perform operations
// on the types within a tuple.
class ValuesToTypes {
 private:
  template <auto kValues>
  class MetaValuesHelper;

 public:
  template <auto kValues>
  using MetaValues = decltype(MetaValuesHelper<kValues>::MetaValues(
      std::declval<
          std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(*kValues)>>>>()));

 private:
  template <auto kValues>
  class MetaValuesHelper {
   public:
    template <std::size_t... Is>
    static constexpr std::tuple<MetaValue<std::get<Is>(*kValues)>...> MetaValues(
        std::index_sequence<Is...>);
  };
};

// ValuesToValues provides value-level functional-style operations on std::tuple values. It also
// supports std::array if <array> header is included. These methods often take a lambda to apply to
// each element of the tuple.
class ValuesToValues {
 public:
  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr void Apply(TupleType tuple,
                              Lambda&& lambda,
                              ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<ExtraLambdaArgTypes...>(tuple,
                                        std::forward<Lambda>(lambda),
                                        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
                                        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr void ApplyWithTemporary(TupleType tuple,
                                           Lambda&& lambda,
                                           ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    ApplyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr void ApplyWithTemporary(TupleType tuple,
                                           TemporaryType tmp,
                                           Lambda&& lambda,
                                           ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType>
  static constexpr TypesToTypes::Enumerate<TupleType> Enumerate(TupleType tuple) {
    return Zip(TypesToTypes::Indexes<TupleType>{}, tuple);
  }

  template <typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType Generate(TupleType tuple,
                                       Lambda&& lambda,
                                       ExtraLambdaArgTypes&&... extra_types) {
    // Note: result is explicitly default initialized, not value initialized.
    // This helps to catch mistakes in costexpr context, because an attempt to assign uninitialized
    // result to constexpr variable would fail. Use the next form if you need initialization.
    OutputType result;
    ApplyHelper<OutputType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType Generate(TupleType tuple,
                                       OutputType result,
                                       Lambda&& lambda,
                                       ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<OutputType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType GenerateWithTemporary(TupleType tuple,
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
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType GenerateWithTemporary(TupleType tuple,
                                                    OutputType result,
                                                    TemporaryType tmp,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    ApplyHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr decltype(auto) Map(TupleType tuple,
                                      Lambda&& lambda,
                                      ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(TupleType tuple,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(TupleType tuple,
                                                   TemporaryType tmp,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleTypeLeft, typename TupleTypeRight>
  static constexpr TypesToTypes::Zip<TupleTypeLeft, TupleTypeRight> Zip(
      TupleTypeLeft tuple_left,
      TupleTypeRight tuple_right) {
    return ZipHelper(tuple_left,
                     tuple_right,
                     std::make_index_sequence<std::min(std::tuple_size_v<TupleTypeLeft>,
                                                       std::tuple_size_v<TupleTypeRight>)>{});
  }

 private:
  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr void ApplyHelper(TupleType tuple,
                                    Lambda&& lambda,
                                    std::index_sequence<Is...>,
                                    ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
         std::get<Is>(tuple), std::forward<ExtraLambdaArgTypes>(extra_types)...),
     ...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr decltype(auto) MapHelper(TupleType tuple,
                                            Lambda&& lambda,
                                            std::index_sequence<Is...>,
                                            ExtraLambdaArgTypes&&... extra_types) {
    // Note: we need to specify the type of tuple here explicitly, because otherwise type deduction
    // would produce `int` where we want `const int&`.
    return std::tuple<decltype(lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
        std::get<Is>(tuple), std::forward<ExtraLambdaArgTypes>(extra_types)...))...> {
      lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
          std::get<Is>(tuple), std::forward<ExtraLambdaArgTypes>(extra_types)...)...
    };
  }

  template <typename TupleTypeLeft, typename TupleTypeRight, std::size_t... Is>
  static constexpr TypesToTypes::Zip<TupleTypeLeft, TupleTypeRight>
  ZipHelper(TupleTypeLeft tuple_left, TupleTypeRight tuple_right, std::index_sequence<Is...>) {
    using ZipType = TypesToTypes::Zip<TupleTypeLeft, TupleTypeRight>;
    return ZipType{std::pair<std::tuple_element_t<Is, TupleTypeLeft>,
                             std::tuple_element_t<Is, TupleTypeRight>>{
        std::get<Is>(tuple_left), std::get<Is>(tuple_right)}...};
  }
};

}  // namespace berberis

#endif  // BERBERIS_BASE_TUPLE_PROCESSING_H_
