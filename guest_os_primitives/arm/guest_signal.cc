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

#include "berberis/guest_os_primitives/guest_signal.h"

#include <cstring>  // memcmp

#include "berberis/base/logging.h"
#include "berberis/guest_os_primitives/guest_signal_arch.h"
#include "berberis/guest_state/guest_addr.h"  // ToHostAddr

namespace berberis {

size_t GetGuest_MINSIGSTKSZ() {
  // See bionic/libc/kernel/uapi/asm-arm/asm/signal.h
  return 2048;
}

void CheckSigactionRestorer(const Guest_sigaction* guest_sa) {
  const GuestAddr sa_restorer = guest_sa->sa_restorer;

  // ATTENTION: kernel tolerates the case when SA_RESTORER is set but sa_restorer is null!
  if (!sa_restorer) {
    return;
  }

  if (bool(sa_restorer & 0x1)) {
    const char* handler = ToHostAddr<const char>(sa_restorer - 1);
    if ((memcmp(handler, "\x77\x27\x00\xdf", 4) != 0) &&  // Thumb sigreturn
        (memcmp(handler, "\xad\x27\x00\xdf", 4) != 0)) {  // Thumb rt_sigreturn
      LOG_ALWAYS_FATAL("Unknown Thumb sa_restorer in guest sigaction!");
    }
  } else {
    const char* handler = ToHostAddr<const char>(sa_restorer);
    if ((memcmp(handler, "\x77\x70\xa0\xe3\x00\x00\x00\xef", 8) != 0) &&  // ARM sigreturn
        (memcmp(handler, "\xad\x70\xa0\xe3\x00\x00\x00\xef", 8) != 0)) {  // ARM rt_sigreturn
      LOG_ALWAYS_FATAL("Unknown ARM sa_restorer in guest sigaction!");
    }
  }
}

void ResetSigactionRestorer(Guest_sigaction* guest_sa) {
  guest_sa->sa_restorer = 0;
}

}  // namespace berberis
