/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "berberis/backend/x86_64/local_guest_context_optimizer.h"

#include <bitset>
#include <cstddef>
#include <optional>
#include <variant>

#include "berberis/backend/x86_64/machine_ir_analysis.h"
#include "berberis/backend/x86_64/machine_ir_opt.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/arena_vector.h"

namespace berberis::x86_64 {

using OffsetCounterMap = ArenaVector<std::pair<size_t, int>>;

void LocalGuestContextOptimizer::UnmapOlderThan(size_t pos, RegType reg_type_to_unmap) {
  MemRegUsageMap& mem_reg_map = context_map_.mem_reg_map;
  RegLifetimeCounter& reg_counter = context_map_.reg_counter;
  for (auto& mapping : mem_reg_map) {
    if (!mapping.has_value()) {
      continue;
    }
    auto reg_usage = mapping.value();
    // Ignore immediates.
    if (std::holds_alternative<uint64_t>(reg_usage.value)) {
      continue;
    }

    MachineReg mapped_reg = std::get<MachineReg>(reg_usage.value);
    const auto& mapped_lifetime = reg_counter.GetLifetimeAt(mapped_reg);
    if (mapped_lifetime.reg_type != reg_type_to_unmap) {
      continue;
    }

    if (mapped_lifetime.end_pos <= pos) {
      mapping = std::nullopt;
    }
  }
}

void LocalGuestContextOptimizer::RemoveLocalGuestContextAccesses() {
  MachineBasicBlockList nonloop_nodes(machine_ir_->arena());
  if (params_.global_opt_enabled) {
    // We need reverse post order for global guest context optimization.
    ReorderBasicBlocksInReversePostOrder(machine_ir_);
    nonloop_nodes = FindNonloopNodes(machine_ir_);
  }

  for (auto* bb : machine_ir_->bb_list()) {
    MemRegUsageMap& mem_reg_map = context_map_.mem_reg_map;
    std::fill(mem_reg_map.begin(), mem_reg_map.end(), std::nullopt);
    if (params_.global_opt_enabled && EligibleForGlobalOpt(nonloop_nodes, bb)) {
      InitMemRegMapFromPreds(bb);
    }
    RegLifetimeCounter& reg_counter = context_map_.reg_counter;
    reg_counter.Count(bb);

    size_t pos = 0;
    for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end();
         insn_it++, pos++) {
      // If the register pressure at the current instruction is too big, then cancel
      // active mappings with lifetimes which end before current instruction.
      if (reg_counter.RegCountAt(pos, RegType::kGeneral) >= params_.general_reg_limit) {
        UnmapOlderThan(pos, RegType::kGeneral);
      }
      if (reg_counter.RegCountAt(pos, RegType::kXmm) >= params_.simd_reg_limit) {
        UnmapOlderThan(pos, RegType::kXmm);
      }

      if (machine_ir_->IsCPUStateGet(*insn_it) || machine_ir_->IsCPUStatePut(*insn_it)) {
        auto* insn = AsMachineInsnX86_64(*insn_it);

        if (machine_ir_->IsCPUStatePut(insn)) {
          // Replacing PUT doesn't prolong the lifetime of its argument. It will
          // only be prolonged if we optimize next GET.
          ReplacePutAndUpdateMap(bb, insn_it);
        } else if (machine_ir_->IsCPUStateGet(insn)) {
          std::optional<MachineReg> src_reg_opt = ReplaceGetAndUpdateMap(insn_it);
          if (!src_reg_opt.has_value()) {
            continue;
          }

          // If GET replacement is successful, it means there was an allowed
          // active mapping, the lifetime for which is now prolonged, which
          // we need to reflect in reg_lifetimes_counter_.
          auto src_reg = src_reg_opt.value();
          const auto& lifetime = reg_counter.GetLifetimeAt(src_reg);
          if (lifetime.reg_type == RegType::kUnknown) {
            continue;
          }
          UpdateRegLastUse(src_reg, pos + 1, AsMachineInsnX86_64(insn)->disp());
        }
      }
    }

    if (params_.global_opt_enabled) {
      PrepareGlobalMappingsForSuccessors(bb, nonloop_nodes);
    }
  }
}

bool LocalGuestContextOptimizer::EligibleForGlobalOpt(const MachineBasicBlockList& nonloop_nodes,
                                                      MachineBasicBlock* bb) {
  if (!Contains(nonloop_nodes, bb)) {
    return false;
  }
  for (auto in_edge : bb->in_edges()) {
    MachineBasicBlock* pred = in_edge->src();
    if (!Contains(nonloop_nodes, pred)) {
      return false;
    }
  }
  return true;
}

void LocalGuestContextOptimizer::InitMemRegMapFromPreds(MachineBasicBlock* /*bb*/) {
  // TODO(b/449996703) implement this.
}

void LocalGuestContextOptimizer::PopulateOffsetsUsed(MachineBasicBlock* bb,
                                                     ArenaVector<uint32_t>& offsets) {
  // Keep track of registers we PUT to and those we have already seen as they should be ignored.
  std::bitset<sizeof(CPUState)> ignored;
  for (auto* insn : bb->insn_list()) {
    if (machine_ir_->IsCPUStatePut(insn)) {
      ignored.set(AsMachineInsnX86_64(insn)->disp());
    } else if (machine_ir_->IsCPUStateGet(insn)) {
      auto disp = AsMachineInsnX86_64(insn)->disp();
      CHECK_LT(disp, ignored.size());
      if (!ignored[disp]) {
        offsets.push_back(disp);
        ignored.set(disp);
      }
    }
  }
}

