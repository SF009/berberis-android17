/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "berberis/runtime_primitives/virtual_guest_call_frame.h"

#include <cstdint>

#include "berberis/base/checks.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

GuestAddr ScopedVirtualGuestCallFrame::g_return_address_ = 0;

ScopedVirtualGuestCallFrame::ScopedVirtualGuestCallFrame(CPUState* cpu, GuestAddr pc) : cpu_(cpu) {
  // push {r4, r5, lr, pc}
  cpu_->r[13] -= 16;
  uint32_t* saved_regs = ToHostAddr<uint32_t>(cpu_->r[13]);
  saved_regs[0] = cpu_->r[4];
  saved_regs[1] = cpu_->r[5];
  saved_regs[2] = cpu_->r[14];
  saved_regs[3] = cpu_->insn_addr;

  // mov r4, sp
  cpu_->r[4] = cpu_->r[13];

  // For safety checks!
  stack_pointer_ = cpu_->r[13];
  link_register_ = cpu_->r[14];
  program_counter_ = cpu_->insn_addr;

  // Set pc and lr as for 'blx <guest>'.
  cpu_->r[14] = g_return_address_;
  cpu_->insn_addr = pc;
}

ScopedVirtualGuestCallFrame::~ScopedVirtualGuestCallFrame() {
  // Safety check - returned to correct pc?
  CHECK_EQ(g_return_address_, cpu_->insn_addr);
  // Safety check - guest call didn't preserve r4?
  CHECK_EQ(stack_pointer_, cpu_->r[4]);

  // mov sp, r4
  cpu_->r[13] = cpu_->r[4];

  // pop {r4, r5, lr, pc}
  const uint32_t* saved_regs = ToHostAddr<uint32_t>(cpu_->r[13]);
  cpu_->r[4] = saved_regs[0];
  cpu_->r[5] = saved_regs[1];
  cpu_->r[14] = saved_regs[2];
  cpu_->insn_addr = saved_regs[3];
  cpu_->r[13] += 16;

  // Safety checks - guest stack was smashed?
  CHECK_EQ(link_register_, cpu_->r[14]);
  CHECK_EQ(program_counter_, cpu_->insn_addr);
}

}  // namespace berberis
