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

#ifndef BERBERIS_DEVICE_ARCH_INFO_X86_64_CALL_IMM_H_
#define BERBERIS_DEVICE_ARCH_INFO_X86_64_CALL_IMM_H_

#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>

#include "berberis/base/bit_util.h"
#include "berberis/base/stringprintf.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/device_arch_info/x86_64/device_arch_info.h"

namespace berberis {

class SIMD128Register;

namespace x86_64::device_arch_info {

namespace call_imm_impl {

using GpResultRegisters = std::tuple<RAX, RDX>;

using SSEResultRegisters = std::tuple<XMM0, XMM1>;

using GpArgumentetersRegisters = std::tuple<RDI, RSI, RDX, RCX, R8, R9>;

using SSEArgumentetersRegisters = std::tuple<XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7>;

using ArgumentetersRegisters =
    TypesToTypes::Concat<GpArgumentetersRegisters, SSEArgumentetersRegisters>;

using ClobberRegisters = std::tuple<RAX,
                                    RDI,
                                    RSI,
                                    RDX,
                                    RCX,
                                    R8,
                                    R9,
                                    R10,
                                    R11,
                                    XMM0,
                                    XMM1,
                                    XMM2,
                                    XMM3,
                                    XMM4,
                                    XMM5,
                                    XMM6,
                                    XMM7,
                                    XMM8,
                                    XMM9,
                                    XMM10,
                                    XMM11,
                                    XMM12,
                                    XMM13,
                                    XMM14,
                                    XMM15,
                                    FLAGS>;

// Information about intrinsic call results.
struct ResultsElementInfo {
  // All results are in the clobber list—they are either returned or simply destroyed.
  // We store index in the ClobberRegisters here.
  std::size_t clobber_class_index;
  // Position in register or memory from the beginning of the register (or struct if memory is
  // used).
  std::size_t element_offset;
};

template <typename ArgumentsTuple>
using CleanTypes = TypesToTypes::FlatMap<ArgumentsTuple, []<typename RawType> {
  using Type = std::remove_cvref_t<RawType>;
  if constexpr (std::is_integral_v<Type> ||
                std::is_same_v<Type, intrinsics::WrappedFloatType<_Float16>> ||
                std::is_same_v<Type, intrinsics::WrappedFloatType<float>> ||
                std::is_same_v<Type, intrinsics::WrappedFloatType<double>>) {
    return kTypes<Type>;
  } else if constexpr (std::is_pointer_v<Type>) {
    return kTypes<uint64_t>;
  } else if constexpr (std::is_same_v<Type, SIMD128Register> ||
                       std::is_same_v<Type,
                                      float __attribute__((__vector_size__(16), may_alias))>) {
    return kTypes<SIMD128Register>;
  } else {
    static_assert(kDependentTypeFalse<Type>);
  }
}>;

template <typename ArgumentsTuple>
using RawTypes = TypesToTypes::FlatMap<ArgumentsTuple, []<typename RawType> {
  using Type = std::remove_cvref_t<RawType>;
  if constexpr (std::is_integral_v<Type>) {
    return kTypes<Type>;
  } else if constexpr (std::is_same_v<Type, intrinsics::WrappedFloatType<_Float16>>) {
    return kTypes<_Float16>;
  } else if constexpr (std::is_same_v<Type, intrinsics::WrappedFloatType<float>>) {
    return kTypes<float>;
  } else if constexpr (std::is_same_v<Type, intrinsics::WrappedFloatType<double>>) {
    return kTypes<double>;
  } else if constexpr (std::is_pointer_v<Type>) {
    return kTypes<uint64_t>;
  } else if constexpr (std::is_same_v<Type, SIMD128Register> ||
                       std::is_same_v<Type,
                                      float __attribute__((__vector_size__(16), may_alias))>) {
    return kTypes<float __attribute__((__vector_size__(16), may_alias))>;
  } else {
    static_assert(kDependentTypeFalse<Type>);
  }
}>;

// When passed as arguments __int128_t and __uint128_t are split in two register, except when they
// are passed in memory (which we don't support currently). But when they are reeturned they are
// only split if there are no other types. So we split arguments, but don't split results. Results
// of type std::tuple<__int128_t> and std::tuple<__uint128_t> are handled separately.
template <typename ArgumentsTuple>
using SplitTypes = TypesToTypes::FlatMap<ArgumentsTuple, []<typename Type>() {
  if constexpr (std::is_same_v<Type, __int128_t> || std::is_same_v<Type, __uint128_t>) {
    return kTypes<int64_t, int64_t>;
  } else {
    return kTypes<Type>;
  }
}>;

// Frontend may need to use different register types for integer results and for SSE results.
// Map spit types to GpRegisters and SSERegisters.
template <typename GpRegister, typename SSERegister>
class ResultRegisterTypesHelper {
 public:
  template <typename ResultType>
  constexpr auto operator()() const {
    if constexpr (std::is_integral_v<ResultType>) {
      return kTypes<GpRegister>;
    } else {
      return kTypes<SSERegister>;
    }
  }
};

template <auto kFunction, typename MacroAssemblers>
inline void Emit(MacroAssemblers& as) {
  // Note that a call to AVX-compiled code may touch YMM bits above 128, which
  // would require `vzeroupper` before we come back to generated code. This is
  // to make sure there is no performance penalty. ABI requires such `vzeroupper`s
  // done by the callee unless the result in returned in full YMM register.
  // Since we don't support full YMM results here, we don't need extra
  // `vzeroupper`s here.
  as.Call(bit_cast<const void*>(kFunction));
}

}  // namespace call_imm_impl

template <auto kFunction>
class CallImm;

template <typename IntrinsicRetType,
          typename... IntrinsicParamTypes,
          IntrinsicRetType (*kFunction)(IntrinsicParamTypes...)>
class CallImm<kFunction> {
 public:
  using CleanRetType = call_imm_impl::CleanTypes<std::conditional_t<
      std::is_same_v<IntrinsicRetType, void>,
      std::tuple<>,
      std::conditional_t<
          std::is_integral_v<IntrinsicRetType> || std::is_pointer_v<IntrinsicRetType> ||
              std::is_same_v<IntrinsicRetType, intrinsics::WrappedFloatType<_Float16>> ||
              std::is_same_v<IntrinsicRetType, intrinsics::WrappedFloatType<float>> ||
              std::is_same_v<IntrinsicRetType, intrinsics::WrappedFloatType<double>> ||
              std::is_same_v<IntrinsicRetType, SIMD128Register> ||
              std::is_same_v<IntrinsicRetType,
                             float __attribute__((__vector_size__(16), may_alias))>,
          std::tuple<IntrinsicRetType>,
          IntrinsicRetType>>>;
  using CleanParamTypes =
      call_imm_impl::SplitTypes<call_imm_impl::CleanTypes<std::tuple<IntrinsicParamTypes...>>>;

