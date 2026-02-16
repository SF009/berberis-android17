/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "berberis/kernel_api/kernel_helpers.h"

#include <cstdint>

#include "berberis/guest_abi/function_wrappers.h"
#include "berberis/guest_abi/guest_params.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/host_code.h"

namespace berberis {

namespace {

// See https://www.kernel.org/doc/Documentation/arm/kernel_user_helpers.txt

// int __kuser_cmpxchg(int32_t oldval, int32_t newval, volatile int32_t *ptr);
void DoCustomTrampoline___kuser_cmpxchg(HostCode /* callee */, ThreadState* state) {
  using PFN_callee =
      int32_t (*)(uint32_t old_value, uint32_t new_value, std::atomic<uint32_t> * addr);
  auto [old_value, new_value, addr] = GuestParamsValues<PFN_callee>(state);
  uint32_t value = old_value;
  std::atomic_compare_exchange_strong_explicit(static_cast<std::atomic<uint32_t>*>(addr),
                                               &value,
                                               static_cast<uint32_t>(new_value),
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if (value == old_value) {
    // Exchange OK.
    auto&& [ret] = GuestReturnReference<PFN_callee>(state);
    ret = 0;
    state->cpu.flags.carry = 1;
  } else {
    // Exchange failed.
    auto&& [ret] = GuestReturnReference<PFN_callee>(state);
    ret = 1;
    state->cpu.flags.carry = 0;
  }
}

// void __kuser_memory_barrier(void);
void DoCustomTrampoline___kuser_memory_barrier(HostCode /* callee */, ThreadState* /* state */) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

}  // namespace

void InitKernelHelpers() {
  MakeTrampolineCallable(
      0xffff'0fc0, false, DoCustomTrampoline___kuser_cmpxchg, nullptr, "__kuser_cmpxchg");
  MakeTrampolineCallable(0xffff'0fa0,
                         false,
                         DoCustomTrampoline___kuser_memory_barrier,
                         nullptr,
                         "__kuser_memory_barrier");
}

}  // namespace berberis
