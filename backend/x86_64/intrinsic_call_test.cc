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
              IntrinsicCall<static_cast<std::tuple<const int8_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<int8_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint8_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<uint8_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int16_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<int16_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint16_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<uint16_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int32_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<int32_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint32_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<uint32_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const int64_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<int64_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const uint64_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<uint64_t>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const __int128_t> (*)()>(nullptr)>::CleanResType,
              std::tuple<__int128_t>>);
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<const __uint128_t> (*)()>(nullptr)>::CleanResType,
        std::tuple<__uint128_t>>);
// Float16/Float32/Float64 qre unmodified.
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<const intrinsics::Float16> (*)()>(
                                 nullptr)>::CleanResType,
                             std::tuple<intrinsics::Float16>>);
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<const intrinsics::Float32> (*)()>(
                                 nullptr)>::CleanResType,
                             std::tuple<intrinsics::Float32>>);
static_assert(std::is_same_v<IntrinsicCall<static_cast<std::tuple<const intrinsics::Float64> (*)()>(
                                 nullptr)>::CleanResType,
                             std::tuple<intrinsics::Float64>>);
// SIMD128Register and __m128 are treated as __m128.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<const SIMD128Register> (*)()>(nullptr)>::CleanResType,
        std::tuple<__m128>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<const __m128> (*)()>(nullptr)>::CleanResType,
              std::tuple<__m128>>);
// Smoke test for Parameters.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<> (*)(const __int128_t, const intrinsics::Float16)>(
            nullptr)>::CleanParamTypes,
        std::tuple<__int128_t, intrinsics::Float16>>);

// Small ints are returned in RAX.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int8_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint8_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int16_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint16_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int32_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint32_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<int64_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<uint64_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
// Large ints , __int128_t and i__unt128 are returned in RAX:RDX.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<__int128_t> (*)()>(nullptr)>::ReturnRegisters,
              std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<__uint128_t> (*)()>(nullptr)>::ReturnRegisters,
              std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
// Eigth one-byte elements can be put in RAX.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<
                  std::tuple<int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, int8_t> (*)()>(
                  nullptr)>::ReturnRegisters,
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
                                                        int8_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
// If you pass mix of ints and floats these are returned in RAX/RDX.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<
                       std::tuple<intrinsics::Float16, int8_t, intrinsics::Float16, int8_t> (*)()>(
                       nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<intrinsics::Float16,
                                                        int8_t,
                                                        intrinsics::Float16,
                                                        int8_t,
                                                        intrinsics::Float16,
                                                        int8_t> (*)()>(nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX, device_arch_info::RDX>>);
// If the only element that comes in the second eightbyte is float it's passed in XMM0, even though
// the first eightbyte is in RAX.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<intrinsics::Float16,
                                             int8_t,
                                             intrinsics::Float16,
                                             int8_t,
                                             intrinsics::Float16> (*)()>(nullptr)>::ReturnRegisters,
        std::tuple<device_arch_info::RAX, device_arch_info::XMM0>>);
// Couple of Float32 can be put in XMM0.
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<std::tuple<intrinsics::Float32, intrinsics::Float32> (*)()>(
                  nullptr)>::ReturnRegisters,
              std::tuple<device_arch_info::XMM0>>);
// Bunch of Float32 would occupy both XMM0 and XMM1
static_assert(std::is_same_v<
              IntrinsicCall<static_cast<
                  std::tuple<intrinsics::Float16, intrinsics::Float32, intrinsics::Float16> (*)()>(
                  nullptr)>::ReturnRegisters,
              std::tuple<device_arch_info::XMM0, device_arch_info::XMM1>>);
// One SIMD128Register would go in XMM0.
static_assert(
    std::is_same_v<
        IntrinsicCall<static_cast<std::tuple<SIMD128Register> (*)()>(nullptr)>::ReturnRegisters,
        std::tuple<device_arch_info::XMM0>>);
// But two would go on stack.
static_assert(
    std::is_same_v<IntrinsicCall<static_cast<std::tuple<SIMD128Register, SIMD128Register> (*)()>(
                       nullptr)>::ReturnRegisters,
                   std::tuple<device_arch_info::RAX>>);

}  // namespace

}  // namespace berberis::x86_64
