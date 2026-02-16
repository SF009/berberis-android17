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

#ifndef BERBERIS_KERNEL_API_SYS_VFS_EMULATION_H_
#define BERBERIS_KERNEL_API_SYS_VFS_EMULATION_H_

#ifdef __BIONIC__
#include <asm/statfs.h>
#endif
#if defined(__GLIBC__) || defined(ANDROID_HOST_MUSL)
#include <sys/vfs.h>
#endif

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

// Musl #defines statfs64 to statfs
#ifndef ANDROID_HOST_MUSL
CHECK_STRUCT_LAYOUT(struct statfs, 512, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_type, 0, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_bsize, 32, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_blocks, 64, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_bfree, 96, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_bavail, 128, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_files, 160, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_ffree, 192, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_fsid, 224, 64);
CHECK_FIELD_LAYOUT(struct statfs, f_namelen, 288, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_frsize, 320, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_flags, 352, 32);
CHECK_FIELD_LAYOUT(struct statfs, f_spare, 384, 128);
#endif

// Struct statfs64 is *manually* aligned on 32, thus it is fully compatible.
// Otherwise it would have been aligned on 64 on ARM.
CHECK_STRUCT_LAYOUT(struct statfs64, 672, 32);
CHECK_FIELD_LAYOUT(struct statfs64, f_type, 0, 32);
CHECK_FIELD_LAYOUT(struct statfs64, f_bsize, 32, 32);
CHECK_FIELD_LAYOUT(struct statfs64, f_blocks, 64, 64);
CHECK_FIELD_LAYOUT(struct statfs64, f_bfree, 128, 64);
CHECK_FIELD_LAYOUT(struct statfs64, f_bavail, 192, 64);
CHECK_FIELD_LAYOUT(struct statfs64, f_files, 256, 64);
CHECK_FIELD_LAYOUT(struct statfs64, f_ffree, 320, 64);
CHECK_FIELD_LAYOUT(struct statfs64, f_fsid, 384, 64);
CHECK_FIELD_LAYOUT(struct statfs64, f_namelen, 448, 32);
CHECK_FIELD_LAYOUT(struct statfs64, f_frsize, 480, 32);
CHECK_FIELD_LAYOUT(struct statfs64, f_flags, 512, 32);
CHECK_FIELD_LAYOUT(struct statfs64, f_spare, 544, 128);

// Since sizes of data structures are different we must do verification check themselves.
// For that we need to know size fo GUEST data structure.
constexpr size_t kGuestStatfs64Size = 88;

}  // namespace berberis

#endif  // BERBERIS_KERNEL_API_SYS_VFS_EMULATION_H_