 private:
  static constexpr std::array<call_imm_impl::ResultsElementInfo,
                              std::tuple_size_v<CleanRetType> +
                                  ((std::is_same_v<CleanRetType, std::tuple<__int128_t>> ||
                                    std::is_same_v<CleanRetType, std::tuple<__uint128_t>>)
                                       ? 1
                                       : 0)>
  GenResultsElements();
  // The same as with ResultsElementInfo: we store indexes to ClobberRegisters here.
  static constexpr std::array<std::size_t, std::tuple_size_v<CleanParamTypes>>
  GenArgumentElements();

 public:
  // Whether the result is returned in memory. In the absence of unions or unaligned fields this
  // happens when and only when size of result is larger than 16 bytes.
  static constexpr bool kIsImplicitPointerResult =
      sizeof(call_imm_impl::RawTypes<CleanRetType>) > 16;
  // Note: we need kResultsElements mostly to calculate ResultRegisters but frontend can use it to
  // unpack values that are returned in one register.
  static constexpr auto kResultsElements = GenResultsElements();
  // Note: we declare RAX as an output operand, but it likely won't be used by frontend, because
  // it's usually an address of a frontend provided area on stack.
  using ResultRegisters = TypesToTypes::
      FlatMap<ValuesToTypes::MetaValues<kResultsElements>, []<typename ResultElementInfo> {
        constexpr call_imm_impl::ResultsElementInfo kResultElementInfo = ResultElementInfo::kValue;
        // Ignore elements except for the first one. Each 8-byte bundle have
        // such element because we are dealing with tuple and not random
        // structs with arbitrary bitfields that may include padding.
        if constexpr (kResultElementInfo.element_offset != 0) {
          return kTypes<>;
        } else if constexpr (kResultElementInfo.clobber_class_index == ~std::size_t{0}) {
          return kTypes<RAX>;
        } else {
          return kTypes<std::tuple_element_t<kResultElementInfo.clobber_class_index,
                                             call_imm_impl::ClobberRegisters>>;
        }
      }>;
  template <typename GpRegister, typename SSERegister>
  using ResultRegiesterTypes =
      TypesToTypes::FlatMap<call_imm_impl::SplitTypes<CleanRetType>,
                            call_imm_impl::ResultRegisterTypesHelper<GpRegister, SSERegister>{}>;
  // Note: we need kArgumentElements mostly to calculate ArgumentRegisters but frontend can use it
  // to unpack values that are supposed to be passed in register.
  static constexpr auto kArgumentElements = GenArgumentElements();
  using ArgumentRegisters = TypesToTypes::Concat<
      std::conditional_t<kIsImplicitPointerResult, std::tuple<RDI>, std::tuple<>>,
      TypesToTypes::FlatMap<ValuesToTypes::MetaValues<kArgumentElements>,
                            []<typename ArgumentElementInfo> {
                              constexpr std::size_t kArgumentElementInfo = ArgumentElementInfo{};
                              return kTypes<std::tuple_element_t<kArgumentElementInfo,
                                                                 call_imm_impl::ClobberRegisters>>;
                            }>>;
  // Clobber registers are registers that a function is allowed to modify. According to the x86-64
  // psABI, these are the caller-saved registers. The registers used to return values are also
  // modified, but their final state is the function's result, so they are not typically listed as
  // "clobbered" in the sense of losing their prior value. Here, we define `ClobberRegisters` as
  // all registers that *could* be modified by the intrinsic call, excluding those that are used
  // to return the function's result. This is done to simplify work for the frontend: frontend
  // doesn't need to dig through list of registers modified by CALL to find the results, results
  // always come first in the list of registers, clobbered registers come after these.
  using ClobberRegisters =
      TypesToTypes::Filter<call_imm_impl::ClobberRegisters, []<typename ClobberedClass>() {
        return TypesToValues::All<ResultRegisters>([]<typename ResultedClass>() {
          return !std::is_same_v<ClobberedClass, ResultedClass>;
        });
      }>;

