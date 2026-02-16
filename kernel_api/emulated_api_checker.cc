/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Common.
#if defined(__arm__) || defined(NATIVE_BRIDGE_GUEST_ARCH_ARM)
#include "arm/signal_emulation.h"
#include "arm/signal_emulation_bionic.h"
#include "arm/sys_ioctl_emulation.h"
#include "arm/sys_msg_emulation.h"
#include "arm/sys_sem_emulation.h"
#include "arm/sys_shm_emulation.h"
#include "arm/sys_socket_emulation.h"
// This header also checks host layout and cannot be used here.
// TODO(b/232598137): consider reenabling.
// #include "arm/sys_stat_emulation.h"
#include "arm/sys_vfs_emulation.h"
#endif

#include "guest_types.h"
