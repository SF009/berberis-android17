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
#include <inttypes.h>
#include <linux/net.h>  // socketcall
#include <linux/unistd.h>
#include <sys/types.h>

#include <bit>
#include <cstddef>

#include "berberis/base/logging.h"
#include "berberis/base/scoped_errno.h"
#include "berberis/base/tracing.h"
#include "berberis/guest_os_primitives/guest_thread.h"
#include "berberis/guest_os_primitives/guest_thread_manager.h"  // GetCurrentGuestThread
#include "berberis/guest_os_primitives/scoped_pending_signals.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/instrument/syscall.h"
#include "berberis/kernel_api/main_executable_real_path_emulation.h"
#include "berberis/kernel_api/runtime_bridge.h"
#include "berberis/kernel_api/syscall_emulation_common.h"
#include "berberis/kernel_api/unistd_emulation.h"
#include "berberis/runtime_primitives/runtime_library.h"

#include "epoll_emulation.h"
#include "signal_emulation.h"
#include "sys_ioctl_emulation.h"
#include "sys_stat_emulation.h"
#include "sys_vfs_emulation.h"  // kGuestStatfs64Size

namespace berberis {

namespace {

int Fstatat64ForGuest(int dirfd, const char* path, struct stat64* buf, int flags) {
  const char* real_path = nullptr;
  if ((flags & AT_SYMLINK_NOFOLLOW) == 0) {
    real_path = TryReadLinkToMainExecutableRealPath(path);
  }
  return syscall(__NR_fstatat64, dirfd, real_path ? real_path : path, buf, flags);
}

long RunGuestSyscall___ARM_NR_cacheflush(long arg_1, long arg_2, long arg_3) {
  // ATTENTION: on arm, arg_2 is address range end, not address range size!
  // ATTENTION: on arm, arg_3 should be 0.
  // See <kernel>/arch/arm/kernel/traps.c
  auto start = std::bit_cast<GuestAddr>(arg_1);
  auto end = std::bit_cast<GuestAddr>(arg_2);
  if (end < start || arg_3 != 0) {
    errno = EINVAL;
    return -1;
  }

  TRACE("icache flush [0x%x, 0x%x)", start, end);
  InvalidateGuestRange(start, end);
  return 0;
}

long RunGuestSyscall___ARM_NR_set_tls(long arg_1) {
  GetCurrentGuestThread()->state()->tls = std::bit_cast<GuestAddr>(arg_1);
  return 0;
}

long RunGuestSyscall___NR_arm_fadvise64_64(long arg_1,
                                           long arg_2,
                                           long arg_3,
                                           long arg_4,
                                           long arg_5,
                                           long arg_6) {
  // arm entry is sys_arm_fadvise64_64(int fd, int advice, loff_t offset, loff_t len)
  // x86 entry is sys_fadvise64_64(int fd, loff_t offset, loff_t len, int advice)
  return syscall(__NR_fadvise64_64, arg_1, arg_3, arg_4, arg_5, arg_6, arg_2);
}

long RunGuestSyscall___NR_arm_sync_file_range(long arg_1,
                                              long arg_2,
                                              long arg_3,
                                              long arg_4,
                                              long arg_5,
                                              long arg_6) {
  // arm entry is sys_sync_file_range2(fd, flags, offset, nbytes)
  // x86 entry is sys_sync_file_range(fd, offset, nbytes, flags)
  return syscall(__NR_sync_file_range, arg_1, arg_3, arg_4, arg_5, arg_6, arg_2);
}

long RunGuestSyscall___NR_bpf(long arg_1, long arg_2, long arg_3) {
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_bpf");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_epoll_wait(long arg_1, long arg_2, long arg_3, long arg_4) {
  return RunGuestSyscall___NR_epoll_pwait(arg_1, arg_2, arg_3, arg_4, 0L, 0L);
}

long RunGuestSyscall___NR_execveat(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_execveat");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_fcntl64(long arg_1, long arg_2, long arg_3) {
  return berberis::GuestFcntl(arg_1, arg_2, arg_3);
}

long RunGuestSyscall___NR_fork() {
  GuestThread* thread = GetCurrentGuestThread();
  long pid = syscall(__NR_fork);
  if (pid == 0) {
    // Child, reset thread table.
    ResetCurrentGuestThreadAfterFork(thread);
  }
  return pid;
}

long RunGuestSyscall___NR_fstat64(long arg_1, long arg_2) {
  struct stat64 host_stat;
  int res = syscall(__NR_fstat64, arg_1, &host_stat);
  if (res != -1) {
    ConvertHostStat64ToGuest(&host_stat, std::bit_cast<Guest_stat64*>(arg_2));
  }
  return res;
}

long RunGuestSyscall___NR_fstatat64(long arg_1, long arg_2, long arg_3, long arg_4) {
  struct stat64 host_stat;
  int res = Fstatat64ForGuest(static_cast<int>(arg_1),            // dirfd
                              std::bit_cast<const char*>(arg_2),  // path
                              &host_stat,
                              static_cast<int>(arg_4));  // flags
  if (res != -1) {
    ConvertHostStat64ToGuest(&host_stat, std::bit_cast<Guest_stat64*>(arg_3));
  }
  return res;
}

long RunGuestSyscall___NR_fstatfs64(long arg_1, long arg_2, long arg_3) {
  // struct matches it's in-kernel counterpart exactly and returns -EINVAL
  // if that's not true.  fstatfs64 in x86 uses smaller data structure
  // but it differs from ARM one only by padding at the end thus we consider
  // them compatible - but we must now do that check here since kernel no
  // longer observes that size.
  if (arg_2 != kGuestStatfs64Size) {
    errno = EINVAL;
    return -1;
  }
  return syscall(__NR_fstatfs64, arg_1, sizeof(struct statfs64), arg_3);
}

long RunGuestSyscall___NR_getdents64(long arg_1, long arg_2, long arg_3) {
  // A potential discrepancy exists in the `d_reclen` field: ARM-based kernels typically
  // return 8-byte aligned `dirent` structures, whereas x86-based kernels return
  // 4-byte aligned ones. This alignment difference is primarily relevant for
  // older ARM architectures that strictly enforce aligned memory access.
  //
  // Modern ARMv7 CPUs are required to support unaligned memory access, and this
  // is the exclusive mode supported by berberis. In this mode, no data
  // structure conversions are necessary, as the layouts are identical apart
  // from the alignment requirement.
  return syscall(__NR_getdents64, arg_1, arg_2, arg_3);
}

long RunGuestSyscall___NR_ioctl(long arg_1, long arg_2, long arg_3) {
  return IoCtlForGuest(arg_1, arg_2, arg_3);
}

long RunGuestSyscall___NR_lstat64(long arg_1, long arg_2) {
  UNUSED(arg_1, arg_2);
  TRACE("unimplemented syscall __NR_lstat64");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_mq_notify(long arg_1, long arg_2) {
  UNUSED(arg_1, arg_2);
  TRACE("unimplemented syscall __NR_mq_notify");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_msgctl(long arg_1, long arg_2, long arg_3) {
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_msgctl");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_open(long arg_1, long arg_2, long arg_3) {
  return static_cast<long>(berberis::OpenForGuest(std::bit_cast<const char*>(arg_1),  // path
                                                  static_cast<int>(arg_2),            // flags
                                                  static_cast<mode_t>(arg_3)));       // mode
}

long RunGuestSyscall___NR_pciconfig_read(long arg_1,
                                         long arg_2,
                                         long arg_3,
                                         long arg_4,
                                         long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_pciconfig_read");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_pciconfig_write(long arg_1,
                                          long arg_2,
                                          long arg_3,
                                          long arg_4,
                                          long arg_5) {
  UNUSED(arg_1, arg_2, arg_3, arg_4, arg_5);
  TRACE("unimplemented syscall __NR_pciconfig_write");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_readlink(long arg_1, long arg_2, long arg_3) {
  return static_cast<long>(ReadLinkForGuest(std::bit_cast<const char*>(arg_1),  // path
                                            std::bit_cast<char*>(arg_2),        // buf
                                            std::bit_cast<size_t>(arg_3)));     // buf_size
}

long RunGuestSyscall___NR_semctl(long arg_1, long arg_2, long arg_3, long arg_4) {
  UNUSED(arg_1, arg_2, arg_3, arg_4);
  TRACE("unimplemented syscall __NR_semctl");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_semop(long arg_1, long arg_2, long arg_3) {
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_semop");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_semtimedop(long arg_1, long arg_2, long arg_3, long arg_4) {
  UNUSED(arg_1, arg_2, arg_3, arg_4);
  TRACE("unimplemented syscall __NR_semtimedop");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_shmctl(long arg_1, long arg_2, long arg_3) {
  UNUSED(arg_1, arg_2, arg_3);
  TRACE("unimplemented syscall __NR_shmctl");
  errno = ENOSYS;
  return -1;
}

long RunGuestSyscall___NR_sigaction(long arg_1, long arg_2, long arg_3) {
  TRACE("'sigaction' called for signal %ld", arg_1);

  Guest_sigaction kernel_act;
  const Guest_sigaction* new_kernel_act = nullptr;

  const Guest_old_sigaction* new_act = std::bit_cast<const Guest_old_sigaction*>(arg_2);
  if (new_act) {
    kernel_act.guest_sa_sigaction = new_act->guest_sa_sigaction;
    kernel_act.sa_flags = new_act->sa_flags;
    kernel_act.sa_restorer = new_act->sa_restorer;
    ConvertToBigSigset(new_act->sa_mask, &kernel_act.sa_mask);
    new_kernel_act = &kernel_act;
  }

  // Note: we couldn't just pass address of errno into SetGuestSignalHandler because it calls
  // various libc functions inside.  Thus we need temporary variable here;
  int error;
  if (!SetGuestSignalHandler(arg_1, new_kernel_act, &kernel_act, &error)) {
    errno = error;
    return -1;
  }

  Guest_old_sigaction* old_act = std::bit_cast<Guest_old_sigaction*>(arg_3);
  if (old_act) {
    old_act->guest_sa_sigaction = kernel_act.guest_sa_sigaction;
    old_act->sa_flags = kernel_act.sa_flags;
    old_act->sa_restorer = kernel_act.sa_restorer;
    ConvertToSmallSigset(kernel_act.sa_mask, &old_act->sa_mask);
  }

  return 0;
}

long RunGuestSyscall___NR_stat64(long arg_1, long arg_2) {
  struct stat64 host_stat;
  int res = Fstatat64ForGuest(AT_FDCWD,                           // dirfd
                              std::bit_cast<const char*>(arg_1),  // path
                              &host_stat,
                              0);  // flags
  if (res != -1) {
    ConvertHostStat64ToGuest(&host_stat, std::bit_cast<Guest_stat64*>(arg_2));
  }
  return res;
}

long RunGuestSyscall___NR_statfs64(long arg_1, long arg_2, long arg_3) {
  // See fstatfs64.
  if (arg_2 != kGuestStatfs64Size) {
    errno = EINVAL;
    return -1;
  }
  return syscall(__NR_statfs64, arg_1, sizeof(struct statfs64), arg_3);
}

long RunGuestSyscall___NR_vfork() {
  // Posix vfork does not allow child to call functions except _exit or exec*.
  // With binary translation, we'll likely need to translate child code before
  // running it, which is barely possible without calling functions.
  // Linux vfork suspends calling thread until child exits or execs.
  // Unfortunately, suspending until child execs is not easy...
  // Fortunately, standards say programs should not rely on this.
  TRACE("replaced vfork() with fork()");
  return RunGuestSyscall___NR_fork();
}

// socketcall - needed for old x86 kernels
//
// TODO(eaeltsin): maybe remove when all supported kernels have direct system calls!

long RunGuestSyscall___NR_accept(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3, 0};
  return syscall(__NR_socketcall, SYS_ACCEPT4, args);
}

long RunGuestSyscall___NR_accept4(long arg_1, long arg_2, long arg_3, long arg_4) {
  long args[] = {arg_1, arg_2, arg_3, arg_4};
  return syscall(__NR_socketcall, SYS_ACCEPT4, args);
}

long RunGuestSyscall___NR_bind(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_BIND, args);
}

long RunGuestSyscall___NR_connect(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_CONNECT, args);
}

long RunGuestSyscall___NR_getpeername(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_GETPEERNAME, args);
}

long RunGuestSyscall___NR_getsockname(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_GETSOCKNAME, args);
}

long RunGuestSyscall___NR_getsockopt(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, arg_5};
  return syscall(__NR_socketcall, SYS_GETSOCKOPT, args);
}

long RunGuestSyscall___NR_listen(long arg_1, long arg_2) {
  long args[] = {arg_1, arg_2};
  return syscall(__NR_socketcall, SYS_LISTEN, args);
}

long RunGuestSyscall___NR_recv(long arg_1, long arg_2, long arg_3, long arg_4) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, 0, 0};
  return syscall(__NR_socketcall, SYS_RECVFROM, args);
}

