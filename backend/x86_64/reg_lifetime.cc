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

#include <iterator>
#include <variant>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/arena_map.h"

namespace berberis::x86_64 {

RegLifetimeMap CountRegLifetimeMap(MachineIR* machine_ir, MachineBasicBlock* bb) {
  RegLifetimeMap ret(machine_ir->arena());
  CHECK(!bb->insn_list().empty());
  // First get all live_ins.
  for (auto reg : bb->live_in()) {
    ret[reg] = RegLifetime{
        .start = LiveIn{},
        .end = bb->insn_list().front(),
        .reg_type = RegType::kUnknown,
    };
  }

  // Note that we don't account for the same register being redefined which
  // would imply separate lifetimes but for our current needs that is sufficient.
  // For example:
  //   v1 = 2
  //   ADD v1 v2
  //   ... # Here v1 can actually be discarded but our algorithm considers it alive.
  //   v1 = 5
  std::variant<LiveOut, berberis::MachineInsn*> next_insn;
  for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end(); insn_it++) {
    auto insn = *insn_it;
    if (std::next(insn_it) == bb->insn_list().end()) {
      next_insn.emplace<LiveOut>(LiveOut{});
    } else {
      next_insn.emplace<berberis::MachineInsn*>(*std::next(insn_it));
    }
    for (int i = 0; i < insn->NumRegOperands(); i++) {
      // Skip flags register.
      if (insn->RegKindAt(i).RegClass()->IsSubsetOf(&kFLAGS)) {
        continue;
      }
      // If this is the first time we are seeing this register set start.
      auto reg = insn->RegAt(i);
      if (!ret.contains(reg)) {
        ret[reg] = RegLifetime{
            .start = insn,
            .reg_type = RegType::kUnknown,
        };
      }
      ret[reg].end = next_insn;
      // Update RegType if still unknown. Note that it's also set to unknown
      // when LiveIn.
      if (ret[reg].reg_type == RegType::kUnknown) {
        // Note not all instructions will explicitly require GP or XMM so it
        // might still be unknown.
        if (insn->RegKindAt(i).RegClass()->IsSubsetOf(&kXmmReg)) {
          ret[reg].reg_type = RegType::kXmm;
        } else if (insn->RegKindAt(i).RegClass()->IsSubsetOf(&kGeneralReg32) ||
                   insn->RegKindAt(i).RegClass()->IsSubsetOf(&kGeneralReg64)) {
          ret[reg].reg_type = RegType::kGeneral;
        }
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

// Increment count in inc_map based on whether lifetime reg_type.
void IncrementBy(RegLifetimeCount* count, RegType reg_type, const int n) {
  switch (reg_type) {
    case RegType::kGeneral:
      count->general += n;
      break;
    case RegType::kXmm:
      count->xmm += n;
      break;
    case RegType::kUnknown:
      // This happens if a register isn't ever used with an instruction which
      // requires either an XMM or General register in the current basic block
      // so we're unable to infer what type it is. For example if it's just
      // live_in and live_out or only used for PSEUDOCOPY.
      // TODO(453652939) Improve this.
      break;
  };
}

void RegLifetimeCount::Decrement(RegType reg_type) {
  IncrementBy(this, reg_type, -1);
}

void RegLifetimeCount::Increment(RegType reg_type) {
  IncrementBy(this, reg_type, 1);
}

RegLifetimeCounts CountRegLifetimes(MachineIR* machine_ir, MachineBasicBlock* bb) {
  RegLifetimeCounts ret(machine_ir->arena());
  CHECK(!bb->insn_list().empty());
  auto lifetime_map = CountRegLifetimeMap(machine_ir, bb);
  auto first_insn = bb->insn_list().front();
  // TODO(452696539): Use ArenaVector here?
  ArenaMap<berberis::MachineInsn*, RegLifetimeCount> increment_map(machine_ir->arena());
  for (auto insn : bb->insn_list()) {
    increment_map[insn] = RegLifetimeCount{
        .general = 0,
        .xmm = 0,
    };
  }

  // This is more complicated than just going through insn_list and checking for
  // start/ends but that would require O(insns * regs) time to loop over regs while
  // this is O(insns) + O(regs) at the expense of using more memory.
  for (auto [_, lifetime] : lifetime_map) {
    if (std::holds_alternative<LiveIn>(lifetime.start)) {
      increment_map[first_insn].Increment(lifetime.reg_type);
    } else {
      auto* insn = std::get<berberis::MachineInsn*>(lifetime.start);
      increment_map[insn].Increment(lifetime.reg_type);
    }

    if (std::holds_alternative<berberis::MachineInsn*>(lifetime.end)) {
      auto* insn = std::get<berberis::MachineInsn*>(lifetime.end);
      increment_map[insn].Decrement(lifetime.reg_type);
    }
  }

  RegLifetimeCount current_count = {
      .general = 0,
      .xmm = 0,
  };
  for (auto* insn : bb->insn_list()) {
    current_count.general += increment_map[insn].general;
    current_count.xmm += increment_map[insn].xmm;
    ret[insn] = current_count;
  }

  return ret;
}

}  // namespace berberis::x86_64
