/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "berberis/code_gen_lib/code_gen_lib.h"

#include <cstdint>

#include "berberis/assembler/x86_64.h"
#include "berberis/base/config_globals.h"
#include "berberis/guest_state/guest_state.h"

namespace berberis {

void EmitStoreMappedRegsIfNeeded(x86_64::Assembler* as) {
  if (!IsConfigFlagSet(kOptimizedInterRegionABI)) {
    return;
  }

  as->Movq({.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(0))}, as->r8);
  as->Movq({.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(1))}, as->r9);
  as->Movq({.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(8))}, as->r10);
  as->Movq({.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(19))}, as->r11);
  as->Movq({.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(30))}, as->r12);
  as->Movq({.base = as->rbp, .disp = offsetof(ThreadState, cpu.sp)}, as->r13);
}

void EmitLoadMappedRegsIfNeeded(x86_64::Assembler* as) {
  if (!IsConfigFlagSet(kOptimizedInterRegionABI)) {
    return;
  }

  as->Movq(as->r8, {.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(0))});
  as->Movq(as->r9, {.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(1))});
  as->Movq(as->r10, {.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(8))});
  as->Movq(as->r11, {.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(19))});
  as->Movq(as->r12, {.base = as->rbp, .disp = static_cast<int32_t>(GetThreadStateRegOffset(30))});
  as->Movq(as->r13, {.base = as->rbp, .disp = offsetof(ThreadState, cpu.sp)});
}

}  // namespace berberis
