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

#include <errno.h>
#include <fcntl.h>  // AT_FDCWD, AT_SYMLINK_NOFOLLOW
#include <linux/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "berberis/base/bit_util.h"
#include "berberis/base/logging.h"
#include "berberis/base/scoped_errno.h"
#include "berberis/base/struct_check.h"
#include "berberis/base/tracing.h"
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/instrument/syscall.h"
#include "berberis/kernel_api/main_executable_real_path_emulation.h"
#include "berberis/kernel_api/runtime_bridge.h"
#include "berberis/kernel_api/syscall_emulation_common.h"

#include "epoll_emulation.h"
#include "guest_types.h"

namespace berberis {

namespace {

int FstatatForGuest(int dirfd, const char* path, struct stat* buf, int flags) {
  const char* real_path = nullptr;
  if ((flags & AT_SYMLINK_NOFOLLOW) == 0) {
    real_path = TryReadLinkToMainExecutableRealPath(path);
  }
  return syscall(__NR_newfstatat, dirfd, real_path ? real_path : path, buf, flags);
}

long RunGuestSyscall___NR_execveat(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_execveat");
  errno = ENOSYS;
  return -1;
}

// This needs to be implemented only in arm64->x86_64 translation since it has different names:
// sys_fadvise64_64 on arm64 but sys_fadvise64 on x86_64, thus not being generated automatically

#if defined(__x86_64__)
long RunGuestSyscall___NR_fadvise64(long arg_1, long arg_2, long arg_3, long arg_4) {
  // on 64-bit architectures, sys_fadvise64 and sys_fadvise64_64 are equal.
  return syscall(__NR_fadvise64, arg_1, arg_2, arg_3, arg_4);
}
#endif

long RunGuestSyscall___NR_ioctl(long arg_1, long arg_2, long arg_3) {
  // TODO(b/128614662): translate!
  TRACE("unimplemented ioctl 0x%lx, running host syscall as is", arg_2);
  return syscall(__NR_ioctl, arg_1, arg_2, arg_3);
}

long RunGuestSyscall___NR_newfstatat(long arg_1, long arg_2, long arg_3, long arg_4) {
  struct stat host_stat;
  int result = FstatatForGuest(static_cast<int>(arg_1),       // dirfd
                               bit_cast<const char*>(arg_2),  // path
                               &host_stat,
                               static_cast<int>(arg_4));  // flags
  if (result != -1) {
    berberis::ConvertHostStatToGuestArch(host_stat, bit_cast<GuestAddr>(arg_3));
  }
  return result;
}

#if defined(__x86_64__)
#include "gen_syscall_emulation_arm64_to_x86_64-inl.h"
#elif defined(__riscv)
#include "gen_syscall_emulation_arm64_to_riscv64-inl.h"
#else
#error Unsupported architecture
#endif

}  // namespace

void RunKernelSyscall(ThreadState* state) {
  // ATTENTION: run guest signal handlers instantly!
  // If signal arrives while in a syscall, syscall should immediately return with EINTR.
  // In this case pending signals are OK, as guest handlers will run on return from syscall.
  // BUT, if signal action has SA_RESTART, certain syscalls will restart instead of returning.
  // In this case, pending signals will never run...
  ScopedPendingSignalsDisabler scoped_pending_signals_disabler(state->thread);
  ScopedErrno scoped_errno;

  long guest_nr = state->cpu.x[8];
  if (kInstrumentSyscalls) {
    OnSyscall(state, guest_nr);
  }

  // Linux allows 6 arguments maximum.
  // TODO(b/161722184): if syscall is interrupted by signal, signal handler might overwrite the
  // return value, so setting x0 here might be incorrect. Investigate!
  long result = RunGuestSyscallImpl(state->cpu.x[8],  // instrumentation can redirect syscall!
                                    state->cpu.x[0],
                                    state->cpu.x[1],
                                    state->cpu.x[2],
                                    state->cpu.x[3],
                                    state->cpu.x[4],
                                    state->cpu.x[5]);
  if (result == -1) {
    state->cpu.x[0] = -errno;
  } else {
    state->cpu.x[0] = result;
  }

  if (kInstrumentSyscalls) {
    OnSyscallReturn(state, guest_nr);
  }
}

void RunGuestSyscall(ThreadState* state) {
  RunKernelSyscall(state);
}

}  // namespace berberis
