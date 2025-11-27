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

#include <algorithm>
#include <cstddef>
#include <functional>
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

template <auto kValueParam>
constexpr MetaValue<kValueParam> kMeta = MetaValue<kValueParam>{};

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

template <typename... Types_>
class Types {
 public:
  using Tuple = std::tuple<Types_...>;
};

template <typename... Types_>
inline constexpr auto kTypes = Types<Types_...>{};

class ValuesToValues;

// TypesToTypes provides type-level metaprogramming utilities for std::tuple (with support for
// std::array if <array> include is present). It uses template specializations to perform operations
// on the types within a tuple.
class TypesToTypes {
 private:
  template <typename Type>
  class CountHelper;
  template <auto kLambda>
  class FilterHelper;
  template <typename Type, auto kLambda>
  class FlatMapHelper;
  template <std::size_t... Is>
  static constexpr std::tuple<MetaValue<Is>...> IndexesHelper(std::index_sequence<Is...>);
  template <typename Type, auto kLambda>
  class MapHelper;
  template <typename Type>
  class RetainHelper;
  template <typename Type>
  class RetainIfNotHelper;
  template <typename TupleType, std::size_t kCount>
  class SkipHelper;
  template <typename TupleType, std::size_t kCount>
  class TakeHelper;
  template <typename TupleType, auto kLambda, auto... extra_lambda_values>
  class TakeSkipHelper;
  template <typename... Types>
  class ZipHelper;
  template <typename... Types>
  class ZipShortestHelper;

  friend class ValuesToValues;

 public:
  template <typename TupleType>
  using Indexes = decltype(IndexesHelper(
      std::declval<std::make_index_sequence<std::tuple_size_v<TupleType>>>()));

  template <typename... Types>
  using Zip = typename ZipHelper<Types...>::Result;

  template <typename TupleType>
  using Enumerate = Zip<Indexes<TupleType>, TupleType>;

  template <typename TupleType, auto kLambda>
  using Filter = typename FlatMapHelper<TupleType, FilterHelper<kLambda>{}>::Result;

  template <typename TupleType, auto kLambda>
  using CountIf = MetaValue<std::tuple_size_v<Filter<TupleType, kLambda>>>;

  template <typename TupleType, auto kLambda>
  using All = MetaValue<std::equal_to<std::size_t>{}(std::tuple_size_v<TupleType>,
                                                     CountIf<TupleType, kLambda>{})>;

  template <typename TupleType, auto kLambda>
  using Any = MetaValue<std::less<std::size_t>{}(std::size_t{0}, CountIf<TupleType, kLambda>{})>;

  template <typename... TuplesTypes>
  using Concat = decltype(std::tuple_cat(std::declval<TuplesTypes>()...));

  template <typename TupleType, typename Type>
  using Count = MetaValue<std::tuple_size_v<Filter<TupleType, CountHelper<Type>{}>>>;

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
  template <typename TupleType, auto kLambda>
  using FlatMap = typename FlatMapHelper<TupleType, kLambda>::Result;

  // Applies a type-level lambda to each type in the input |Type| tuple.
  // The lambda is expected to return a single type for each input type.
  // The result is a new tuple with the same number of elements, where each element's type
  // is the result of the lambda applied to the corresponding input type.
  // Example:
  //   Map<std::tuple<int, bool>, []<typename>(){ return float{}; }>
  // results in std::tuple<float, float>.
  // The need to return type simplifies the code if type can be constructed, but makes it
  // impossible to return certain types (e.g. references).
  template <typename TupleType, auto kLambda>
  using Map = typename MapHelper<TupleType, kLambda>::Result;

  template <typename TupleType, typename Type>
  using Retain = FlatMap<TupleType, RetainHelper<Type>{}>;

  template <typename TupleType, typename Type>
  using RetainIfNot = FlatMap<TupleType, RetainIfNotHelper<Type>{}>;

  template <typename TupleType, std::size_t kCount>
  using Skip =
      typename FlatMapHelper<Enumerate<TupleType>, SkipHelper<TupleType, kCount>{}>::Result;

