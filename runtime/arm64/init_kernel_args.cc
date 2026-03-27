/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "berberis/runtime_library/runtime_library.h"

#include <sys/auxv.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

#include "berberis/base/bit_util.h"  // AlignDown
#include "berberis/calling_conventions/calling_conventions_arm64.h"
#include "berberis/guest_state/guest_addr.h"
#include "berberis/kernel_api/exec_emulation.h"  // DemangleGuestEnvp

// glibc doesn't define AT_HWCAP2
#if defined(__GLIBC__)
#define AT_HWCAP2 26
#endif

namespace berberis {

constexpr uint64_t kArm64HwcapFp = 0x001;
constexpr uint64_t kArm64HwcapAsimd = 0x002;
// constexpr uint64_t kArm64HwcapEvtstrme= 0x004;
constexpr uint64_t kArm64HwcapAes = 0x008;
constexpr uint64_t kArm64HwcapPmull = 0x010;
// constexpr uint64_t kArm64HwcapSha1    = 0x020;
// constexpr uint64_t kArm64HwcapSha2    = 0x040;
constexpr uint64_t kArm64HwcapCrc32 = 0x080;
constexpr uint64_t kArm64HwcapAtomics = 0x100;
// constexpr uint64_t kArm64HwcapFphp    = 0x200;
// constexpr uint64_t kArm64HwcapAsimdhp = 0x400;

constexpr uint64_t kArm64ValueHwcap = kArm64HwcapFp | kArm64HwcapAsimd | kArm64HwcapAes |
                                      kArm64HwcapPmull | kArm64HwcapCrc32 | kArm64HwcapAtomics;

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
  const uint64_t auxv[] = {
      AT_HWCAP,        kArm64ValueHwcap,
      AT_HWCAP2,       0,
      AT_RANDOM,       ToGuestAddr(random_bytes),
      AT_SECURE,       false,
      AT_BASE,         linker_base_addr,
      AT_PHDR,         phdr,
      AT_PHNUM,        phdr_count,
      AT_ENTRY,        main_executable_entry_point,
      AT_PAGESZ,       static_cast<uint64_t>(sysconf(_SC_PAGESIZE)),
      AT_CLKTCK,       static_cast<uint64_t>(sysconf(_SC_CLK_TCK)),
      AT_SYSINFO_EHDR, ehdr_vdso,
      AT_UID,          getuid(),
      AT_EUID,         geteuid(),
      AT_GID,          getgid(),
      AT_EGID,         getegid(),
      AT_NULL,
  };

  size_t envp_count = 0;  // number of environment variables + nullptr
  while (envp[envp_count++] != nullptr) {
  }

  guest_sp -= sizeof(uint64_t) +               // argc
              sizeof(uint64_t) * (argc + 1) +  // argv + nullptr
              sizeof(uint64_t) * envp_count +  // envp + nullptr
              sizeof(auxv);                    // auxv
  guest_sp = AlignDown(guest_sp, berberis::arm64::CallingConventions::kStackAlignmentBeforeCall);

  uint64_t* curr = ToHostAddr<uint64_t>(guest_sp);

  // argc
  *curr++ = argc;

  // argv
  for (size_t i = 0; i < argc; ++i) {
    *curr++ = reinterpret_cast<uint64_t>(argv[i]);
  }
  *curr++ = kNullGuestAddr;

  // envp
  curr = reinterpret_cast<uint64_t*>(DemangleGuestEnvp(reinterpret_cast<char**>(curr), envp));

  // auxv
  memcpy(curr, auxv, sizeof(auxv));

  return guest_sp;
}

}  // namespace berberis
