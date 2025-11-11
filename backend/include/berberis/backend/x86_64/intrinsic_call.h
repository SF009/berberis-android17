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

#ifndef BERBERIS_BACKEND_X86_64_INTRINSIC_CALL_H_
#define BERBERIS_BACKEND_X86_64_INTRINSIC_CALL_H_

#include <array>
#include <cstdint>
#include <tuple>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/stringprintf.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/intrinsics/simd_register.h"

namespace berberis::x86_64 {

template <auto function>
class IntrinsicCall;

template <typename IntrinsicResTuple,
          typename... IntrinsicParamTypes,
          IntrinsicResTuple (*function)(IntrinsicParamTypes...)>
class IntrinsicCall<function> {
 private:
  template <typename ArgumentsTuple>
  using CleanTypes = TypesToTypes::FlatMap<ArgumentsTuple, []<typename RawType> {
    using Type = std::remove_cvref_t<RawType>;
    if constexpr (std::is_integral_v<Type> || std::is_same_v<Type, intrinsics::Float16> ||
                  std::is_same_v<Type, intrinsics::Float32> ||
                  std::is_same_v<Type, intrinsics::Float64>) {
      return kTypes<Type>;
    } else if constexpr (std::is_same_v<Type, SIMD128Register> || std::is_same_v<Type, __m128>) {
      return kTypes<__m128>;
    } else {
      static_assert(kDependentTypeFalse<Type>);
    }
  }>;

 public:
  using CleanResType = CleanTypes<IntrinsicResTuple>;
  using CleanParamTypes = CleanTypes<std::tuple<IntrinsicParamTypes...>>;

 private:
  struct ElementInfo {
    // Register class for elements passed in register. Note that ABI permits coalescion of few
    // elements of tuple in one register. Note: nullptr means everything is passed in memory.
    const MachineRegClass* register_class;
    // Position in register or memory from the beginning of the register (or struct if memory is
    // used).
    std::size_t element_offset;
  };
  static constexpr std::array<ElementInfo,
                              std::tuple_size_v<CleanResType> +
                                  ((std::is_same_v<CleanResType, std::tuple<__int128_t>> ||
                                    std::is_same_v<CleanResType, std::tuple<__uint128_t>>)
                                       ? 1
                                       : 0)>
  GenResultsElements();

