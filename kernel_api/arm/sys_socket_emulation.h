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

#ifndef BERBERIS_KERNEL_API_SYS_SOCKET_EMULATION_H_
#define BERBERIS_KERNEL_API_SYS_SOCKET_EMULATION_H_

#include <sys/socket.h>

#include "berberis/base/struct_check.h"  // CHECK_*_LAYOUT

namespace berberis {

CHECK_STRUCT_LAYOUT(sockaddr, 128, 16);
CHECK_FIELD_LAYOUT(sockaddr, sa_family, 0, 16);
CHECK_FIELD_LAYOUT(sockaddr, sa_data, 16, 112);

// Note: that type is never mentioned directly, all functions which accept it are declared as
// accepting void* - yet it's still important to have it compatible.
CHECK_STRUCT_LAYOUT(linger, 64, 32);
CHECK_FIELD_LAYOUT(linger, l_onoff, 0, 32);
CHECK_FIELD_LAYOUT(linger, l_linger, 32, 32);

// The same as with linger above.
CHECK_STRUCT_LAYOUT(ucred, 96, 32);
CHECK_FIELD_LAYOUT(ucred, pid, 0, 32);
CHECK_FIELD_LAYOUT(ucred, uid, 32, 32);
CHECK_FIELD_LAYOUT(ucred, gid, 64, 32);

CHECK_STRUCT_LAYOUT(msghdr, 224, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_name, 0, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_namelen, 32, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_iov, 64, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_iovlen, 96, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_control, 128, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_controllen, 160, 32);
CHECK_FIELD_LAYOUT(msghdr, msg_flags, 192, 32);

CHECK_STRUCT_LAYOUT(mmsghdr, 256, 32);
CHECK_FIELD_LAYOUT(mmsghdr, msg_hdr, 0, 224);
CHECK_FIELD_LAYOUT(mmsghdr, msg_len, 224, 32);

}  // namespace berberis

#endif  // BERBERIS_KERNEL_API_SYS_SOCKET_EMULATION_H_