// Optimizes a GET instruction if possible by replacing with COPY, while
// also setting up the mapping for future optimizations. On successful
// optimization, returns the source register we made a copy from.
std::optional<MachineReg> LocalGuestContextOptimizer::ReplaceGetAndUpdateMap(
    const MachineInsnList::iterator insn_it) {
  auto* insn = AsMachineInsnX86_64(*insn_it);
  auto dst = insn->RegAt(0);
  auto disp = insn->disp();
  auto& mem_reg_map = context_map_.mem_reg_map;

  // We only need to keep this load instruction if this is the first access to
  // the guest context at disp.
  if (!mem_reg_map[disp].has_value()) {
    mem_reg_map[disp] = {dst, {}};
    return std::nullopt;
  }

  if (std::holds_alternative<MachineReg>(mem_reg_map[disp].value().value)) {
    MachineReg ret = std::get<MachineReg>(mem_reg_map[disp].value().value);
    *insn_it = machine_ir_->NewInsn<Copy>(dst, ret, insn->RegKindAt(0).RegClass());
    return ret;
  } else {
    CHECK(insn->opcode() != kMachineOpMovdqaXRegMemBaseDisp &&
          insn->opcode() != kMachineOpMovsdXRegMemBaseDisp);
    *insn_it =
        machine_ir_->NewInsn<MovqRegImm>(dst, std::get<uint64_t>(mem_reg_map[disp].value().value));
  }
  return std::nullopt;
}

void LocalGuestContextOptimizer::ReplacePutAndUpdateMap(MachineBasicBlock* bb,
                                                        const MachineInsnList::iterator insn_it) {
  auto* insn = AsMachineInsnX86_64(*insn_it);
  auto disp = insn->disp();
  auto& mem_reg_map = context_map_.mem_reg_map;

  if (mem_reg_map[disp].has_value() && mem_reg_map[disp].value().last_store.has_value()) {
    // Remove the last store instruction.
    auto last_store_it = mem_reg_map[disp].value().last_store.value();
    bb->insn_list().erase(last_store_it);
  }

  MappedValue new_value;
  if (insn->opcode() == kMachineOpMovqMemBaseDispImm) {
    new_value = insn->imm();
  } else {
    new_value = insn->RegAt(1);
  }
  mem_reg_map[disp] = {new_value, {insn_it}};
}

void LocalGuestContextOptimizer::PrepareGlobalMappingsForSuccessors(
    MachineBasicBlock* bb,
    const MachineBasicBlockList& nonloop_nodes) {
  auto& offsets_used = offsets_used_reservation_;
  auto& mem_reg_map = context_map_.mem_reg_map;
  auto& reg_counter = context_map_.reg_counter;
  // Get offsets used in successors.
  offsets_used.clear();
  for (auto* edge : bb->out_edges()) {
    // TODO(449996703): we should probably coalesce when there are two successors.
    if (EligibleForGlobalOpt(nonloop_nodes, edge->dst())) {
      PopulateOffsetsUsed(edge->dst(), offsets_used);
    }
  }

  for (uint32_t offset : offsets_used) {
    if (!mem_reg_map.at(offset).has_value()) {
      continue;
    }
    ArenaVector<DispRegMapping>& global_mappings = global_mappings_.at(bb->id());
    MappedValue mapped_val = mem_reg_map.at(offset).value().value;
    // For constants we can always reuse them.
    if (std::holds_alternative<uint64_t>(mapped_val)) {
      global_mappings.push_back(
          DispRegMapping{.disp = offset, .value = std::get<uint64_t>(mapped_val)});
    } else if (std::holds_alternative<MachineReg>(mapped_val)) {
      global_mappings.push_back(
          DispRegMapping{.disp = offset, .value = std::get<MachineReg>(mapped_val)});
      MachineReg reg = std::get<MachineReg>(mapped_val);
      // Note we do not push them to live_out until they're actually used.
      // Although we try our best to emulate the register lifetimes, it's not
      // exact so if we pushed it to live_out it's possible we would
      // unnecessarily cause register overuse as the registers aren't
      // guaranteed to be used in successors here.
      // Update register lifetimes and unmap if necessary.
      UpdateRegLastUse(reg, reg_counter.GetCounts().size(), offset);
    }
  }
}

void LocalGuestContextOptimizer::UpdateRegLastUse(MachineReg reg, size_t new_pos, uint32_t offset) {
  const size_t kLimit = IsSimdOffset(offset) ? params_.simd_reg_limit : params_.general_reg_limit;
  std::optional<size_t> pos_over_limit =
      context_map_.reg_counter.UpdateLastUse(reg, new_pos, kLimit);
  if (pos_over_limit.has_value()) {
    UnmapOlderThan(pos_over_limit.value(),
                   IsSimdOffset(offset) ? RegType::kXmm : RegType::kGeneral);
  }
}

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params) {
  LocalGuestContextOptimizer optimizer(machine_ir, params);
  optimizer.RemoveLocalGuestContextAccesses();
}

}  // namespace berberis::x86_64
