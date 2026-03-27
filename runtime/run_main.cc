/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <algorithm>   // std::generate
#include <climits>     // CHAR_BIT
#include <cstdint>
#include <cstdlib>
#include <functional>  // std::ref
#include <random>

#include "berberis/guest_os_primitives/guest_thread.h"
#include "berberis/guest_os_primitives/guest_thread_manager.h"  // GetCurrentGuestThread
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/virtual_guest_call_frame.h"

namespace berberis {

namespace {

void FillRandomBuf(uint8_t* buf, size_t size) {
  // arc4random was introduced in GLIBC 2.36
#if defined(__GLIBC__) && ((__GLIBC__ < 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ < 36))) || \
    defined(ANDROID_HOST_MUSL)
  // Fall back to implementation-defined stl random
  std::random_device random_device("/dev/urandom");
  std::independent_bits_engine<std::default_random_engine, CHAR_BIT, uint8_t> engine(
      random_device());
  std::generate(buf, buf + size, std::ref(engine));
#else
  // use arc4random for everything else
  arc4random_buf(buf, size);
#endif
}

}  // namespace

void RunMain(GuestAddr entry_point,
             size_t argc,
             const char* argv[],
             char* envp[],
             GuestAddr linker_base_addr,
             GuestAddr main_executable_entry_point,
             GuestAddr phdr_table,
             size_t phdr_count,
             GuestAddr vdso_base_addr) {
  uint8_t random_bytes[16];
  FillRandomBuf(random_bytes, sizeof(random_bytes));

  GuestThread* main_thread = GetCurrentGuestThread();
  ThreadState* state = main_thread->state();

  ScopedPendingSignalsEnabler scoped_pending_signals_enabler(main_thread);

  CPUState& cpu = state->cpu;
  ScopedVirtualGuestCallFrame virtual_guest_call_frame(&cpu, entry_point);

  GuestAddr updated_stack = InitKernelArgs(GetStackRegister(cpu),
                                           argc,
                                           argv,
                                           envp,
                                           linker_base_addr,
                                           main_executable_entry_point,
                                           phdr_table,
                                           phdr_count,
                                           vdso_base_addr,
                                           &random_bytes);
  SetStackRegister(cpu, updated_stack);

  // Main thread's stack contains envp and aux that may be used by other threads.
  // Prevent stack unmap on main thread exit so the data remains available.
  main_thread->DisallowStackUnmap();

  ExecuteGuestCall(state);
}

}  // namespace berberis