  template <typename TupleType, auto kLambda>
  using SkipWhile = Skip<TupleType, TakeSkipHelper<TupleType, kLambda>::Produce()>;

  template <typename TupleType, std::size_t kCount>
  using Take =
      typename FlatMapHelper<Enumerate<TupleType>, TakeHelper<TupleType, kCount>{}>::Result;

  template <typename TupleType, auto kLambda>
  using TakeWhile = Take<TupleType, TakeSkipHelper<TupleType, kLambda>::Produce()>;

  template <typename... Types>
  using ZipShortest = typename ZipShortestHelper<Types...>::Result;

 private:
  template <typename Type>
  class CountHelper {
   public:
    template <typename TypeToCheck>
    constexpr auto operator()() const {
      return std::is_same_v<Type, TypeToCheck>;
    }
  };

  template <auto kLambda>
  class FilterHelper {
   public:
    template <typename Type>
    constexpr auto operator()() const {
      constexpr bool kAccepted = kLambda.template operator()<Type>();
      if constexpr (kAccepted) {
        return kTypes<Type>;
      } else {
        return kTypes<>;
      }
    }
  };

  template <typename... Types, auto kLambda>
  class FlatMapHelper<std::tuple<Types...>, kLambda> {
   public:
    using Result = Concat<typename decltype(kLambda.template operator()<Types>())::Tuple...>;
  };
  template <typename TupleType, auto kLambda>
  class FlatMapHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = typename FlatMapHelper<Concat<TupleType>, kLambda>::Result;
  };

  template <typename... Types, auto kLambda>
  class MapHelper<std::tuple<Types...>, kLambda> {
   public:
    using Result =
        std::tuple<decltype(kLambda.template operator()<Types>(std::declval<Types>()))...>;
  };
  template <typename TupleType, auto kLambda>
  class MapHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = typename MapHelper<Concat<TupleType>, kLambda>::Result;
  };

  template <typename Type>
  class RetainHelper {
   public:
    template <typename TypeToCheck>
    constexpr auto operator()() const {
      if constexpr (std::is_same_v<Type, TypeToCheck>) {
        return kTypes<TypeToCheck>;
      } else {
        return kTypes<>;
      }
    }
  };

  template <typename Type>
  class RetainIfNotHelper {
   public:
    template <typename TypeToCheck>
    constexpr auto operator()() const {
      if constexpr (std::is_same_v<Type, TypeToCheck>) {
        return kTypes<>;
      } else {
        return kTypes<TypeToCheck>;
      }
    }
  };

  template <typename TupleType, std::size_t kCount>
  class SkipHelper {
   public:
    template <typename EnumeratedType>
    constexpr auto operator()() const {
      constexpr std::size_t kIdx = std::tuple_element_t<0, EnumeratedType>{};
      static_assert(kIdx <= std::tuple_size_v<TupleType>);
      constexpr bool kAccepted = kIdx >= kCount;
      if constexpr (kAccepted) {
        return kTypes<std::tuple_element_t<1, EnumeratedType>>;
      } else {
        return kTypes<>;
      }
    }
  };

  template <typename TupleType, std::size_t kCount>
  class TakeHelper {
   public:
    template <typename EnumeratedType>
    constexpr auto operator()() const {
      constexpr std::size_t kIdx = std::tuple_element_t<0, EnumeratedType>{};
      static_assert(kIdx <= std::tuple_size_v<TupleType>);
      constexpr bool kAccepted = kIdx < kCount;
      if constexpr (kAccepted) {
        return kTypes<std::tuple_element_t<1, EnumeratedType>>;
      } else {
        return kTypes<>;
      }
    }
  };

  template <typename... Types>
  class ZipHelper<std::tuple<Types...>> {
   public:
    using Result = std::tuple<std::tuple<Types>...>;
  };
  template <typename... TypesLeft, typename... TypesRight>
  class ZipHelper<std::tuple<TypesLeft...>, std::tuple<TypesRight...>> {
   public:
    static_assert(sizeof...(TypesLeft) == sizeof...(TypesRight));
    using Result = std::tuple<std::pair<TypesLeft, TypesRight>...>;
  };
  template <typename... Types>
  class ZipHelperWithIndexes;
  template <typename... TypesLeft, typename... TypesRight, typename... Tuples>
  class ZipHelper<std::tuple<TypesLeft...>, std::tuple<TypesRight...>, Tuples...> {
   public:
    static_assert(sizeof...(TypesLeft) == sizeof...(TypesRight));
    static_assert(((sizeof...(TypesLeft) == std::tuple_size_v<Tuples>) && ...));
    using Result = typename ZipHelperWithIndexes<
        Indexes<std::tuple<TypesLeft...>>,
        std::tuple<std::tuple<TypesLeft...>, std::tuple<TypesRight...>, Tuples...>>::Result;
  };
  template <typename... Types>
  class ZipHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = typename ZipHelper<Concat<Types>...>::Result;
  };
  template <typename... Types>
  class ZipShortestHelper<std::tuple<Types...>> {
   public:
    using Result = std::tuple<std::tuple<Types>...>;
  };
  template <typename... TypesLeft, typename... Tuples>
  class ZipShortestHelper<std::tuple<TypesLeft...>, Tuples...> {
   public:
    using Result = typename ZipHelperWithIndexes<
        decltype(IndexesHelper(std::declval<std::make_index_sequence<std::min(
                                   {sizeof...(TypesLeft), std::tuple_size_v<Tuples>...})>>())),
        std::tuple<std::tuple<TypesLeft...>, Tuples...>>::Result;
  };
  template <typename... Types>
  class ZipShortestHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = typename ZipShortestHelper<Concat<Types>...>::Result;
  };
  template <std::size_t... kIndexes, typename Tuples>
  class ZipHelperWithIndexes<std::tuple<MetaValue<kIndexes>...>, Tuples> {
   public:
    using Result =
        std::tuple<typename ZipHelperWithIndexes<MetaValue<kIndexes>, Tuples>::Result...>;
  };
  template <std::size_t kIndex, typename TupleLeft, typename TupleRight>
  class ZipHelperWithIndexes<MetaValue<kIndex>, std::tuple<TupleLeft, TupleRight>> {
   public:
    using Result = std::pair<std::tuple_element_t<kIndex, TupleLeft>,
                             std::tuple_element_t<kIndex, TupleRight>>;
  };
  template <std::size_t kIndex, typename... Tuples>
  class ZipHelperWithIndexes<MetaValue<kIndex>, std::tuple<Tuples...>> {
   public:
    using Result = std::tuple<std::tuple_element_t<kIndex, Tuples>...>;
  };
};