long RunGuestSyscall___NR_recvfrom(long arg_1,
                                   long arg_2,
                                   long arg_3,
                                   long arg_4,
                                   long arg_5,
                                   long arg_6) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, arg_5, arg_6};
  return syscall(__NR_socketcall, SYS_RECVFROM, args);
}

long RunGuestSyscall___NR_recvmmsg(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, arg_5};
  return syscall(__NR_socketcall, SYS_RECVMMSG, args);
}

long RunGuestSyscall___NR_recvmsg(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_RECVMSG, args);
}

long RunGuestSyscall___NR_send(long arg_1, long arg_2, long arg_3, long arg_4) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, 0, 0};
  return syscall(__NR_socketcall, SYS_SENDTO, args);
}

long RunGuestSyscall___NR_sendmmsg(long arg_1, long arg_2, long arg_3, long arg_4) {
  long args[] = {arg_1, arg_2, arg_3, arg_4};
  return syscall(__NR_socketcall, SYS_SENDMMSG, args);
}

long RunGuestSyscall___NR_sendmsg(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_SENDMSG, args);
}

long RunGuestSyscall___NR_sendto(long arg_1,
                                 long arg_2,
                                 long arg_3,
                                 long arg_4,
                                 long arg_5,
                                 long arg_6) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, arg_5, arg_6};
  return syscall(__NR_socketcall, SYS_SENDTO, args);
}

