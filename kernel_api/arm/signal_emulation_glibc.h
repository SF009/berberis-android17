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

#ifndef BERBERIS_KERNEL_API_SIGNAL_EMULATION_GLIBC_H_
#define BERBERIS_KERNEL_API_SIGNAL_EMULATION_GLIBC_H_

#include <signal.h>

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

CHECK_STRUCT_LAYOUT(siginfo_t, 1024, 32);
CHECK_FIELD_LAYOUT(siginfo_t, si_signo, 0, 32);
CHECK_FIELD_LAYOUT(siginfo_t, si_errno, 32, 32);
CHECK_FIELD_LAYOUT(siginfo_t, si_code, 64, 32);
CHECK_FIELD_LAYOUT(siginfo_t, _sifields, 96, 928);

// We want to test layout of some struct types here, but they are anonymous.
// Dereference nullptr to yield fake field value that can be used in decltype.
//
// Note: since we not actually dereferencing anything here, but just use decltype we
// don't have undefined behavior.
#define SIFIELD_TYPE(name) decltype(static_cast<siginfo_t*>(nullptr)->_sifields.name)

// These defines conflict with CHECK_FIELD_LAYOUT tests below so we need to undef them.
// E.g. si_pid is defined as _sifields._kill.si_pid which means that it's impossible to
// refer to _sifields._sigchld.si_pid with that define in place.
#undef si_pid
#undef si_uid
#undef si_timerid
#undef si_overrun
#undef si_status
#undef si_utime
#undef si_stime
#undef si_value
#undef si_int
#undef si_ptr
#undef si_addr
#undef si_addr_lsb
#undef si_lower
#undef si_upper
#undef si_pkey
#undef si_band
#undef si_fd
#undef si_call_addr
#undef si_syscall
#undef si_arch

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_pad), 928, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_kill), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_kill), si_pid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_kill), si_uid, 32, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_timer), 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), si_tid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), si_overrun, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), si_sigval, 64, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_rt), 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_rt), si_pid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_rt), si_uid, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_rt), si_sigval, 64, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigchld), 160, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), si_pid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), si_uid, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), si_status, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), si_utime, 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), si_stime, 128, 32);

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 18
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault), 32, 32);
#else
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 22
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault), 64, 32);
#else
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault), 128, 32);
#endif
#endif
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), si_addr, 0, 32);

#if __GLIBC__ == 2 && __GLIBC_MINOR__ >= 18
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), si_addr_lsb, 32, 16);
#endif

#if __GLIBC__ == 2 && __GLIBC_MINOR__ >= 22 && __GLIBC_MINOR__ < 26
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault.si_addr_bnd), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), si_addr_bnd._lower, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), si_addr_bnd._upper, 96, 32);
#endif
#if __GLIBC__ == 2 && __GLIBC_MINOR__ >= 26
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault._bounds.si_addr_bnd), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _bounds._addr_bnd._lower, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _bounds._addr_bnd._upper, 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _bounds._pkey, 64, 32);
#endif

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigpoll), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigpoll), si_band, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigpoll), si_fd, 32, 32);

#ifdef __SI_HAVE_SIGSYS
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigsys), 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigsys), _call_addr, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigsys), _syscall, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigsys), _arch, 64, 32);
#endif

#undef SIFIELD_TYPE

// On glibc sigset_t is an array.
CHECK_STRUCT_LAYOUT(sigset_t, 1024, 32);

}  // namespace berberis

#endif  // BERBERIS_KERNEL_API_SIGNAL_EMULATION_GLIBC_H_
