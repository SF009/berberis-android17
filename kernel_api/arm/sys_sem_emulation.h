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

#ifndef BERBERIS_KERNEL_API_SYS_SEM_EMULATION_H_
#define BERBERIS_KERNEL_API_SYS_SEM_EMULATION_H_

#if __has_include(<sys/sem.h>)

// Get sys/ipc.h and sys/sem.h kernel data structures.
//
// That's what we need to include - but we need both kernel and libc data structures thus
// complicated dance with #define/#undef and different for different libraries.
//
// #include <sys/ipc.h>
// #include <sys/sem.h>
// #include <sys/types.h>

#ifdef __GLIBC__

#define ipc_perm __kernel_legacy_ipc_perm
#define semid_ds __kernel_legacy_semid_ds
#define seminfo __kernel_legacy_seminfo
#define sembuf __kernel_legacy_sembuf
#include <linux/ipc.h>
#include <linux/sem.h>
#undef ipc_perm
#undef seminfo
#undef sembuf
#undef semid_ds
#include <sys/ipc.h>
#include <sys/sem.h>

#elif defined(__BIONIC__)

#ifndef ipc_perm
#define ipc_perm __kernel_legacy_ipc_perm
#include <sys/ipc.h>
#endif
#include <sys/sem.h>

#else
#error Unknown C library
#endif

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

// This is quite complex union used for multiplexor-syscall symctl
CHECK_STRUCT_LAYOUT(semun, 32, 32);

// General layout
CHECK_FIELD_LAYOUT(semun, val, 0, 32);
CHECK_FIELD_LAYOUT(semun, buf, 0, 32);
CHECK_FIELD_LAYOUT(semun, array, 0, 32);
CHECK_FIELD_LAYOUT(semun, __buf, 0, 32);
CHECK_FIELD_LAYOUT(semun, __pad, 0, 32);

// semun.buf
CHECK_STRUCT_LAYOUT(__kernel_legacy_semid_ds, 352, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_perm, 0, 128);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_otime, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_ctime, 160, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_base, 192, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_pending, 224, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_pending_last, 256, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, undo, 288, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_semid_ds, sem_nsems, 320, 16);

// semun.buf.sem_perm
CHECK_STRUCT_LAYOUT(__kernel_legacy_ipc_perm, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, key, 0, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, uid, 32, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, gid, 48, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, cuid, 64, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, cgid, 80, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, mode, 96, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, seq, 112, 16);

// semun.__buf
CHECK_STRUCT_LAYOUT(seminfo, 320, 32);
CHECK_FIELD_LAYOUT(seminfo, semmap, 0, 32);
CHECK_FIELD_LAYOUT(seminfo, semmni, 32, 32);
CHECK_FIELD_LAYOUT(seminfo, semmns, 64, 32);
CHECK_FIELD_LAYOUT(seminfo, semmnu, 96, 32);
CHECK_FIELD_LAYOUT(seminfo, semmsl, 128, 32);
CHECK_FIELD_LAYOUT(seminfo, semopm, 160, 32);
CHECK_FIELD_LAYOUT(seminfo, semume, 192, 32);
CHECK_FIELD_LAYOUT(seminfo, semusz, 224, 32);
CHECK_FIELD_LAYOUT(seminfo, semvmx, 256, 32);
CHECK_FIELD_LAYOUT(seminfo, semaem, 288, 32);

#ifdef __GLIBC__
CHECK_STRUCT_LAYOUT(__kernel_legacy_seminfo, 320, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semmap, 0, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semmni, 32, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semmns, 64, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semmnu, 96, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semmsl, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semopm, 160, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semume, 192, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semusz, 224, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semvmx, 256, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_seminfo, semaem, 288, 32);
#endif

// semun.__buf
CHECK_STRUCT_LAYOUT(sembuf, 48, 16);
CHECK_FIELD_LAYOUT(sembuf, sem_num, 0, 16);
CHECK_FIELD_LAYOUT(sembuf, sem_op, 16, 16);
CHECK_FIELD_LAYOUT(sembuf, sem_flg, 32, 16);

#ifdef __GLIBC__
CHECK_STRUCT_LAYOUT(__kernel_legacy_sembuf, 48, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_sembuf, sem_num, 0, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_sembuf, sem_op, 16, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_sembuf, sem_flg, 32, 16);
#endif

}  // namespace berberis

#endif  // __has_include(<sys/sem.h>)

#endif  // BERBERIS_KERNEL_API_SYS_SEM_EMULATION_H_
