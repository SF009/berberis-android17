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

#ifndef BERBERIS_KERNEL_API_ARM_GUEST_TYPES_ARCH_H_
#define BERBERIS_KERNEL_API_ARM_GUEST_TYPES_ARCH_H_

#include <dirent.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/epoll.h>
#include <sys/file.h>

#include <climits>

#include "berberis/base/struct_check.h"

namespace berberis {

static_assert(EPOLL_CTL_ADD == 1);
static_assert(EPOLL_CTL_DEL == 2);
static_assert(EPOLL_CTL_MOD == 3);
static_assert(EPOLL_CLOEXEC == 02000000);

struct Guest_epoll_event {
  uint32_t events;
  alignas(64 / CHAR_BIT) uint64_t data;
};

// Verify precise layouts so ConvertHostEPollEventArrayToGuestInPlace is safe.
CHECK_STRUCT_LAYOUT(Guest_epoll_event, 128, 64);
CHECK_FIELD_LAYOUT(Guest_epoll_event, events, 0, 32);
CHECK_FIELD_LAYOUT(Guest_epoll_event, data, 64, 64);
static_assert(sizeof(epoll_event) <= sizeof(Guest_epoll_event));
static_assert(alignof(epoll_event) <= alignof(Guest_epoll_event));

// Guest dirent is more aligned than host dirent and thus has greater size.
// However, offsets and sizes of all fields are equal, so guest dirent can be
// used in place of host dirent. Vice versa is safe, too, since getdents
// interface does not use dirent structures as "normal" structures - instead
// d_reclen field contains size of structure on return!
CHECK_STRUCT_LAYOUT(dirent64, 2208, 32);
CHECK_FIELD_LAYOUT(dirent64, d_ino, 0, 64);
CHECK_FIELD_LAYOUT(dirent64, d_off, 64, 64);
CHECK_FIELD_LAYOUT(dirent64, d_reclen, 128, 16);
CHECK_FIELD_LAYOUT(dirent64, d_type, 144, 8);
CHECK_FIELD_LAYOUT(dirent64, d_name, 152, 2048);

// For PR_SET_SECCOMP.
CHECK_STRUCT_LAYOUT(struct sock_fprog, 64, 32);
CHECK_FIELD_LAYOUT(struct sock_fprog, len, 0, 16);
CHECK_FIELD_LAYOUT(struct sock_fprog, filter, 32, 32);

CHECK_STRUCT_LAYOUT(struct sock_filter, 64, 32);
CHECK_FIELD_LAYOUT(struct sock_filter, code, 0, 16);
CHECK_FIELD_LAYOUT(struct sock_filter, jt, 16, 8);
CHECK_FIELD_LAYOUT(struct sock_filter, jf, 24, 8);
CHECK_FIELD_LAYOUT(struct sock_filter, jf, 24, 8);
CHECK_FIELD_LAYOUT(struct sock_filter, k, 32, 32);

}  // namespace berberis

#endif  // BERBERIS_KERNEL_API_ARM_GUEST_TYPES_ARCH_H_
