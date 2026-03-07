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

#include "berberis/runtime_primitives/arm32/runtime_library.h"

#include <signal.h>
#include <sys/syscall.h>

#include "berberis/base/checks.h"
#include "berberis/base/gettid.h"
#include "berberis/base/tracing.h"
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

namespace {

// TODO(b/232598137): include guest_state.h into assembly files instead.
template <size_t value_in_c, size_t value_in_asm>
constexpr void HardcodedConstantAssert() {
  static_assert(value_in_c == value_in_asm,
                "Fix the corresponding constant in "
                "runtime_library_x86_32.S");
}

// Artificial scope for static_asserts wrapped into a constexpr function.
__attribute__((unused)) void Asserts() {
  HardcodedConstantAssert<kOutsideGeneratedCode, 0>();
  HardcodedConstantAssert<kInsideGeneratedCode, 1>();
  HardcodedConstantAssert<offsetof(berberis::ThreadState, cpu.insn_addr), 0x48>();
  HardcodedConstantAssert<offsetof(berberis::ThreadState, residence), 0x169>();
}

}  // namespace

// ATTENTION: this symbol gets called directly, without PLT. To keep text
// sharable we should prevent preemption of this symbol, so do not export it!
// TODO(eaeltsin): may be set default visibility to protected instead?
extern "C" __attribute__((used, __visibility__("hidden"))) void berberis_HandleUnpredictable(
    berberis::ThreadState* state) {
  CHECK_EQ(0x2, state->cpu.insn_addr & 0x3);
  TRACE("UNPREDICTABLE interworking branch to 0x%x, ARM enforced", state->cpu.insn_addr);
  // Most hardware force align UNPREDICTABLE pc, so do the same.
  // We can also raise an exception here if needed.
  // See b/30266317 for nasty details.
  state->cpu.insn_addr &= ~0x3;
}

}  // namespace berberis
