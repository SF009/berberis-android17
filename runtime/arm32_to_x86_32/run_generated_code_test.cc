/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "gtest/gtest.h"

#include "berberis/assembler/machine_code.h"
#include "berberis/assembler/x86_32.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_library/runtime_library.h"  // berberis_RunGeneratedCode
#include "berberis/test_utils/scoped_exec_region.h"

namespace berberis {

namespace x86_32 {

using berberis::x86_32::Assembler;

}  // namespace x86_32

namespace {

TEST(RunGeneratedCodeArm, Smoke) {
  MachineCode machine_code;
  x86_32::Assembler as(&machine_code);

  // Perform r2 = r0 + r1.
  as.Movl(as.eax, {.base = as.ebp, .disp = offsetof(ThreadState, cpu.r[0])});
  as.Movl(as.ebx, {.base = as.ebp, .disp = offsetof(ThreadState, cpu.r[1])});
  as.Addl(as.eax, as.ebx);
  as.Movl({.base = as.ebp, .disp = offsetof(ThreadState, cpu.r[2])}, as.eax);
  as.Jmp(kEntryExitGeneratedCode);

  ScopedExecRegion exec(&machine_code);

  ThreadState state;
  state.cpu.r[0] = 2;
  state.cpu.r[1] = 7;
  state.cpu.r[2] = 11;  // To be overwritten.

  berberis_RunGeneratedCode(&state, exec.get());

  EXPECT_EQ(state.cpu.r[2], 9U);
}

TEST(RunGeneratedCodeArm, Residence) {
  MachineCode machine_code;
  x86_32::Assembler as(&machine_code);

  // Perform r0 = ThreadState::residence.
  as.Movl(as.eax, {.base = as.ebp, .disp = offsetof(ThreadState, residence)});
  as.Movl({.base = as.ebp, .disp = offsetof(ThreadState, cpu.r[0])}, as.eax);
  as.Jmp(kEntryExitGeneratedCode);

  ScopedExecRegion exec(&machine_code);

  ThreadState state;
  state.cpu.r[0] = 0xdead'beef;

  EXPECT_EQ(state.residence, kOutsideGeneratedCode);

  berberis_RunGeneratedCode(&state, exec.get());

  EXPECT_EQ(state.residence, kOutsideGeneratedCode);

  // x0 hold the value of residence when we were inside the generated code.
  EXPECT_EQ(state.cpu.r[0], kInsideGeneratedCode);
}

}  // namespace

}  // namespace berberis