  static constexpr bool kDynamicFunction =
      kFunction == static_cast<IntrinsicRetType (*)(IntrinsicParamTypes...)>(nullptr);
  template <typename MacroAssemblers>
  class MachineInsn {
   public:
    using DeviceInsnInfo = DeviceInsnInfo<
        std::conditional_t<
            kDynamicFunction,
            MetaValue<static_cast<void (
                std::tuple_element_t<1, typename MacroAssemblers::Assemblers>::*)(const void*)>(
                &std::tuple_element_t<1, typename MacroAssemblers::Assemblers>::Call)>,
            MetaValue<call_imm_impl::Emit<kFunction, MacroAssemblers>>>::kValue,
        "CALL",
        true,
        []<typename Opcode> { return Opcode::kMachineOpCallImm; },
        NoCPUIDRestriction,
        TypesToTypes::Concat<
            std::tuple<OperandInfo<Comment, kUse>>,
            std::conditional_t<kDynamicFunction,
                               std::tuple<OperandInfo<ImmPCode, kUse>>,
                               std::tuple<>>,
            TypesToTypes::Map<ResultRegisters,
                              []<typename RegisterClass>(RegisterClass) {
                                return OperandInfo<RegisterClass, kDef>{};
                              }>,
            TypesToTypes::Map<ArgumentRegisters,
                              []<typename RegisterClass>(RegisterClass) {
                                return OperandInfo<RegisterClass, kUse>{};
                              }>,
            TypesToTypes::Map<ClobberRegisters, []<typename RegisterClass>(RegisterClass) {
              return OperandInfo<RegisterClass, kDef>{};
            }>>>;
  };
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
template <typename IntrinsicRetType,
          typename... IntrinsicParamTypes,
          IntrinsicRetType (*kFunction)(IntrinsicParamTypes...)>
constexpr auto CallImm<kFunction>::GenResultsElements()
    -> std::array<call_imm_impl::ResultsElementInfo,
                  std::tuple_size_v<CleanRetType> +
                      ((std::is_same_v<CleanRetType, std::tuple<__int128_t>> ||
                        std::is_same_v<CleanRetType, std::tuple<__uint128_t>>)
                           ? 1
                           : 0)> {
  // __int128, __uint128_t and __m128 are the only types that may span 8byte chunk, and they can be
  // only passed in registers if they are used alone.
  if constexpr (std::is_same_v<CleanRetType, std::tuple<__int128>> ||
                std::is_same_v<CleanRetType, std::tuple<__uint128_t>>) {
    return std::array{
        call_imm_impl::ResultsElementInfo{.clobber_class_index = 0, .element_offset = 0},
        call_imm_impl::ResultsElementInfo{.clobber_class_index = 3, .element_offset = 0}};
  } else if constexpr (std::is_same_v<
                           CleanRetType,
                           std::tuple<float __attribute__((__vector_size__(16), may_alias))>>) {
    return std::array{
        call_imm_impl::ResultsElementInfo{.clobber_class_index = 9, .element_offset = 0}};
  } else {
    std::array<call_imm_impl::ResultsElementInfo, std::tuple_size_v<CleanRetType>> result =
        ValuesToValues::ToArray<call_imm_impl::ResultsElementInfo>(
            TypesToValues::MapWithTemporary<call_imm_impl::RawTypes<CleanRetType>,
                                            /* offset = */ std::size_t>([]<typename Type>(
                                                                            std::size_t& offset) {
              static_assert(IsPowerOf2(sizeof(Type)));
              std::size_t current_offset = AlignUp<sizeof(Type)>(offset);
              offset = current_offset + sizeof(Type);
              if constexpr (kIsImplicitPointerResult) {
                return call_imm_impl::ResultsElementInfo{.clobber_class_index = ~std::size_t{0},
                                                         .element_offset = current_offset};
              } else if constexpr (std::is_integral_v<Type>) {
                return call_imm_impl::ResultsElementInfo{.clobber_class_index = ~std::size_t{1},
                                                         .element_offset = current_offset};
              } else {
                return call_imm_impl::ResultsElementInfo{.clobber_class_index = ~std::size_t{2},
                                                         .element_offset = current_offset};
              }
            }));
    // If struct size is larger than 16 bytes then it's returned in memory.
    if constexpr (kIsImplicitPointerResult) {
      return result;
    }
    std::size_t int_register = 0, sse_register = 0;
    for (std::size_t offset : {0, 8}) {
      bool use_integer_register = false;
      for (auto& element : result) {
        if (element.element_offset >= offset && element.element_offset < offset + 8 &&
            element.clobber_class_index == ~std::size_t{1}) {
          use_integer_register = true;
          break;
        }
      }
      std::size_t clobber_class_index;
      if (use_integer_register) {
        clobber_class_index = std::array<std::size_t, 2>{0, 3}[int_register++];
      } else {
        clobber_class_index = std::array<std::size_t, 2>{9, 10}[sse_register++];
      }
      for (auto& element : result) {
        if (element.element_offset >= offset && element.element_offset < offset + 8) {
          element.clobber_class_index = clobber_class_index;
          element.element_offset = element.element_offset % 8;
        }
      }
    }
    return result;
  }
}

template <typename IntrinsicRetType,
          typename... IntrinsicParamTypes,
          IntrinsicRetType (*kFunction)(IntrinsicParamTypes...)>
constexpr auto CallImm<kFunction>::GenArgumentElements()
    -> std::array<std::size_t, std::tuple_size_v<CleanParamTypes>> {
  return ValuesToValues::ToArray<std::size_t>(TypesToValues::MapWithTemporary<CleanParamTypes>(
      /* integer_index, sse_index = */ std::tuple{std::size_t{kIsImplicitPointerResult ? 1 : 0},
                                                  std::size_t{0}},
      []<typename CleanArgumentType>(std::tuple<std::size_t, std::size_t>& indexes) {
        auto& [integer_index, sse_index] = indexes;
        if constexpr (std::is_integral_v<CleanArgumentType>) {
          CHECK_LE(++integer_index, 6);  // There are maximum 6 integer parameters.
          return integer_index;
        } else {
          CHECK_LE(++sse_index, 8);  // There are maximum 8 SSE parameters.
          return 8 + sse_index;
        }
      }));
}

}  // namespace x86_64::device_arch_info

}  // namespace berberis

namespace berberis::x86_64 {

template <auto kFunction>
using CallImm = device_arch_info::CallImm<kFunction>;

}  // namespace berberis::x86_64

#endif  // BERBERIS_DEVICE_ARCH_INFO_X86_64_CALL_IMM_H_
