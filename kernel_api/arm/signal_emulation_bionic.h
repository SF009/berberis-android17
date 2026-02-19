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

#ifndef BERBERIS_KERNEL_API_SIGNAL_EMULATION_BIONIC_H_
#define BERBERIS_KERNEL_API_SIGNAL_EMULATION_BIONIC_H_

#include <signal.h>

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

CHECK_STRUCT_LAYOUT(sigset64_t, 64, 32);

CHECK_STRUCT_LAYOUT(siginfo_t, 1024, 32);
CHECK_FIELD_LAYOUT(siginfo_t, si_signo, 0, 32);
CHECK_FIELD_LAYOUT(siginfo_t, si_errno, 32, 32);
CHECK_FIELD_LAYOUT(siginfo_t, si_code, 64, 32);

// We want to test layout of some struct types here, but they are anonymous.
// Dereference nullptr to yield fake field value that can be used in decltype.
//
// Note: since we not actually dereferencing anything here, but just use decltype we
// don't have undefined behavior.
#define SIFIELD_TYPE(name) decltype(static_cast<siginfo_t*>(nullptr)->_sifields.name)

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_kill), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_kill), _pid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_kill), _uid, 32, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_timer), 128, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), _tid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), _overrun, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), _sigval, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_timer), _sys_private, 96, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_rt), 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_rt), _pid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_rt), _uid, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_rt), _sigval, 64, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigchld), 160, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), _pid, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), _uid, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), _status, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), _utime, 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigchld), _stime, 128, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault), 128, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr_lsb, 32, 16);

#if __has_include(<linux/arm_sdei.h>)
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault._addr_bnd), 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr_bnd._lower, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr_bnd._upper, 96, 32);
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault._addr_pkey), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr_pkey._pkey, 64, 32);
#else
CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigfault._addr_bnd), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr_bnd._lower, 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _addr_bnd._upper, 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigfault), _pkey, 64, 32);
#endif

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigpoll), 64, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigpoll), _band, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigpoll), _fd, 32, 32);

CHECK_STRUCT_LAYOUT(SIFIELD_TYPE(_sigsys), 96, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigsys), _call_addr, 0, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigsys), _syscall, 32, 32);
CHECK_FIELD_LAYOUT(SIFIELD_TYPE(_sigsys), _arch, 64, 32);

#undef SIFIELD_TYPE

}  // namespace berberis

#endif  // BERBERIS_KERNEL_API_SIGNAL_EMULATION_BIONIC_H_
