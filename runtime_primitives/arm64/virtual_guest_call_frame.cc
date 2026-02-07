/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "berberis/base/checks.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

GuestAddr ScopedVirtualGuestCallFrame::g_return_address_ = 0;

ScopedVirtualGuestCallFrame::ScopedVirtualGuestCallFrame(CPUState* cpu, GuestAddr pc) : cpu_(cpu) {
  // Alloc frame.
  cpu_->sp -= 32;

  // TODO(eaeltsin): handle shadow call stack? Anyway, x30 is saved to stack below.
  // *ToHostAddr<GuestAddr>(cpu_->x[18]) = cpu_->x[30];
  // cpu_->x[18] += 8;

  // Save fp, lr and pc to stack.
  GuestAddr* saved_regs = ToHostAddr<GuestAddr>(cpu_->sp);
  saved_regs[0] = cpu_->x[29];
  saved_regs[1] = cpu_->x[30];
  saved_regs[2] = cpu_->insn_addr;

  cpu_->x[29] = cpu_->sp;

  // For safety checks!
  stack_pointer_ = cpu_->sp;
  program_counter_ = cpu_->insn_addr;
  link_register_ = cpu_->x[30];

  cpu_->x[30] = g_return_address_;
  cpu_->insn_addr = pc;
}

ScopedVirtualGuestCallFrame::~ScopedVirtualGuestCallFrame() {
  // Safety check - returned to correct pc?
  CHECK_EQ(g_return_address_, cpu_->insn_addr);
  // Safety check - guest call didn't preserve x29?
  CHECK_EQ(stack_pointer_, cpu_->x[29]);

  cpu_->sp = cpu_->x[29];

  // Restore fp, lr and pc from stack.
  const GuestAddr* saved_regs = ToHostAddr<GuestAddr>(cpu_->sp);
  cpu_->x[29] = saved_regs[0];
  cpu_->x[30] = saved_regs[1];
  cpu_->insn_addr = saved_regs[2];

  // TODO(eaeltsin): handle shadow call stack? Anyway, x30 is restored from stack above.
  // cpu_->x[18] -= 8;
  // cpu_->x[30] = *ToHostAddr<GuestAddr>(cpu_->x[18]);

  // Free frame.
  cpu_->sp += 32;

  // Safety checks - guest stack was smashed?
  CHECK_EQ(link_register_, cpu_->x[30]);
  CHECK_EQ(program_counter_, cpu_->insn_addr);
}

}  // namespace berberis
