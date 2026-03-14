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

#ifndef BERBERIS_TESTS_INSN_TESTS_HARNESS_ARM64_
#define BERBERIS_TESTS_INSN_TESTS_HARNESS_ARM64_

#include "gtest/gtest.h"

#include <cstddef>
#include <cstdint>

#include "berberis/guest_os_primitives/guest_map_shadow.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/guest_state/guest_state_utils.h"
#include "berberis/runtime_primitives/config.h"

namespace berberis {

template <bool RunOneInstruction(ThreadState*, GuestAddr)>
class InsnTestHarnessArm64 {
 public:
  InsnTestHarnessArm64(const uint32_t* code, size_t size)
      : state_{}, code_(ToGuestAddr(code)), size_(size) {
    GuestMapShadow::GetInstance()->SetExecutable(code_, size_);
    Reset();
  }

  void Reset() { state_.cpu.insn_addr = code_; }

  bool RunBranch(uint64_t expected_target) {
    bool success = RunOneInstruction(&state_, expected_target);
    return success && state_.cpu.insn_addr == expected_target;
  }

  bool Run() {
    GuestAddr next_insn_addr = state_.cpu.insn_addr + 4;
    bool success = RunOneInstruction(&state_, next_insn_addr);
    return success && state_.cpu.insn_addr == next_insn_addr;
  }

  void SetNZCV(uint8_t nzcv_bits) { berberis::SetNZCV(&state()->cpu, MakeNZCV(nzcv_bits)); }

  void CheckNZCV(uint8_t nzcv_bits) {
    EXPECT_EQ(berberis::GetNZCV(&state()->cpu) & MakeNZCV(0b1111), MakeNZCV(nzcv_bits));
  }

  void CheckNZCVAfterSub(uint8_t nzcv_bits) {
    // Carry is not-borrow
    CheckNZCV(nzcv_bits ^ 0b0010);
  }

  ThreadState* state() { return &state_; }

 private:
  ThreadState state_;
  GuestAddr code_;
  size_t size_;
};

constexpr uint64_t kTestValue64 = 0xffff'0000'ffff'0000ULL;
constexpr uint64_t kTestAddr64 = 0x0000'aaaa'ffff'0000ULL;  // 64-bit address doesn't use high bits

}  // namespace berberis

#endif  // BERBERIS_TESTS_INSN_TESTS_HARNESS_ARM64_
