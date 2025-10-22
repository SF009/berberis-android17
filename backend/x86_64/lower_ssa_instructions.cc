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

#include "berberis/backend/x86_64/lower_ssa_instructions.h"

#include <utility>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/checks.h"

namespace berberis::x86_64 {

void LowerSSAInstructions(MachineIR* machine_ir) {
  for (auto* bb : machine_ir->bb_list()) {
    auto& insn_list = bb->insn_list();
    for (auto insn_it = insn_list.begin(); insn_it != insn_list.end();) {
      if (!((*insn_it)->opcode() & (1 << kSSAOpcodeBit))) {
        ++insn_it;
        continue;
      }
      auto new_insns = machine_ir->LowerInsn(*insn_it);
      insn_it = insn_list.erase(insn_it);
      insn_list.splice(insn_it, std::move(new_insns));
    }
  }
}

}  // namespace berberis::x86_64
