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

#ifndef BERBERIS_KERNEL_API_SYS_MSG_EMULATION_H_
#define BERBERIS_KERNEL_API_SYS_MSG_EMULATION_H_

#if __has_include(<sys/msg.h>)

// Get sys/ipc.h and sys/msg.h kernel data structures.
//
// That's what we need to include - but we need both kernel and libc data structures thus
// complicated dance with #define/#undef and different for different libraries.
//
// #include <sys/ipc.h>
// #include <sys/msg.h>
// #include <sys/types.h>

#ifdef __GLIBC__

#define ipc_perm __kernel_legacy_ipc_perm
#define msginfo __kernel_legacy_msginfo
#define msqid_ds __kernel_legacy_msqid_ds
#define msgbuf __kernel_legacy_msgbuf
#include <linux/ipc.h>
#include <linux/msg.h>
#undef ipc_perm
#undef msginfo
#undef msqid_ds
#undef msgbuf
#include <sys/ipc.h>
#include <sys/msg.h>
#undef msg_cbytes

#elif defined(__BIONIC__)

#ifndef ipc_perm
#define ipc_perm __kernel_legacy_ipc_perm
#include <sys/ipc.h>
#endif
#include <sys/msg.h>

#else
#error Unknown C library
#endif

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

// 32-bit msgid_ds
CHECK_STRUCT_LAYOUT(__kernel_legacy_msqid_ds, 448, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_perm, 0, 128);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_first, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_last, 160, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_stime, 192, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_rtime, 224, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_ctime, 256, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_lcbytes, 288, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_lqbytes, 320, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_cbytes, 352, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_qnum, 368, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_qbytes, 384, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_lspid, 400, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_msqid_ds, msg_lrpid, 416, 16);

// msgid_ds.msg_perm
CHECK_STRUCT_LAYOUT(__kernel_legacy_ipc_perm, 128, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, key, 0, 32);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, uid, 32, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, gid, 48, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, cuid, 64, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, cgid, 80, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, mode, 96, 16);
CHECK_FIELD_LAYOUT(__kernel_legacy_ipc_perm, seq, 112, 16);

// 64-bit msqid_ds
CHECK_STRUCT_LAYOUT(msqid_ds, 704, 32);
CHECK_FIELD_LAYOUT(msqid_ds, msg_perm, 0, 288);
CHECK_FIELD_LAYOUT(msqid_ds, msg_stime, 288, 32);
CHECK_FIELD_LAYOUT(msqid_ds, msg_rtime, 352, 32);
CHECK_FIELD_LAYOUT(msqid_ds, msg_ctime, 416, 32);
#ifdef __GLIBC__
CHECK_FIELD_LAYOUT(msqid_ds, __msg_cbytes, 480, 32);
#else
CHECK_FIELD_LAYOUT(msqid_ds, msg_cbytes, 480, 32);
#endif
CHECK_FIELD_LAYOUT(msqid_ds, msg_qnum, 512, 32);
CHECK_FIELD_LAYOUT(msqid_ds, msg_qbytes, 544, 32);
CHECK_FIELD_LAYOUT(msqid_ds, msg_lspid, 576, 32);
CHECK_FIELD_LAYOUT(msqid_ds, msg_lrpid, 608, 32);
CHECK_FIELD_LAYOUT(msqid_ds, __unused4, 640, 32);
CHECK_FIELD_LAYOUT(msqid_ds, __unused5, 672, 32);

// ipc_perm
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

// msginfo
CHECK_STRUCT_LAYOUT(msginfo, 256, 32);
CHECK_FIELD_LAYOUT(msginfo, msgpool, 0, 32);
CHECK_FIELD_LAYOUT(msginfo, msgmap, 32, 32);
CHECK_FIELD_LAYOUT(msginfo, msgmax, 64, 32);
CHECK_FIELD_LAYOUT(msginfo, msgmnb, 96, 32);
CHECK_FIELD_LAYOUT(msginfo, msgmni, 128, 32);
CHECK_FIELD_LAYOUT(msginfo, msgssz, 160, 32);
CHECK_FIELD_LAYOUT(msginfo, msgtql, 192, 32);
CHECK_FIELD_LAYOUT(msginfo, msgseg, 224, 16);

// msgbuf
CHECK_STRUCT_LAYOUT(msgbuf, 64, 32);
CHECK_FIELD_LAYOUT(msgbuf, mtype, 0, 32);
CHECK_FIELD_LAYOUT(msgbuf, mtext, 32, 8);

}  // namespace berberis

#endif  // if __has_include(<sys/msg.h>)

#endif  // BERBERIS_KERNEL_API_SYS_MSG_EMULATION_H_
