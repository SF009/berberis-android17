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

namespace berberis::x86_64 {

struct OptimizeLocalParams {
  size_t general_reg_limit = 13;
  size_t simd_reg_limit = 13;
};

using MappedValue = std::variant<MachineReg, uint64_t>;
struct MappedRegUsage {
  MappedValue value;
  std::optional<MachineInsnList::iterator> last_store;
};

using MemRegUsageMap = ArenaVector<std::optional<MappedRegUsage>>;

class LocalGuestContextOptimizer {
 public:
  explicit LocalGuestContextOptimizer(x86_64::MachineIR* machine_ir)
      : machine_ir_(machine_ir),
        ctx_map_{SingleContextMapping{
            MemRegUsageMap(sizeof(CPUState), std::nullopt, machine_ir->arena()),
            RegLifetimeCounter(machine_ir)}} {}

  // Removes entries from mem_reg_map with last use <= pos.
  void UnmapOlderThan(size_t pos, RegType reg_type);
  const RegLifetimeCounter& GetLifetimeCounterForTesting() const {
    return std::get<SingleContextMapping>(ctx_map_).reg_counter;
  }
  const MemRegUsageMap& GetMemRegUsageMapForTesting() const {
    return std::get<SingleContextMapping>(ctx_map_).mem_reg_map;
  }
  void RemoveLocalGuestContextAccesses(const OptimizeLocalParams& params);

 private:
  struct SingleContextMapping {
    MemRegUsageMap mem_reg_map;
    RegLifetimeCounter reg_counter;
  };

  std::optional<MachineReg> ReplaceGetAndUpdateMap(const MachineInsnList::iterator insn_it);
  void ReplacePutAndUpdateMap(MachineInsnList& insn_list, const MachineInsnList::iterator insn_it);

  MachineIR* machine_ir_;
  std::variant<SingleContextMapping> ctx_map_;
};

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params = OptimizeLocalParams());

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_LOCAL_GUEST_CONTEXT_OPTIMIZER_H_
