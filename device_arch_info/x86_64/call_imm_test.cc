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

#include "berberis/device_arch_info/x86_64/call_imm.h"

namespace berberis::x86_64::device_arch_info {

namespace {

using Float16 = WrappedFloatType<_Float16>;
using Float32 = WrappedFloatType<float>;
using Float64 = WrappedFloatType<double>;

using Float32x4 = float __attribute__((__vector_size__(16), __aligned__(16), may_alias));

// Non-tuple return values.

// Most integer types are unmodified.
static_assert(
    std::is_same_v<CallImm<static_cast<int8_t (*)()>(nullptr)>::CleanRetType, std::tuple<int8_t>>);
static_assert(std::is_same_v<CallImm<static_cast<uint8_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<uint8_t>>);
static_assert(std::is_same_v<CallImm<static_cast<int16_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<int16_t>>);
static_assert(std::is_same_v<CallImm<static_cast<uint16_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<uint16_t>>);
static_assert(std::is_same_v<CallImm<static_cast<int32_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<int32_t>>);
static_assert(std::is_same_v<CallImm<static_cast<uint32_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<uint32_t>>);
static_assert(std::is_same_v<CallImm<static_cast<int64_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<int64_t>>);
static_assert(std::is_same_v<CallImm<static_cast<uint64_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<uint64_t>>);
static_assert(std::is_same_v<CallImm<static_cast<__int128_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<__int128_t>>);
static_assert(std::is_same_v<CallImm<static_cast<__uint128_t (*)()>(nullptr)>::CleanRetType,
                             std::tuple<__uint128_t>>);
// Float16/Float32/Float64 qre unmodified.
static_assert(std::is_same_v<CallImm<static_cast<Float16 (*)()>(nullptr)>::CleanRetType,
                             std::tuple<Float16>>);
static_assert(std::is_same_v<CallImm<static_cast<Float32 (*)()>(nullptr)>::CleanRetType,
                             std::tuple<Float32>>);
static_assert(std::is_same_v<CallImm<static_cast<Float64 (*)()>(nullptr)>::CleanRetType,
                             std::tuple<Float64>>);
// SIMD128Register and __m128 are treated as SIMD128Register.
static_assert(std::is_same_v<CallImm<static_cast<SIMD128Register (*)()>(nullptr)>::CleanRetType,
                             std::tuple<SIMD128Register>>);
static_assert(std::is_same_v<CallImm<static_cast<Float32x4 (*)()>(nullptr)>::CleanRetType,
                             std::tuple<SIMD128Register>>);

// Tuple return values.

// Most integer types are unmodified.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const int8_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<int8_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const uint8_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<uint8_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const int16_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<int16_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const uint16_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<uint16_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const int32_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<int32_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const uint32_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<uint32_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const int64_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<int64_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const uint64_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<uint64_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const __int128_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<__int128_t>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const __uint128_t> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<__uint128_t>>);
// Float16/Float32/Float64 qre unmodified.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const Float16> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<Float16>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const Float32> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<Float32>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const Float64> (*)()>(nullptr)>::CleanRetType,
                   std::tuple<Float64>>);
// SIMD128Register and __m128 are treated as SIMD128Register.
static_assert(std::is_same_v<
              CallImm<static_cast<std::tuple<const SIMD128Register> (*)()>(nullptr)>::CleanRetType,
              std::tuple<SIMD128Register>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<const float __attribute__((__vector_size__(16),
                                                                             may_alias))> (*)()>(
                       nullptr)>::CleanRetType,
                   std::tuple<SIMD128Register>>);

// Smoke test for Parameters.
static_assert(
    std::is_same_v<
        CallImm<static_cast<void (*)(const __int128_t, const Float16)>(nullptr)>::CleanParamTypes,
        std::tuple<int64_t, int64_t, Float16>>);

// Small ints are returned in RAX.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<int8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<uint8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<int16_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<uint16_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<int32_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<uint32_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<int64_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<uint64_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
// Large ints , __int128_t and i__unt128 are returned in RAX:RDX.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<__int128_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX, RDX>>);
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<__uint128_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX, RDX>>);
// Eigth one-byte elements can be put in RAX.
static_assert(std::is_same_v<
              CallImm<static_cast<
                  std::tuple<int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t> (*)()>(
                  nullptr)>::ResultRegisters,
              std::tuple<RAX>>);
