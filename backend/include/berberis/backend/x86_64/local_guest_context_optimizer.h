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

#ifndef BERBERIS_BACKEND_X86_64_LOCAL_GUEST_CONTEXT_OPTIMIZER_H_
#define BERBERIS_BACKEND_X86_64_LOCAL_GUEST_CONTEXT_OPTIMIZER_H_

#include <cstddef>
#include <optional>
#include <variant>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/reg_lifetime.h"
#include "berberis/base/arena_vector.h"
#include "berberis/base/config_globals.h"

namespace berberis::x86_64 {

struct OptimizeLocalParams {
  size_t general_reg_limit = 13;
  size_t simd_reg_limit = 13;
  bool global_opt_enabled = IsConfigFlagSet(kGlobalContextOptimization);
};

struct PredecessorReg {
  MachineReg reg;
};

using MappedValue = std::variant<MachineReg, uint64_t, PredecessorReg>;
struct MappedRegUsage {
  MappedValue value;
  std::optional<MachineInsnList::iterator> last_store;
};

using MemRegUsageMap = ArenaVector<std::optional<MappedRegUsage>>;

class LocalGuestContextOptimizer {
 public:
  explicit LocalGuestContextOptimizer(x86_64::MachineIR* machine_ir,
                                      const OptimizeLocalParams& params)
      : machine_ir_(machine_ir),
        context_map_(CreateContextMap(machine_ir, params.global_opt_enabled)),
        params_(params) {
    params_.general_reg_limit =
        machine_ir_->abi() == MachineIR::ABI::kOptimizedEnabled
            ? (params_.general_reg_limit >= 6 ? params_.general_reg_limit - 6 : 0UL)
            : params_.general_reg_limit;
  }

  // Removes entries from mem_reg_map with last use <= pos.
  void UnmapOlderThan(MachineBasicBlock* bb, size_t pos, RegType reg_type);
  const RegLifetimeCounter& GetLifetimeCounterForTesting(MachineBasicBlock* bb) const {
    const ContextMapping& cm = std::holds_alternative<ContextMapping>(context_map_)
                                   ? std::get<ContextMapping>(context_map_)
                                   : std::get<ContextMappingArray>(context_map_).at(bb->id());
    return cm.reg_counter;
  }
  const MemRegUsageMap& GetMemRegUsageMapForTesting(MachineBasicBlock* bb) const {
    const ContextMapping& cm = std::holds_alternative<ContextMapping>(context_map_)
                                   ? std::get<ContextMapping>(context_map_)
                                   : std::get<ContextMappingArray>(context_map_).at(bb->id());
    return cm.mem_reg_map;
  }
  void RemoveLocalGuestContextAccesses();

 private:
  struct ContextMapping {
    MemRegUsageMap mem_reg_map;
    RegLifetimeCounter reg_counter;
  };
  using ContextMappingArray = ArenaVector<ContextMapping>;

  static std::variant<ContextMapping, ContextMappingArray> CreateContextMap(
      MachineIR* machine_ir,
      bool global_opt_enabled) {
    return global_opt_enabled
               ? std::variant<ContextMapping, ContextMappingArray>(ContextMappingArray(
                     machine_ir->NumBasicBlocks(),
                     ContextMapping{
                         MemRegUsageMap(sizeof(CPUState), std::nullopt, machine_ir->arena()),
                         RegLifetimeCounter(machine_ir)},
                     machine_ir->arena()))
               : std::variant<ContextMapping, ContextMappingArray>(ContextMapping{
                     MemRegUsageMap(sizeof(CPUState), std::nullopt, machine_ir->arena()),
                     RegLifetimeCounter(machine_ir)});
  }

  ContextMapping& GetContextMapping(MachineBasicBlock* bb) {
    return std::holds_alternative<ContextMapping>(context_map_)
               ? std::get<ContextMapping>(context_map_)
               : std::get<ContextMappingArray>(context_map_).at(bb->id());
  }

  bool EligibleForGlobalOpt(const MachineBasicBlockList& nonloop_nodes, MachineBasicBlock* bb);
  void InitMemRegMapFromPreds(MachineBasicBlock* bb);
  std::optional<MachineReg> ReplaceGetAndUpdateMap(MachineBasicBlock* bb,
                                                   const MachineInsnList::iterator insn_it);
  void ReplacePutAndUpdateMap(MachineBasicBlock* bb, const MachineInsnList::iterator insn_it);

  MachineIR* machine_ir_;
  std::variant<ContextMapping, ContextMappingArray> context_map_;
  OptimizeLocalParams params_;
};

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params = OptimizeLocalParams());

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_LOCAL_GUEST_CONTEXT_OPTIMIZER_H_
