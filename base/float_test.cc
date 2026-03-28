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

#include "gtest/gtest.h"

#include "berberis/base/float.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "berberis/base/int.h"
#include "berberis/base/type_traits.h"

namespace berberis {

namespace {

// We couldn't use bit_cast in the BitCastToRaw and memcpy is not allowed in constexpr context.
TEST(FloatTest, Smoke) {
  // BitCastToFloat and BitCastToRaw don't change the bits of integer, they just treat them
  // differently.
  static_assert(std::bit_cast<uint16_t>(BitCastToFloat(Int16{0x3c00})) ==
                std::bit_cast<uint16_t>(Float16{_Float16{1.0}}));
  static_assert(std::bit_cast<uint16_t>(BitCastToFloat(RawInt16{0x3c00})) ==
                std::bit_cast<uint16_t>(Float16{_Float16{1.0}}));
  static_assert(std::bit_cast<uint16_t>(BitCastToFloat(SatInt16{0x3c00})) ==
                std::bit_cast<uint16_t>(Float16{_Float16{1.0}}));
  static_assert(std::bit_cast<uint16_t>(BitCastToFloat(SatUInt16{0x3c00})) ==
                std::bit_cast<uint16_t>(Float16{_Float16{1.0}}));
  static_assert(std::bit_cast<uint16_t>(BitCastToFloat(UInt16{0x3c00})) ==
                std::bit_cast<uint16_t>(Float16{_Float16{1.0}}));

  EXPECT_EQ(std::bit_cast<uint16_t>(BitCastToRaw(Float16{_Float16{1.0}})),
            std::bit_cast<uint16_t>(Int16{0x3c00}));

  static_assert(std::bit_cast<uint32_t>(Widen(Float16{_Float16{1.0}})) ==
                std::bit_cast<uint32_t>(Float32{1.0f}));
  static_assert(std::bit_cast<uint64_t>(Widen(Float32{1.0f})) ==
                std::bit_cast<uint64_t>(Float64{1.0}));
#if defined(__x86_64__)
  auto widened_float = Widen(Float64{1.0});
  auto wide_float = static_cast<long double>(1.0);
  // Note: long double has 10 bytes of payload plus 6 bytes of passing on x86 platform.
  // We don't need to compate padding and it's also why we couldn't use std::bit_cast and constexpr.
  EXPECT_EQ(memcmp(&widened_float, &wide_float, 10), 0);
#endif

  static_assert(std::bit_cast<uint16_t>(Narrow(Float32{1.0f})) ==
                std::bit_cast<uint16_t>(Float16{_Float16{1.0}}));
  static_assert(std::bit_cast<uint32_t>(Narrow(Float64{1.0})) ==
                std::bit_cast<uint32_t>(Float32{1.0f}));
}

}  // namespace

}  // namespace berberis
