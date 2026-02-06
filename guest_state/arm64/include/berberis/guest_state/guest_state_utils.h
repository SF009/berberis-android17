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

#include <array>
#include <cstdint>

#include "berberis/base/config_globals.h"
#include "berberis/guest_state/guest_state_arch.h"
#include "berberis/guest_state/guest_state_opaque.h"
#include "native_bridge_support/arm64/guest_state/guest_state_cpu_state.h"

namespace berberis {

inline constexpr uint64_t kArm64CacheTypeRegisterValue =
    // The smallest instruction cache line size among all caches.
    // For 64-bytes, the value is Log2(16 words) = 4.
    (0b0100ULL << 0) |
    // Instruction cache policy, return reserved 0b00 to avoid claiming anything definite.
    (0b00ULL << 15) |
    // The smallest data cache line size among all caches.
    // For 64-bytes, the value is Log2(16 words) = 4.
    (0b0100ULL << 16) |
    // Maximum reservation granule for exclusive load/store
    // 0b0000 means - no info provided, assume the default 2Kbytes.
    (0b0000ULL << 20) |
    // Maximum amount of memory overwritten when a line is evicted.
    // 0b0000 means - no info provided, assume the default 2Kbytes.
    (0b0000ULL << 24);

inline void SetNZCV(CPUState* cpu_state, uint16_t nzcv) {
  cpu_state->flags = nzcv ^ CPUState::kFlagCarry;
}

inline uint16_t GetNZCV(const CPUState* cpu_state) {
  return cpu_state->flags ^ CPUState::kFlagCarry;
}

inline bool GetN(uint16_t nzcv) {
  return nzcv & CPUState::kFlagNegative;
}

inline bool GetZ(uint16_t nzcv) {
  return nzcv & CPUState::kFlagZero;
}

inline bool GetC(uint16_t nzcv) {
  return nzcv & CPUState::kFlagCarry;
}

inline bool GetV(uint16_t nzcv) {
  return nzcv & CPUState::kFlagOverflow;
}

inline bool GetN(const CPUState* cpu_state) {
  return GetN(cpu_state->flags);
}

inline bool GetZ(const CPUState* cpu_state) {
  return GetZ(cpu_state->flags);
}

inline bool GetC(const CPUState* cpu_state) {
  return GetC(cpu_state->flags);
}

inline bool GetV(const CPUState* cpu_state) {
  return GetV(cpu_state->flags);
}

inline uint16_t MakeNZCV(bool n, bool z, bool c, bool v) {
  return (n ? CPUState::kFlagNegative : 0) | (z ? CPUState::kFlagZero : 0) |
         (c ? CPUState::kFlagCarry : 0) | (v ? CPUState::kFlagOverflow : 0);
}

// The below comment applies to x86 host. For riscv we use only 4-bit NZCV.
// The 4-bit NZCV representation should only be used in two cases
//  1. As a syntax sugar to create CPUState flags: e.g. MakeNZCV(0b1010) instead
//  of MakeNZCV(true, false, true, false)
//  2. To convert the 4-bit instruction fields (like alt_nzcv in
//  ConditionalCompareImm) to the canonical CPUState format.
inline uint16_t MakeNZCV(uint8_t nzcv_bits) {
  return MakeNZCV(nzcv_bits & 0b1000, nzcv_bits & 0b0100, nzcv_bits & 0b0010, nzcv_bits & 0b0001);
}

}  // namespace berberis

#endif  // BERBERIS_GUEST_STATE_GUEST_STATE_UTILS_H_