// Nine one-byte elements would require RDX.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<int8_t,
                                                  int8_t,
                                                  int8_t,
                                                  int8_t,
                                                  int8_t,
                                                  int8_t,
                                                  int8_t,
                                                  int8_t,
                                                  int8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<RAX, RDX>>);
// If you pass mix of ints and floats these are returned in RAX/RDX.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<Float16, int8_t, Float16, int8_t> (*)()>(
                       nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);
static_assert(
    std::is_same_v<
        CallImm<static_cast<std::tuple<Float16, int8_t, Float16, int8_t, Float16, int8_t> (*)()>(
            nullptr)>::ResultRegisters,
        std::tuple<RAX, RDX>>);
// If the only element that comes in the second eightbyte is float it's passed in XMM0, even though
// the first eightbyte is in RAX.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<Float16, int8_t, Float16, int8_t, Float16> (*)()>(
                       nullptr)>::ResultRegisters,
                   std::tuple<RAX, XMM0>>);
// Couple of Float32 can be put in XMM0.
static_assert(std::is_same_v<
              CallImm<static_cast<std::tuple<Float32, Float32> (*)()>(nullptr)>::ResultRegisters,
              std::tuple<XMM0>>);
// Bunch of Float32 would occupy both XMM0 and XMM1
static_assert(
    std::is_same_v<
        CallImm<static_cast<std::tuple<Float16, Float32, Float16> (*)()>(nullptr)>::ResultRegisters,
        std::tuple<XMM0, XMM1>>);
// One SIMD128Register would go in XMM0.
static_assert(std::is_same_v<
              CallImm<static_cast<std::tuple<SIMD128Register> (*)()>(nullptr)>::ResultRegisters,
              std::tuple<XMM0>>);
// But two would go on stack.
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<SIMD128Register, SIMD128Register> (*)()>(
                       nullptr)>::ResultRegisters,
                   std::tuple<RAX>>);

// Verify that types properly go into proper register types.
static_assert(std::is_same_v<CallImm<static_cast<void (*)(Float16,
                                                          int8_t,
                                                          Float32,
                                                          int16_t,
                                                          Float64,
                                                          int32_t,
                                                          SIMD128Register,
                                                          int64_t,
                                                          Float32x4,
                                                          __int128_t)>(nullptr)>::ArgumentRegisters,
                             std::tuple<XMM0, RDI, XMM1, RSI, XMM2, RDX, XMM3, RCX, XMM4, R8, R9>>);
// If result is returned in memory then first integer register is used for pointer to result.
static_assert(std::is_same_v<CallImm<static_cast<std::tuple<SIMD128Register, SIMD128Register> (*)(
                                 Float16,
                                 int16_t,
                                 Float16,
                                 int32_t,
                                 Float64,
                                 int64_t,
                                 SIMD128Register,
                                 __int128_t,
                                 Float32x4)>(nullptr)>::ArgumentRegisters,
                             std::tuple<RDI, XMM0, RSI, XMM1, RDX, XMM2, RCX, XMM3, R8, R9, XMM4>>);

// both RAX and XMM0 are properly removed from ClobberRegisters
static_assert(
    std::is_same_v<CallImm<static_cast<std::tuple<Float16, int8_t, Float16, int8_t, Float16> (*)()>(
                       nullptr)>::ClobberRegisters,
                   std::tuple<RDI,
                              RSI,
                              RDX,
                              RCX,
                              R8,
                              R9,
                              R10,
                              R11,
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
                              FLAGS>>);

}  // namespace

}  // namespace berberis::x86_64::device_arch_info
