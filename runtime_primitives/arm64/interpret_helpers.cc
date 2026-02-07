/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "berberis/runtime_primitives/arm64/interpret_helpers.h"

#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>

#include "berberis/base/tracing.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

void UndefinedInsn(GuestAddr pc) {
  TRACE_AND_ALOGE("Undefined instruction 0x%08x at 0x%016zx", *ToHostAddr<uint32_t>(pc), pc);
#ifdef __GLIBC__
  // Our old 2.17 glibc has a bug resulting in raise() failing after any CLONE_VM.
  // Work around by calling tgkill directly.
  pid_t pid = syscall(__NR_getpid);
  pid_t tid = syscall(__NR_gettid);
  syscall(SYS_tgkill, pid, tid, SIGILL);
#else
  raise(SIGILL);
#endif
}

}  // namespace berberis