 public:
  // Note: we only need kResultsElements mostly to calculate ReturnRegisters but frontend can use it
  // to unpack values that are returned in one register.
  static constexpr auto kResultsElements = GenResultsElements();
  // Note: we declare RAX as an output operand, but it likely won't be used by frontend, because
  // it's usually an address of a frontend provided area on stack.
  using ReturnRegisters =
      TypesToTypes::FlatMap<ValuesToTypes::MetaValues<&kResultsElements>,
                            []<typename ResultElementInfo> {
                              constexpr ElementInfo kResultElementInfo = ResultElementInfo::kValue;
                              // Ignore elements except for the first one. Each 8-byte bundle have
                              // such element because we are dealing with tuple and not random
                              // structs with arbitrary bitfields that may include padding.
                              if constexpr (kResultElementInfo.element_offset != 0) {
                                return kTypes<>;
                              } else if constexpr (kResultElementInfo.register_class == nullptr ||
                                                   kResultElementInfo.register_class ==
                                                       &kRegisterClass<device_arch_info::RAX>) {
                                return kTypes<device_arch_info::RAX>;
                              } else if constexpr (kResultElementInfo.register_class ==
                                                   &kRegisterClass<device_arch_info::RDX>) {
                                return kTypes<device_arch_info::RDX>;
                              } else if constexpr (kResultElementInfo.register_class ==
                                                   &kRegisterClass<device_arch_info::XMM0>) {
                                return kTypes<device_arch_info::XMM0>;
                              } else if constexpr (kResultElementInfo.register_class ==
                                                   &kRegisterClass<device_arch_info::XMM1>) {
                                return kTypes<device_arch_info::XMM1>;
                              } else {
                                static_assert(
                                    kDependentValueFalse<kResultElementInfo.register_class>);
                              }
                            }>;
};

// Short description of the algorithm used and why it's correct.
//
// psABI (on https://gitlab.com/x86-psABIs/x86-64-ABI ) is the official definition,
// but it can be explained simpler.
//
// First, all “large” types have special handling that is ONLY in effect when they are handled
// separately without any other types in the aggregate. That's because of classification of
// aggregate step 5c: “If the size of the aggregate exceeds two eightbytes and the first eightbyte
// isn’t SSE or any other eightbyte isn’t SSEUP, the whole argument is passed in memory”.
//
// The only eightbytes that may be of type SSEUP are high bytes of types __m128, __m256, or __m512.
// Both having two such types in an aggregate or having something else there would violate that rule
// and would trigger “the whole argument is passed in memory” condition.
//
// This includes long double: it's the only type that has X87 and X87UP parts and that type is
// already “two eightbytes”, addition of anything else to it would trigger that rules. The same
// happens with COMPLEX_X87 type: said type is 32 bytes long by itself and if not for special
// exception that allows it to be returned from function in st0 and st1 it would have been passed in
// memory. It IS passed in memory when used as an argument of function.
//
// This leaves us with four classes: NO_CLASS (padding bytes), MEMORY (unaligned fields), INTEGER
// (most remaining types) and SSE. Unaligned fields immediately mean that everything is passed in
// memory, padding bytes are never passed at all (but may trigger the rule about size being larger
// than two eightbytes) and this leaves us with just two classes: INTEGER and SSE. If eightbyte
// chunk contains anything of type INTEGER then it's passed in RAX or RDX, if everything in there is
// either padding or SSE then it's returned in XMM0 or XMM1.
//
// Note: intrinsics only support limited number of simple types in a tuple which means unaligned
// fields are impossible. Also note that tuple works like an aggregate on clang with libc++. If
// libstdc++ is used (which is default on Linux) then tuple is “non-trivial for the purpose of
// calls” and is passed on stack.
//
// Currently we don't support long double and thus don't have special case for X87/X87UP classes,
// but, as noted above, we don't need to add any special rules for them: if they would ever be
// supported then they would be only handled separately when passed as one long double or complex
// long double—which means they have to be treated specially only when aggregate contains them
// without anything else.
template <typename IntrinsicResTuple,
          typename... IntrinsicParamTypes,
          IntrinsicResTuple (*function)(IntrinsicParamTypes...)>
constexpr auto IntrinsicCall<function>::GenResultsElements()
    -> std::array<ElementInfo,
                  std::tuple_size_v<CleanResType> +
                      ((std::is_same_v<CleanResType, std::tuple<__int128_t>> ||
                        std::is_same_v<CleanResType, std::tuple<__uint128_t>>)
                           ? 1
                           : 0)> {
  // If we don't have elements to process then we are done.
  if constexpr (std::tuple_size_v<CleanResType> == 0) {
    return std::array<ElementInfo, 0>{};
    // __int128, __uint128_t and __m128 are the only types that may span 8byte chunk, and they can
    // be only passed in registers if they are used alone.
  } else if constexpr (std::is_same_v<CleanResType, std::tuple<__int128>> ||
                       std::is_same_v<CleanResType, std::tuple<__uint128_t>>) {
    return std::array{
        ElementInfo{.register_class = &kRegisterClass<device_arch_info::RAX>, .element_offset = 0},
        ElementInfo{.register_class = &kRegisterClass<device_arch_info::RDX>, .element_offset = 0}};
  } else if constexpr (std::is_same_v<CleanResType, std::tuple<__m128>>) {
    return std::array{ElementInfo{.register_class = &kRegisterClass<device_arch_info::XMM0>,
                                  .element_offset = 0}};
  } else {
    std::array<ElementInfo, std::tuple_size_v<CleanResType>> result =
        ToArray(TypesToValues::MapWithTemporary<CleanResType, /* offset = */ std::size_t>(
            []<typename Type>(std::size_t& offset) {
              static_assert(IsPowerOf2(sizeof(Type)));
              std::size_t current_offset = AlignUp<sizeof(Type)>(offset);
              offset = current_offset + sizeof(Type);
              if constexpr (std::is_integral_v<Type>) {
                return ElementInfo{.register_class = &kGeneralReg64,
                                   .element_offset = current_offset};
              } else {
                return ElementInfo{.register_class = &kXmmReg, .element_offset = current_offset};
              }
            }));
    // If struct size is larger than 16 bytes then it's returned in memory.
    if constexpr (sizeof(CleanResType) > 16) {
      for (auto& element : result) {
        element.register_class = nullptr;
      }
      return result;
    }
    constexpr auto kIntRegisters =
        std::array{&kRegisterClass<device_arch_info::RAX>, &kRegisterClass<device_arch_info::RDX>};
    constexpr auto kXMMRegisters = std::array{&kRegisterClass<device_arch_info::XMM0>,
                                              &kRegisterClass<device_arch_info::XMM1>};
    std::size_t int_register = 0, xmm_register = 0;
    for (std::size_t offset : {0, 8}) {
      bool use_integer_register = false;
      for (auto& element : result) {
        if (element.element_offset >= offset && element.element_offset < offset + 8 &&
            element.register_class == &kGeneralReg64) {
          use_integer_register = true;
          break;
        }
      }
      const MachineRegClass* register_class;
      if (use_integer_register) {
        register_class = kIntRegisters[int_register++];
      } else {
        register_class = kXMMRegisters[xmm_register++];
      }
      for (auto& element : result) {
        if (element.element_offset >= offset && element.element_offset < offset + 8) {
          element.element_offset = element.element_offset % 8;
          element.register_class = register_class;
        }
      }
    }
    return result;
  }
}

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_INTRINSIC_CALL_H_
