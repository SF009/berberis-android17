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

#ifndef BERBERIS_KERNEL_API_SYS_SHM_EMULATION_H_
#define BERBERIS_KERNEL_API_SYS_SHM_EMULATION_H_

#if __has_include(<sys/shm.h>)

// Get sys/ipc.h and sys/shm.h kernel data structures.
//
// That's what we need to include - but we need both kernel and libc data structures thus
// complicated dance with #define/#undef and different for different libraries.
//
// #include <sys/ipc.h>
// #include <sys/shm.h>
// #include <sys/types.h>

#ifdef __GLIBC__

#define ipc_perm __kernel_legacy_ipc_perm
#define shmid_ds __kernel_legacy_shmid_ds
#define shmid64_ds __kernel_shmid64_ds
#define shm_info __kernel_legacy_shm_info
#define shminfo __kernel_legacy_shminfo
#include <linux/ipc.h>
#include <linux/shm.h>
#undef ipc_perm
#undef shmid_ds
#undef shmid64_ds
#undef shm_info
#undef shminfo
#define shmid_ds shmid64_ds
#include <sys/ipc.h>
#include <sys/shm.h>
#undef shmid_ds

#elif defined(__BIONIC__)

#ifndef ipc_perm
#define ipc_perm __kernel_legacy_ipc_perm
#include <sys/ipc.h>
#endif
#define shm_info __kernel_legacy_shm_info
#define shminfo __kernel_legacy_shminfo
#include <sys/shm.h>
#undef shm_info
#undef shminfo

#else
#error Unknown C library
#endif

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

// 32-bit shmid_ds
CHECK_STRUCT_LAYOUT(__kernel_legacy_shmid_ds, 384, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_perm, 0, 128);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_segsz, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_atime, 160, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_dtime, 192, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_ctime, 224, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_cpid, 256, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_lpid, 272, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_nattch, 288, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_unused, 304, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_unused2, 320, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shmid_ds, shm_unused3, 352, 32);

// SHMun.buf.SHM_perm
CHECK_STRUCT_LAYOUT(__kernel_legacy_ipc_perm, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, key, 0, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, uid, 32, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, gid, 48, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, cuid, 64, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, cgid, 80, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, mode, 96, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, seq, 112, 16);

// 64-bit shmid_ds
CHECK_STRUCT_LAYOUT(shmid64_ds, 672, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_perm, 0, 288);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_segsz, 288, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_atime, 320, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_dtime, 384, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_ctime, 448, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_cpid, 512, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_lpid, 544, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, shm_nattch, 576, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, __unused4, 608, 32);
CHECK_FIELD_LAYOUT(shmid64_ds, __unused5, 640, 32);

#ifdef __GLIBC__
CHECK_STRUCT_LAYOUT(__kernel_shmid64_ds, 672, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_perm, 0, 288);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_segsz, 288, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_atime, 320, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, __unused1, 352, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_dtime, 384, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, __unused2, 416, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_ctime, 448, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, __unused3, 480, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_cpid, 512, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_lpid, 544, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, shm_nattch, 576, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, __unused4, 608, 32);
CHECK_FIELD_LAYOUT(__kernel_shmid64_ds, __unused5, 640, 32);
#endif

// ipc64_perm
CHECK_STRUCT_LAYOUT(ipc64_perm, 288, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, key, 0, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, uid, 32, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, gid, 64, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, cuid, 96, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, cgid, 128, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, mode, 160, 16);
CHECK_FIELD_LAYOUT(ipc64_perm, __pad1, 176, 16);
CHECK_FIELD_LAYOUT(ipc64_perm, seq, 192, 16);
CHECK_FIELD_LAYOUT(ipc64_perm, __pad2, 208, 16);
CHECK_FIELD_LAYOUT(ipc64_perm, __unused1, 224, 32);
CHECK_FIELD_LAYOUT(ipc64_perm, __unused2, 256, 32);

// 32-bit shminfo
CHECK_STRUCT_LAYOUT(__kernel_legacy_shminfo, 160, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shminfo, shmmax, 0, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shminfo, shmmin, 32, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shminfo, shmmni, 64, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shminfo, shmseg, 96, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shminfo, shmall, 128, 32);

// 64-bit shminfo
CHECK_STRUCT_LAYOUT(shminfo64, 288, 32);
CHECK_FIELD_LAYOUT(shminfo64, shmmax, 0, 32);
CHECK_FIELD_LAYOUT(shminfo64, shmmin, 32, 32);
CHECK_FIELD_LAYOUT(shminfo64, shmmni, 64, 32);
CHECK_FIELD_LAYOUT(shminfo64, shmseg, 96, 32);
CHECK_FIELD_LAYOUT(shminfo64, shmall, 128, 32);
CHECK_FIELD_LAYOUT(shminfo64, __unused1, 160, 32);
CHECK_FIELD_LAYOUT(shminfo64, __unused2, 192, 32);
CHECK_FIELD_LAYOUT(shminfo64, __unused3, 224, 32);
CHECK_FIELD_LAYOUT(shminfo64, __unused4, 256, 32);

// 32-bit shm_info
CHECK_STRUCT_LAYOUT(__kernel_legacy_shm_info, 192, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shm_info, used_ids, 0, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shm_info, shm_tot, 32, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shm_info, shm_rss, 64, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shm_info, shm_swp, 96, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shm_info, swap_attempts, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_shm_info, swap_successes, 160, 32);

}  // namespace berberis

#endif  // #if __has_include(<sys/shm.h>)

#endif  // BERBERIS_KERNEL_API_SYS_SHM_EMULATION_H_
