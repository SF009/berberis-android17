/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef BERBERIS_KERNEL_API_SYS_STAT_EMULATION_H_
#define BERBERIS_KERNEL_API_SYS_STAT_EMULATION_H_

#include <inttypes.h>
#include <stdint.h>
#if defined(ANDROID_HOST_MUSL) && defined(__i386__)
// Musl's stat64 contains extra fields for their time64 ABI compatibility,
// use the kernel one instead.
#include <asm/stat.h>
#else
#include <sys/stat.h>  // struct stat
#endif

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

#if defined(ANDROID_HOST_MUSL) && defined(__i386__)
struct Guest_timespec {
  uint32_t tv_sec;
  uint32_t tv_nsec;
};
#else
using Guest_timespec = struct timespec;
#endif

struct Guest_stat64 {
  alignas(8) uint64_t st_dev;
  unsigned char __pad0[4];
  uint32_t __st_ino;
  uint32_t st_mode;
  uint32_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  alignas(8) uint64_t st_rdev;
  unsigned char __pad3[4];
  alignas(8) int64_t st_size;
  uint32_t st_blksize;
  alignas(8) uint64_t st_blocks;
  Guest_timespec st_atim;
  Guest_timespec st_mtim;
  Guest_timespec st_ctim;
  alignas(8) uint64_t st_ino;
};

CHECK_STRUCT_LAYOUT(Guest_stat64, 832, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, st_dev, 0, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, __pad0, 64, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, __st_ino, 96, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_mode, 128, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_nlink, 160, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_uid, 192, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_gid, 224, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_rdev, 256, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, __pad3, 320, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_size, 384, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, st_blksize, 448, 32);
CHECK_FIELD_LAYOUT(Guest_stat64, st_blocks, 512, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, st_atim, 576, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, st_mtim, 640, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, st_ctim, 704, 64);
CHECK_FIELD_LAYOUT(Guest_stat64, st_ino, 768, 64);

CHECK_STRUCT_LAYOUT(struct stat64, 768, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_dev, 0, 64);
// CHECK_FIELD_LAYOUT(struct stat64, __pad0, 64, 32);
CHECK_FIELD_LAYOUT(struct stat64, __st_ino, 96, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_mode, 128, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_nlink, 160, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_uid, 192, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_gid, 224, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_rdev, 256, 64);
// CHECK_FIELD_LAYOUT(struct stat64, __pad3, 320, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_size, 352, 64);
CHECK_FIELD_LAYOUT(struct stat64, st_blksize, 416, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_blocks, 448, 64);
#if defined(ANDROID_HOST_MUSL) && defined(__i386__)
// The kernel definition used for musl uses individual longs instead of a struct timespec.
CHECK_FIELD_LAYOUT(struct stat64, st_atime, 512, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_atime_nsec, 544, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_mtime, 576, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_mtime_nsec, 608, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_ctime, 640, 32);
CHECK_FIELD_LAYOUT(struct stat64, st_ctime_nsec, 672, 32);
#else
CHECK_FIELD_LAYOUT(struct stat64, st_atim, 512, 64);
CHECK_FIELD_LAYOUT(struct stat64, st_mtim, 576, 64);
CHECK_FIELD_LAYOUT(struct stat64, st_ctim, 640, 64);
#endif
CHECK_FIELD_LAYOUT(struct stat64, st_ino, 704, 64);

inline void ConvertHostStat64ToGuest(const struct stat64* host_stat, Guest_stat64* guest_stat) {
  guest_stat->st_dev = host_stat->st_dev;
  guest_stat->__st_ino = host_stat->st_ino;
  guest_stat->st_mode = host_stat->st_mode;
  guest_stat->st_nlink = host_stat->st_nlink;
  guest_stat->st_uid = host_stat->st_uid;
  guest_stat->st_gid = host_stat->st_gid;
  guest_stat->st_rdev = host_stat->st_rdev;
  guest_stat->st_size = host_stat->st_size;
  guest_stat->st_blksize = host_stat->st_blksize;
  guest_stat->st_blocks = host_stat->st_blocks;
#if defined(ANDROID_HOST_MUSL) && defined(__i386__)
  // The kernel definition used for musl uses individual longs instead of a struct timespec.
  guest_stat->st_atim.tv_sec = host_stat->st_atime;
  guest_stat->st_atim.tv_nsec = host_stat->st_atime_nsec;
  guest_stat->st_mtim.tv_sec = host_stat->st_mtime;
  guest_stat->st_mtim.tv_nsec = host_stat->st_mtime_nsec;
  guest_stat->st_ctim.tv_sec = host_stat->st_ctime;
  guest_stat->st_ctim.tv_nsec = host_stat->st_ctime_nsec;
#else
  guest_stat->st_atim = host_stat->st_atim;
  guest_stat->st_mtim = host_stat->st_mtim;
  guest_stat->st_ctim = host_stat->st_ctim;
#endif
  guest_stat->st_ino = host_stat->st_ino;
}

}  // namespace berberis

#endif  // BERBERIS_KERNEL_API_SYS_STAT_EMULATION_H_
