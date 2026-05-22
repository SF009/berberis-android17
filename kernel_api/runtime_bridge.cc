/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "berberis/kernel_api/runtime_bridge.h"

#include <bit>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <linux/sched.h>

#include "berberis/base/config.h"
#include "berberis/base/tracing.h"
#include "berberis/guest_os_primitives/guest_signal.h"
#include "berberis/guest_os_primitives/guest_thread.h"
#include "berberis/guest_os_primitives/guest_thread_manager.h"
#include "berberis/kernel_api/sys_mman_emulation.h"

#include "sigevent_emulation.h"

namespace berberis {

namespace {

struct Guest_clone_args {
  uint64_t flags;
  uint64_t pidfd;
  uint64_t child_tid;
  uint64_t parent_tid;
  uint64_t exit_signal;
  uint64_t stack;
  uint64_t stack_size;
  uint64_t tls;
  uint64_t set_tid;
  uint64_t set_tid_size;
  uint64_t cgroup;
};

}  // namespace

long RunGuestSyscall___NR_rt_sigaction(long sig_num_arg,
                                       long act_arg,
                                       long old_act_arg,
                                       long sigset_size_arg) {
  TRACE("'rt_sigaction' called for signal %ld", sig_num_arg);
  int sig_num = static_cast<int>(sig_num_arg);
  const Guest_sigaction* act = std::bit_cast<const Guest_sigaction*>(act_arg);
  Guest_sigaction* old_act = std::bit_cast<Guest_sigaction*>(old_act_arg);
  size_t sigset_size = std::bit_cast<size_t>(sigset_size_arg);

  if (sigset_size != sizeof(Guest_sigset_t)) {
    errno = EINVAL;
    return -1;
  }

  int error;
  if (SetGuestSignalHandler(sig_num, act, old_act, &error)) {
    return 0;
  }
  errno = error;
  return -1;
}

long RunGuestSyscall___NR_sigaltstack(long stack, long old_stack) {
  int error;
  if (GetCurrentGuestThread()->SigAltStack(
          std::bit_cast<const stack_t*>(stack), std::bit_cast<stack_t*>(old_stack), &error)) {
    return 0;
  }
  errno = error;
  return -1;
}

long RunGuestSyscall___NR_timer_create(long arg_1, long arg_2, long arg_3) {
  struct sigevent host_sigevent;
  return syscall(__NR_timer_create,
                 arg_1,
                 ConvertGuestSigeventToHost(std::bit_cast<struct sigevent*>(arg_2), &host_sigevent),
                 arg_3);
}

long RunGuestSyscall___NR_exit(long code) {
  ExitCurrentThread(code);
  return 0;
}

long RunGuestSyscall___NR_clone(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  // NOTE: clone syscall argument ordering is architecture dependent.  This implementation assumes
  // CLONE_BACKWARDS is enabled (tls before child_tid), which is true for both x86 and RISC-V.
  return CloneGuestThread(GetCurrentGuestThread(), arg_1, arg_2, arg_3, arg_4, arg_5);
}

long RunGuestSyscall___NR_clone3(long arg_1, long arg_2) {
  GuestAddr guest_args_addr = static_cast<GuestAddr>(arg_1);
  size_t guest_args_size = static_cast<size_t>(arg_2);

  if (guest_args_size == 0) {
    errno = EINVAL;
    return -1;
  }

  Guest_clone_args local_args = {};
  size_t copy_size = std::min(guest_args_size, sizeof(Guest_clone_args));

  if (guest_args_addr == 0) {
    errno = EFAULT;
    return -1;
  }

  // Copy to ensure that when copy_size is less than sizeof(Guest_clone_args)
  // the rest of the struct is zero-initialized.
  memcpy(&local_args, std::bit_cast<void*>(guest_args_addr), copy_size);

  // If guest_args_size is larger than the size we know about, we must verify
  // that all additional bytes are zero, following the extensible system call
  // convention (copy_struct_from_user contract in the Linux kernel).
  // If any extra byte is non-zero, it means the guest is trying to use a newer
  // feature that we don't support, so we must fail with E2BIG.
  if (guest_args_size > sizeof(Guest_clone_args)) {
    const char* extra_bytes =
        std::bit_cast<const char*>(guest_args_addr) + sizeof(Guest_clone_args);
    size_t extra_size = guest_args_size - sizeof(Guest_clone_args);
    for (size_t i = 0; i < extra_size; ++i) {
      if (extra_bytes[i] != 0) {
        errno = E2BIG;
        return -1;
      }
    }
  }

  // Check for unsupported features.
  if (local_args.set_tid != 0 || local_args.cgroup != 0) {
    TRACE("clone3: set_tid or cgroup is not supported");
    errno = ENOSYS;
    return -1;
  }

  // Combine flags and exit_signal.
  // CSIGNAL is 0xff on Linux.
  int legacy_flags = (static_cast<int>(local_args.flags) & ~0xff) |
                     (static_cast<int>(local_args.exit_signal) & 0xff);

  // Calculate guest_stack_top.
  GuestAddr guest_stack_top = 0;
  if (local_args.stack != 0) {
    guest_stack_top = local_args.stack + local_args.stack_size;
  }

  // Handle pidfd and parent_tid.
  if ((local_args.flags & CLONE_PIDFD) && (local_args.flags & CLONE_PARENT_SETTID)) {
    TRACE("clone3: both CLONE_PIDFD and CLONE_PARENT_SETTID are set, not supported");
    errno = ENOSYS;
    return -1;
  }

  GuestAddr parent_tid_arg = kNullGuestAddr;
  if (local_args.flags & CLONE_PIDFD) {
    parent_tid_arg = local_args.pidfd;
  } else if (local_args.flags & CLONE_PARENT_SETTID) {
    parent_tid_arg = local_args.parent_tid;
  }

  GuestAddr child_tid_arg = kNullGuestAddr;
  if (local_args.flags & CLONE_CHILD_SETTID) {
    child_tid_arg = local_args.child_tid;
  }

  GuestAddr tls_arg = kNullGuestAddr;
  if (local_args.flags & CLONE_SETTLS) {
    tls_arg = local_args.tls;
  }

  return CloneGuestThread(GetCurrentGuestThread(),
                          legacy_flags,
                          guest_stack_top,
                          parent_tid_arg,
                          tls_arg,
                          child_tid_arg);
}

long RunGuestSyscall___NR_mmap(long arg_1,
                               long arg_2,
                               long arg_3,
                               long arg_4,
                               long arg_5,
                               long arg_6) {
  return std::bit_cast<long>(MmapForGuest(std::bit_cast<void*>(arg_1),    // addr
                                          std::bit_cast<size_t>(arg_2),   // length
                                          static_cast<int>(arg_3),        // prot
                                          static_cast<int>(arg_4),        // flags
                                          static_cast<int>(arg_5),        // fd
                                          static_cast<off64_t>(arg_6)));  // offset
}

long RunGuestSyscall___NR_mmap2(long arg_1,
                                long arg_2,
                                long arg_3,
                                long arg_4,
                                long arg_5,
                                long arg_6) {
  return std::bit_cast<long>(
      MmapForGuest(std::bit_cast<void*>(arg_1),                             // addr
                   std::bit_cast<size_t>(arg_2),                            // length
                   static_cast<int>(arg_3),                                 // prot
                   static_cast<int>(arg_4),                                 // flags
                   static_cast<int>(arg_5),                                 // fd
                   static_cast<off64_t>(arg_6) * config::kGuestPageSize));  // pgoffset to offset
}

long RunGuestSyscall___NR_munmap(long arg_1, long arg_2) {
  return static_cast<long>(MunmapForGuest(std::bit_cast<void*>(arg_1),     // addr
                                          std::bit_cast<size_t>(arg_2)));  // length
}

long RunGuestSyscall___NR_mprotect(long arg_1, long arg_2, long arg_3) {
  return static_cast<long>(MprotectForGuest(std::bit_cast<void*>(arg_1),   // addr
                                            std::bit_cast<size_t>(arg_2),  // length
                                            static_cast<int>(arg_3)));     // prot
}

long RunGuestSyscall___NR_mremap(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  return std::bit_cast<long>(MremapForGuest(std::bit_cast<void*>(arg_1),    // old_addr
                                            std::bit_cast<size_t>(arg_2),   // old_size
                                            std::bit_cast<size_t>(arg_3),   // new_size
                                            static_cast<int>(arg_4),        // flags
                                            std::bit_cast<void*>(arg_5)));  // new_addr
}

}  // namespace berberis
