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

#include "guest_loader_impl.h"

#include <sys/auxv.h>
#include <cstdint>

#include "berberis/base/bit_util.h"  // AlignDown
#include "berberis/calling_conventions/calling_conventions_arm.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/kernel_api/exec_emulation.h"  // DemangleGuestEnvp

// glibc doesn't define AT_HWCAP2
#if defined(__GLIBC__)
#define AT_HWCAP2 26
#endif

#define ARM_HWCAP_SWP (1 << 0)
#define ARM_HWCAP_HALF (1 << 1)
#define ARM_HWCAP_THUMB (1 << 2)
#define ARM_HWCAP_26BIT (1 << 3)
#define ARM_HWCAP_FAST_MULT (1 << 4)
#define ARM_HWCAP_FPA (1 << 5)
#define ARM_HWCAP_VFP (1 << 6)
#define ARM_HWCAP_EDSP (1 << 7)
#define ARM_HWCAP_JAVA (1 << 8)
#define ARM_HWCAP_IWMMXT (1 << 9)
#define ARM_HWCAP_CRUNCH (1 << 10)
#define ARM_HWCAP_THUMBEE (1 << 11)
#define ARM_HWCAP_NEON (1 << 12)
#define ARM_HWCAP_VFPv3 (1 << 13)
#define ARM_HWCAP_VFPv3D16 (1 << 14)
#define ARM_HWCAP_TLS (1 << 15)
#define ARM_HWCAP_VFPv4 (1 << 16)
#define ARM_HWCAP_IDIVA (1 << 17)
#define ARM_HWCAP_IDIVT (1 << 18)
#define ARM_HWCAP_VFPD32 (1 << 19)
#define ARM_HWCAP_IDIV (ARM_HWCAP_IDIVA | ARM_HWCAP_IDIVT)
#define ARM_HWCAP_LPAE (1 << 20)
#define ARM_HWCAP_EVTSTRM (1 << 21)
#define ARM_HWCAP2_AES (1 << 0)
#define ARM_HWCAP2_PMULL (1 << 1)
#define ARM_HWCAP2_SHA1 (1 << 2)
#define ARM_HWCAP2_SHA2 (1 << 3)
#define ARM_HWCAP2_CRC32 (1 << 4)

// See prebuilt/system/lib/arm/berberis/cpuinfo
// Features: neon vfp swp half thumb fastmult edsp vfpv3 vfpv4 idiva idivt
constexpr uint32_t kArmValueHwcap = ARM_HWCAP_EDSP | ARM_HWCAP_FAST_MULT | ARM_HWCAP_HALF |
                                    ARM_HWCAP_IDIVA | ARM_HWCAP_IDIVT | ARM_HWCAP_NEON |
                                    ARM_HWCAP_SWP | ARM_HWCAP_THUMB | ARM_HWCAP_VFP |
                                    ARM_HWCAP_VFPv3 | ARM_HWCAP_VFPv4;

constexpr uint32_t kArmValueHwcap2 = 0x0;

namespace berberis {

// Paths required by guest_loader_impl.h.
const char* kAppProcessRelativePath = "arm/app_process";
const char* kPtInterpRelativePath = "arm/linker";
const char* kVdsoRelativePath = "arm/libnative_bridge_vdso.so";
const char* kProxyPrefix = "libproxy_arm_to_x86_";

GuestAddr InitKernelArgs(GuestAddr guest_sp,
                         size_t argc,
                         const char* argv[],
                         char* envp[],
                         GuestAddr linker_base_addr,
                         GuestAddr main_executable_entry_point,
                         GuestAddr phdr,
                         size_t phdr_count,
                         GuestAddr ehdr_vdso,
                         const uint8_t (*random_bytes)[16]) {
  constexpr uint32_t argc_count = 1;     // always 1
  const uint32_t argv_count = argc + 1;  // number of arguments + nullptr

  uint32_t envp_count = 0;  // number of environment variables + nullptr
  while (envp[envp_count++] != nullptr) {
  }

  const uint32_t auxv[] = {
      AT_HWCAP,        kArmValueHwcap,
      AT_HWCAP2,       kArmValueHwcap2,
      AT_RANDOM,       ToGuestAddr(random_bytes),
      AT_SECURE,       false,
      AT_BASE,         linker_base_addr,
      AT_PHDR,         phdr,
      AT_PHNUM,        phdr_count,
      AT_ENTRY,        main_executable_entry_point,
      AT_PAGESZ,       static_cast<uint32_t>(sysconf(_SC_PAGESIZE)),
      AT_CLKTCK,       static_cast<uint32_t>(sysconf(_SC_CLK_TCK)),
      AT_SYSINFO_EHDR, ehdr_vdso,
      AT_UID,          getuid(),
      AT_EUID,         geteuid(),
      AT_GID,          getgid(),
      AT_EGID,         getegid(),
      AT_NULL,
  };
  constexpr uint32_t auxv_count = std::size(auxv);

  guest_sp -= (argc_count + argv_count + envp_count + auxv_count) * sizeof(uint32_t);
  guest_sp = AlignDown(guest_sp, berberis::arm::CallingConventions::kStackAlignmentBeforeCall);

  uint32_t* curr = reinterpret_cast<uint32_t*>(guest_sp);

  // argc
  *curr++ = argc;

  // argv
  for (uint32_t i = 0; i < argc; ++i) {
    *curr++ = reinterpret_cast<uint32_t>(argv[i]);
  }
  *curr++ = 0;

  // envp
  curr = reinterpret_cast<uint32_t*>(DemangleGuestEnvp(reinterpret_cast<char**>(curr), envp));

  // auxv
  memcpy(curr, auxv, sizeof(auxv));

  return guest_sp;
}

}  // namespace berberis
