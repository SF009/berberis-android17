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

#include "berberis/guest_os_primitives/guest_setjmp.h"

#include <csetjmp>
#include <cstring>

#include "berberis/base/host_signal.h"
#include "berberis/base/logging.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/arm32/runtime_library.h"

namespace berberis {

namespace {

// jmp_buf format is totally Bionic-private (see bionic/libc/arch-arm/bionic/setjmp.S)
// We don't have to use the original format as save/restore is only done here.
// Still, let's keep it compatible with some release as it might help debugging...
//
// word   name            description
// 0      sigflag/cookie  setjmp cookie in top 31 bits, signal mask flag in low bit
// 1      sigmask         64-bit signal mask (not used with _setjmp / _longjmp)
// 2      "               "
// 3      reserved        (unused to allow float_base to be maximally aligned;
//                        this avoids software emulation of unaligned loads/stores)
// 4      float_base      base of float registers (d8 to d15)
// 20     float_state     floating-point status and control register
// 21     core_base       base of core registers (r4-r11, r13-r14)
// 31     checksum        checksum of all of the core registers, to give better error messages
// 32     reserved        reserved entries (room to grow)
// ...
// 40     x86 jmp_buf ptr
// 41     reserved
// ...
// 63     "               "
//
// ATTENTION: CalcJumpBufChecksum relies on checksum being the last meaningful word!
const int kJmpBufSigFlagAndCookieWord = 0;
const int kJmpBufSigMaskWord = 1;
const int kJmpBufFpuBaseWord = 4;
const int kJmpBufFpuStateWord = 20;
const int kJmpBufCoreBaseWord = 21;
const int kJmpBufChecksumWord = 31;
const int kJmpBufHostBufWord = 40;

// jmp_buf cookie can be anything but 0 (see bionic/tests/setjmp_test.cpp: setjmp_cookie)
// ATTENTION: keep low bit 0 for signal mask flag!
const uint32_t kJmpBufCookie = 0x12'3210;

uint32_t CalcJumpBufChecksum(const uint32_t* buf) {
  uint32_t res = 0;
  for (int i = 0; i < kJmpBufChecksumWord; ++i) {
    res ^= buf[i];
  }
  return res;
}

}  // namespace

void SaveRegsToJumpBuf(const ThreadState* state, void* guest_jmp_buf, int save_sig_mask) {
  uint32_t* buf = reinterpret_cast<uint32_t*>(guest_jmp_buf);

  // Clear the buffer in case the format has gaps.
  memset(buf, 0, kJmpBufChecksumWord * sizeof(uint32_t));

  // Cookie, signal flag, signal mask
  buf[kJmpBufSigFlagAndCookieWord] = kJmpBufCookie;
  if (save_sig_mask) {
    buf[kJmpBufSigFlagAndCookieWord] |= 0x1;
    RTSigprocmaskSyscallOrDie(
        SIG_SETMASK, nullptr, reinterpret_cast<HostSigset*>(buf + kJmpBufSigMaskWord));
  }

  // d8 - d15
  memcpy(buf + kJmpBufFpuBaseWord, &state->cpu.d[8], 8 * sizeof(state->cpu.d[0]));

  // fpu state
  // Note: GetFPEnvironment combines host FP flags and FP flags
  // in cpu->fpflags and returns as full FPSCR.
  buf[kJmpBufFpuStateWord] = GetFPEnvironment(state->cpu.fpflags);

  // r4 - r11, r13 - r14
  memcpy(buf + kJmpBufCoreBaseWord, state->cpu.r + 4, 8 * sizeof(state->cpu.r[0]));
  memcpy(buf + kJmpBufCoreBaseWord + 8, state->cpu.r + 13, 2 * sizeof(state->cpu.r[0]));

  // Checksum
  buf[kJmpBufChecksumWord] = CalcJumpBufChecksum(buf);
}

void RestoreRegsFromJumpBuf(ThreadState* state, void* guest_jmp_buf, int retval) {
  const uint32_t* buf = reinterpret_cast<const uint32_t*>(guest_jmp_buf);

  // Checksum
  if (buf[kJmpBufChecksumWord] != CalcJumpBufChecksum(buf)) {
    LOG_ALWAYS_FATAL("setjmp checksum mismatch");
  }

  // Cookie
  if ((buf[kJmpBufSigFlagAndCookieWord] & ~0x1) != kJmpBufCookie) {
    LOG_ALWAYS_FATAL("setjmp cookie mismatch");
  }

  // Signal mask
  if (buf[kJmpBufSigFlagAndCookieWord] & 0x1) {
    RTSigprocmaskSyscallOrDie(
        SIG_SETMASK, reinterpret_cast<const HostSigset*>(buf + kJmpBufSigMaskWord), nullptr);
  }

  // d8 - d15
  memcpy(&state->cpu.d[8], buf + kJmpBufFpuBaseWord, 8 * sizeof(state->cpu.d[0]));

  // fpu state
  // Note: SetFPEnvironment takes full FPSCR and stores parts of
  // it in the host FP flags and return parts which are not stored there as result.
  state->cpu.fpflags = SetFPEnvironment(buf[kJmpBufFpuStateWord]);

  // r4 - r11, r13 - r14
  memcpy(state->cpu.r + 4, buf + kJmpBufCoreBaseWord, 8 * sizeof(state->cpu.r[0]));
  memcpy(state->cpu.r + 13, buf + kJmpBufCoreBaseWord + 8, 2 * sizeof(state->cpu.r[0]));

  // Function return
  state->cpu.insn_addr = state->cpu.r[14];
  state->cpu.r[0] = retval;
}

jmp_buf** GetHostJmpBufPtr(void* guest_jmp_buf) {
  uint32_t* buf = reinterpret_cast<uint32_t*>(guest_jmp_buf);
  return reinterpret_cast<jmp_buf**>(buf + kJmpBufHostBufWord);
}

}  // namespace berberis
