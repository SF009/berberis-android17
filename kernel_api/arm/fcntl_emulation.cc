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

// We need 32-bit functions here. Suppress unconditional use of 64-bit offsets.
// Functions with 64-bit offsets are still available when used with "64" suffix.
//
// Note: this is actually only needed for host build since Android build system
// insists on defining _FILE_OFFSET_BITS=64 for host-host binaries.
//
// _FILE_OFFSET_BITS is NOT defined when we are building target-host binaries.
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif

#include "berberis/kernel_api/fcntl_emulation.h"

#include <fcntl.h>
#include <sys/file.h>

#include <bit>
#include <cstddef>
#include <tuple>

#include "berberis/base/bit_util.h"
#include "berberis/base/logging.h"
#include "berberis/base/struct_check.h"

static_assert(F_GETLK64 == 12);
static_assert(F_SETLK64 == 13);
static_assert(F_SETLKW64 == 14);

namespace berberis {

#ifdef ANDROID_HOST_MUSL
struct Guest_flock {
  int16_t l_type;
  int16_t l_whence;
  int32_t l_start;
  int32_t l_len;
  int32_t l_pid;
};
#else
using Guest_flock = struct flock;
#endif

struct Guest_flock64 {
  int16_t l_type;
  int16_t l_whence;
  alignas(64 / CHAR_BIT) int64_t l_start;
  alignas(64 / CHAR_BIT) int64_t l_len;
  int32_t l_pid;
};

CHECK_STRUCT_LAYOUT(Guest_flock, 128, 32);
CHECK_FIELD_LAYOUT(Guest_flock, l_type, 0, 16);
CHECK_FIELD_LAYOUT(Guest_flock, l_whence, 16, 16);
CHECK_FIELD_LAYOUT(Guest_flock, l_start, 32, 32);
CHECK_FIELD_LAYOUT(Guest_flock, l_len, 64, 32);
CHECK_FIELD_LAYOUT(Guest_flock, l_pid, 96, 32);

CHECK_STRUCT_LAYOUT(Guest_flock64, 256, 64);
CHECK_FIELD_LAYOUT(Guest_flock64, l_type, 0, 16);
CHECK_FIELD_LAYOUT(Guest_flock64, l_whence, 16, 16);
CHECK_FIELD_LAYOUT(Guest_flock64, l_start, 64, 64);
CHECK_FIELD_LAYOUT(Guest_flock64, l_len, 128, 64);
CHECK_FIELD_LAYOUT(Guest_flock64, l_pid, 192, 32);

namespace {

#define GUEST_F_GETLK 5
#define GUEST_F_SETLK 6
#define GUEST_F_SETLKW 7
#define GUEST_F_GETLK64 12
#define GUEST_F_SETLK64 13
#define GUEST_F_SETLKW64 14

#if defined(ANDROID_HOST_MUSL)
// Musl only has a 64-bit flock that it uses for flock and flock64.
const struct flock64* ConvertGuestFlockToHostFlock64(const Guest_flock* guest,
                                                     struct flock64* host) {
  if (!guest) {
    return nullptr;
  }
  *host = {guest->l_type, guest->l_whence, guest->l_start, guest->l_len, guest->l_pid};
  return host;
}

void ConvertHostFlock64ToGuestFlock(const struct flock64* host, Guest_flock* guest) {
  CHECK_NE(guest, nullptr);
  CHECK_LE(host->l_start, INT32_MAX);
  CHECK_GE(host->l_start, INT32_MIN);
  CHECK_LE(host->l_len, INT32_MAX);
  CHECK_GE(host->l_len, INT32_MIN);
  *guest = {host->l_type,
            host->l_whence,
            static_cast<int32_t>(host->l_start),
            static_cast<int32_t>(host->l_len),
            host->l_pid};
}
#endif

const struct flock64* ConvertGuestFlock64ToHost(const Guest_flock64* guest, struct flock64* host) {
  if (!guest) {
    return nullptr;
  }
  *host = {guest->l_type, guest->l_whence, guest->l_start, guest->l_len, guest->l_pid};
  return host;
}

void ConvertHostFlock64ToGuest(const struct flock64* host, Guest_flock64* guest) {
  CHECK_NE(guest, nullptr);
  *guest = {host->l_type, host->l_whence, host->l_start, host->l_len, host->l_pid};
}

}  // namespace

std::tuple<bool, int> GuestFcntlArch(int fd, int cmd, long arg_3) {
  switch (cmd) {
#ifdef ANDROID_HOST_MUSL
    case GUEST_F_SETLK:
    case GUEST_F_SETLKW:
    case GUEST_F_GETLK: {
      // Musl only has a 64-bit flock for both flock and flock64, translate flock calls to flock64.
      Guest_flock* guest_flock = std::bit_cast<Guest_flock*>(arg_3);
      struct flock64 host_flock64;
      // In case of GETLK input flock describes region
      // to check, thus conversion is also required.
      auto result = fcntl(fd,
                          cmd + F_SETLK - GUEST_F_SETLK,
                          ConvertGuestFlockToHostFlock64(guest_flock, &host_flock64));
      if (result == 0 && cmd == GUEST_F_GETLK) {
        // Output contains the result of lock check.
        ConvertHostFlock64ToGuestFlock(&host_flock64, guest_flock);
      }
      return {true, result};
    }
#endif
    // These require struct flock64 conversion.
    case GUEST_F_SETLK64:
    case GUEST_F_SETLKW64:
    case GUEST_F_GETLK64: {
      Guest_flock64* guest_flock64 = std::bit_cast<Guest_flock64*>(arg_3);
      struct flock64 host_flock64;
      // In case of GETLK input flock describes region
      // to check, thus conversion is also required.
      auto result = fcntl(fd, cmd, ConvertGuestFlock64ToHost(guest_flock64, &host_flock64));
      if (result == 0 && cmd == F_GETLK64) {
        // Output contains the result of lock check.
        ConvertHostFlock64ToGuest(&host_flock64, guest_flock64);
      }
      return {true, result};
    }
    default:
      return {false, -1};
  }
}

}  // namespace berberis
