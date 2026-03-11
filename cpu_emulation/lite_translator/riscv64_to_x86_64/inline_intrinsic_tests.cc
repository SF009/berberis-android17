/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "gtest/gtest.h"

#include <cstdint>
#include <type_traits>

#include "berberis/assembler/machine_code.h"
#include "berberis/assembler/x86_64.h"
#include "berberis/base/dependent_false.h"
#include "berberis/intrinsics/guest_cpu_flags.h"
#include "berberis/intrinsics/intrinsics.h"
#include "berberis/intrinsics/macro_assembler.h"
#include "berberis/intrinsics/simd_register.h"
#include "berberis/intrinsics/vector_intrinsics.h"
#include "berberis/runtime_primitives/platform.h"

#include "inline_intrinsic.h"

namespace berberis {

namespace {

class RegAlloc {
 public:
  static x86_64::Assembler::Register AllocTempReg() { return x86_64::Assembler::rax; }

  static x86_64::Assembler::XMMRegister AllocTempSimdReg() { return x86_64::Assembler::xmm0; }
};

// Helper to split function proto into args and result in the specialization.
template <typename Type>
class TryInlineIntrinsicWithTestParams {};

template <typename Result, typename... Args>
class TryInlineIntrinsicWithTestParams<Result (*)(Args...)> {
 public:
  template <auto kFunction, typename... ExplicitArgs>
  static bool Call(MacroAssembler<x86_64::Assembler>* as, ExplicitArgs&&... args) {
    return Call<kFunction>(
        as, std::make_index_sequence<sizeof...(Args)>{}, std::forward<ExplicitArgs>(args)...);
  }

 private:
  template <auto kFunction, std::size_t... I, typename... ExplicitArgs>
  static bool Call(MacroAssembler<x86_64::Assembler>* as,
                   std::integer_sequence<std::size_t, I...>,
                   ExplicitArgs&&... args) {
    RegAlloc reg_alloc;
    return inline_intrinsic::TryInlineIntrinsic<kFunction>(
        *as,
        [&reg_alloc]() { return reg_alloc.AllocTempReg(); },
        [&reg_alloc]() { return reg_alloc.AllocTempSimdReg(); },
        AllocResult(),
        AllocArg<Args, I>(std::tuple{std::forward<ExplicitArgs>(args)...})...);
  }

  static auto AllocResult() {
    if constexpr (std::is_same_v<Result, std::tuple<uint32_t>> ||
                  std::is_same_v<Result, std::tuple<int32_t>> ||
                  std::is_same_v<Result, std::tuple<uint64_t>> ||
                  std::is_same_v<Result, std::tuple<int64_t>>) {
      return std::tuple{x86_64::Assembler::rax};
    } else if constexpr (std::is_same_v<Result, std::tuple<SIMD128Register, uint32_t>>) {
      return std::tuple{x86_64::Assembler::xmm0, x86_64::Assembler::rax};
    } else if constexpr (std::is_same_v<Result, std::tuple<SIMD128Register>> ||
                         std::is_same_v<Result, std::tuple<Float32>> ||
                         std::is_same_v<Result, std::tuple<Float64>>) {
      return std::tuple{x86_64::Assembler::xmm0};
    } else {
      static_assert(kDependentTypeFalse<Result>);
    }
  }

  template <typename Arg, std::size_t I, typename ExplicitArgs>
  static auto AllocArg(ExplicitArgs explicit_args) {
    if constexpr (I < std::tuple_size_v<ExplicitArgs>) {
      return Arg{std::get<I>(explicit_args)};
    } else if constexpr (std::is_integral_v<Arg>) {
      Arg value = Arg{};
      return value;
    } else if constexpr (std::is_same_v<Arg, SIMD128Register> || std::is_same_v<Arg, Float32> ||
                         std::is_same_v<Arg, Float64>) {
      return x86_64::Assembler::xmm0;
    } else {
      static_assert(kDependentTypeFalse<Arg>);
    }
  }
};

// Syntax sugar.
#define TEST_SUPPORTED(func, ...)                                                       \
  EXPECT_TRUE((TryInlineIntrinsicWithTestParams<decltype(&func)>::template Call<&func>( \
      &as __VA_OPT__(, ) __VA_ARGS__)))

#define TEST_UNSUPPORTED(func, ...)                                                      \
  EXPECT_FALSE((TryInlineIntrinsicWithTestParams<decltype(&func)>::template Call<&func>( \
      &as __VA_OPT__(, ) __VA_ARGS__)))

TEST(InlineIntrinsicRiscv64Test, SupportedInstructions) {
  MachineCode machine_code;
  MacroAssembler<x86_64::Assembler> as(&machine_code);
  TEST_SUPPORTED((intrinsics::FMul<Float64>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FMul<Float64>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FMul<Float32>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FMul<Float32>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FMulHostRounding<Float64>));
  TEST_SUPPORTED((intrinsics::FAdd<Float64>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FAdd<Float64>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FAdd<Float32>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FAdd<Float32>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FAddHostRounding<Float64>));
  TEST_SUPPORTED((intrinsics::FSub<Float64>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FSub<Float64>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FSub<Float32>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FSub<Float32>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FSubHostRounding<Float64>));
  TEST_SUPPORTED((intrinsics::FDiv<Float64>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FDiv<Float64>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FDiv<Float32>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FDiv<Float32>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FDivHostRounding<Float64>));
  TEST_SUPPORTED((intrinsics::FCvtFloatToInteger<int64_t, Float64>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FCvtFloatToInteger<int64_t, Float64>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FCvtFloatToIntegerHostRounding<int64_t, Float64>));
  TEST_SUPPORTED((intrinsics::FCvtFloatToInteger<int64_t, Float32>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FCvtFloatToInteger<int64_t, Float32>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FCvtFloatToIntegerHostRounding<int64_t, Float32>));
  TEST_SUPPORTED((intrinsics::FCvtFloatToInteger<int32_t, Float64>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FCvtFloatToInteger<int32_t, Float64>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FCvtFloatToIntegerHostRounding<int32_t, Float64>));
  TEST_SUPPORTED((intrinsics::FCvtFloatToInteger<int32_t, Float32>), int8_t{FPFlags::DYN});
  TEST_UNSUPPORTED((intrinsics::FCvtFloatToInteger<int32_t, Float32>), int8_t{FPFlags::RNE});
  TEST_SUPPORTED((intrinsics::FCvtFloatToIntegerHostRounding<int32_t, Float32>));
}

#undef TEST_SUPPORTED
#undef TEST_UNSUPPORTED

}  // namespace

}  // namespace berberis
