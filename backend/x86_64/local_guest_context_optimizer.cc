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

void LocalGuestContextOptimizer::UnmapOlderThan(size_t pos, RegType reg_type) {
  for (auto& mapping : mem_reg_map_) {
    if (!mapping.has_value()) {
      continue;
    }
    auto reg_usage = mapping.value();
    // Ignore immediates.
    if (std::holds_alternative<uint64_t>(reg_usage.value)) {
      continue;
    }

    MachineReg reg = std::get<MachineReg>(reg_usage.value);
    const auto& lifetime_map = reg_lifetime_counter_.GetMap();
    if (lifetime_map.at(reg).reg_type != reg_type) {
      continue;
    }

    if (lifetime_map.at(reg).end_pos <= pos) {
      mapping = std::nullopt;
    }
  }
}

void LocalGuestContextOptimizer::RemoveLocalGuestContextAccesses(
    const OptimizeLocalParams& params) {
  const size_t kGenRegLimit =
      machine_ir_->abi() == MachineIR::ABI::kOptimizedEnabled
          ? (params.general_reg_limit >= 6 ? params.general_reg_limit - 6 : 0UL)
          : params.general_reg_limit;
  const size_t kSimdRegLimit = params.simd_reg_limit;

  for (auto* bb : machine_ir_->bb_list()) {
    std::fill(mem_reg_map_.begin(), mem_reg_map_.end(), std::nullopt);
    reg_lifetime_counter_.Count(bb);

    size_t pos = 0;
    for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end();
         insn_it++, pos++) {
      // If the register pressure at the current instruction is too big, then cancel
      // all active mappings. So that we don't prolong lifetimes through this
      // instruction.
      if (reg_lifetime_counter_.RegCountAt(pos, RegType::kGeneral) >= kGenRegLimit) {
        UnmapOlderThan(pos, RegType::kGeneral);
      }
      if (reg_lifetime_counter_.RegCountAt(pos, RegType::kXmm) >= kSimdRegLimit) {
        UnmapOlderThan(pos, RegType::kXmm);
      }

      if (machine_ir_->IsCPUStateGet(*insn_it) || machine_ir_->IsCPUStatePut(*insn_it)) {
        auto* insn = AsMachineInsnX86_64(*insn_it);

        if (machine_ir_->IsCPUStatePut(insn)) {
          // Replacing PUT doesn't prolong the lifetime of its argument. It will
          // only be prolonged if we optimize next GET.
          ReplacePutAndUpdateMap(bb->insn_list(), insn_it);
        } else if (machine_ir_->IsCPUStateGet(insn)) {
          std::optional<MachineReg> src_reg_opt = ReplaceGetAndUpdateMap(insn_it);
          if (!src_reg_opt.has_value()) {
            continue;
          }

          // If GET replacement is successful, it means there was an allowed
          // active mapping, the lifetime for which is now prolonged, which
          // we need to reflect in reg_lifetimes_counter_.
          auto src_reg = src_reg_opt.value();
          if (reg_lifetime_counter_.LifetimeAt(src_reg).reg_type == RegType::kUnknown) {
            continue;
          }
          size_t old_end_pos = reg_lifetime_counter_.LifetimeAt(src_reg).end_pos;
          // TODO(b/459820538): UpdateLastUse could return positions where we go
          // over the limit so we don't have to recompute it again below.
          reg_lifetime_counter_.UpdateLastUse(src_reg, *std::next(insn_it), pos + 1);

          // TODO(b/459820538): optimize this.
          // Now, with the prolonged lifetime, the pressure may be reaching the
          // limit at one of the previous instructions. If that happens, cancel
          // all active mappings to make sure the next optimization doesn't
          // overflow that limit.
          auto reg_type = reg_lifetime_counter_.LifetimeAt(src_reg).reg_type;
          const size_t kLimit = reg_type == RegType::kGeneral ? kGenRegLimit : kSimdRegLimit;

          for (size_t i = pos; i >= old_end_pos; i--) {
            size_t count = reg_type == RegType::kGeneral
                               ? reg_lifetime_counter_.RegCountAt(i, RegType::kGeneral)
                               : reg_lifetime_counter_.RegCountAt(i, RegType::kXmm);
            if (count >= kLimit) {
              UnmapOlderThan(i, reg_type);
              break;
            }
          }
        }
      }
    }
  }
}

// Optimizes a GET instruction if possible by replacing with PSEUDOCOPY, while
// also setting up the mapping for future optimizations. On successful
// optimization, returns the source register we made a copy from.
std::optional<MachineReg> LocalGuestContextOptimizer::ReplaceGetAndUpdateMap(
    const MachineInsnList::iterator insn_it) {
  auto* insn = AsMachineInsnX86_64(*insn_it);
  auto dst = insn->RegAt(0);
  auto disp = insn->disp();

  // We only need to keep this load instruction if this is the first access to
  // the guest context at disp.
  if (!mem_reg_map_[disp].has_value()) {
    mem_reg_map_[disp] = {dst, {}};
    return std::nullopt;
  }

  auto copy_size = insn->opcode() == kMachineOpMovdqaXRegMemBaseDisp ? 16 : 8;
  if (std::holds_alternative<MachineReg>(mem_reg_map_[disp].value().value)) {
    MachineReg ret = std::get<MachineReg>(mem_reg_map_[disp].value().value);
    *insn_it = machine_ir_->NewInsn<Copy>(dst, ret, copy_size);
    return ret;
  } else {
    CHECK(insn->opcode() != kMachineOpMovdqaXRegMemBaseDisp &&
          insn->opcode() != kMachineOpMovsdXRegMemBaseDisp);
    *insn_it =
        machine_ir_->NewInsn<MovqRegImm>(dst, std::get<uint64_t>(mem_reg_map_[disp].value().value));
  }
  return std::nullopt;
}

void LocalGuestContextOptimizer::ReplacePutAndUpdateMap(MachineInsnList& insn_list,
                                                        const MachineInsnList::iterator insn_it) {
  auto* insn = AsMachineInsnX86_64(*insn_it);
  auto disp = insn->disp();

  if (mem_reg_map_[disp].has_value() && mem_reg_map_[disp].value().last_store.has_value()) {
    // Remove the last store instruction.
    auto last_store_it = mem_reg_map_[disp].value().last_store.value();
    insn_list.erase(last_store_it);
  }

  MappedValue new_value;
  if (insn->opcode() == kMachineOpMovqMemBaseDispImm) {
    new_value = insn->imm();
  } else {
    new_value = insn->RegAt(1);
  }
  mem_reg_map_[disp] = {new_value, {insn_it}};
}

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params) {
  LocalGuestContextOptimizer optimizer(machine_ir);
  optimizer.RemoveLocalGuestContextAccesses(params);
}

}  // namespace berberis::x86_64
