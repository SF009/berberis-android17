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

#include "berberis/backend/x86_64/intrinsic_call.h"

namespace berberis::x86_64 {

namespace {

// Most integer types are unmodified.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int8_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<int8_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint8_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<uint8_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int16_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<int16_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint16_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<uint16_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int32_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<int32_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint32_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<uint32_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int64_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<int64_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint64_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<uint64_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const __int128_t> (*)()>(nullptr)>::CleanRetType,
              std::tuple<__int128_t>>);
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<const __uint128_t> (*)()>(nullptr)>::CleanRetType,
        std::tuple<__uint128_t>>);
// Float16/Float32/Float64 qre unmodified.
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<const intrinsics::Float16> (*)()>(
                                 nullptr)>::CleanRetType,
                             std::tuple<intrinsics::Float16>>);
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<const intrinsics::Float32> (*)()>(
                                 nullptr)>::CleanRetType,
                             std::tuple<intrinsics::Float32>>);
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<const intrinsics::Float64> (*)()>(
                                 nullptr)>::CleanRetType,
                             std::tuple<intrinsics::Float64>>);
// SIMD128Register and __m128 are treated as __m128.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<const SIMD128Register> (*)()>(nullptr)>::CleanRetType,
        std::tuple<__m128>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const __m128> (*)()>(nullptr)>::CleanRetType,
              std::tuple<__m128>>);
// Smoke test for Parameters.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<> (*)(const __int128_t, const intrinsics::Float16)>(
            nullptr)>::CleanParamTypes,
        std::tuple<__int128_t, intrinsics::Float16>>);

// Small ints are returned in RAX.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int16_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint16_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int32_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint32_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int64_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint64_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
// Large ints , __int128_t and i__unt128 are returned in RAX:RDX.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<__int128_t> (*)()>(nullptr)>::ResultRegisters,
              std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<__uint128_t> (*)()>(nullptr)>::ResultRegisters,
              std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
// Eigth one-byte elements can be put in RAX.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<
                  std::tuple<int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t> (*)()>(
                  nullptr)>::ResultRegisters,
              std::tuple<device_arch_info::RAX>>);
// Nine one-byte elements would require RDX.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int8_t,
                                                        int8_t,
                                                        int8_t,
                                                        int8_t,
                                                        int8_t,
                                                        int8_t,
                                                        int8_t,
                                                        int8_t,
                                                        int8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
// If you pass mix of ints and floats these are returned in RAX/RDX.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<
                       std::tuple<intrinsics::Float16, int8_t, intrinsics::Float16, int8_t> (*)()>(
                       nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<intrinsics::Float16,
                                                        int8_t,
                                                        intrinsics::Float16,
                                                        int8_t,
                                                        intrinsics::Float16,
                                                        int8_t> (*)()>(nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
// If the only element that comes in the second eightbyte is float it's passed in XMM0, even though
// the first eightbyte is in RAX.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<intrinsics::Float16,
                                             int8_t,
                                             intrinsics::Float16,
                                             int8_t,
                                             intrinsics::Float16> (*)()>(nullptr)>::ResultRegisters,
        std::tuple<device_arch_info::RAX, device_arch_info::XMM0>>);
// Couple of Float32 can be put in XMM0.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<intrinsics::Float32, intrinsics::Float32> (*)()>(
                  nullptr)>::ResultRegisters,
              std::tuple<device_arch_info::XMM0>>);
// Bunch of Float32 would occupy both XMM0 and XMM1
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<
                  std::tuple<intrinsics::Float16, intrinsics::Float32, intrinsics::Float16> (*)()>(
                  nullptr)>::ResultRegisters,
              std::tuple<device_arch_info::XMM0, device_arch_info::XMM1>>);
// One SIMD128Register would go in XMM0.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<SIMD128Register> (*)()>(nullptr)>::ResultRegisters,
        std::tuple<device_arch_info::XMM0>>);
// But two would go on stack.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<SIMD128Register, SIMD128Register> (*)()>(
                       nullptr)>::ResultRegisters,
                   std::tuple<device_arch_info::RAX>>);

// Verify that types properly go into proper register types.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<> (*)(intrinsics::Float16,
                                                         int8_t,
                                                         intrinsics::Float32,
                                                         int16_t,
                                                         intrinsics::Float64,
                                                         int32_t,
                                                         SIMD128Register,
                                                         int64_t,
                                                         __m128,
                                                         __int128_t)>(nullptr)>::ArgumentRegisters,
              std::tuple<device_arch_info::XMM0,
                         device_arch_info::RDI,
                         device_arch_info::XMM1,
                         device_arch_info::RSI,
                         device_arch_info::XMM2,
                         device_arch_info::RDX,
                         device_arch_info::XMM3,
                         device_arch_info::RCX,
                         device_arch_info::XMM4,
                         device_arch_info::R8,
                         device_arch_info::R9>>);

// both RAX and XMM0 are properly removed from ClobberRegisters
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<intrinsics::Float16,
                                                                  int8_t,
                                                                  intrinsics::Float16,
                                                                  int8_t,
                                                                  intrinsics::Float16> (*)()>(
                                 nullptr)>::ClobberRegisters,
                             std::tuple<device_arch_info::RDI,
                                        device_arch_info::RSI,
                                        device_arch_info::RDX,
                                        device_arch_info::RCX,
                                        device_arch_info::R8,
                                        device_arch_info::R9,
                                        device_arch_info::R10,
                                        device_arch_info::R11,
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
                                        device_arch_info::FLAGS>>);

}  // namespace

}  // namespace berberis::x86_64