// TypesToValues provides value-level functional-style operations on std::tuple types. It also
// supports std::array if <array> header is included. These methods often take a lambda to apply to
// each element of the tuple.
class TypesToValues {
 public:
  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr bool All(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return AllHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr bool AllWithTemporary(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return AllHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr bool AllWithTemporary(TemporaryType tmp,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    return AllHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr bool Any(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return AnyHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr bool AnyWithTemporary(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return AnyHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr bool AnyWithTemporary(TemporaryType tmp,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    return AnyHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename Type>
  static constexpr std::size_t Count() {
    return TypesToTypes::Count<TupleType, Type>{};
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr std::size_t CountIf(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr std::size_t CountIfWithTemporary(Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return CountIfHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr std::size_t CountIfWithTemporary(TemporaryType tmp,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr decltype(auto) FlatMap(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return FlatMapHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr decltype(auto) FlatMapWithTemporary(Lambda&& lambda,
                                                       ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return FlatMapHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr decltype(auto) FlatMapWithTemporary(TemporaryType tmp,
                                                       Lambda&& lambda,
                                                       ExtraLambdaArgTypes&&... extra_types) {
    return FlatMapHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr void ForEach(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr void ForEachWithTemporary(Lambda&& lambda,
                                             ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    ForEachHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TupleType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr void ForEachWithTemporary(TemporaryType tmp,
                                             Lambda&& lambda,
                                             ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<TupleType, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
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

  template <typename TupleType,
            typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename Lambda>
  static constexpr OutputType Produce(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    OutputType result;
    ForEachHelper<TupleType, OutputType&, ExtraLambdaArgTypes...>(
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
  static constexpr OutputType Produce(OutputType result,
                                      Lambda&& lambda,
                                      ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<TupleType, OutputType&, ExtraLambdaArgTypes...>(
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
  static constexpr OutputType ProduceWithTemporary(Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    OutputType result;
    TemporaryType tmp{};
    ForEachHelper<TupleType, OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
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
  static constexpr OutputType ProduceWithTemporary(OutputType result,
                                                   TemporaryType tmp,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<TupleType, OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<TupleType>>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }

 private:
  template <typename TupleType, typename... ExtraLambdaArgTypes, std::size_t... Is, typename Lambda>
  static constexpr bool AllHelper(Lambda&& lambda,
                                  std::index_sequence<Is...>,
                                  ExtraLambdaArgTypes&&... extra_types) {
    return (static_cast<bool>(lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) &&
            ... && true);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, std::size_t... Is, typename Lambda>
  static constexpr bool AnyHelper(Lambda&& lambda,
                                  std::index_sequence<Is...>,
                                  ExtraLambdaArgTypes&&... extra_types) {
    return (static_cast<bool>(lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) ||
            ... || false);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, std::size_t... Is, typename Lambda>
  static constexpr std::size_t CountIfHelper(Lambda&& lambda,
                                             std::index_sequence<Is...>,
                                             ExtraLambdaArgTypes&&... extra_types) {
    return (static_cast<bool>(lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) +
            ... + std::size_t{0});
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda, std::size_t... Is>
  static constexpr decltype(auto) FlatMapHelper(Lambda&& lambda,
                                                std::index_sequence<Is...>,
                                                ExtraLambdaArgTypes&&... extra_types) {
    return std::tuple_cat(lambda.template operator()<std::tuple_element_t<Is, TupleType>>(
        std::forward<ExtraLambdaArgTypes>(extra_types)...)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda, std::size_t... Is>
  static constexpr void ForEachHelper(Lambda&& lambda,
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

template <typename TupleType, auto kLambda, auto... extra_lambda_values>
class TypesToTypes::TakeSkipHelper {
 public:
  static constexpr std::size_t Produce() {
    std::size_t size = 0;
    // Note: we don't really need the return value of TypesToValues::All here, instead we rely on
    // the fact that TypesToValues::All stops calculations when it finds first false.
    TypesToValues::All<TupleType>(TakeSkipHelper{}, size);
    return size;
  }
  template <typename Type>
  constexpr auto operator()(std::size_t& size) const {
    if (kLambda.template operator()<Type>(extra_lambda_values...)) {
      size++;
      return true;
    } else {
      return false;
    }
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
  using MetaValues = typename MetaValuesHelper<kValues>::MetaValues;

 private:
  template <auto kValues>
  class MetaValuesHelper {
   public:
    template <std::size_t... Is>
    static constexpr std::tuple<MetaValue<std::get<Is>(kValues)>...> MetaValuesFunc(
        std::index_sequence<Is...>);
    using MetaValues = decltype(MetaValuesFunc(
        std::declval<std::make_index_sequence<
            std::tuple_size_v<std::remove_reference_t<decltype(kValues)>>>>()));
  };
  template <typename TupleName, TupleName* kValues>
  class MetaValuesHelper<kValues> {
   public:
    template <std::size_t... Is>
    static constexpr std::tuple<MetaValue<std::get<Is>(*kValues)>...> MetaValuesFunc(
        std::index_sequence<Is...>);
    using MetaValues = decltype(MetaValuesFunc(
        std::declval<std::make_index_sequence<
            std::tuple_size_v<std::remove_reference_t<decltype(*kValues)>>>>()));
  };
};

// ValuesToValues provides value-level functional-style operations on std::tuple values. It also
// supports std::array if <array> header is included. These methods often take a lambda to apply to
// each element of the tuple.
class ValuesToValues {
 public:
  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr bool All(TupleType&& tuple,
                            Lambda&& lambda,
                            ExtraLambdaArgTypes&&... extra_types) {
    return AllHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr bool AllWithTemporary(TupleType&& tuple,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return AllHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr bool AllWithTemporary(TupleType&& tuple,
                                         TemporaryType tmp,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    return AllHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr bool Any(TupleType&& tuple,
                            Lambda&& lambda,
                            ExtraLambdaArgTypes&&... extra_types) {
    return AnyHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr bool AnyWithTemporary(TupleType&& tuple,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return AnyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr bool AnyWithTemporary(TupleType&& tuple,
                                         TemporaryType tmp,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    return AnyHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename Type, typename TupleType>
  static constexpr decltype(auto) Count(TupleType&&) {
    return TypesToTypes::Count<TupleType, Type>{};
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr bool CountIf(TupleType&& tuple,
                                Lambda&& lambda,
                                ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr bool CountIfWithTemporary(TupleType&& tuple,
                                             Lambda&& lambda,
                                             ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return CountIfHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr bool CountIfWithTemporary(TupleType&& tuple,
                                             TemporaryType tmp,
                                             Lambda&& lambda,
                                             ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType>
  static constexpr TypesToTypes::Enumerate<std::remove_cvref_t<TupleType>> Enumerate(
      TupleType&& tuple) {
    return Zip(TypesToTypes::Indexes<std::remove_cvref_t<TupleType>>{}, tuple);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr decltype(auto) Filter(TupleType&& tuple,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    return FilterHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) FilterWithTemporary(TupleType&& tuple,
                                                      Lambda&& lambda,
                                                      ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return FilterHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) FilterWithTemporary(TupleType&& tuple,
                                                      TemporaryType tmp,
                                                      Lambda&& lambda,
                                                      ExtraLambdaArgTypes&&... extra_types) {
    return FilterHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr decltype(auto) FlatMap(TupleType&& tuple,
                                          Lambda&& lambda,
                                          ExtraLambdaArgTypes&&... extra_types) {
    return FlatMapHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) FlatMapWithTemporary(TupleType&& tuple,
                                                       Lambda&& lambda,
                                                       ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return FlatMapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) FlatMapWithTemporary(TupleType&& tuple,
                                                       TemporaryType tmp,
                                                       Lambda&& lambda,
                                                       ExtraLambdaArgTypes&&... extra_types) {
    return FlatMapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr void ForEach(TupleType&& tuple,
                                Lambda&& lambda,
                                ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr void ForEachWithTemporary(TupleType&& tuple,
                                             Lambda&& lambda,
                                             ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    ForEachHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr void ForEachWithTemporary(TupleType&& tuple,
                                             TemporaryType tmp,
                                             Lambda&& lambda,
                                             ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr decltype(auto) Map(TupleType&& tuple,
                                      Lambda&& lambda,
                                      ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(TupleType&& tuple,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr decltype(auto) MapWithTemporary(TupleType&& tuple,
                                                   TemporaryType tmp,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType Produce(TupleType&& tuple,
                                      Lambda&& lambda,
                                      ExtraLambdaArgTypes&&... extra_types) {
    // Note: result is explicitly default initialized, not value initialized.
    // This helps to catch mistakes in costexpr context, because an attempt to assign uninitialized
    // result to constexpr variable would fail. Use the next form if you need initialization.
    OutputType result;
    ForEachHelper<OutputType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType Produce(TupleType&& tuple,
                                      OutputType result,
                                      Lambda&& lambda,
                                      ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<OutputType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        result,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }
  template <typename OutputType,
            typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr OutputType ProduceWithTemporary(TupleType&& tuple,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    // Note: result is explicitly default initialized, not value initialized.
    // This helps to catch mistakes in costexpr context, because an attempt to assign uninitialized
    // result to constexpr variable would fail. Use the next form if you need initialization.
    OutputType result;
    TemporaryType tmp{};
    ForEachHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
  static constexpr OutputType ProduceWithTemporary(TupleType&& tuple,
                                                   OutputType result,
                                                   TemporaryType tmp,
                                                   Lambda&& lambda,
                                                   ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<OutputType&, TemporaryType&, ExtraLambdaArgTypes...>(
        tuple,
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        result,
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
    return result;
  }

  template <typename Type, typename TupleType>
  static constexpr decltype(auto) Retain(TupleType&& tuple) {
    return FlatMapHelper(
        tuple,
        []<typename TypeToCheck>(auto&& elem) -> decltype(auto) {
          if constexpr (std::is_same_v<Type, TypeToCheck>) {
            return std::tuple<TypeToCheck>{std::forward<decltype(elem)>(elem)};
          } else {
            return std::tuple<>{};
          }
        },
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{});
  }
  template <typename Type, typename TupleType>
  static constexpr decltype(auto) RetainIfNot(TupleType&& tuple) {
    return FlatMapHelper(
        tuple,
        []<typename TypeToCheck>(auto&& elem) -> decltype(auto) {
          if constexpr (std::is_same_v<Type, TypeToCheck>) {
            return std::tuple<>{};
          } else {
            return std::tuple<TypeToCheck>{std::forward<decltype(elem)>(elem)};
          }
        },
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{});
  }

  template <auto kCount, typename TupleType>
  static constexpr decltype(auto) Skip(TupleType&& tuple) {
    return FlatMapHelper(
        Enumerate(std::forward<TupleType>(tuple)),
        []<typename Type>(auto&& elem) -> decltype(auto) {
          constexpr std::size_t kIdx = std::tuple_element_t<0, Type>{};
          if constexpr (kIdx >= kCount) {
            return std::tuple<std::tuple_element_t<1, Type>>(
                std::get<1>(std::forward<decltype(elem)>(elem)));
          } else {
            return std::tuple<>{};
          }
        },
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{});
  }

  template <typename TupleType, auto kCount>
  static constexpr decltype(auto) Skip(TupleType&& tuple, MetaValue<kCount>) {
    return Skip<kCount, TupleType>(tuple);
  }

  template <auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) SkipWhile(TupleType&& tuple) {
    constexpr std::size_t kCount = TypesToTypes::
        TakeSkipHelper<std::remove_cvref_t<TupleType>, kLambda, extra_lambda_values...>::Produce();
    return Skip<kCount, TupleType>(tuple);
  }

  template <typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) SkipWhile(TupleType&& tuple,
                                            MetaValue<kLambda>,
                                            MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount = TypesToTypes::
        TakeSkipHelper<std::remove_cvref_t<TupleType>, kLambda, extra_lambda_values...>::Produce();
    return Skip<kCount, TupleType>(tuple);
  }

  template <typename InitialValueType,
            auto kLambda,
            auto... extra_lambda_values,
            typename TupleType>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  InitialValueType{},
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Skip<kCount, TupleType>(tuple);
  }

  template <typename InitialValueType,
            typename TupleType,
            auto kLambda,
            auto... extra_lambda_values>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  InitialValueType{},
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Skip<kCount, TupleType>(tuple);
  }

  template <auto initial_value, auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  initial_value,
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Skip<kCount, TupleType>(tuple);
  }

  template <auto initial_value, typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<initial_value>,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  initial_value,
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Skip<kCount, TupleType>(tuple);
  }

  template <auto kCount, typename TupleType>
  static constexpr decltype(auto) Take(TupleType&& tuple) {
    return FlatMapHelper(
        Enumerate(std::forward<TupleType>(tuple)),
        []<typename Type>(auto&& elem) -> decltype(auto) {
          constexpr std::size_t kIdx = std::tuple_element_t<0, Type>{};
          if constexpr (kIdx < kCount) {
            return std::tuple<std::tuple_element_t<1, Type>>(
                std::get<1>(std::forward<decltype(elem)>(elem)));
          } else {
            return std::tuple<>{};
          }
        },
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{});
  }

  template <auto kCount, typename TupleType>
  static constexpr decltype(auto) Take(TupleType&& tuple, MetaValue<kCount>) {
    return Take<kCount, TupleType>(tuple);
  }

  template <auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) TakeWhile(TupleType&& tuple) {
    constexpr std::size_t kCount = TypesToTypes::
        TakeSkipHelper<std::remove_cvref_t<TupleType>, kLambda, extra_lambda_values...>::Produce();
    return Take<kCount, TupleType>(tuple);
  }

  template <typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) TakeWhile(TupleType&& tuple,
                                            MetaValue<kLambda>,
                                            MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount = TypesToTypes::
        TakeSkipHelper<std::remove_cvref_t<TupleType>, kLambda, extra_lambda_values...>::Produce();
    return Take<kCount, TupleType>(tuple);
  }

  template <typename InitialValueType,
            auto kLambda,
            auto... extra_lambda_values,
            typename TupleType>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  InitialValueType{},
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Take<kCount, TupleType>(tuple);
  }

  template <typename InitialValueType,
            typename TupleType,
            auto kLambda,
            auto... extra_lambda_values>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  InitialValueType{},
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Take<kCount, TupleType>(tuple);
  }

  template <auto initial_value, auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  initial_value,
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Take<kCount, TupleType>(tuple);
  }

  template <auto initial_value, typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<initial_value>,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount = TakeSkipHelper<std::remove_cvref_t<TupleType>,
                                                  initial_value,
                                                  kLambda,
                                                  extra_lambda_values...>();
    return Take<kCount, TupleType>(tuple);
  }

  template <typename... TupleTypes>
  static constexpr TypesToTypes::Zip<std::remove_cvref_t<TupleTypes>...> Zip(
      TupleTypes&&... tuples) {
    return ZipHelper<TypesToTypes::Zip<std::remove_cvref_t<TupleTypes>...>>(
        std::tuple<std::remove_cvref_t<TupleTypes>...>{tuples...},
        std::make_index_sequence<std::min(
            {std::tuple_size_v<std::remove_reference_t<TupleTypes>>...})>{},
        std::make_index_sequence<sizeof...(TupleTypes)>{});
  }
  template <typename... TupleTypes>
  static constexpr TypesToTypes::ZipShortest<std::remove_cvref_t<TupleTypes>...> ZipShortest(
      TupleTypes&&... tuples) {
    return ZipHelper<TypesToTypes::ZipShortest<std::remove_cvref_t<TupleTypes>...>>(
        std::tuple<std::remove_cvref_t<TupleTypes>...>{tuples...},
        std::make_index_sequence<std::min(
            {std::tuple_size_v<std::remove_reference_t<TupleTypes>>...})>{},
        std::make_index_sequence<sizeof...(TupleTypes)>{});
  }

 private:
  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr bool AllHelper(TupleType&& tuple,
                                  Lambda&& lambda,
                                  std::index_sequence<Is...>,
                                  ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
                std::get<Is>(std::forward<TupleType>(tuple)),
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) &&
        ... && true);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr bool AnyHelper(TupleType&& tuple,
                                  Lambda&& lambda,
                                  std::index_sequence<Is...>,
                                  ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
                std::get<Is>(std::forward<TupleType>(tuple)),
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) ||
        ... || false);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr bool CountIfHelper(TupleType&& tuple,
                                      Lambda&& lambda,
                                      std::index_sequence<Is...>,
                                      ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
                std::get<Is>(std::forward<TupleType>(tuple)),
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) +
        ... + std::size_t{0});
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr decltype(auto) FilterHelper(TupleType&& tuple,
                                               Lambda&& lambda,
                                               std::index_sequence<Is...>,
                                               ExtraLambdaArgTypes&&... extra_types) {
    return std::tuple_cat(
        [lambda = std::forward<Lambda>(lambda)]<typename ElementType>(
            auto&& element, ExtraLambdaArgTypes&&... extra_types) {
          auto lambda_result = lambda.template operator()<ElementType>(
              std::forward<ExtraLambdaArgTypes>(extra_types)...);
          constexpr bool kCheckResult = decltype(lambda_result){};
          if constexpr (kCheckResult) {
            return std::tuple<ElementType>{element};
          } else {
            return std::tuple<>{};
          }
        }
            .template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
                std::get<Is>(std::forward<TupleType>(tuple)),
                std::forward<ExtraLambdaArgTypes>(extra_types)...)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr decltype(auto) FlatMapHelper(TupleType&& tuple,
                                                Lambda&& lambda,
                                                std::index_sequence<Is...>,
                                                ExtraLambdaArgTypes&&... extra_types) {
    return std::tuple_cat(
        lambda.template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
            std::get<Is>(std::forward<TupleType>(tuple)),
            std::forward<ExtraLambdaArgTypes>(extra_types)...)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr void ForEachHelper(TupleType&& tuple,
                                      Lambda&& lambda,
                                      std::index_sequence<Is...>,
                                      ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
         std::get<Is>(std::forward<TupleType>(tuple)),
         std::forward<ExtraLambdaArgTypes>(extra_types)...),
     ...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr decltype(auto) MapHelper(TupleType&& tuple,
                                            Lambda&& lambda,
                                            std::index_sequence<Is...>,
                                            ExtraLambdaArgTypes&&... extra_types) {
    // Note: we need to specify the type of tuple here explicitly, because otherwise type deduction
    // would produce `int` where we want `const int&`.
    return std::tuple<decltype(lambda.template
                               operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
                                   std::get<Is>(std::forward<TupleType>(tuple)),
                                   std::forward<ExtraLambdaArgTypes>(extra_types)...))...> {
      lambda.template operator()<std::tuple_element_t<Is, std::remove_cvref_t<TupleType>>>(
          std::get<Is>(std::forward<TupleType>(tuple)),
          std::forward<ExtraLambdaArgTypes>(extra_types)...)...
    };
  }

  template <typename TupleType, auto initial_value, auto kLambda, auto... extra_lambda_values>
  static constexpr std::size_t TakeSkipHelper() {
    std::size_t size = 0;
    auto value = initial_value;
    // Note: we don't really need the return value of TypesToValues::All here, instead we rely on
    // the fact that TypesToValues::All stops calculations when it finds first false.
    TypesToValues::All<TupleType>(
        []<typename Type>(auto& value, std::size_t& size) {
          if (kLambda.template operator()<Type>(value, extra_lambda_values...)) {
            size++;
            return true;
          } else {
            return false;
          }
        },
        value,
        size);
    return size;
  }

  template <typename ZipType, typename TupleTypeLeft, typename TupleTypeRight, std::size_t... Is>
  static constexpr ZipType ZipHelper(std::tuple<TupleTypeLeft, TupleTypeRight>&& tuples,
                                     std::index_sequence<Is...>,
                                     std::index_sequence<std::size_t{0}, std::size_t{1}>) {
    return ZipType{std::pair<std::tuple_element_t<Is, std::remove_cvref_t<TupleTypeLeft>>,
                             std::tuple_element_t<Is, std::remove_cvref_t<TupleTypeRight>>>{
        std::get<Is>(std::forward<TupleTypeLeft>(std::get<0>(tuples))),
        std::get<Is>(std::forward<TupleTypeRight>(std::get<1>(tuples)))}...};
  }
  template <typename ZipType, typename TupleTypes, std::size_t... Is, std::size_t... Ts>
  static constexpr ZipType ZipHelper(TupleTypes&& tuples,
                                     std::index_sequence<Is...>,
                                     std::index_sequence<Ts...> ts) {
    return ZipType{
        ZipHelper<Is, std::tuple_element_t<Is, ZipType>>(std::forward<TupleTypes>(tuples), ts)...};
  }
  template <std::size_t I, typename ZipType, typename TupleTypes, std::size_t... Ts>
  static constexpr ZipType ZipHelper(TupleTypes&& tuples, std::index_sequence<Ts...>) {
    return ZipType{
        std::get<I>(std::forward<std::tuple_element_t<Ts, TupleTypes>>(std::get<Ts>(tuples)))...};
  }
};

}  // namespace berberis

#endif  // BERBERIS_BASE_TUPLE_PROCESSING_H_
