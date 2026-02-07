/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/virtual_guest_call_frame.h"

namespace berberis {

namespace {

TEST(VirtualGuestFrame, InitReturnAddress) {
  constexpr GuestAddr kVirtualGuestFrameReturnAddress = 0xbeef'face;
  ScopedVirtualGuestCallFrame::SetReturnAddress(kVirtualGuestFrameReturnAddress);

  CPUState cpu{};

  alignas(8) char stack[128];
  cpu.r[13] = ToGuestAddr(stack + sizeof(stack));

  ScopedVirtualGuestCallFrame virtual_guest_call_frame(&cpu, 0xdead'beef);

  EXPECT_EQ(kVirtualGuestFrameReturnAddress, cpu.r[14]);

  // Pretend guest code executed up to return address.
  cpu.insn_addr = cpu.r[14];
}

void RunGuestCall(CPUState* cpu) {
  ScopedVirtualGuestCallFrame virtual_guest_call_frame(cpu, 0xbaaa'aaad);

  // Pretend guest code executed up to return address.
  cpu->insn_addr = cpu->r[14];

  // Host call frame allows random adjustments of sp and lr.
  cpu->r[13] = 0x000f'f1ce;
  cpu->r[14] = 0xbaad'f00d;
}

TEST(VirtualGuestFrame, Restore) {
  CPUState cpu{};

  alignas(8) char stack[128];
  const GuestAddr sp = ToGuestAddr(stack + sizeof(stack));
  const GuestAddr lr = 0xdead'beef;
  const GuestAddr r4 = 0xcafe'feed;
  const GuestAddr r5 = 0xbabe'face;

  cpu.r[4] = r4;
  cpu.r[5] = r5;
  cpu.r[13] = sp;
  cpu.r[14] = lr;

  RunGuestCall(&cpu);

  EXPECT_EQ(r4, cpu.r[4]);
  EXPECT_EQ(r5, cpu.r[5]);
  EXPECT_EQ(sp, cpu.r[13]);
  EXPECT_EQ(lr, cpu.r[14]);
}

}  // namespace

}  // namespace berberis
