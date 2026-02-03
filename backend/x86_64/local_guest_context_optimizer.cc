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

#include <cstddef>
#include <optional>
#include <variant>

#include "berberis/base/arena_vector.h"

namespace berberis::x86_64 {

using OffsetCounterMap = ArenaVector<std::pair<size_t, int>>;

void LocalGuestContextOptimizer::UnmapOlderThan(MachineBasicBlock* bb,
                                                size_t pos,
                                                RegType reg_type) {
  MemRegUsageMap& mem_reg_map = GetContextMapping(bb).mem_reg_map;
  RegLifetimeCounter& reg_counter = GetContextMapping(bb).reg_counter;
  for (auto& mapping : mem_reg_map) {
    if (!mapping.has_value()) {
      continue;
    }
    auto reg_usage = mapping.value();
    // Ignore immediates.
    if (std::holds_alternative<uint64_t>(reg_usage.value)) {
      continue;
    }

    MachineReg reg = std::get<MachineReg>(reg_usage.value);
    const auto& lifetime = reg_counter.GetLifetimeAt(reg);
    if (lifetime.reg_type != reg_type) {
      continue;
    }

    if (lifetime.end_pos <= pos) {
      mapping = std::nullopt;
    }
  }
}

void LocalGuestContextOptimizer::RemoveLocalGuestContextAccesses(
    const OptimizeLocalParams& params) {
  OptimizeLocalParams params_copy = params;

  params_copy.general_reg_limit =
      machine_ir_->abi() == MachineIR::ABI::kOptimizedEnabled
          ? (params_copy.general_reg_limit >= 6 ? params_copy.general_reg_limit - 6 : 0UL)
          : params_copy.general_reg_limit;

  for (auto* bb : machine_ir_->bb_list()) {
    MemRegUsageMap& mem_reg_map = GetContextMapping(bb).mem_reg_map;
    if (std::holds_alternative<SingleContextMapping>(context_map_)) {
      std::fill(mem_reg_map.begin(), mem_reg_map.end(), std::nullopt);
    }
    RegLifetimeCounter& reg_counter = GetContextMapping(bb).reg_counter;
    reg_counter.Count(bb);

    size_t pos = 0;
    for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end();
         insn_it++, pos++) {
      // If the register pressure at the current instruction is too big, then cancel
      // active mappings with lifetimes which end before current instruction.
      if (reg_counter.RegCountAt(pos, RegType::kGeneral) >= params_copy.general_reg_limit) {
        UnmapOlderThan(bb, pos, RegType::kGeneral);
      }
      if (reg_counter.RegCountAt(pos, RegType::kXmm) >= params_copy.simd_reg_limit) {
        UnmapOlderThan(bb, pos, RegType::kXmm);
      }

      if (machine_ir_->IsCPUStateGet(*insn_it) || machine_ir_->IsCPUStatePut(*insn_it)) {
        auto* insn = AsMachineInsnX86_64(*insn_it);

        if (machine_ir_->IsCPUStatePut(insn)) {
          // Replacing PUT doesn't prolong the lifetime of its argument. It will
          // only be prolonged if we optimize next GET.
          ReplacePutAndUpdateMap(bb, insn_it);
        } else if (machine_ir_->IsCPUStateGet(insn)) {
          std::optional<MachineReg> src_reg_opt = ReplaceGetAndUpdateMap(bb, insn_it);
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
          const size_t kLimit = lifetime.reg_type == RegType::kGeneral
                                    ? params_copy.general_reg_limit
                                    : params_copy.simd_reg_limit;
          std::optional<size_t> pos_over_limit =
              reg_counter.UpdateLastUse(src_reg, std::next(insn_it), pos + 1, kLimit);

          // Now, with the prolonged lifetime, the pressure may be reaching the
          // limit at one of the previous instructions. If that happens, cancel
          // all active mappings to make sure the next optimization doesn't
          // overflow that limit.
          if (pos_over_limit.has_value()) {
            UnmapOlderThan(bb, pos_over_limit.value(), lifetime.reg_type);
          }
        }
      }
    }
  }
}

// Optimizes a GET instruction if possible by replacing with COPY, while
// also setting up the mapping for future optimizations. On successful
// optimization, returns the source register we made a copy from.
std::optional<MachineReg> LocalGuestContextOptimizer::ReplaceGetAndUpdateMap(
    MachineBasicBlock* bb,
    const MachineInsnList::iterator insn_it) {
  auto* insn = AsMachineInsnX86_64(*insn_it);
  auto dst = insn->RegAt(0);
  auto disp = insn->disp();
  auto& mem_reg_map = GetContextMapping(bb).mem_reg_map;

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
  auto& mem_reg_map = GetContextMapping(bb).mem_reg_map;

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

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params) {
  LocalGuestContextOptimizer optimizer(machine_ir);
  optimizer.RemoveLocalGuestContextAccesses(params);
}

}  // namespace berberis::x86_64
