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

#include "berberis/backend/x86_64/reg_lifetime.h"

#include <variant>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/arena_map.h"

namespace berberis::x86_64 {

RegLifetimeMap CountRegLifetimeMap(MachineIR* machine_ir, MachineBasicBlock* bb) {
  RegLifetimeMap ret(machine_ir->arena());
  // First get all live_ins.
  for (auto reg : bb->live_in()) {
    ret[reg] = RegLifetime{
        .start = LiveIn{},
        .end = LiveIn{},
    };
  }

  // Note that we don't account for the same register being redefined which
  // would imply separate lifetimes but for our current needs that is sufficient.
  // For example:
  //   v1 = 2
  //   ADD v1 v2
  //   ... # Here v1 can actually be discarded but our algorithm considers it alive.
  //   v1 = 5
  for (auto* insn : bb->insn_list()) {
    for (int i = 0; i < insn->NumRegOperands(); i++) {
      // If this is the first time we are seeing this register set start and end.
      auto reg = insn->RegAt(i);
      if (!ret.contains(reg)) {
        ret[reg] = RegLifetime{
            .start = insn,
            .end = insn,
        };
      } else {
        // Otherwise just update end.
        ret[reg].end = insn;
      }
    }
  }

  // Finally check live_outs.
  for (auto reg : bb->live_out()) {
    CHECK(ret.contains(reg));
    ret[reg].end = LiveOut{};
  }
  return ret;
}

}  // namespace berberis::x86_64
