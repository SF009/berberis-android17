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

#include <cstddef>
#include <iterator>
#include <optional>
#include <variant>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/arena_map.h"

namespace berberis::x86_64 {

void RegLifetimeCounter::AddLiveInLifetime(const MachineReg& reg, RegType reg_type) {
  lifetime_map_.at(reg.GetVRegIndex()) =
      RegLifetime{.start_pos = 0, .end_pos = 0, .reg_type = reg_type};
}

void RegLifetimeCounter::Count(MachineBasicBlock* bb) {
  CountRegLifetimeMap(bb);
  CountRegLifetimes(bb);
}

size_t RegLifetimeCounter::RegCountAt(size_t pos, RegType reg_type) const {
  CHECK(reg_type != RegType::kUnknown);
  return reg_type == RegType::kGeneral ? lifetime_counts_.at(pos).general
                                       : lifetime_counts_.at(pos).xmm;
}

std::optional<size_t> RegLifetimeCounter::UpdateLastUse(MachineReg reg,
                                                        size_t end_pos,
                                                        const size_t kLimit) {
  CHECK(LifetimeAt(reg).has_value());
  CHECK_LE(static_cast<size_t>(end_pos), lifetime_counts_.size());

  auto& lifetime = lifetime_map_.at(reg.GetVRegIndex()).value();
  size_t old_end_pos = lifetime.end_pos;

  // Don't do anything if this doesn't extend lifetime.
  if (end_pos <= old_end_pos) {
    return std::nullopt;
  }

  std::optional<size_t> ret = std::nullopt;
  for (size_t i = old_end_pos; i < end_pos; i++) {
    size_t count = lifetime_counts_[i].Increment(lifetime.reg_type);
    if (count >= kLimit) {
      ret = {i};
    }
  }

  lifetime.end_pos = end_pos;
  return ret;
}

void RegLifetimeCounter::CountRegLifetimeMap(MachineBasicBlock* bb) {
  CHECK(!bb->insn_list().empty());
  std::fill(lifetime_map_.begin(), lifetime_map_.end(), std::nullopt);
  // First get all live_ins.
  for (auto reg : bb->live_in()) {
    if (reg.IsVReg()) {
      lifetime_map_[reg.GetVRegIndex()] = RegLifetime{
          .start_pos = 0,
          .end_pos = 0,
          .reg_type = RegType::kUnknown,
      };
    }
  }

  // Note that we don't account for the same register being redefined which
  // would imply separate lifetimes but for our current needs that is sufficient.
  // For example:
  //   v1 = 2
  //   ADD v1 v2
  //   ... # Here v1 can actually be discarded but our algorithm considers it alive.
  //   v1 = 5
  size_t pos = 0;
  for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end(); insn_it++, pos++) {
    auto insn = *insn_it;
    for (int i = 0; i < insn->NumRegOperands(); i++) {
      // Skip flags register.
      if (insn->RegKindAt(i).RegClass()->IsSubsetOf(&kFLAGS)) {
        continue;
      }
      // If this is the first time we are seeing this register set start.
      auto reg = insn->RegAt(i);
      // Only count vregs.
      if (!reg.IsVReg()) {
        continue;
      }
      if (!LifetimeAt(reg).has_value()) {
        lifetime_map_[reg.GetVRegIndex()] = RegLifetime{
            .start_pos = pos,
            .reg_type = RegType::kUnknown,
        };
      }
      auto& lifetime = lifetime_map_.at(reg.GetVRegIndex()).value();
      lifetime.end_pos = pos + 1;
      // Update RegType if still unknown. Note that it's also set to unknown
      // when LiveIn.
      if (lifetime.reg_type == RegType::kUnknown) {
        // Note not all instructions will explicitly require GP or XMM so it
        // might still be unknown.
        if (insn->RegKindAt(i).RegClass()->IsSubsetOf(&kXmmReg)) {
          lifetime.reg_type = RegType::kXmm;
        } else if (insn->RegKindAt(i).RegClass()->IsSubsetOf(&kGeneralReg32) ||
                   insn->RegKindAt(i).RegClass()->IsSubsetOf(&kGeneralReg64)) {
          lifetime.reg_type = RegType::kGeneral;
        }
      }
    }
  }

  // Finally check live_outs.
  for (auto reg : bb->live_out()) {
    if (reg.IsVReg()) {
      CHECK(LifetimeAt(reg).has_value());
      auto& lifetime = lifetime_map_[reg.GetVRegIndex()].value();
      lifetime.end_pos = pos;
    }
  }
}

// Increment count in inc_map based on whether lifetime reg_type.
size_t IncrementBy(RegLifetimeCount* count, RegType reg_type, const int n) {
  switch (reg_type) {
    case RegType::kGeneral:
      count->general += n;
      return count->general;
    case RegType::kXmm:
      count->xmm += n;
      return count->xmm;
    default:
      // This happens if a register isn't ever used with an instruction which
      // requires either an XMM or General register in the current basic block
      // so we're unable to infer what type it is. For example if it's just
      // live_in and live_out or only used for COPY.
      // TODO(453652939) Improve this.
      break;
  };
  return 0;
}

size_t RegLifetimeCount::Decrement(RegType reg_type) {
  return IncrementBy(this, reg_type, -1);
}

size_t RegLifetimeCount::Increment(RegType reg_type) {
  return IncrementBy(this, reg_type, 1);
}

void RegLifetimeCounter::CountRegLifetimes(MachineBasicBlock* bb) {
  CHECK(!bb->insn_list().empty());
  lifetime_counts_.resize(bb->insn_list().size());
  std::fill(lifetime_counts_.begin(), lifetime_counts_.end(), RegLifetimeCount{});
  ArenaVector<RegLifetimeCount> increment_map(
      // size+1 to make handling live_outs simpler.
      bb->insn_list().size() + 1,
      RegLifetimeCount{
          .general = 0,
          .xmm = 0,
      },
      machine_ir_->arena());

  // This is more complicated than just going through insn_list and checking for
  // start/ends but that would require O(insns * regs) time to loop over regs while
  // this is O(insns) + O(regs) at the expense of using more memory.
  for (const auto& lifetime_opt : lifetime_map_) {
    if (lifetime_opt.has_value()) {
      auto lifetime = lifetime_opt.value();
      increment_map[lifetime.start_pos].Increment(lifetime.reg_type);
      increment_map[lifetime.end_pos].Decrement(lifetime.reg_type);
    }
  }

  RegLifetimeCount current_count = {
      .general = 0,
      .xmm = 0,
  };
  int pos = 0;
  for (auto* _ : bb->insn_list()) {
    current_count.general += increment_map[pos].general;
    current_count.xmm += increment_map[pos].xmm;
    lifetime_counts_[pos] = current_count;
    pos++;
  }
}

}  // namespace berberis::x86_64
