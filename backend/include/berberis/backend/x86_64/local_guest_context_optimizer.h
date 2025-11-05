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

#include "berberis/backend/x86_64/machine_ir.h"

namespace berberis::x86_64 {

struct OptimizeLocalParams {
  size_t general_reg_limit = 12;
  size_t simd_reg_limit = 12;
};

class LocalGuestContextOptimizer {
 public:
  explicit LocalGuestContextOptimizer(x86_64::MachineIR* machine_ir)
      : machine_ir_(machine_ir),
        mem_reg_map_(sizeof(CPUState), std::nullopt, machine_ir->arena()) {}

  void RemoveLocalGuestContextAccesses(const OptimizeLocalParams& params);

 private:
  using MappedValue = std::variant<MachineReg, uint64_t>;
  struct MappedRegUsage {
    MappedValue value;
    std::optional<MachineInsnList::iterator> last_store;
  };

  void ReplaceGetAndUpdateMap(const MachineInsnList::iterator insn_it);
  void ReplacePutAndUpdateMap(MachineInsnList& insn_list, const MachineInsnList::iterator insn_it);

  MachineIR* machine_ir_;
  ArenaVector<std::optional<MappedRegUsage>> mem_reg_map_;
};

void RemoveLocalGuestContextAccesses(x86_64::MachineIR* machine_ir,
                                     const OptimizeLocalParams& params = OptimizeLocalParams());

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_LOCAL_GUEST_CONTEXT_OPTIMIZER_H_
