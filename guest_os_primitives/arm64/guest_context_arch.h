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

#ifndef BERBERIS_GUEST_OS_PRIMITIVES_GUEST_CONTEXT_ARCH_H_
#define BERBERIS_GUEST_OS_PRIMITIVES_GUEST_CONTEXT_ARCH_H_

#include "berberis/base/struct_check.h"
#include "berberis/guest_os_primitives/guest_signal.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

class GuestContext {
 public:
  GuestContext() {}

  GuestContext(const GuestContext&) = delete;
  GuestContext& operator=(const GuestContext&) = delete;

  void* ptr() { return &ctx_; }

  void Save(const CPUState* cpu) {
    // Save everything.
    cpu_ = *cpu;

    // Save context.
    memset(&ctx_, 0, sizeof(ctx_));
    memcpy(ctx_.uc_mcontext.regs, cpu->x, sizeof(ctx_.uc_mcontext.regs));
    ctx_.uc_mcontext.sp = cpu->sp;
    ctx_.uc_mcontext.pc = cpu->insn_addr;
  }

  void Restore(CPUState* cpu) const {
    // Restore everything.
    *cpu = cpu_;

    // Overwrite from context.
    memcpy(cpu->x, ctx_.uc_mcontext.regs, sizeof(ctx_.uc_mcontext.regs));
    cpu->sp = ctx_.uc_mcontext.sp;
    cpu->insn_addr = ctx_.uc_mcontext.pc;
  }

 private:
  // See <linux-kernel>/arch/arm64/include/uapi/asm/sigcontext.h
  // TODO(eaeltsin): update!
  struct Guest_sigcontext {
    uint64_t fault_address;
    // AArch64 registers
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
    // 4K reserved for FP/SIMD state and future expansion
    // ATTENTION: align on 16!
    // TODO(eaeltsin): implement!
    uint8_t __reserved[1] __attribute__((__aligned__(16)));
  };

  // See <linux-kernel>/arch/arm64/include/uapi/asm/ucontext.h
  // TODO(eaeltsin): update!
  struct Guest_ucontext {
    uint64_t uc_flags;
    Guest_ucontext* uc_link;
    // We assume guest stack_t is compatible with host (see RunGuestSyscall___NR_sigaltstack).
    stack_t uc_stack;
    Guest_sigset_t uc_sigmask;
    // glibc uses a 1024-bit sigset_t
    uint8_t __linux_unused[1024 / 8 - sizeof(Guest_sigset_t)];
    Guest_sigcontext uc_mcontext;
  };

  // TODO(b/144324673): Enable the correct check when context is fully emulated.
  // CHECK_STRUCT_LAYOUT(Guest_ucontext, 4560 * 8, 16 * 8);
  CHECK_STRUCT_LAYOUT(Guest_ucontext, 480 * 8, 16 * 8);
  CHECK_FIELD_LAYOUT(Guest_ucontext, uc_flags, 0, 8 * 8);
  CHECK_FIELD_LAYOUT(Guest_ucontext, uc_link, 8 * 8, 8 * 8);
  CHECK_FIELD_LAYOUT(Guest_ucontext, uc_stack, 16 * 8, 24 * 8);
  CHECK_FIELD_LAYOUT(Guest_ucontext, uc_sigmask, 40 * 8, 8 * 8);
  CHECK_FIELD_LAYOUT(Guest_ucontext, __linux_unused, 48 * 8, 120 * 8);
  // We don't emulate uc_mcontext size correctly, but the offset
  // check is important for the current stub to function properly.
  // TODO(b/144324673): Enable the correct check when context is fully emulated.
  // CHECK_FIELD_LAYOUT(Guest_ucontext, uc_mcontext, 176 * 8, 4384 * 8);
  CHECK_FIELD_LAYOUT(Guest_ucontext, uc_mcontext, 176 * 8, 304 * 8);

  Guest_ucontext ctx_;
  CPUState cpu_;
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_OS_PRIMITIVES_GUEST_CONTEXT_ARCH_H_
