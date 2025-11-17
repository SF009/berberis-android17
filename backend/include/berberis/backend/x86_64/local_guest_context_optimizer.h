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
        mem_reg_map_(sizeof(CPUState), std::nullopt, machine_ir->arena()),
        reg_lifetime_counter_(machine_ir) {}

  // Removes entries from mem_reg_map_ with last use <= pos.
  void UnmapOlderThan(size_t pos, RegType reg_type);
  const RegLifetimeCounter& GetLifetimeCounterForTesting() const { return reg_lifetime_counter_; }
  const MemRegUsageMap& GetMemRegUsageMapForTesting() const { return mem_reg_map_; }
  void RemoveLocalGuestContextAccesses(OptimizeLocalParams params);

 private:
  std::optional<MachineReg> ReplaceGetAndUpdateMap(const MachineInsnList::iterator insn_it);
  void ReplacePutAndUpdateMap(MachineInsnList& insn_list, const MachineInsnList::iterator insn_it);

  MachineIR* machine_ir_;
  MemRegUsageMap mem_reg_map_;
  RegLifetimeCounter reg_lifetime_counter_;
};

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params = OptimizeLocalParams());

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_LOCAL_GUEST_CONTEXT_OPTIMIZER_H_
