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

#include <setjmp.h>

#include <cstdint>

#include "berberis/base/checks.h"
#include "berberis/base/host_signal.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

namespace {

// See bionic/libc/private/bionic_constants.h
#define SCS_SIZE (16 * 1024)
#define SCS_MASK (SCS_SIZE - 1)

// jmp_buf format is totally Bionic-private (see bionic/libc/arch-arm64/bionic/setjmp.S)
// We don't have to use the original format as save/restore is only done here.
// Still, let's keep it compatible with some release as it might help debugging...
//
// word   name            description
// 0      sigflag/cookie  setjmp cookie in top 31 bits, signal mask flag in low bit
// 1      sigmask         signal mask (not used with _setjmp / _longjmp)
// 2      core_base       base of core registers (x18-x30, sp)
//                        (We only store the low bits of x18 to avoid leaking the
//                        shadow call stack address into memory.)
// 16     float_base      base of float registers (d8-d15)
// 24     checksum        checksum of core registers
// 25     reserved        reserved entries (room to grow)
// ...
// 31     x86_64 jmp_buf ptr
// 32
//
const int kJmpBufSigFlagAndCookieWord = 0;
const int kJmpBufSigMaskWord = 1;
const int kJmpBufCoreBaseWord = 2;
const int kJmpBufFpuBaseWord = 16;
const int kJmpBufChecksumWord = 24;
const int kJmpBufHostBufWord = 31;

// jmp_buf cookie can be anything but 0 (see bionic/tests/setjmp_test.cpp: setjmp_cookie)
// ATTENTION: keep low bit 0 for signal mask flag!
const uint64_t kJmpBufCookie = 0x12'3210ULL;

uint64_t CalcJumpBufChecksum(const uint64_t* buf) {
  uint64_t res = 0;
  for (int i = 0; i < kJmpBufChecksumWord; ++i) {
    res ^= buf[i];
  }
  return res;
}

}  // namespace

void SaveRegsToJumpBuf(const ThreadState* state, void* guest_jmp_buf, int save_sig_mask) {
  uint64_t* buf = reinterpret_cast<uint64_t*>(guest_jmp_buf);

  // Clear the buffer in case the format has gaps.
  memset(buf, 0, kJmpBufChecksumWord * sizeof(uint64_t));

  // Cookie, signal flag, signal mask
  buf[kJmpBufSigFlagAndCookieWord] = kJmpBufCookie;
  if (save_sig_mask) {
    buf[kJmpBufSigFlagAndCookieWord] |= 0x1;
    RTSigprocmaskSyscallOrDie(
        SIG_SETMASK, nullptr, reinterpret_cast<HostSigset*>(buf + kJmpBufSigMaskWord));
  }

  // Only lower halfs of v8-v15 (d8-d15) are callee saved.
  for (int i = 0; i < 8; ++i) {
    buf[kJmpBufFpuBaseWord + i] = static_cast<uint64_t>(state->cpu.v[8 + i]);
  }

  // x18 aka shadow stack pointer. Only store lower bits to avoid leaking shadow stack location in
  // memory.
  buf[kJmpBufCoreBaseWord] = state->cpu.x[18] & uint64_t{SCS_MASK};

  // x19-x30, sp.
  memcpy(buf + kJmpBufCoreBaseWord + 1, state->cpu.x + 19, 12 * sizeof(state->cpu.x[0]));
  buf[kJmpBufCoreBaseWord + 13] = state->cpu.sp;

  // Checksum
  buf[kJmpBufChecksumWord] = CalcJumpBufChecksum(buf);
}

void RestoreRegsFromJumpBuf(ThreadState* state, void* guest_jmp_buf, int retval) {
  const uint64_t* buf = reinterpret_cast<const uint64_t*>(guest_jmp_buf);

  // Checksum
  if (buf[kJmpBufChecksumWord] != CalcJumpBufChecksum(buf)) {
    FATAL("setjmp checksum mismatch");
  }

  // Cookie
  if ((buf[kJmpBufSigFlagAndCookieWord] & ~0x1) != kJmpBufCookie) {
    FATAL("setjmp cookie mismatch");
  }

  // Signal mask
  if (buf[kJmpBufSigFlagAndCookieWord] & 0x1) {
    RTSigprocmaskSyscallOrDie(
        SIG_SETMASK, reinterpret_cast<const HostSigset*>(buf + kJmpBufSigMaskWord), nullptr);
  }

  // Zero extend saved d8-d15 into v8-v15.
  for (int i = 0; i < 8; ++i) {
    state->cpu.v[8 + i] = __uint128_t{buf[kJmpBufFpuBaseWord + i]};
  }

  state->cpu.x[18] = (state->cpu.x[18] & ~uint64_t{SCS_MASK}) | buf[kJmpBufCoreBaseWord];
  // x19-x30, sp.
  memcpy(state->cpu.x + 19, buf + kJmpBufCoreBaseWord + 1, 12 * sizeof(state->cpu.x[0]));
  state->cpu.sp = buf[kJmpBufCoreBaseWord + 13];

  // Function return
  state->cpu.insn_addr = state->cpu.x[30];
  state->cpu.x[0] = retval;
}

jmp_buf** GetHostJmpBufPtr(void* guest_jmp_buf) {
  uint64_t* buf = reinterpret_cast<uint64_t*>(guest_jmp_buf);
  return reinterpret_cast<jmp_buf**>(buf + kJmpBufHostBufWord);
}

}  // namespace berberis
