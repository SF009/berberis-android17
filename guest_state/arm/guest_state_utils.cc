/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "berberis/guest_state/guest_state_utils.h"

#include <cstdint>

#include "native_bridge_support/arm/guest_state/guest_state_cpu_state.h"

namespace berberis {

namespace {

uint32_t ToZeroOrOne(uint32_t flag) {
  return flag ? 1 : 0;
}

}  // namespace

bool InThumbMode(const CPUState* cpu) {
  return (cpu->insn_addr & 0x1) != 0;
}

uint32_t GetCurrInsnAddr(const CPUState* cpu) {
  return cpu->insn_addr & ~0x1;
}

uint32_t PackGeFlags(uint32_t ge) {
// Note: x86 ABI includes instructions which very quickly packs ge flags,
// but backward conversion could only be done with slow bit manipulations
// or lookup table.
#if defined(__i386__) || defined(__x86_64__)
  asm("pmovmskb %1, %0" : "=r"(ge) : "x"(ge));
  return ge & 0x0f;
#else
  return ToZeroOrOne(cpu->flags.ge & 0x1) | ToZeroOrOne(cpu->flags.ge & 0x100) << 1 |
         ToZeroOrOne(cpu->flags.ge & 0x1'0000) << 2 | ToZeroOrOne(cpu->flags.ge & 0x100'0000) << 3;
#endif
}

uint32_t UnpackGeFlags(uint32_t ge) {
  alignas(16) static constexpr uint32_t kExpandGe[16] = {0x0000'0000,
                                                         0x0000'00ff,
                                                         0x0000'ff00,
                                                         0x0000'ffff,
                                                         0x00ff'0000,
                                                         0x00ff'00ff,
                                                         0x00ff'ff00,
                                                         0x00ff'ffff,
                                                         0xff00'0000,
                                                         0xff00'00ff,
                                                         0xff00'ff00,
                                                         0xff00'ffff,
                                                         0xffff'0000,
                                                         0xffff'00ff,
                                                         0xffff'ff00,
                                                         0xffff'ffff};
  return kExpandGe[ge & 0x0f];
}

// Pack/unpack PSR in ARMv7-A/ARMv7-R format (ARMv7-M PSR format differs)!
uint32_t GetPSR(const CPUState* cpu) {
  // Flag values must always be either 1 or 0.
  // But if something went wrong, avoid extra confusion when ORing garbage.
  // TODO(levarum): Can we do CHECKs instead?
  return (ToZeroOrOne(cpu->flags.negative) << 31) | (ToZeroOrOne(cpu->flags.zero) << 30) |
         (ToZeroOrOne(cpu->flags.carry) << 29) | (ToZeroOrOne(cpu->flags.overflow) << 28) |
         (ToZeroOrOne(cpu->flags.saturation) << 27) | (PackGeFlags(cpu->flags.ge) << 16) |
         ((InThumbMode(cpu) ? 1 : 0) << 5);
}

void SetPSR(CPUState* cpu, uint32_t value, bool write_flags, bool write_ge) {
  if (write_flags) {
    cpu->flags.negative = (value >> 31) & 1;
    cpu->flags.zero = (value >> 30) & 1;
    cpu->flags.carry = (value >> 29) & 1;
    cpu->flags.overflow = (value >> 28) & 1;
    cpu->flags.saturation = (value >> 27) & 1;
  }
  if (write_ge) {
    cpu->flags.ge = UnpackGeFlags((value >> 16) & 0xf);
  }
}

}  // namespace berberis
