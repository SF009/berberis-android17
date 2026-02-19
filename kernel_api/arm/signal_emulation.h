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

#ifndef BERBERIS_KERNEL_API_SIGNAL_EMULATION_H_
#define BERBERIS_KERNEL_API_SIGNAL_EMULATION_H_

#include <signal.h>

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT
#include "berberis/guest_os_primitives/guest_signal.h"
#include "berberis/guest_os_primitives/guest_signal_arch.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

CHECK_STRUCT_LAYOUT(stack_t, 96, 32);
CHECK_FIELD_LAYOUT(stack_t, ss_sp, 0, 32);
CHECK_FIELD_LAYOUT(stack_t, ss_flags, 32, 32);
CHECK_FIELD_LAYOUT(stack_t, ss_size, 64, 32);

// Guest sigset_t, as expected by guest sigprocmask syscall (not rt_sigprocmask!).
struct Guest_old_sigset_t {
  unsigned long __bits[1];
};

CHECK_STRUCT_LAYOUT(Guest_old_sigset_t, 32, 32);

// Guest struct sigaction, as expected by sigaction syscall (not rt_sigaction!).
struct Guest_old_sigaction {
  // Prefix for avoiding conflict with original 'sa_sigaction' defined as macro.
  GuestAddr guest_sa_sigaction;
  Guest_old_sigset_t sa_mask;
  uint32_t sa_flags;
  GuestAddr sa_restorer;
};

CHECK_STRUCT_LAYOUT(Guest_old_sigaction, 128, 32);
CHECK_FIELD_LAYOUT(Guest_old_sigaction, guest_sa_sigaction, 0, 32);
CHECK_FIELD_LAYOUT(Guest_old_sigaction, sa_mask, 32, 32);
CHECK_FIELD_LAYOUT(Guest_old_sigaction, sa_flags, 64, 32);
CHECK_FIELD_LAYOUT(Guest_old_sigaction, sa_restorer, 96, 32);

}  // namespace berberis

#ifdef __BIONIC__
#include "signal_emulation_bionic.h"
#endif
#ifdef __GLIBC__
#include "signal_emulation_glibc.h"
#endif

#endif  // BERBERIS_KERNEL_API_SIGNAL_EMULATION_H_
