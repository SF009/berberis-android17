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

template <typename IntrinsicRetTuple,
          typename... IntrinsicParamTypes,
          IntrinsicRetTuple (*function)(IntrinsicParamTypes...)>
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
  using CleanRetType = CleanTypes<IntrinsicRetTuple>;
  using CleanParamTypes = CleanTypes<std::tuple<IntrinsicParamTypes...>>;

 private:
  using AllGpArgumentetersRegisters = std::tuple<device_arch_info::RDI,
                                                 device_arch_info::RSI,
                                                 device_arch_info::RDX,
                                                 device_arch_info::RCX,
                                                 device_arch_info::R8,
                                                 device_arch_info::R9>;
  using AllXmmArgumentetersRegisters = std::tuple<device_arch_info::XMM0,
                                                  device_arch_info::XMM1,
                                                  device_arch_info::XMM2,
                                                  device_arch_info::XMM3,
                                                  device_arch_info::XMM4,
                                                  device_arch_info::XMM5,
                                                  device_arch_info::XMM6,
                                                  device_arch_info::XMM7>;
  using AllArgumentetersRegisters =
      TypesToTypes::Concat<AllGpArgumentetersRegisters, AllXmmArgumentetersRegisters>;
  using AllClobberRegisters = std::tuple<device_arch_info::RAX,
                                         device_arch_info::RDI,
                                         device_arch_info::RSI,
                                         device_arch_info::RDX,
                                         device_arch_info::RCX,
                                         device_arch_info::R8,
                                         device_arch_info::R9,
                                         device_arch_info::R10,
                                         device_arch_info::R11,
                                         device_arch_info::XMM0,
                                         device_arch_info::XMM1,
                                         device_arch_info::XMM2,
                                         device_arch_info::XMM3,
                                         device_arch_info::XMM4,
                                         device_arch_info::XMM5,
                                         device_arch_info::XMM6,
                                         device_arch_info::XMM7,
                                         device_arch_info::XMM8,
                                         device_arch_info::XMM9,
                                         device_arch_info::XMM10,
                                         device_arch_info::XMM11,
                                         device_arch_info::XMM12,
                                         device_arch_info::XMM13,
                                         device_arch_info::XMM14,
                                         device_arch_info::XMM15,
                                         device_arch_info::FLAGS>;

  // Helper convertor to go from kRegisterClass variable back to Register's class.
  template <auto kRegisterClass_, typename RegistersClassesList_>
  using kRegisterClassToClassTuple =
      TypesToTypes::Filter<RegistersClassesList_, []<typename RegisterClass>() {
        return kRegisterClass_ == &kRegisterClass<RegisterClass>;
      }>;

  struct ResultsElementInfo {
    // Register class for elements passed in register. Note that ABI permits coalescion of few
    // elements of tuple in one register. Note: nullptr means everything is passed in memory.
    const MachineRegClass* register_class;
    // Position in register or memory from the beginning of the register (or struct if memory is
    // used).
    std::size_t element_offset;
  };
  static constexpr std::array<ResultsElementInfo,
                              std::tuple_size_v<CleanRetType> +
                                  ((std::is_same_v<CleanRetType, std::tuple<__int128_t>> ||
                                    std::is_same_v<CleanRetType, std::tuple<__uint128_t>>)
                                       ? 1
                                       : 0)>
  GenResultsElements();
  // Note: we don't support aggregates, which means most parameters go into one register.
  // We also don't support stack-passed parameters for now. But __int128_t uses two registers.
  enum ArgumentElementType { kInteger, kXMM, kInt128 };
  struct ArgumentsElementInfo {
    ArgumentElementType param_type;
    std::array<const MachineRegClass*, 2> register_classes;
  };
  static constexpr std::array<ArgumentsElementInfo, std::tuple_size_v<CleanParamTypes>>
  GenArgumentElements();

 public:
  // Note: we need kResultsElements mostly to calculate ResultRegisters but frontend can use it to
  // unpack values that are returned in one register.
  static constexpr auto kResultsElements = GenResultsElements();
  // Note: we declare RAX as an output operand, but it likely won't be used by frontend, because
  // it's usually an address of a frontend provided area on stack.
  using ResultRegisters = TypesToTypes::
      FlatMap<ValuesToTypes::MetaValues<kResultsElements>, []<typename ResultElementInfo> {
        constexpr ResultsElementInfo kResultElementInfo = ResultElementInfo::kValue;
        // Ignore elements except for the first one. Each 8-byte bundle have
        // such element because we are dealing with tuple and not random
        // structs with arbitrary bitfields that may include padding.
        if constexpr (kResultElementInfo.element_offset != 0) {
          return kTypes<>;
        } else if constexpr (kResultElementInfo.register_class == nullptr) {
          return kTypes<device_arch_info::RAX>;
        } else {
          using RegisterClassTuple =
              kRegisterClassToClassTuple<kResultElementInfo.register_class, AllClobberRegisters>;
          static_assert(std::tuple_size_v<RegisterClassTuple> == 1);
          return kTypes<std::tuple_element_t<0, RegisterClassTuple>>;
        }
      }>;
  // Note: we need kArgumentElements mostly to calculate ArgumentRegisters but frontend can use it
  // to unpack values that are supposed to be passed in register.
  static constexpr auto kArgumentElements = GenArgumentElements();
  // Clobber registers are registers that a function is allowed to modify. According to the x86-64
  // psABI, these are the caller-saved registers. The registers used to return values are also
  // modified, but their final state is the function's result, so they are not typically listed as
  // "clobbered" in the sense of losing their prior value. Here, we define `ClobberRegisters` as all
  // registers that *could* be modified by the intrinsic call, excluding those that are used to
  // return the function's result. This is done to simplify work for the frontend: frontend doesn't
  // need to dig through list of registers modified by CALL to find the results, results always come
  // first in the list of registers, clobbered registers come after these.
  using ClobberRegisters = TypesToTypes::Filter<AllClobberRegisters, []<typename ClobberedClass>() {
    return TypesToValues::All<ResultRegisters>(
        []<typename ResultedClass>() { return !std::is_same_v<ClobberedClass, ResultedClass>; });
  }>;
  using ArgumentRegisters = TypesToTypes::
      FlatMap<ValuesToTypes::MetaValues<kArgumentElements>, []<typename ArgumentElementInfo> {
        constexpr ArgumentsElementInfo kArgumentElementInfo = ArgumentElementInfo::kValue;
        if constexpr (kArgumentElementInfo.param_type == kInt128) {
          using RegisterClassTuple1 =
              kRegisterClassToClassTuple<kArgumentElementInfo.register_classes[0],
                                         AllArgumentetersRegisters>;
          using RegisterClassTuple2 =
              kRegisterClassToClassTuple<kArgumentElementInfo.register_classes[1],
                                         AllArgumentetersRegisters>;
          static_assert(std::tuple_size_v<RegisterClassTuple1> == 1);
          static_assert(std::tuple_size_v<RegisterClassTuple2> == 1);
          return kTypes<std::tuple_element_t<0, RegisterClassTuple1>,
                        std::tuple_element_t<0, RegisterClassTuple2>>;
        } else {
          using RegisterClassTuple =
              kRegisterClassToClassTuple<kArgumentElementInfo.register_classes[0],
                                         AllArgumentetersRegisters>;
          static_assert(std::tuple_size_v<RegisterClassTuple> == 1);
          return kTypes<std::tuple_element_t<0, RegisterClassTuple>>;
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
template <typename IntrinsicRetTuple,
          typename... IntrinsicParamTypes,
          IntrinsicRetTuple (*function)(IntrinsicParamTypes...)>
constexpr auto IntrinsicCall<function>::GenResultsElements()
    -> std::array<ResultsElementInfo,
                  std::tuple_size_v<CleanRetType> +
                      ((std::is_same_v<CleanRetType, std::tuple<__int128_t>> ||
                        std::is_same_v<CleanRetType, std::tuple<__uint128_t>>)
                           ? 1
                           : 0)> {
  // If we don't have elements to process then we are done.
  if constexpr (std::tuple_size_v<CleanRetType> == 0) {
    return std::array<ResultsElementInfo, 0>{};
    // __int128, __uint128_t and __m128 are the only types that may span 8byte chunk, and they can
    // be only passed in registers if they are used alone.
  } else if constexpr (std::is_same_v<CleanRetType, std::tuple<__int128>> ||
                       std::is_same_v<CleanRetType, std::tuple<__uint128_t>>) {
    return std::array{ResultsElementInfo{.register_class = &kRegisterClass<device_arch_info::RAX>,
                                         .element_offset = 0},
                      ResultsElementInfo{.register_class = &kRegisterClass<device_arch_info::RDX>,
                                         .element_offset = 0}};
  } else if constexpr (std::is_same_v<CleanRetType, std::tuple<__m128>>) {
    return std::array{ResultsElementInfo{.register_class = &kRegisterClass<device_arch_info::XMM0>,
                                         .element_offset = 0}};
  } else {
    std::array<ResultsElementInfo, std::tuple_size_v<CleanRetType>> result =
        ToArray(TypesToValues::MapWithTemporary<CleanRetType, /* offset = */ std::size_t>(
            []<typename Type>(std::size_t& offset) {
              static_assert(IsPowerOf2(sizeof(Type)));
              std::size_t current_offset = AlignUp<sizeof(Type)>(offset);
              offset = current_offset + sizeof(Type);
              if constexpr (std::is_integral_v<Type>) {
                return ResultsElementInfo{.register_class = &kGeneralReg64,
                                          .element_offset = current_offset};
              } else {
                return ResultsElementInfo{.register_class = &kXmmReg,
                                          .element_offset = current_offset};
              }
            }));
    // If struct size is larger than 16 bytes then it's returned in memory.
    if constexpr (sizeof(CleanRetType) > 16) {
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

template <typename IntrinsicRetTuple,
          typename... IntrinsicParamTypes,
          IntrinsicRetTuple (*function)(IntrinsicParamTypes...)>
constexpr auto IntrinsicCall<function>::GenArgumentElements()
    -> std::array<ArgumentsElementInfo, std::tuple_size_v<CleanParamTypes>> {
  if constexpr (std::tuple_size_v<CleanParamTypes> == 0) {
    return std::array<ArgumentsElementInfo, 0>{};
  } else {
    struct ArgumentsElementPreliminaryInfo {
      ArgumentElementType param_type;
      std::size_t register_argument_number;
    };
    constexpr auto kPreliminaryResult =
        ToArray(TypesToValues::MapWithTemporary<
                CleanParamTypes,
                /* integer_index, xmm_index = */ std::tuple<std::size_t, std::size_t>>(
            []<typename CleanArgumentType>(std::tuple<std::size_t, std::size_t>& indexes) {
              auto& [integer_index, xmm_index] = indexes;
              if constexpr (std::is_same_v<CleanArgumentType, __int128> ||
                            std::is_same_v<CleanArgumentType, __uint128_t>) {
                std::size_t first_integer_index = integer_index;
                integer_index += 2;
                return ArgumentsElementPreliminaryInfo{kInt128, first_integer_index};
              } else if constexpr (std::is_integral_v<CleanArgumentType>) {
                return ArgumentsElementPreliminaryInfo{kInteger, integer_index++};
              } else {
                return ArgumentsElementPreliminaryInfo{kXMM, xmm_index++};
              }
            }));
    return ToArray(TypesToValues::Map<ValuesToTypes::MetaValues<kPreliminaryResult>>(
        []<typename ArgumentInfo>() {
          constexpr ArgumentsElementPreliminaryInfo kArgumentInfo = ArgumentInfo{};
          if constexpr (kArgumentInfo.param_type == kInt128) {
            return ArgumentsElementInfo{
                kInt128,
                {&kRegisterClass<std::tuple_element_t<kArgumentInfo.register_argument_number,
                                                      AllGpArgumentetersRegisters>>,
                 &kRegisterClass<std::tuple_element_t<kArgumentInfo.register_argument_number + 1,
                                                      AllGpArgumentetersRegisters>>}};
          } else if constexpr (kArgumentInfo.param_type == kInteger) {
            return ArgumentsElementInfo{
                kInteger,
                {&kRegisterClass<std::tuple_element_t<kArgumentInfo.register_argument_number,
                                                      AllGpArgumentetersRegisters>>,
                 nullptr}};
          } else if constexpr (kArgumentInfo.param_type == kXMM) {
            return ArgumentsElementInfo{
                kXMM,
                {&kRegisterClass<std::tuple_element_t<kArgumentInfo.register_argument_number,
                                                      AllXmmArgumentetersRegisters>>,
                 nullptr}};
          } else {
            static_assert(kDependentValueFalse<kArgumentInfo.param_type>);
          }
        }));
  }
}

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_INTRINSIC_CALL_H_