long RunGuestSyscall___NR_setsockopt(long arg_1, long arg_2, long arg_3, long arg_4, long arg_5) {
  long args[] = {arg_1, arg_2, arg_3, arg_4, arg_5};
  return syscall(__NR_socketcall, SYS_SETSOCKOPT, args);
}

long RunGuestSyscall___NR_shutdown(long arg_1, long arg_2) {
  long args[] = {arg_1, arg_2};
  return syscall(__NR_socketcall, SYS_SHUTDOWN, args);
}

long RunGuestSyscall___NR_socket(long arg_1, long arg_2, long arg_3) {
  long args[] = {arg_1, arg_2, arg_3};
  return syscall(__NR_socketcall, SYS_SOCKET, args);
}

long RunGuestSyscall___NR_socketpair(long arg_1, long arg_2, long arg_3, long arg_4) {
  long args[] = {arg_1, arg_2, arg_3, arg_4};
  return syscall(__NR_socketcall, SYS_SOCKETPAIR, args);
}

#include "gen_syscall_emulation_arm-inl.h"

}  // namespace

void RunKernelSyscall(ThreadState* state) {
  // ATTENTION: run guest signal handlers instantly!
  // If signal arrives while in a syscall, syscall should immediately return with EINTR.
  // In this case pending signals are OK, as guest handlers will run on return from syscall.
  // BUT, if signal action has SA_RESTART, certain syscalls will restart instead of returning.
  // In this case, pending signals will never run...
  ScopedPendingSignalsDisabler scoped_pending_signals_disabler(state->thread);
  ScopedErrno scoped_errno;

  long guest_nr = state->cpu.r[7];
  if (kInstrumentSyscalls) {
    OnSyscall(state, guest_nr);
  }

  // Per ARM specification syscalls could support 7 arguments, but Linux only uses 6 because on
  // x86 there are only 6 registers available.
  // TODO(b/161722184): if syscall is interrupted by signal, signal handler might overwrite the
  // return value, so setting r0 here might be incorrect. Investigate!
  long result = RunGuestSyscallImpl(state->cpu.r[7],  // instrumentation can redirect syscall!
                                    state->cpu.r[0],
                                    state->cpu.r[1],
                                    state->cpu.r[2],
                                    state->cpu.r[3],
                                    state->cpu.r[4],
                                    state->cpu.r[5]);
  if (result == -1) {
    state->cpu.r[0] = -errno;
  } else {
    state->cpu.r[0] = result;
  }

  if (kInstrumentSyscalls) {
    OnSyscallReturn(state, guest_nr);
  }
}

void RunGuestSyscall(ThreadState* state) {
  RunKernelSyscall(state);
}

}  // namespace berberis
