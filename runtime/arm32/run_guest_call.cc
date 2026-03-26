/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "berberis/runtime_library/runtime_library.h"

#include <cstdint>
#include <cstring>

#include "berberis/base/checks.h"
#include "berberis/calling_conventions/calling_conventions_arm.h"
#include "berberis/guest_abi/guest_arguments.h"
#include "berberis/guest_os_primitives/guest_thread.h"
#include "berberis/guest_os_primitives/guest_thread_manager.h"  // GetCurrentGuestThread
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/instrument/guest_call.h"
#include "berberis/runtime_primitives/virtual_guest_call_frame.h"

namespace berberis {

namespace {

void RunGuestCall(GuestAddr pc, int argc, int resc, uint32_t* argv) {
  GuestThread* thread = GetCurrentGuestThread();
  ThreadState* state = thread->state();

  ScopedPendingSignalsEnabler scoped_pending_signals_enabler(thread);

  ScopedVirtualGuestCallFrame virtual_guest_call_frame(&state->cpu, pc);

  state->cpu.r[13] =
      AlignDown(state->cpu.r[13], arm::CallingConventions::kStackAlignmentBeforeCall);

  if (argc < 1) {
    CHECK_EQ(argc, 0);
    // Avoid memcpy in this case. Even with size 0, we need to supply
    // memcpy with valid pointers, or else we get undefined behavior.
  } else if (argc <= 4) {
    CHECK(argv);
    memcpy(state->cpu.r, argv, sizeof(uint32_t) * argc);
  } else if (argc > 4) {
    CHECK(argv);
    memcpy(state->cpu.r, argv, sizeof(uint32_t) * 4);
    int stack_size = sizeof(uint32_t) * (argc - 4);
    state->cpu.r[13] -= stack_size;
    state->cpu.r[13] =
        AlignDown(state->cpu.r[13], arm::CallingConventions::kStackAlignmentBeforeCall);
    memcpy(reinterpret_cast<void*>(state->cpu.r[13]), argv + 4, stack_size);
  }

  if (kInstrumentWrappers) {
    OnWrappedGuestCall(state, pc);
  }

  // ATTENTION: ScopedVirtualGuestCallFrame sets state as for 'blx <pc>'!
  ExecuteGuestCall(state);

  if (kInstrumentWrappers) {
    OnWrappedGuestReturn(state, pc);
  }

  if (resc > 0) {
    CHECK(argv);
    CHECK_LE(resc, 4);
    memcpy(argv, state->cpu.r, sizeof(uint32_t) * resc);
  }
}

}  // namespace

void RunGuestCall(GuestAddr pc, GuestArgumentBuffer* buf) {
  RunGuestCall(pc, buf->argc, buf->resc, buf->argv);
}

}  // namespace berberis