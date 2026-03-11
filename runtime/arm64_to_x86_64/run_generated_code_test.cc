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

#include "gtest/gtest.h"

#include "berberis/assembler/machine_code.h"
#include "berberis/assembler/x86_64.h"
#include "berberis/guest_state/guest_state.h"
#include "berberis/runtime_primitives/runtime_library.h"  // berberis_RunGeneratedCode
#include "berberis/test_utils/scoped_exec_region.h"

namespace berberis {

namespace {

TEST(RunGeneratedCodeArm64, Smoke) {
  MachineCode machine_code;
  x86_64::Assembler as(&machine_code);

  // Perform x2 = x0 + x1.
  as.Movq(as.rax, {.base = as.rbp, .disp = offsetof(ThreadState, cpu.x[0])});
  as.Movq(as.rbx, {.base = as.rbp, .disp = offsetof(ThreadState, cpu.x[1])});
  as.Addq(as.rax, as.rbx);
  as.Movq({.base = as.rbp, .disp = offsetof(ThreadState, cpu.x[2])}, as.rax);
  as.Jmp(kEntryExitGeneratedCode);

  ScopedExecRegion exec(&machine_code);

  ThreadState state;
  state.cpu.x[0] = 2;
  state.cpu.x[1] = 7;
  state.cpu.x[2] = 11;  // To be overwritten.

  berberis_RunGeneratedCode(&state, exec.get());

  EXPECT_EQ(state.cpu.x[2], 9ULL);
}

TEST(RunGeneratedCodeArm64, Residence) {
  MachineCode machine_code;
  x86_64::Assembler as(&machine_code);

  // Perform x0 = ThreadState::residence.
  as.Movq(as.rax, {.base = as.rbp, .disp = offsetof(ThreadState, residence)});
  as.Movq({.base = as.rbp, .disp = offsetof(ThreadState, cpu.x[0])}, as.rax);
  as.Jmp(kEntryExitGeneratedCode);

  ScopedExecRegion exec(&machine_code);

  ThreadState state;
  state.cpu.x[0] = 0xdead'beef'dead'beef;
  EXPECT_EQ(state.residence, kOutsideGeneratedCode);

  berberis_RunGeneratedCode(&state, exec.get());

  EXPECT_EQ(state.residence, kOutsideGeneratedCode);
  // x0 holds the value of residence when we were inside the generated code.
  EXPECT_EQ(state.cpu.x[0], kInsideGeneratedCode);
}

}  // namespace

}  // namespace berberis