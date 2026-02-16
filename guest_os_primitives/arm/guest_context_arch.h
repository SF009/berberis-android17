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

#include <string.h>

#include <tuple>

#include "berberis/guest_state/guest_state.h"
#include "berberis/guest_state/guest_state_utils.h"  // {Get,Set}PSR
#include "berberis/runtime_primitives/arm32/runtime_library.h"

namespace berberis {

class GuestContext {
 public:
  GuestContext() {}

  GuestContext(const GuestContext&) = delete;
  GuestContext& operator=(const GuestContext&) = delete;

  void* ptr() { return &ctx_; }

  void Save(const CPUState* cpu) {
    ctx_.sig_ctx.arm_r0 = cpu->r[0];
    ctx_.sig_ctx.arm_r1 = cpu->r[1];
    ctx_.sig_ctx.arm_r2 = cpu->r[2];
    ctx_.sig_ctx.arm_r3 = cpu->r[3];
    ctx_.sig_ctx.arm_r4 = cpu->r[4];
    ctx_.sig_ctx.arm_r5 = cpu->r[5];
    ctx_.sig_ctx.arm_r6 = cpu->r[6];
    ctx_.sig_ctx.arm_r7 = cpu->r[7];
    ctx_.sig_ctx.arm_r8 = cpu->r[8];
    ctx_.sig_ctx.arm_r9 = cpu->r[9];
    ctx_.sig_ctx.arm_r10 = cpu->r[10];
    ctx_.sig_ctx.arm_fp = cpu->r[11];
    ctx_.sig_ctx.arm_ip = cpu->r[12];
    ctx_.sig_ctx.arm_sp = cpu->r[13];
    ctx_.sig_ctx.arm_lr = cpu->r[14];
    // ATTENTION: arm_pc is not PC register value, but real instruction address (never odd)!
    ctx_.sig_ctx.arm_pc = cpu->insn_addr & ~0x1;
    ctx_.sig_ctx.arm_cpsr = GetPSR(cpu);

    memcpy(&saved_d_registers_, &cpu->d, sizeof(saved_d_registers_));
    // Note: GetFPEnvironment combines host FP flags and FP flags
    // in cpu->fpflags and returns as full FPSCR.
    saved_fpflags_ = GetFPEnvironment(cpu->fpflags);
  }

  void Restore(CPUState* cpu) const {
    cpu->r[0] = ctx_.sig_ctx.arm_r0;
    cpu->r[1] = ctx_.sig_ctx.arm_r1;
    cpu->r[2] = ctx_.sig_ctx.arm_r2;
    cpu->r[3] = ctx_.sig_ctx.arm_r3;
    cpu->r[4] = ctx_.sig_ctx.arm_r4;
    cpu->r[5] = ctx_.sig_ctx.arm_r5;
    cpu->r[6] = ctx_.sig_ctx.arm_r6;
    cpu->r[7] = ctx_.sig_ctx.arm_r7;
    cpu->r[8] = ctx_.sig_ctx.arm_r8;
    cpu->r[9] = ctx_.sig_ctx.arm_r9;
    cpu->r[10] = ctx_.sig_ctx.arm_r10;
    cpu->r[11] = ctx_.sig_ctx.arm_fp;
    cpu->r[12] = ctx_.sig_ctx.arm_ip;
    cpu->r[13] = ctx_.sig_ctx.arm_sp;
    cpu->r[14] = ctx_.sig_ctx.arm_lr;
    SetPSR(cpu, ctx_.sig_ctx.arm_cpsr, true, true);
    // ATTENTION: arm_pc is not PC register value, but value to ld into r15 (might be odd)!
    cpu->insn_addr = ctx_.sig_ctx.arm_pc;
    bool is_thumb = (ctx_.sig_ctx.arm_cpsr >> 5) & 0x1;
    if (is_thumb) {
      cpu->insn_addr |= 0x1;
    }

    memcpy(&cpu->d, &saved_d_registers_, sizeof(saved_d_registers_));
    // Note: SetFPEnvironment takes full FPSCR and stores parts of
    // it in the host FP flags and return parts which are not stored there as result.
    cpu->fpflags = SetFPEnvironment(saved_fpflags_);
  }

 private:
  // See <linux-kernel>/arch/arm/include/uapi/asm/sigcontext.h
  struct Guest_sigcontext {
    uint32_t trap_no;
    uint32_t error_code;
    uint32_t oldmask;
    uint32_t arm_r0;
    uint32_t arm_r1;
    uint32_t arm_r2;
    uint32_t arm_r3;
    uint32_t arm_r4;
    uint32_t arm_r5;
    uint32_t arm_r6;
    uint32_t arm_r7;
    uint32_t arm_r8;
    uint32_t arm_r9;
    uint32_t arm_r10;
    uint32_t arm_fp;
    uint32_t arm_ip;
    uint32_t arm_sp;
    uint32_t arm_lr;
    uint32_t arm_pc;
    uint32_t arm_cpsr;
    uint32_t fault_address;
  };

  // See <linux-kernel>/arch/arm/include/asm/ucontext.h
  // TODO(eaeltsin): update!
  struct Guest_ucontext {
    uint32_t uc_flags;
    struct Guest_ucontext* uc_link;
    struct {
      void* p;
      int flags;
      size_t size;
    } sstack_data;
    Guest_sigcontext sig_ctx;
  };

  Guest_ucontext ctx_;
  uint64_t saved_d_registers_[32];
  uint32_t saved_fpflags_;
};

}  // namespace berberis

#endif  // BERBERIS_GUEST_OS_PRIMITIVES_GUEST_CONTEXT_ARCH_H_
