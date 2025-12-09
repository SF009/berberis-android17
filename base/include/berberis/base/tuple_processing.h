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
template <auto kValue_>
class MetaValue final {
 public:
  using ValueType = std::remove_cvref_t<decltype(kValue_)>;
  static constexpr auto kValue = kValue_;
  constexpr operator ValueType() const { return kValue; }
};

template <auto kValue_>
constexpr MetaValue<kValue_> kMeta = MetaValue<kValue_>{};

// Sometimes we need an address of variable (e.g. function name) that's passed to a template
// function by value. We can create static constexpr in the function, but then all these different
// static variables have different addresses and may bloat the resulting binary. Use of memoization
// ( https://en.wikipedia.org/wiki/Memoization ) allows us to keep addresses identical which then
// permits linker to merge these functions.
template <auto kValue>
inline constexpr auto kMemoizedValue = kValue;

#pragma push_macro("DEFINE_VALUE_OPERATOR")
#undef DEFINE_VALUE_OPERATOR
#define DEFINE_VALUE_OPERATOR(operator_name)                                   \
  template <auto kValue1, auto kValue2>                                        \
  constexpr MetaValue<(kValue1 operator_name kValue2)> operator operator_name( \
      MetaValue<kValue1>, MetaValue<kValue2>) {                                \
    return {};                                                                 \
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

template <typename BaseType>
class Raw;

template <typename BaseType>
class Saturating;

template <typename BaseType>
class Wrapping;

namespace intrinsics {

template <typename BaseType>
class WrappedFloatType;

}  // namespace intrinsics

template <typename T>
struct TypeTraits;

template <typename Type_>
class MetaType final {
 public:
  using Type = Type_;
  static constexpr MetaType<std::add_const_t<Type>> AddConst() { return {}; }
  static constexpr MetaType<std::add_cv_t<Type>> AddCv() { return {}; }
  static constexpr MetaType<std::add_lvalue_reference_t<Type>> AddLvalueReference() { return {}; }
  static constexpr MetaType<std::add_pointer_t<Type>> AddPointer() { return {}; }
  static constexpr MetaType<std::add_rvalue_reference_t<Type>> AddRvalueReference() { return {}; }
  static constexpr MetaType<std::add_volatile_t<Type>> AddVolatile() { return {}; }
  static constexpr size_t AlignmentOf() { return std::alignment_of_v<Type>; }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_reference_t<Type, OtherTypes...>> CommonReference(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_type_t<Type, OtherTypes...>> CommonType(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr bool Conjunction(MetaType<OtherTypes>...) {
    return std::conjunction_v<Type, OtherTypes...>;
  }
  static constexpr MetaType<std::decay_t<Type>> Decay() { return {}; }
  template <typename... OtherTypes>
  static constexpr bool Disjunction(MetaType<OtherTypes>...) {
    return std::disjunction_v<Type, OtherTypes...>;
  }
  static constexpr size_t Extent() { return std::extent_v<Type>; }
  static constexpr auto Float() {
    if constexpr (sizeof(typename TypeTraits<Type>::Float) == sizeof(_Float16)) {
      return MetaType<_Float16>{};
    } else if constexpr (sizeof(typename TypeTraits<Type>::Float) == sizeof(float)) {
      MetaType<float>{};
    } else {
      static_assert(sizeof(typename TypeTraits<Type>::Float) == sizeof(double));
      return MetaType<double>{};
    }
  }
  static constexpr bool HasUniqueObjectRepresentations() {
    return std::has_unique_object_representations_v<Type>;
  }
  static constexpr bool HasVirtualDestructor() { return std::has_virtual_destructor_v<Type>; }
  static constexpr auto Int() { return MetaType<typename TypeTraits<Type>::Int>{}; }
  template <typename... ArgTypes>
  static constexpr MetaType<std::invoke_result_t<Type, ArgTypes...>> InvokeResult(
      MetaType<ArgTypes>...) {
    return {};
  }
  static constexpr bool IsAbstract() { return std::is_abstract_v<Type>; }
  static constexpr bool IsAggregate() { return std::is_aggregate_v<Type>; }
  static constexpr bool IsArithmetic() { return std::is_arithmetic_v<Type>; }
  static constexpr bool IsArray() { return std::is_array_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsAssignable(MetaType<OtherType>) {
    return std::is_assignable_v<Type, OtherType>;
  }
  template <typename OtherType>
  static constexpr bool IsBaseOf() {
    return std::is_base_of_v<Type, OtherType>;
  }
  static constexpr bool IsBoundedArray() { return std::is_bounded_array_v<Type>; }
  static constexpr bool IsClass() { return std::is_class_v<Type>; }
  static constexpr bool IsCompound() { return std::is_compound_v<Type>; }
  static constexpr bool IsConst() { return std::is_const_v<Type>; }
  static constexpr bool IsConstructible() { return std::is_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsConvertible() {
    return std::is_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsCopyAssignable() { return std::is_copy_assignable_v<Type>; }
  static constexpr bool IsCopyConstructible() { return std::is_copy_constructible_v<Type>; }
  static constexpr bool IsDefaultConstructible() { return std::is_default_constructible_v<Type>; }
  static constexpr bool IsDestructible() { return std::is_destructible_v<Type>; }
  static constexpr bool IsEmpty() { return std::is_empty_v<Type>; }
  static constexpr bool IsEnum() { return std::is_enum_v<Type>; }
  static constexpr bool IsFinal() { return std::is_final_v<Type>; }
  static constexpr bool IsFloatingPoint() { return std::is_floating_point_v<Type>; }
  static constexpr bool IsFunction() { return std::is_function_v<Type>; }
  static constexpr bool IsFundamental() { return std::is_fundamental_v<Type>; }
  static constexpr bool IsIntegral() { return std::is_integral_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsInvocable(MetaType<ArgTypes>...) {
    return std::is_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsLvalueReference() { return std::is_lvalue_reference_v<Type>; }
  static constexpr bool IsMemberFunctionPointer() {
    return std::is_member_function_pointer_v<Type>;
  }
  static constexpr bool IsMemberObjectPointer() { return std::is_member_object_pointer_v<Type>; }
  static constexpr bool IsMemberPointer() { return std::is_member_pointer_v<Type>; }
  static constexpr bool IsMoveAssignable() { return std::is_move_assignable_v<Type>; }
  static constexpr bool IsMoveConstructible() { return std::is_move_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowAssignable(MetaType<OtherType>) {
    return std::is_nothrow_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowConstructible() { return std::is_nothrow_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowConvertible() {
    return std::is_nothrow_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowCopyAssignable() {
    return std::is_nothrow_copy_assignable_v<Type>;
  }
  static constexpr bool IsNothrowCopyConstructible() {
    return std::is_nothrow_copy_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDefaultConstructible() {
    return std::is_nothrow_default_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDestructible() { return std::is_nothrow_destructible_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsNothrowInvocable(MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsNothrowInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsNothrowMoveAssignable() {
    return std::is_nothrow_move_assignable_v<Type>;
  }
  static constexpr bool IsNothrowMoveConstructible() {
    return std::is_nothrow_move_constructible_v<Type>;
  }
  static constexpr bool IsNothrowSwappable() { return std::is_nothrow_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowSwappableWith() {
    return std::is_nothrow_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsNullPointer() { return std::is_null_pointer_v<Type>; }
  static constexpr bool IsObject() { return std::is_object_v<Type>; }
  static constexpr bool IsPointer() { return std::is_pointer_v<Type>; }
  static constexpr bool IsPolymorphic() { return std::is_polymorphic_v<Type>; }
  static constexpr bool IsReference() { return std::is_reference_v<Type>; }
  static constexpr bool IsRvalueReference() { return std::is_rvalue_reference_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSame() {
    return std::is_same_v<Type, OtherType>;
  }
  static constexpr bool IsScalar() { return std::is_scalar_v<Type>; }
  static constexpr bool IsSigned() { return std::is_signed_v<Type>; }
  static constexpr bool IsStandardLayout() { return std::is_standard_layout_v<Type>; }
  static constexpr bool IsSwappable() { return std::is_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSwappableWith(MetaType<OtherType>) {
    return std::is_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsTrivial() { return std::is_trivial_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsTriviallyAssignable(MetaType<OtherType>) {
    return std::is_trivially_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsTriviallyConstructible() {
    return std::is_trivially_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyCopyable() { return std::is_trivially_copyable_v<Type>; }
  static constexpr bool IsTriviallyCopyAssignable() {
    return std::is_trivially_copy_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyCopyConstructible() {
    return std::is_trivially_copy_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDefaultConstructible() {
    return std::is_trivially_default_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDestructible() { return std::is_trivially_destructible_v<Type>; }
  static constexpr bool IsTriviallyMoveAssignable() {
    return std::is_trivially_move_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyMoveConstructible() {
    return std::is_trivially_move_constructible_v<Type>;
  }
  static constexpr bool IsUnboundedArray() { return std::is_unbounded_array_v<Type>; }
  static constexpr bool IsUnion() { return std::is_union_v<Type>; }
  static constexpr bool IsUnsigned() { return std::is_unsigned_v<Type>; }
  static constexpr bool IsVoid() { return std::is_void_v<Type>; }
  static constexpr bool IsVolatile() { return std::is_volatile_v<Type>; }
  static constexpr auto MakeSigned() { return MetaType<std::make_signed_t<Type>>{}; }
  static constexpr auto MakeUnsigned() { return MetaType<std::make_unsigned_t<Type>>{}; }
  static constexpr auto Narrow() { return MetaType<typename TypeTraits<Type>::Narrow>{}; }
  static constexpr bool Negation() { return std::negation_v<Type>; }
  static constexpr size_t Rank() { return std::rank_v<Type>; }
  static constexpr auto Raw() { return MetaType<berberis::Raw<Type>>{}; }
  static constexpr MetaType<std::remove_all_extents_t<Type>> RemoveAllExtents() { return {}; }
  static constexpr MetaType<std::remove_const_t<Type>> RemoveConst() { return {}; }
  static constexpr MetaType<std::remove_cv_t<Type>> RemoveCv() { return {}; }
  static constexpr MetaType<std::remove_cvref_t<Type>> RemoveCvref() { return {}; }
  static constexpr MetaType<std::remove_extent_t<Type>> RemoveExtent() { return {}; }
  static constexpr MetaType<std::remove_pointer_t<Type>> RemovePointer() { return {}; }
  static constexpr MetaType<std::remove_reference_t<Type>> RemoveReference() { return {}; }
  static constexpr MetaType<std::remove_volatile_t<Type>> RemoveVolatile() { return {}; }
  static constexpr auto Saturating() { return MetaType<berberis::Saturating<Type>>{}; }
  static constexpr MetaType<std::type_identity_t<Type>> TypeIdentity() { return {}; }
  static constexpr auto UnderlyingType() { return MetaType<std::underlying_type_t<Type>>{}; }
  static constexpr MetaType<std::unwrap_ref_decay_t<Type>> UnwrapRefDecay() { return {}; }
  static constexpr MetaType<std::unwrap_reference_t<Type>> UnwrapReference() { return {}; }
  template <typename... OtherTypes>
  MetaType<void> Void(MetaType<OtherTypes>...) {
    return {};
  }
  static constexpr auto Wrapped() { return MetaType<intrinsics::WrappedFloatType<Type>>{}; }
  static constexpr auto Wrapping() { return MetaType<berberis::Wrapping<Type>>{}; }
  static constexpr auto Wide() { return MetaType<typename TypeTraits<Type>::Wide>{}; }
  static constexpr auto Widen() { return MetaType<typename TypeTraits<Type>::Wide>{}; }
};

template <typename UnderlyingType_>
class MetaType<berberis::Raw<UnderlyingType_>> final {
 public:
  using Type = berberis::Raw<UnderlyingType_>;

  static constexpr MetaType<std::add_const_t<Type>> AddConst() { return {}; }
  static constexpr MetaType<std::add_cv_t<Type>> AddCv() { return {}; }
  static constexpr MetaType<std::add_lvalue_reference_t<Type>> AddLvalueReference() { return {}; }
  static constexpr MetaType<std::add_pointer_t<Type>> AddPointer() { return {}; }
  static constexpr MetaType<std::add_rvalue_reference_t<Type>> AddRvalueReference() { return {}; }
  static constexpr MetaType<std::add_volatile_t<Type>> AddVolatile() { return {}; }
  static constexpr size_t AlignmentOf() { return std::alignment_of_v<Type>; }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_reference_t<Type, OtherTypes...>> CommonReference(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_type_t<Type, OtherTypes...>> CommonType(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr bool Conjunction(MetaType<OtherTypes>...) {
    return std::conjunction_v<Type, OtherTypes...>;
  }
  static constexpr MetaType<std::decay_t<Type>> Decay() { return {}; }
  template <typename... OtherTypes>
  static constexpr bool Disjunction(MetaType<OtherTypes>...) {
    return std::disjunction_v<Type, OtherTypes...>;
  }
  static constexpr size_t Extent() { return std::extent_v<Type>; }
  static constexpr auto Float() { return MetaType<typename TypeTraits<UnderlyingType_>::Float>{}; }
  static constexpr bool HasUniqueObjectRepresentations() {
    return std::has_unique_object_representations_v<Type>;
  }
  static constexpr bool HasVirtualDestructor() { return std::has_virtual_destructor_v<Type>; }
  static constexpr auto Int() {
    return MetaType<berberis::Raw<typename TypeTraits<UnderlyingType_>::Int>>{};
  }
  template <typename... ArgTypes>
  static constexpr MetaType<std::invoke_result_t<Type, ArgTypes...>> InvokeResult(
      MetaType<ArgTypes>...) {
    return {};
  }
  static constexpr bool IsAbstract() { return std::is_abstract_v<Type>; }
  static constexpr bool IsAggregate() { return std::is_aggregate_v<Type>; }
  static constexpr bool IsArithmetic() { return std::is_arithmetic_v<Type>; }
  static constexpr bool IsArray() { return std::is_array_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsAssignable(MetaType<OtherType>) {
    return std::is_assignable_v<Type, OtherType>;
  }
  template <typename OtherType>
  static constexpr bool IsBaseOf() {
    return std::is_base_of_v<Type, OtherType>;
  }
  static constexpr bool IsBoundedArray() { return std::is_bounded_array_v<Type>; }
  static constexpr bool IsClass() { return std::is_class_v<Type>; }
  static constexpr bool IsCompound() { return std::is_compound_v<Type>; }
  static constexpr bool IsConst() { return std::is_const_v<Type>; }
  static constexpr bool IsConstructible() { return std::is_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsConvertible() {
    return std::is_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsCopyAssignable() { return std::is_copy_assignable_v<Type>; }
  static constexpr bool IsCopyConstructible() { return std::is_copy_constructible_v<Type>; }
  static constexpr bool IsDefaultConstructible() { return std::is_default_constructible_v<Type>; }
  static constexpr bool IsDestructible() { return std::is_destructible_v<Type>; }
  static constexpr bool IsEmpty() { return std::is_empty_v<Type>; }
  static constexpr bool IsEnum() { return std::is_enum_v<Type>; }
  static constexpr bool IsFinal() { return std::is_final_v<Type>; }
  static constexpr bool IsFloatingPoint() { return std::is_floating_point_v<Type>; }
  static constexpr bool IsFunction() { return std::is_function_v<Type>; }
  static constexpr bool IsFundamental() { return std::is_fundamental_v<Type>; }
  static constexpr bool IsIntegral() { return std::is_integral_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsInvocable(MetaType<ArgTypes>...) {
    return std::is_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsLvalueReference() { return std::is_lvalue_reference_v<Type>; }
  static constexpr bool IsMemberFunctionPointer() {
    return std::is_member_function_pointer_v<Type>;
  }
  static constexpr bool IsMemberObjectPointer() { return std::is_member_object_pointer_v<Type>; }
  static constexpr bool IsMemberPointer() { return std::is_member_pointer_v<Type>; }
  static constexpr bool IsMoveAssignable() { return std::is_move_assignable_v<Type>; }
  static constexpr bool IsMoveConstructible() { return std::is_move_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowAssignable(MetaType<OtherType>) {
    return std::is_nothrow_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowConstructible() { return std::is_nothrow_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowConvertible() {
    return std::is_nothrow_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowCopyAssignable() {
    return std::is_nothrow_copy_assignable_v<Type>;
  }
  static constexpr bool IsNothrowCopyConstructible() {
    return std::is_nothrow_copy_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDefaultConstructible() {
    return std::is_nothrow_default_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDestructible() { return std::is_nothrow_destructible_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsNothrowInvocable(MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsNothrowInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsNothrowMoveAssignable() {
    return std::is_nothrow_move_assignable_v<Type>;
  }
  static constexpr bool IsNothrowMoveConstructible() {
    return std::is_nothrow_move_constructible_v<Type>;
  }
  static constexpr bool IsNothrowSwappable() { return std::is_nothrow_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowSwappableWith() {
    return std::is_nothrow_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsNullPointer() { return std::is_null_pointer_v<Type>; }
  static constexpr bool IsObject() { return std::is_object_v<Type>; }
  static constexpr bool IsPointer() { return std::is_pointer_v<Type>; }
  static constexpr bool IsPolymorphic() { return std::is_polymorphic_v<Type>; }
  static constexpr bool IsReference() { return std::is_reference_v<Type>; }
  static constexpr bool IsRvalueReference() { return std::is_rvalue_reference_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSame() {
    return std::is_same_v<Type, OtherType>;
  }
  static constexpr bool IsScalar() { return std::is_scalar_v<Type>; }
  static constexpr bool IsSigned() { return std::is_signed_v<UnderlyingType_>; }
  static constexpr bool IsStandardLayout() { return std::is_standard_layout_v<Type>; }
  static constexpr bool IsSwappable() { return std::is_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSwappableWith(MetaType<OtherType>) {
    return std::is_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsTrivial() { return std::is_trivial_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsTriviallyAssignable(MetaType<OtherType>) {
    return std::is_trivially_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsTriviallyConstructible() {
    return std::is_trivially_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyCopyable() { return std::is_trivially_copyable_v<Type>; }
  static constexpr bool IsTriviallyCopyAssignable() {
    return std::is_trivially_copy_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyCopyConstructible() {
    return std::is_trivially_copy_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDefaultConstructible() {
    return std::is_trivially_default_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDestructible() { return std::is_trivially_destructible_v<Type>; }
  static constexpr bool IsTriviallyMoveAssignable() {
    return std::is_trivially_move_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyMoveConstructible() {
    return std::is_trivially_move_constructible_v<Type>;
  }
  static constexpr bool IsUnboundedArray() { return std::is_unbounded_array_v<Type>; }
  static constexpr bool IsUnion() { return std::is_union_v<Type>; }
  static constexpr bool IsUnsigned() { return std::is_unsigned_v<UnderlyingType_>; }
  static constexpr bool IsVoid() { return std::is_void_v<Type>; }
  static constexpr bool IsVolatile() { return std::is_volatile_v<Type>; }
  static constexpr auto MakeSigned() {
    return MetaType<berberis::Raw<std::make_signed_t<UnderlyingType_>>>{};
  }
  static constexpr auto MakeUnsigned() {
    return MetaType<berberis::Raw<std::make_unsigned_t<UnderlyingType_>>>{};
  }
  static constexpr auto Narrow() {
    return MetaType<berberis::Raw<typename TypeTraits<UnderlyingType_>::Narrow>>{};
  }
  static constexpr bool Negation() { return std::negation_v<Type>; }
  static constexpr size_t Rank() { return std::rank_v<Type>; }
  static constexpr auto Raw() { return MetaType<berberis::Raw<Type>>{}; }
  static constexpr MetaType<std::remove_all_extents_t<Type>> RemoveAllExtents() { return {}; }
  static constexpr MetaType<std::remove_const_t<Type>> RemoveConst() { return {}; }
  static constexpr MetaType<std::remove_cv_t<Type>> RemoveCv() { return {}; }
  static constexpr MetaType<std::remove_cvref_t<Type>> RemoveCvref() { return {}; }
  static constexpr MetaType<std::remove_extent_t<Type>> RemoveExtent() { return {}; }
  static constexpr MetaType<std::remove_pointer_t<Type>> RemovePointer() { return {}; }
  static constexpr MetaType<std::remove_reference_t<Type>> RemoveReference() { return {}; }
  static constexpr MetaType<std::remove_volatile_t<Type>> RemoveVolatile() { return {}; }
  static constexpr auto Saturating() { return MetaType<berberis::Saturating<UnderlyingType_>>{}; }
  static constexpr MetaType<std::type_identity_t<Type>> TypeIdentity() { return {}; }
  static constexpr MetaType<UnderlyingType_> UnderlyingType() { return {}; }
  static constexpr MetaType<std::unwrap_ref_decay_t<Type>> UnwrapRefDecay() { return {}; }
  static constexpr MetaType<std::unwrap_reference_t<Type>> UnwrapReference() { return {}; }
  template <typename... OtherTypes>
  MetaType<void> Void(MetaType<OtherTypes>...) {
    return {};
  }
  static constexpr auto Wrapped() {
    return MetaType<intrinsics::WrappedFloatType<UnderlyingType_>>{};
  }
  static constexpr auto Wrapping() { return MetaType<berberis::Wrapping<UnderlyingType_>>{}; }
  static constexpr auto Wide() {
    return MetaType<berberis::Raw<typename TypeTraits<UnderlyingType_>::Wide>>{};
  }
  static constexpr auto Widen() {
    return MetaType<berberis::Raw<typename TypeTraits<UnderlyingType_>::Wide>>{};
  }
};

template <typename UnderlyingType_>
class MetaType<berberis::Saturating<UnderlyingType_>> final {
 public:
  using Type = berberis::Saturating<UnderlyingType_>;

  static constexpr MetaType<std::add_const_t<Type>> AddConst() { return {}; }
  static constexpr MetaType<std::add_cv_t<Type>> AddCv() { return {}; }
  static constexpr MetaType<std::add_lvalue_reference_t<Type>> AddLvalueReference() { return {}; }
  static constexpr MetaType<std::add_pointer_t<Type>> AddPointer() { return {}; }
  static constexpr MetaType<std::add_rvalue_reference_t<Type>> AddRvalueReference() { return {}; }
  static constexpr MetaType<std::add_volatile_t<Type>> AddVolatile() { return {}; }
  static constexpr size_t AlignmentOf() { return std::alignment_of_v<Type>; }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_reference_t<Type, OtherTypes...>> CommonReference(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_type_t<Type, OtherTypes...>> CommonType(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr bool Conjunction(MetaType<OtherTypes>...) {
    return std::conjunction_v<Type, OtherTypes...>;
  }
  static constexpr MetaType<std::decay_t<Type>> Decay() { return {}; }
  template <typename... OtherTypes>
  static constexpr bool Disjunction(MetaType<OtherTypes>...) {
    return std::disjunction_v<Type, OtherTypes...>;
  }
  static constexpr size_t Extent() { return std::extent_v<Type>; }
  static constexpr auto Float() { return MetaType<typename TypeTraits<UnderlyingType_>::Float>{}; }
  static constexpr bool HasUniqueObjectRepresentations() {
    return std::has_unique_object_representations_v<Type>;
  }
  static constexpr bool HasVirtualDestructor() { return std::has_virtual_destructor_v<Type>; }
  static constexpr auto Int() {
    return MetaType<berberis::Saturating<typename TypeTraits<UnderlyingType_>::Int>>{};
  }
  template <typename... ArgTypes>
  static constexpr MetaType<std::invoke_result_t<Type, ArgTypes...>> InvokeResult(
      MetaType<ArgTypes>...) {
    return {};
  }
  static constexpr bool IsAbstract() { return std::is_abstract_v<Type>; }
  static constexpr bool IsAggregate() { return std::is_aggregate_v<Type>; }
  static constexpr bool IsArithmetic() { return std::is_arithmetic_v<Type>; }
  static constexpr bool IsArray() { return std::is_array_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsAssignable(MetaType<OtherType>) {
    return std::is_assignable_v<Type, OtherType>;
  }
  template <typename OtherType>
  static constexpr bool IsBaseOf() {
    return std::is_base_of_v<Type, OtherType>;
  }
  static constexpr bool IsBoundedArray() { return std::is_bounded_array_v<Type>; }
  static constexpr bool IsClass() { return std::is_class_v<Type>; }
  static constexpr bool IsCompound() { return std::is_compound_v<Type>; }
  static constexpr bool IsConst() { return std::is_const_v<Type>; }
  static constexpr bool IsConstructible() { return std::is_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsConvertible() {
    return std::is_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsCopyAssignable() { return std::is_copy_assignable_v<Type>; }
  static constexpr bool IsCopyConstructible() { return std::is_copy_constructible_v<Type>; }
  static constexpr bool IsDefaultConstructible() { return std::is_default_constructible_v<Type>; }
  static constexpr bool IsDestructible() { return std::is_destructible_v<Type>; }
  static constexpr bool IsEmpty() { return std::is_empty_v<Type>; }
  static constexpr bool IsEnum() { return std::is_enum_v<Type>; }
  static constexpr bool IsFinal() { return std::is_final_v<Type>; }
  static constexpr bool IsFloatingPoint() { return std::is_floating_point_v<Type>; }
  static constexpr bool IsFunction() { return std::is_function_v<Type>; }
  static constexpr bool IsFundamental() { return std::is_fundamental_v<Type>; }
  static constexpr bool IsIntegral() { return std::is_integral_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsInvocable(MetaType<ArgTypes>...) {
    return std::is_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsLvalueReference() { return std::is_lvalue_reference_v<Type>; }
  static constexpr bool IsMemberFunctionPointer() {
    return std::is_member_function_pointer_v<Type>;
  }
  static constexpr bool IsMemberObjectPointer() { return std::is_member_object_pointer_v<Type>; }
  static constexpr bool IsMemberPointer() { return std::is_member_pointer_v<Type>; }
  static constexpr bool IsMoveAssignable() { return std::is_move_assignable_v<Type>; }
  static constexpr bool IsMoveConstructible() { return std::is_move_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowAssignable(MetaType<OtherType>) {
    return std::is_nothrow_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowConstructible() { return std::is_nothrow_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowConvertible() {
    return std::is_nothrow_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowCopyAssignable() {
    return std::is_nothrow_copy_assignable_v<Type>;
  }
  static constexpr bool IsNothrowCopyConstructible() {
    return std::is_nothrow_copy_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDefaultConstructible() {
    return std::is_nothrow_default_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDestructible() { return std::is_nothrow_destructible_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsNothrowInvocable(MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsNothrowInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsNothrowMoveAssignable() {
    return std::is_nothrow_move_assignable_v<Type>;
  }
  static constexpr bool IsNothrowMoveConstructible() {
    return std::is_nothrow_move_constructible_v<Type>;
  }
  static constexpr bool IsNothrowSwappable() { return std::is_nothrow_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowSwappableWith() {
    return std::is_nothrow_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsNullPointer() { return std::is_null_pointer_v<Type>; }
  static constexpr bool IsObject() { return std::is_object_v<Type>; }
  static constexpr bool IsPointer() { return std::is_pointer_v<Type>; }
  static constexpr bool IsPolymorphic() { return std::is_polymorphic_v<Type>; }
  static constexpr bool IsReference() { return std::is_reference_v<Type>; }
  static constexpr bool IsRvalueReference() { return std::is_rvalue_reference_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSame() {
    return std::is_same_v<Type, OtherType>;
  }
  static constexpr bool IsScalar() { return std::is_scalar_v<Type>; }
  static constexpr bool IsSigned() { return std::is_signed_v<UnderlyingType_>; }
  static constexpr bool IsStandardLayout() { return std::is_standard_layout_v<Type>; }
  static constexpr bool IsSwappable() { return std::is_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSwappableWith(MetaType<OtherType>) {
    return std::is_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsTrivial() { return std::is_trivial_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsTriviallyAssignable(MetaType<OtherType>) {
    return std::is_trivially_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsTriviallyConstructible() {
    return std::is_trivially_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyCopyable() { return std::is_trivially_copyable_v<Type>; }
  static constexpr bool IsTriviallyCopyAssignable() {
    return std::is_trivially_copy_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyCopyConstructible() {
    return std::is_trivially_copy_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDefaultConstructible() {
    return std::is_trivially_default_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDestructible() { return std::is_trivially_destructible_v<Type>; }
  static constexpr bool IsTriviallyMoveAssignable() {
    return std::is_trivially_move_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyMoveConstructible() {
    return std::is_trivially_move_constructible_v<Type>;
  }
  static constexpr bool IsUnboundedArray() { return std::is_unbounded_array_v<Type>; }
  static constexpr bool IsUnion() { return std::is_union_v<Type>; }
  static constexpr bool IsUnsigned() { return std::is_unsigned_v<UnderlyingType_>; }
  static constexpr bool IsVoid() { return std::is_void_v<Type>; }
  static constexpr bool IsVolatile() { return std::is_volatile_v<Type>; }
  static constexpr auto MakeSigned() {
    return MetaType<berberis::Saturating<std::make_signed_t<UnderlyingType_>>>{};
  }
  static constexpr auto MakeUnsigned() {
    return MetaType<berberis::Saturating<std::make_unsigned_t<UnderlyingType_>>>{};
  }
  static constexpr auto Narrow() {
    return MetaType<berberis::Saturating<typename TypeTraits<UnderlyingType_>::Narrow>>{};
  }
  static constexpr bool Negation() { return std::negation_v<Type>; }
  static constexpr size_t Rank() { return std::rank_v<Type>; }
  static constexpr auto Raw() { return MetaType<berberis::Raw<Type>>{}; }
  static constexpr MetaType<std::remove_all_extents_t<Type>> RemoveAllExtents() { return {}; }
  static constexpr MetaType<std::remove_const_t<Type>> RemoveConst() { return {}; }
  static constexpr MetaType<std::remove_cv_t<Type>> RemoveCv() { return {}; }
  static constexpr MetaType<std::remove_cvref_t<Type>> RemoveCvref() { return {}; }
  static constexpr MetaType<std::remove_extent_t<Type>> RemoveExtent() { return {}; }
  static constexpr MetaType<std::remove_pointer_t<Type>> RemovePointer() { return {}; }
  static constexpr MetaType<std::remove_reference_t<Type>> RemoveReference() { return {}; }
  static constexpr MetaType<std::remove_volatile_t<Type>> RemoveVolatile() { return {}; }
  static constexpr auto Saturating() { return MetaType<berberis::Saturating<UnderlyingType_>>{}; }
  static constexpr MetaType<std::type_identity_t<Type>> TypeIdentity() { return {}; }
  static constexpr MetaType<UnderlyingType_> UnderlyingType() { return {}; }
  static constexpr MetaType<std::unwrap_ref_decay_t<Type>> UnwrapRefDecay() { return {}; }
  static constexpr MetaType<std::unwrap_reference_t<Type>> UnwrapReference() { return {}; }
  template <typename... OtherTypes>
  MetaType<void> Void(MetaType<OtherTypes>...) {
    return {};
  }
  static constexpr auto Wrapped() {
    return MetaType<intrinsics::WrappedFloatType<UnderlyingType_>>{};
  }
  static constexpr auto Wrapping() { return MetaType<berberis::Wrapping<UnderlyingType_>>{}; }
  static constexpr auto Wide() {
    return MetaType<berberis::Saturating<typename TypeTraits<UnderlyingType_>::Wide>>{};
  }
  static constexpr auto Widen() {
    return MetaType<berberis::Saturating<typename TypeTraits<UnderlyingType_>::Wide>>{};
  }
};

template <typename UnderlyingType_>
class MetaType<berberis::Wrapping<UnderlyingType_>> final {
 public:
  using Type = berberis::Wrapping<UnderlyingType_>;

  static constexpr MetaType<std::add_const_t<Type>> AddConst() { return {}; }
  static constexpr MetaType<std::add_cv_t<Type>> AddCv() { return {}; }
  static constexpr MetaType<std::add_lvalue_reference_t<Type>> AddLvalueReference() { return {}; }
  static constexpr MetaType<std::add_pointer_t<Type>> AddPointer() { return {}; }
  static constexpr MetaType<std::add_rvalue_reference_t<Type>> AddRvalueReference() { return {}; }
  static constexpr MetaType<std::add_volatile_t<Type>> AddVolatile() { return {}; }
  static constexpr size_t AlignmentOf() { return std::alignment_of_v<Type>; }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_reference_t<Type, OtherTypes...>> CommonReference(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_type_t<Type, OtherTypes...>> CommonType(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr bool Conjunction(MetaType<OtherTypes>...) {
    return std::conjunction_v<Type, OtherTypes...>;
  }
  static constexpr MetaType<std::decay_t<Type>> Decay() { return {}; }
  template <typename... OtherTypes>
  static constexpr bool Disjunction(MetaType<OtherTypes>...) {
    return std::disjunction_v<Type, OtherTypes...>;
  }
  static constexpr size_t Extent() { return std::extent_v<Type>; }
  static constexpr auto Float() { return MetaType<typename TypeTraits<UnderlyingType_>::Float>{}; }
  static constexpr bool HasUniqueObjectRepresentations() {
    return std::has_unique_object_representations_v<Type>;
  }
  static constexpr bool HasVirtualDestructor() { return std::has_virtual_destructor_v<Type>; }
  static constexpr auto Int() {
    return MetaType<berberis::Wrapping<typename TypeTraits<UnderlyingType_>::Int>>{};
  }
  template <typename... ArgTypes>
  static constexpr MetaType<std::invoke_result_t<Type, ArgTypes...>> InvokeResult(
      MetaType<ArgTypes>...) {
    return {};
  }
  static constexpr bool IsAbstract() { return std::is_abstract_v<Type>; }
  static constexpr bool IsAggregate() { return std::is_aggregate_v<Type>; }
  static constexpr bool IsArithmetic() { return std::is_arithmetic_v<Type>; }
  static constexpr bool IsArray() { return std::is_array_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsAssignable(MetaType<OtherType>) {
    return std::is_assignable_v<Type, OtherType>;
  }
  template <typename OtherType>
  static constexpr bool IsBaseOf() {
    return std::is_base_of_v<Type, OtherType>;
  }
  static constexpr bool IsBoundedArray() { return std::is_bounded_array_v<Type>; }
  static constexpr bool IsClass() { return std::is_class_v<Type>; }
  static constexpr bool IsCompound() { return std::is_compound_v<Type>; }
  static constexpr bool IsConst() { return std::is_const_v<Type>; }
  static constexpr bool IsConstructible() { return std::is_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsConvertible() {
    return std::is_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsCopyAssignable() { return std::is_copy_assignable_v<Type>; }
  static constexpr bool IsCopyConstructible() { return std::is_copy_constructible_v<Type>; }
  static constexpr bool IsDefaultConstructible() { return std::is_default_constructible_v<Type>; }
  static constexpr bool IsDestructible() { return std::is_destructible_v<Type>; }
  static constexpr bool IsEmpty() { return std::is_empty_v<Type>; }
  static constexpr bool IsEnum() { return std::is_enum_v<Type>; }
  static constexpr bool IsFinal() { return std::is_final_v<Type>; }
  static constexpr bool IsFloatingPoint() { return std::is_floating_point_v<Type>; }
  static constexpr bool IsFunction() { return std::is_function_v<Type>; }
  static constexpr bool IsFundamental() { return std::is_fundamental_v<Type>; }
  static constexpr bool IsIntegral() { return std::is_integral_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsInvocable(MetaType<ArgTypes>...) {
    return std::is_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsLvalueReference() { return std::is_lvalue_reference_v<Type>; }
  static constexpr bool IsMemberFunctionPointer() {
    return std::is_member_function_pointer_v<Type>;
  }
  static constexpr bool IsMemberObjectPointer() { return std::is_member_object_pointer_v<Type>; }
  static constexpr bool IsMemberPointer() { return std::is_member_pointer_v<Type>; }
  static constexpr bool IsMoveAssignable() { return std::is_move_assignable_v<Type>; }
  static constexpr bool IsMoveConstructible() { return std::is_move_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowAssignable(MetaType<OtherType>) {
    return std::is_nothrow_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowConstructible() { return std::is_nothrow_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowConvertible() {
    return std::is_nothrow_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowCopyAssignable() {
    return std::is_nothrow_copy_assignable_v<Type>;
  }
  static constexpr bool IsNothrowCopyConstructible() {
    return std::is_nothrow_copy_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDefaultConstructible() {
    return std::is_nothrow_default_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDestructible() { return std::is_nothrow_destructible_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsNothrowInvocable(MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsNothrowInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsNothrowMoveAssignable() {
    return std::is_nothrow_move_assignable_v<Type>;
  }
  static constexpr bool IsNothrowMoveConstructible() {
    return std::is_nothrow_move_constructible_v<Type>;
  }
  static constexpr bool IsNothrowSwappable() { return std::is_nothrow_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowSwappableWith() {
    return std::is_nothrow_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsNullPointer() { return std::is_null_pointer_v<Type>; }
  static constexpr bool IsObject() { return std::is_object_v<Type>; }
  static constexpr bool IsPointer() { return std::is_pointer_v<Type>; }
  static constexpr bool IsPolymorphic() { return std::is_polymorphic_v<Type>; }
  static constexpr bool IsReference() { return std::is_reference_v<Type>; }
  static constexpr bool IsRvalueReference() { return std::is_rvalue_reference_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSame() {
    return std::is_same_v<Type, OtherType>;
  }
  static constexpr bool IsScalar() { return std::is_scalar_v<Type>; }
  static constexpr bool IsSigned() { return std::is_signed_v<UnderlyingType_>; }
  static constexpr bool IsStandardLayout() { return std::is_standard_layout_v<Type>; }
  static constexpr bool IsSwappable() { return std::is_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSwappableWith(MetaType<OtherType>) {
    return std::is_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsTrivial() { return std::is_trivial_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsTriviallyAssignable(MetaType<OtherType>) {
    return std::is_trivially_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsTriviallyConstructible() {
    return std::is_trivially_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyCopyable() { return std::is_trivially_copyable_v<Type>; }
  static constexpr bool IsTriviallyCopyAssignable() {
    return std::is_trivially_copy_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyCopyConstructible() {
    return std::is_trivially_copy_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDefaultConstructible() {
    return std::is_trivially_default_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDestructible() { return std::is_trivially_destructible_v<Type>; }
  static constexpr bool IsTriviallyMoveAssignable() {
    return std::is_trivially_move_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyMoveConstructible() {
    return std::is_trivially_move_constructible_v<Type>;
  }
  static constexpr bool IsUnboundedArray() { return std::is_unbounded_array_v<Type>; }
  static constexpr bool IsUnion() { return std::is_union_v<Type>; }
  static constexpr bool IsUnsigned() { return std::is_unsigned_v<UnderlyingType_>; }
  static constexpr bool IsVoid() { return std::is_void_v<Type>; }
  static constexpr bool IsVolatile() { return std::is_volatile_v<Type>; }
  static constexpr auto MakeSigned() {
    return MetaType<berberis::Wrapping<std::make_signed_t<UnderlyingType_>>>{};
  }
  static constexpr auto MakeUnsigned() {
    return MetaType<berberis::Wrapping<std::make_unsigned_t<UnderlyingType_>>>{};
  }
  static constexpr auto Narrow() {
    return MetaType<berberis::Wrapping<typename TypeTraits<UnderlyingType_>::Narrow>>{};
  }
  static constexpr bool Negation() { return std::negation_v<Type>; }
  static constexpr size_t Rank() { return std::rank_v<Type>; }
  static constexpr auto Raw() { return MetaType<berberis::Raw<Type>>{}; }
  static constexpr MetaType<std::remove_all_extents_t<Type>> RemoveAllExtents() { return {}; }
  static constexpr MetaType<std::remove_const_t<Type>> RemoveConst() { return {}; }
  static constexpr MetaType<std::remove_cv_t<Type>> RemoveCv() { return {}; }
  static constexpr MetaType<std::remove_cvref_t<Type>> RemoveCvref() { return {}; }
  static constexpr MetaType<std::remove_extent_t<Type>> RemoveExtent() { return {}; }
  static constexpr MetaType<std::remove_pointer_t<Type>> RemovePointer() { return {}; }
  static constexpr MetaType<std::remove_reference_t<Type>> RemoveReference() { return {}; }
  static constexpr MetaType<std::remove_volatile_t<Type>> RemoveVolatile() { return {}; }
  static constexpr auto Saturating() { return MetaType<berberis::Saturating<UnderlyingType_>>{}; }
  static constexpr MetaType<std::type_identity_t<Type>> TypeIdentity() { return {}; }
  static constexpr MetaType<UnderlyingType_> UnderlyingType() { return {}; }
  static constexpr MetaType<std::unwrap_ref_decay_t<Type>> UnwrapRefDecay() { return {}; }
  static constexpr MetaType<std::unwrap_reference_t<Type>> UnwrapReference() { return {}; }
  template <typename... OtherTypes>
  MetaType<void> Void(MetaType<OtherTypes>...) {
    return {};
  }
  static constexpr auto Wrapped() {
    return MetaType<intrinsics::WrappedFloatType<UnderlyingType_>>{};
  }
  static constexpr auto Wrapping() { return MetaType<berberis::Wrapping<UnderlyingType_>>{}; }
  static constexpr auto Wide() {
    return MetaType<berberis::Wrapping<typename TypeTraits<UnderlyingType_>::Wide>>{};
  }
  static constexpr auto Widen() {
    return MetaType<berberis::Wrapping<typename TypeTraits<UnderlyingType_>::Wide>>{};
  }
};

template <typename UnderlyingType_>
class MetaType<intrinsics::WrappedFloatType<UnderlyingType_>> final {
 public:
  using Type = intrinsics::WrappedFloatType<UnderlyingType_>;

  static constexpr MetaType<std::add_const_t<Type>> AddConst() { return {}; }
  static constexpr MetaType<std::add_cv_t<Type>> AddCv() { return {}; }
  static constexpr MetaType<std::add_lvalue_reference_t<Type>> AddLvalueReference() { return {}; }
  static constexpr MetaType<std::add_pointer_t<Type>> AddPointer() { return {}; }
  static constexpr MetaType<std::add_rvalue_reference_t<Type>> AddRvalueReference() { return {}; }
  static constexpr MetaType<std::add_volatile_t<Type>> AddVolatile() { return {}; }
  static constexpr size_t AlignmentOf() { return std::alignment_of_v<Type>; }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_reference_t<Type, OtherTypes...>> CommonReference(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr MetaType<std::common_type_t<Type, OtherTypes...>> CommonType(
      MetaType<OtherTypes>...) {
    return {};
  }
  template <typename... OtherTypes>
  static constexpr bool Conjunction(MetaType<OtherTypes>...) {
    return std::conjunction_v<Type, OtherTypes...>;
  }
  static constexpr MetaType<std::decay_t<Type>> Decay() { return {}; }
  template <typename... OtherTypes>
  static constexpr bool Disjunction(MetaType<OtherTypes>...) {
    return std::disjunction_v<Type, OtherTypes...>;
  }
  static constexpr size_t Extent() { return std::extent_v<Type>; }
  static constexpr MetaType<Type> Float() { return {}; }
  static constexpr bool HasUniqueObjectRepresentations() {
    return std::has_unique_object_representations_v<Type>;
  }
  static constexpr bool HasVirtualDestructor() { return std::has_virtual_destructor_v<Type>; }
  static constexpr auto Int() {
    return MetaType<berberis::Raw<std::make_unsigned_t<typename TypeTraits<Type>::Int>>>{};
  }
  template <typename... ArgTypes>
  static constexpr MetaType<std::invoke_result_t<Type, ArgTypes...>> InvokeResult(
      MetaType<ArgTypes>...) {
    return {};
  }
  static constexpr bool IsAbstract() { return std::is_abstract_v<Type>; }
  static constexpr bool IsAggregate() { return std::is_aggregate_v<Type>; }
  static constexpr bool IsArithmetic() { return std::is_arithmetic_v<Type>; }
  static constexpr bool IsArray() { return std::is_array_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsAssignable(MetaType<OtherType>) {
    return std::is_assignable_v<Type, OtherType>;
  }
  template <typename OtherType>
  static constexpr bool IsBaseOf() {
    return std::is_base_of_v<Type, OtherType>;
  }
  static constexpr bool IsBoundedArray() { return std::is_bounded_array_v<Type>; }
  static constexpr bool IsClass() { return std::is_class_v<Type>; }
  static constexpr bool IsCompound() { return std::is_compound_v<Type>; }
  static constexpr bool IsConst() { return std::is_const_v<Type>; }
  static constexpr bool IsConstructible() { return std::is_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsConvertible() {
    return std::is_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsCopyAssignable() { return std::is_copy_assignable_v<Type>; }
  static constexpr bool IsCopyConstructible() { return std::is_copy_constructible_v<Type>; }
  static constexpr bool IsDefaultConstructible() { return std::is_default_constructible_v<Type>; }
  static constexpr bool IsDestructible() { return std::is_destructible_v<Type>; }
  static constexpr bool IsEmpty() { return std::is_empty_v<Type>; }
  static constexpr bool IsEnum() { return std::is_enum_v<Type>; }
  static constexpr bool IsFinal() { return std::is_final_v<Type>; }
  static constexpr bool IsFloatingPoint() { return std::is_floating_point_v<Type>; }
  static constexpr bool IsFunction() { return std::is_function_v<Type>; }
  static constexpr bool IsFundamental() { return std::is_fundamental_v<Type>; }
  static constexpr bool IsIntegral() { return std::is_integral_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsInvocable(MetaType<ArgTypes>...) {
    return std::is_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsLvalueReference() { return std::is_lvalue_reference_v<Type>; }
  static constexpr bool IsMemberFunctionPointer() {
    return std::is_member_function_pointer_v<Type>;
  }
  static constexpr bool IsMemberObjectPointer() { return std::is_member_object_pointer_v<Type>; }
  static constexpr bool IsMemberPointer() { return std::is_member_pointer_v<Type>; }
  static constexpr bool IsMoveAssignable() { return std::is_move_assignable_v<Type>; }
  static constexpr bool IsMoveConstructible() { return std::is_move_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowAssignable(MetaType<OtherType>) {
    return std::is_nothrow_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowConstructible() { return std::is_nothrow_constructible_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowConvertible() {
    return std::is_nothrow_convertible_v<Type, OtherType>;
  }
  static constexpr bool IsNothrowCopyAssignable() {
    return std::is_nothrow_copy_assignable_v<Type>;
  }
  static constexpr bool IsNothrowCopyConstructible() {
    return std::is_nothrow_copy_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDefaultConstructible() {
    return std::is_nothrow_default_constructible_v<Type>;
  }
  static constexpr bool IsNothrowDestructible() { return std::is_nothrow_destructible_v<Type>; }
  template <typename... ArgTypes>
  static constexpr bool IsNothrowInvocable(MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_v<Type, ArgTypes...>;
  }
  template <typename Fn, typename... ArgTypes>
  static constexpr bool IsNothrowInvocableR(MetaType<Fn>, MetaType<ArgTypes>...) {
    return std::is_nothrow_invocable_r_v<Type, Fn, ArgTypes...>;
  }
  static constexpr bool IsNothrowMoveAssignable() {
    return std::is_nothrow_move_assignable_v<Type>;
  }
  static constexpr bool IsNothrowMoveConstructible() {
    return std::is_nothrow_move_constructible_v<Type>;
  }
  static constexpr bool IsNothrowSwappable() { return std::is_nothrow_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsNothrowSwappableWith() {
    return std::is_nothrow_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsNullPointer() { return std::is_null_pointer_v<Type>; }
  static constexpr bool IsObject() { return std::is_object_v<Type>; }
  static constexpr bool IsPointer() { return std::is_pointer_v<Type>; }
  static constexpr bool IsPolymorphic() { return std::is_polymorphic_v<Type>; }
  static constexpr bool IsReference() { return std::is_reference_v<Type>; }
  static constexpr bool IsRvalueReference() { return std::is_rvalue_reference_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSame() {
    return std::is_same_v<Type, OtherType>;
  }
  static constexpr bool IsScalar() { return std::is_scalar_v<Type>; }
  static constexpr bool IsSigned() { return std::is_signed_v<UnderlyingType_>; }
  static constexpr bool IsStandardLayout() { return std::is_standard_layout_v<Type>; }
  static constexpr bool IsSwappable() { return std::is_swappable_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsSwappableWith(MetaType<OtherType>) {
    return std::is_swappable_with_v<Type, OtherType>;
  }
  static constexpr bool IsTrivial() { return std::is_trivial_v<Type>; }
  template <typename OtherType>
  static constexpr bool IsTriviallyAssignable(MetaType<OtherType>) {
    return std::is_trivially_assignable_v<Type, OtherType>;
  }
  static constexpr bool IsTriviallyConstructible() {
    return std::is_trivially_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyCopyable() { return std::is_trivially_copyable_v<Type>; }
  static constexpr bool IsTriviallyCopyAssignable() {
    return std::is_trivially_copy_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyCopyConstructible() {
    return std::is_trivially_copy_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDefaultConstructible() {
    return std::is_trivially_default_constructible_v<Type>;
  }
  static constexpr bool IsTriviallyDestructible() { return std::is_trivially_destructible_v<Type>; }
  static constexpr bool IsTriviallyMoveAssignable() {
    return std::is_trivially_move_assignable_v<Type>;
  }
  static constexpr bool IsTriviallyMoveConstructible() {
    return std::is_trivially_move_constructible_v<Type>;
  }
  static constexpr bool IsUnboundedArray() { return std::is_unbounded_array_v<Type>; }
  static constexpr bool IsUnion() { return std::is_union_v<Type>; }
  static constexpr bool IsUnsigned() { return std::is_unsigned_v<UnderlyingType_>; }
  static constexpr bool IsVoid() { return std::is_void_v<Type>; }
  static constexpr bool IsVolatile() { return std::is_volatile_v<Type>; }
  static constexpr auto MakeSigned() {
    return MetaType<berberis::Raw<std::make_signed_t<UnderlyingType_>>>{};
  }
  static constexpr auto MakeUnsigned() {
    return MetaType<berberis::Raw<std::make_unsigned_t<UnderlyingType_>>>{};
  }
  static constexpr auto Narrow() {
    return MetaType<typename TypeTraits<UnderlyingType_>::Narrow>{};
  }
  static constexpr bool Negation() { return std::negation_v<Type>; }
  static constexpr size_t Rank() { return std::rank_v<Type>; }
  static constexpr auto Raw() {
    return MetaType<berberis::Raw<std::make_unsigned_t<typename TypeTraits<Type>::Int>>>{};
  }
  static constexpr MetaType<std::remove_all_extents_t<Type>> RemoveAllExtents() { return {}; }
  static constexpr MetaType<std::remove_const_t<Type>> RemoveConst() { return {}; }
  static constexpr MetaType<std::remove_cv_t<Type>> RemoveCv() { return {}; }
  static constexpr MetaType<std::remove_cvref_t<Type>> RemoveCvref() { return {}; }
  static constexpr MetaType<std::remove_extent_t<Type>> RemoveExtent() { return {}; }
  static constexpr MetaType<std::remove_pointer_t<Type>> RemovePointer() { return {}; }
  static constexpr MetaType<std::remove_reference_t<Type>> RemoveReference() { return {}; }
  static constexpr MetaType<std::remove_volatile_t<Type>> RemoveVolatile() { return {}; }
  static constexpr auto Saturating() {
    return MetaType<berberis::Saturating<typename TypeTraits<Type>::Int>>{};
  }
  static constexpr MetaType<std::type_identity_t<Type>> TypeIdentity() { return {}; }
  static constexpr MetaType<UnderlyingType_> UnderlyingType() { return {}; }
  static constexpr MetaType<std::unwrap_ref_decay_t<Type>> UnwrapRefDecay() { return {}; }
  static constexpr MetaType<std::unwrap_reference_t<Type>> UnwrapReference() { return {}; }
  template <typename... OtherTypes>
  MetaType<void> Void(MetaType<OtherTypes>...) {
    return {};
  }
  static constexpr MetaType<Type> Wrapped() { return {}; }
  static constexpr auto Wrapping() {
    MetaType<berberis::Wrapping<typename TypeTraits<Type>::Int>>{};
  }
  static constexpr auto Wide() { return MetaType<typename TypeTraits<Type>::Wide>{}; }
  static constexpr auto Widen() { return MetaType<typename TypeTraits<Type>::Wide>{}; }
};

template <typename LeftType, typename RightType>
inline bool constexpr operator==(MetaType<LeftType>, MetaType<RightType>) {
  return std::is_same_v<LeftType, RightType>;
}

template <typename LeftType, typename RightType>
inline bool constexpr operator!=(MetaType<LeftType>, MetaType<RightType>) {
  return !std::is_same_v<LeftType, RightType>;
}

template <typename Type>
inline constexpr auto kType = MetaType<Type>{};

template <typename... Types>
inline constexpr auto kTypes = std::tuple<MetaType<Types>...>{};

class TypesToValues;
class ValuesToValues;

// TypesToTypes provides type-level metaprogramming utilities for std::tuple (with support for
// std::array if <array> include is present). It uses template specializations to perform operations
// on the types within a tuple.
class TypesToTypes {
 private:
  template <typename TupleType>
  class ConcatHelper;
  template <typename Type>
  class CountHelper;
  template <auto kLambda>
  class FilterHelper;
  template <typename Type, auto kLambda>
  class FlatMapHelper;
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
  template <typename ArrayType, typename TupleType>
  class ToArrayHelper;
  template <typename... Types>
  class ZipHelper;
  template <typename... Types>
  class ZipShortestHelper;

  class TupleTypesHelper {
   public:
    template <typename Type>
    constexpr auto operator()() const {
      return kTypes<MetaType<Type>>;
    };
  };

  friend class TypesToValues;
  friend class ValuesToValues;

 public:
  // Trying to make it private is a compile-time error on clang 13, 14, 15, 16, 17, and 18.
  // Fixed in clang 19.
  template <std::size_t... Is>
  static constexpr std::tuple<MetaValue<Is>...> IndexesHelper(std::index_sequence<Is...>);

  template <typename... TupleTypes>
  using Indexes =
      decltype(IndexesHelper(std::declval<std::make_index_sequence<std::min(
                                 {std::tuple_size_v<std::remove_reference_t<TupleTypes>>...})>>()));

  template <typename... TupleTypes>
  using Zip = typename ZipHelper<TupleTypes...>::Result;

  template <typename... TupleTypes>
  using ZipShortest = typename ZipShortestHelper<TupleTypes...>::Result;

  template <typename... TupleTypes>
  using Enumerate = Zip<Indexes<TupleTypes...>, TupleTypes...>;

  template <typename... TupleTypes>
  using EnumerateShortest = ZipShortest<Indexes<TupleTypes...>, TupleTypes...>;

  template <typename TupleType, auto kLambda>
  using Filter = typename FlatMapHelper<TupleType, FilterHelper<kLambda>{}>::Result;

  template <typename TupleType, auto kLambda>
  using CountIf = MetaValue<std::tuple_size_v<Filter<TupleType, kLambda>>>;

  template <typename TupleType, auto kLambda>
  using All =
      MetaValue<std::equal_to<std::size_t>{}(std::tuple_size_v<std::remove_reference_t<TupleType>>,
                                             CountIf<TupleType, kLambda>{})>;

  template <typename TupleType, auto kLambda>
  using Any = MetaValue<std::less<std::size_t>{}(std::size_t{0}, CountIf<TupleType, kLambda>{})>;

  template <typename... TupleTypes>
  using Concat =
      decltype(std::tuple_cat(std::declval<typename ConcatHelper<TupleTypes>::Result>()...));

  template <typename TupleType, typename Type>
  using Contains =
      MetaValue<std::less{}(0, std::tuple_size_v<Filter<TupleType, CountHelper<Type>{}>>)>;

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

  template <typename ArrayType, typename TupleType = void>
  using ToArray = typename ToArrayHelper<ArrayType, TupleType>::Result;

  template <typename TupleType>
  using TupleMetaTypes = typename FlatMapHelper<TupleType, TupleTypesHelper{}>::Result;

 private:
  template <typename... Types>
  class ConcatHelper<std::tuple<Types...>> {
   public:
    using Result = std::tuple<Types...>;
  };
  class ConcatHelperConst {
   public:
    template <typename TypeToProcess>
    constexpr auto operator()() const {
      return kTypes<const TypeToProcess>;
    }
  };
  template <typename TupleType>
  class ConcatHelper<const TupleType> {
   public:
    using Result = typename FlatMapHelper<TupleType, ConcatHelperConst{}>::Result;
  };
  class ConcatHelperConstLValueReference {
   public:
    template <typename TypeToProcess>
    constexpr auto operator()() const {
      return kTypes<const TypeToProcess&>;
    }
  };
  template <typename TupleType>
  class ConcatHelper<const TupleType&> {
   public:
    using Result = typename FlatMapHelper<TupleType, ConcatHelperConstLValueReference{}>::Result;
  };
  class ConcatHelperConstRValueReference {
   public:
    template <typename TypeToProcess>
    constexpr auto operator()() const {
      return kTypes<const TypeToProcess&&>;
    }
  };
  template <typename TupleType>
  class ConcatHelper<const TupleType&&> {
   public:
    using Result = typename FlatMapHelper<TupleType, ConcatHelperConstRValueReference{}>::Result;
  };
  class ConcatHelperLValueReference {
   public:
    template <typename TypeToProcess>
    constexpr auto operator()() const {
      return kTypes<TypeToProcess&>;
    }
  };
  template <typename TupleType>
  class ConcatHelper<TupleType&> {
   public:
    using Result = typename FlatMapHelper<TupleType, ConcatHelperLValueReference{}>::Result;
  };
  class ConcatHelperRValueReference {
   public:
    template <typename TypeToProcess>
    constexpr auto operator()() const {
      return kTypes<TypeToProcess&&>;
    }
  };
  template <typename TupleType>
  class ConcatHelper<TupleType&&> {
   public:
    using Result = typename FlatMapHelper<TupleType, ConcatHelperRValueReference{}>::Result;
  };
  template <typename TupleType>
  class ConcatHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = decltype(std::tuple_cat(std::declval<TupleType>()));
  };

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

  template <typename>
  class TupleOfTypesHelper;
  template <typename... Types>
  class TupleOfTypesHelper<std::tuple<MetaType<Types>...>> {
   public:
    using Tuple = std::tuple<Types...>;
  };
  template <typename... Types>
  class TupleOfTypesHelper<const std::tuple<MetaType<Types>...>> {
   public:
    using Tuple = std::tuple<Types...>;
  };
  template <typename... Types, auto kLambda>
  class FlatMapHelper<std::tuple<Types...>, kLambda> {
   public:
    using Result = Concat<
        typename TupleOfTypesHelper<decltype(kLambda.template operator()<Types>())>::Tuple...>;
  };
  template <typename TupleType, auto kLambda>
  class FlatMapHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result =
        typename FlatMapHelper<typename ConcatHelper<TupleType>::Result, kLambda>::Result;
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
    using Result = typename MapHelper<typename ConcatHelper<TupleType>::Result, kLambda>::Result;
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
      static_assert(kIdx <= std::tuple_size_v<std::remove_reference_t<TupleType>>);
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
      static_assert(kIdx <= std::tuple_size_v<std::remove_reference_t<TupleType>>);
      constexpr bool kAccepted = kIdx < kCount;
      if constexpr (kAccepted) {
        return kTypes<std::tuple_element_t<1, EnumeratedType>>;
      } else {
        return kTypes<>;
      }
    }
  };

  template <typename ArrayType>
  class ToConstArrayHelper;

  template <typename Type, std::size_t kSize>
  class ToConstArrayHelper<std::array<Type, kSize>> {
   public:
    using Result = std::array<const Type, kSize>;
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
    static_assert(((sizeof...(TypesLeft) == std::tuple_size_v<std::remove_reference_t<Tuples>>) &&
                   ...));
    using Result =
        typename ZipHelperWithIndexes<Indexes<std::tuple<TypesLeft...>>,
                                      std::tuple<std::tuple<TypesLeft...>,
                                                 std::tuple<TypesRight...>,
                                                 typename ConcatHelper<Tuples>::Result...>>::Result;
  };
  template <typename... Types>
  class ZipHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = typename ZipHelper<typename ConcatHelper<Types>::Result...>::Result;
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
                                   {sizeof...(TypesLeft),
                                    std::tuple_size_v<std::remove_reference_t<Tuples>>...})>>())),
        std::tuple<std::tuple<TypesLeft...>, typename ConcatHelper<Tuples>::Result...>>::Result;
  };
  template <typename... Types>
  class ZipShortestHelper {
   public:
    // Note: Concat here ensures that we would use the specialization above and wouldn't cause
    // endless recursion here.
    using Result = typename ZipShortestHelper<typename ConcatHelper<Types>::Result...>::Result;
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

template <typename TupleType>
inline constexpr auto kTupleMetaTypes = TypesToTypes::TupleMetaTypes<TupleType>{};

// TypesToValues provides value-level functional-style operations on std::tuple types. It also
// supports std::array if <array> header is included. These methods often take a lambda to apply to
// each element of the tuple.
class TypesToValues {
 public:
  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr bool All(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return AllHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr bool Any(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return AnyHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename Type>
  static constexpr std::size_t Contains() {
    return TypesToTypes::Contains<TupleType, Type>{};
  }

  template <typename TupleType, typename Type>
  static constexpr std::size_t Count() {
    return TypesToTypes::Count<TupleType, Type>{};
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr std::size_t CountIf(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr decltype(auto) FlatMap(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return FlatMapHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr void ForEach(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    ForEachHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda>
  static constexpr decltype(auto) Map(Lambda&& lambda, ExtraLambdaArgTypes&&... extra_types) {
    return MapHelper<TupleType, ExtraLambdaArgTypes...>(
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
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
    return (
        static_cast<bool>(
            lambda.template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) &&
        ... && true);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, std::size_t... Is, typename Lambda>
  static constexpr bool AnyHelper(Lambda&& lambda,
                                  std::index_sequence<Is...>,
                                  ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) ||
        ... || false);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, std::size_t... Is, typename Lambda>
  static constexpr std::size_t CountIfHelper(Lambda&& lambda,
                                             std::index_sequence<Is...>,
                                             ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) +
        ... + std::size_t{0});
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda, std::size_t... Is>
  static constexpr decltype(auto) FlatMapHelper(Lambda&& lambda,
                                                std::index_sequence<Is...>,
                                                ExtraLambdaArgTypes&&... extra_types) {
    return std::tuple_cat(
        lambda.template operator()<
            std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
            std::forward<ExtraLambdaArgTypes>(extra_types)...)...);
  }

  template <typename TupleType, typename... ExtraLambdaArgTypes, typename Lambda, std::size_t... Is>
  static constexpr void ForEachHelper(Lambda&& lambda,
                                      std::index_sequence<Is...>,
                                      ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template
     operator()<std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
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
      lambda.template
      operator()<std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename TupleType, typename Type>
  static constexpr bool Contains(TupleType&& tuple, Type&& element) {
    return Any(std::forward<TupleType>(tuple),
               [&element]<typename TypeToCheck>(TypeToCheck&& element_to_check) {
                 if constexpr (kComparable<Type, TypeToCheck>) {
                   return element == element_to_check;
                 } else {
                   return false;
                 }
               });
  }

  template <typename Type, typename TupleType>
  static constexpr bool Contains(TupleType&&) {
    return TypesToTypes::Contains<TupleType, Type>{};
  }

  template <typename TupleType, typename Type>
  static constexpr std::size_t Count(TupleType&& tuple, Type&& element) {
    return CountIf(std::forward<TupleType>(tuple),
                   [&element]<typename TypeToCheck>(TypeToCheck&& element_to_check) {
                     if constexpr (kComparable<Type, TypeToCheck>) {
                       return element == element_to_check;
                     } else {
                       return false;
                     }
                   });
  }

  template <typename Type, typename TupleType>
  static constexpr std::size_t Count(TupleType&&) {
    return TypesToTypes::Count<TupleType, Type>{};
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr std::size_t CountIf(TupleType&& tuple,
                                       Lambda&& lambda,
                                       ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<ExtraLambdaArgTypes...>(
        std::forward<TupleType>(tuple),
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr std::size_t CountIfWithTemporary(TupleType&& tuple,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    TemporaryType tmp{};
    return CountIfHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<TupleType>(tuple),
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }
  template <typename TemporaryType,
            typename... ExtraLambdaArgTypes,
            typename TupleType,
            typename Lambda>
  static constexpr std::size_t CountIfWithTemporary(TupleType&& tuple,
                                                    TemporaryType tmp,
                                                    Lambda&& lambda,
                                                    ExtraLambdaArgTypes&&... extra_types) {
    return CountIfHelper<TemporaryType&, ExtraLambdaArgTypes...>(
        std::forward<TupleType>(tuple),
        std::forward<Lambda>(lambda),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleType>>>{},
        tmp,
        std::forward<ExtraLambdaArgTypes>(extra_types)...);
  }

  template <typename... TupleTypes>
  static constexpr TypesToTypes::Enumerate<TupleTypes...> Enumerate(TupleTypes&&... tuples) {
    return Zip(TypesToTypes::Indexes<TupleTypes...>{}, std::forward<TupleTypes>(tuples)...);
  }

  template <typename... TupleTypes>
  static constexpr TypesToTypes::EnumerateShortest<TupleTypes...> EnumerateShortest(
      TupleTypes&&... tuples) {
    return ZipShortest(TypesToTypes::Indexes<TupleTypes...>{}, std::forward<TupleTypes>(tuples)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, typename Lambda>
  static constexpr decltype(auto) Filter(TupleType&& tuple,
                                         Lambda&& lambda,
                                         ExtraLambdaArgTypes&&... extra_types) {
    return FilterHelper<ExtraLambdaArgTypes...>(
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
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
        std::forward<TupleType>(tuple),
        []<typename TypeToCheck>(TypeToCheck&& elem) -> decltype(auto) {
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
        std::forward<TupleType>(tuple),
        []<typename TypeToCheck>(TypeToCheck&& elem) -> decltype(auto) {
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
        []<typename Type>(Type&& elem) -> decltype(auto) {
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
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) SkipWhile(TupleType&& tuple) {
    constexpr std::size_t kCount =
        TypesToTypes::TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                                     kLambda,
                                     extra_lambda_values...>::Produce();
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) SkipWhile(TupleType&& tuple,
                                            MetaValue<kLambda>,
                                            MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount =
        TypesToTypes::TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                                     kLambda,
                                     extra_lambda_values...>::Produce();
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename InitialValueType,
            auto kLambda,
            auto... extra_lambda_values,
            typename TupleType>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       InitialValueType{},
                       kLambda,
                       extra_lambda_values...>();
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename InitialValueType,
            typename TupleType,
            auto kLambda,
            auto... extra_lambda_values>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       InitialValueType{},
                       kLambda,
                       extra_lambda_values...>();
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto initial_value, auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       initial_value,
                       kLambda,
                       extra_lambda_values...>();
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto initial_value, typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) SkipWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<initial_value>,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       initial_value,
                       kLambda,
                       extra_lambda_values...>();
    return Skip<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto kCount, typename TupleType>
  static constexpr decltype(auto) Take(TupleType&& tuple) {
    return FlatMapHelper(
        Enumerate(std::forward<TupleType>(tuple)),
        []<typename Type>(Type&& elem) -> decltype(auto) {
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
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) TakeWhile(TupleType&& tuple) {
    constexpr std::size_t kCount =
        TypesToTypes::TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                                     kLambda,
                                     extra_lambda_values...>::Produce();
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) TakeWhile(TupleType&& tuple,
                                            MetaValue<kLambda>,
                                            MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount =
        TypesToTypes::TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                                     kLambda,
                                     extra_lambda_values...>::Produce();
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename InitialValueType,
            auto kLambda,
            auto... extra_lambda_values,
            typename TupleType>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       InitialValueType{},
                       kLambda,
                       extra_lambda_values...>();
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename InitialValueType,
            typename TupleType,
            auto kLambda,
            auto... extra_lambda_values>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       InitialValueType{},
                       kLambda,
                       extra_lambda_values...>();
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto initial_value, auto kLambda, auto... extra_lambda_values, typename TupleType>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       initial_value,
                       kLambda,
                       extra_lambda_values...>();
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <auto initial_value, typename TupleType, auto kLambda, auto... extra_lambda_values>
  static constexpr decltype(auto) TakeWhileWithTemporary(TupleType&& tuple,
                                                         MetaValue<initial_value>,
                                                         MetaValue<kLambda>,
                                                         MetaValue<extra_lambda_values>...) {
    constexpr std::size_t kCount =
        TakeSkipHelper<typename TypesToTypes::ConcatHelper<TupleType>::Result,
                       initial_value,
                       kLambda,
                       extra_lambda_values...>();
    return Take<kCount, TupleType>(std::forward<TupleType>(tuple));
  }

  template <typename TupleType>
  static constexpr auto ToArray(TupleType&& tuple) {
    return std::apply(
        []<typename... Arg>(Arg&&... arg) { return std::array{std::forward<Arg>(arg)...}; },
        std::forward<TupleType>(tuple));
  }

  template <typename ArrayElement, typename TupleType>
  static constexpr auto ToArray(TupleType&& tuple) {
    return std::apply(
        []<typename... Arg>(Arg&&... arg) {
          return std::array<ArrayElement, sizeof...(Arg)>{std::forward<Arg>(arg)...};
        },
        std::forward<TupleType>(tuple));
  }

  template <typename... TupleTypes>
  static constexpr TypesToTypes::Zip<TupleTypes...> Zip(TupleTypes&&... tuples) {
    return ZipHelper<TypesToTypes::Zip<TupleTypes...>>(
        std::tuple<TupleTypes...>{std::forward<TupleTypes>(tuples)...},
        std::make_index_sequence<std::min(
            {std::tuple_size_v<std::remove_reference_t<TupleTypes>>...})>{},
        std::make_index_sequence<sizeof...(TupleTypes)>{});
  }
  template <typename... TupleTypes>
  static constexpr TypesToTypes::ZipShortest<TupleTypes...> ZipShortest(TupleTypes&&... tuples) {
    return ZipHelper<TypesToTypes::ZipShortest<TupleTypes...>>(
        std::tuple<TupleTypes...>{std::forward<TupleTypes>(tuples)...},
        std::make_index_sequence<std::min(
            {std::tuple_size_v<std::remove_reference_t<TupleTypes>>...})>{},
        std::make_index_sequence<sizeof...(TupleTypes)>{});
  }

 private:
  template <typename LeftType, typename RightType, typename = void>
  static constexpr bool kComparable = false;

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr bool AllHelper(TupleType&& tuple,
                                  Lambda&& lambda,
                                  std::index_sequence<Is...>,
                                  ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
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
            lambda.template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
                std::get<Is>(std::forward<TupleType>(tuple)),
                std::forward<ExtraLambdaArgTypes>(extra_types)...)) ||
        ... || false);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr std::size_t CountIfHelper(TupleType&& tuple,
                                             Lambda&& lambda,
                                             std::index_sequence<Is...>,
                                             ExtraLambdaArgTypes&&... extra_types) {
    return (
        static_cast<bool>(
            lambda.template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
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
            ElementType&& element, ExtraLambdaArgTypes&&... extra_types) {
          auto lambda_result = lambda.template operator()<ElementType>(
              std::forward<ExtraLambdaArgTypes>(extra_types)...);
          constexpr bool kCheckResult = decltype(lambda_result){};
          if constexpr (kCheckResult) {
            return std::tuple<ElementType>{element};
          } else {
            return std::tuple<>{};
          }
        }
            .template operator()<
                std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
                std::get<Is>(std::forward<TupleType>(tuple)),
                std::forward<ExtraLambdaArgTypes>(extra_types)...)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr decltype(auto) FlatMapHelper(TupleType&& tuple,
                                                Lambda&& lambda,
                                                std::index_sequence<Is...>,
                                                ExtraLambdaArgTypes&&... extra_types) {
    return std::tuple_cat(
        lambda.template operator()<
            std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
            std::get<Is>(std::forward<TupleType>(tuple)),
            std::forward<ExtraLambdaArgTypes>(extra_types)...)...);
  }

  template <typename... ExtraLambdaArgTypes, typename TupleType, std::size_t... Is, typename Lambda>
  static constexpr void ForEachHelper(TupleType&& tuple,
                                      Lambda&& lambda,
                                      std::index_sequence<Is...>,
                                      ExtraLambdaArgTypes&&... extra_types) {
    (lambda.template
     operator()<std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
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
    return std::tuple<
        decltype(lambda.template operator()<
                 std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
            std::get<Is>(std::forward<TupleType>(tuple)),
            std::forward<ExtraLambdaArgTypes>(extra_types)...))...> {
      lambda.template
      operator()<std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleType>::Result>>(
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
    return ZipType{std::pair<
        std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleTypeLeft>::Result>,
        std::tuple_element_t<Is, typename TypesToTypes::ConcatHelper<TupleTypeRight>::Result>>{
        std::get<Is>(std::get<0>(tuples)), std::get<Is>(std::get<1>(tuples))}...};
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

template <typename LeftType, typename RightType>
constexpr bool ValuesToValues::kComparable<
    LeftType,
    RightType,
    std::enable_if_t<sizeof(std::declval<LeftType>() == std::declval<RightType>()) >= 0>> = true;

template <typename TupleType>
class TypesToTypes::ToArrayHelper<const TupleType, void> {
 public:
  using Result = typename ToConstArrayHelper<decltype(ValuesToValues::ToArray(
      std::declval<TupleType>()))>::Result;
};
template <typename TupleType>
class TypesToTypes::ToArrayHelper<const TupleType&, void> {
 public:
  using Result = typename ToConstArrayHelper<decltype(ValuesToValues::ToArray(
      std::declval<TupleType>()))>::Result;
};
template <typename TupleType>
class TypesToTypes::ToArrayHelper<const TupleType&&, void> {
 public:
  using Result = typename ToConstArrayHelper<decltype(ValuesToValues::ToArray(
      std::declval<TupleType>()))>::Result;
};
template <typename TupleType>
class TypesToTypes::ToArrayHelper<TupleType, void> {
 public:
  using Result = decltype(ValuesToValues::ToArray(std::declval<TupleType>()));
};
template <typename ArrayType, typename TupleType>
class TypesToTypes::ToArrayHelper {
 public:
  using Result = decltype(ValuesToValues::ToArray<ArrayType>(std::declval<TupleType>()));
};

}  // namespace berberis

#endif  // BERBERIS_BASE_TUPLE_PROCESSING_H_
