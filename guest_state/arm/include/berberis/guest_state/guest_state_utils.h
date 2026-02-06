/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef BERBERIS_GUEST_STATE_GUEST_STATE_UTILS_H_
#define BERBERIS_GUEST_STATE_GUEST_STATE_UTILS_H_

#include <climits>  // CHAR_BIT
#include <cstdint>

#include "berberis/guest_state/guest_state_opaque.h"
#include "native_bridge_support/arm/guest_state/guest_state_cpu_state.h"

namespace berberis {

using Monitor = berberis::Reservation;
using berberis::CPUState;

// Syntax sugar for the offsets to the commonly used fields.
constexpr size_t kOffsetFlagsNegative = offsetof(CPUState, flags.negative);
constexpr size_t kOffsetFlagsZero = offsetof(CPUState, flags.zero);
constexpr size_t kOffsetFlagsCarry = offsetof(CPUState, flags.carry);
constexpr size_t kOffsetFlagsOverflow = offsetof(CPUState, flags.overflow);
constexpr size_t kOffsetFlagsFirst = kOffsetFlagsNegative;
constexpr size_t kOffsetFlagsLast = kOffsetFlagsOverflow;
constexpr size_t kNumFlags = 4;

constexpr bool AreNZCVFlagsConsecutiveBytes() {
  return kOffsetFlagsFirst + 0 == kOffsetFlagsNegative &&
         kOffsetFlagsFirst + 1 == kOffsetFlagsZero && kOffsetFlagsFirst + 2 == kOffsetFlagsCarry &&
         kOffsetFlagsFirst + 3 == kOffsetFlagsOverflow;
}

static_assert(alignof(CPUState) % (128 / CHAR_BIT) == 0, "CPUState must be 128-bit aligned");

static_assert(offsetof(CPUState, d) % (128 / CHAR_BIT) == 0,
              "CPUState::d registers must be 128-bit aligned");

uint32_t GetPSR(const CPUState* state);
void SetPSR(CPUState* state, uint32_t value, bool write_flags, bool write_ge);

uint32_t GetCurrInsnAddr(const CPUState* cpu_state);

bool InThumbMode(const CPUState* cpu);

}  // namespace berberis

#endif  // BERBERIS_GUEST_STATE_GUEST_STATE_UTILS_H_
