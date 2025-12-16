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

#ifndef BERBERIS_REGS_H_
#define BERBERIS_REGS_H_

#include <cstdint>
#include <cstring>

#include "berberis/base/bit_util.h"
#include "berberis/base/float.h"

namespace berberis {

template <typename IntegerType>
inline auto GPRRegToInteger(uint64_t arg)
    -> std::enable_if_t<std::is_integral_v<IntegerType> && sizeof(IntegerType) <= sizeof(uint64_t),
                        IntegerType> {
  return static_cast<IntegerType>(arg);
}

template <typename IntegerType>
inline auto IntegerToGPRReg(IntegerType arg)
    -> std::enable_if_t<std::is_integral_v<IntegerType> && sizeof(IntegerType) <= sizeof(uint64_t),
                        uint64_t> {
  if constexpr (sizeof(IntegerType) <= sizeof(uint32_t)) {
    return static_cast<int32_t>(arg);
  } else {
    return bit_cast<uint64_t>(arg);
  }
}

template <typename FloatType>
inline FloatType FPRegToFloat(uint64_t arg);

template <>
inline Float32 FPRegToFloat<Float32>(uint64_t arg) {
  Float32 result;
  memcpy(&result, &arg, sizeof(Float32));
  return result;
}

template <>
inline Float64 FPRegToFloat<Float64>(uint64_t arg) {
  return bit_cast<Float64>(arg);
}

template <typename FloatType>
inline uint64_t FloatToFPReg(FloatType arg);

template <>
inline uint64_t FloatToFPReg<Float32>(Float32 arg) {
  // Note: NanBoxAndSetFpReg would properly Nan-box the value.
  return bit_cast<uint32_t>(arg);
}

template <>
inline uint64_t FloatToFPReg<Float64>(Float64 arg) {
  return bit_cast<uint64_t>(arg);
}

}  // namespace berberis

#endif  // BERBERIS_REGS_H_
