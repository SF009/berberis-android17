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

#include "berberis/backend/x86_64/merge_basic_blocks.h"

#include <iterator>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/checks.h"

namespace berberis::x86_64 {

// Merges the contents of basic block bb_from into bb_to.
// In doing so, it modifies bb_from, making bb_from invalid after ApplyMerge is called.
void ApplyMerge(MachineBasicBlock* bb_to, MachineBasicBlock* bb_from) {
  // BB merges should occur at the stage where BB's have no live_out registers.
  CHECK_EQ(bb_to->live_out().size(), 0);

  CHECK_EQ(bb_to->insn_list().back()->opcode(), kMachineOpPseudoBranch);
  bb_to->insn_list().pop_back();
  bb_to->insn_list().splice(bb_to->insn_list().end(), bb_from->insn_list());

  bb_to->out_edges().clear();
  for (auto edge : bb_from->out_edges()) {
    edge->set_src(bb_to);
    bb_to->out_edges().push_back(edge);
  }
}

void MergeBasicBlocks(MachineIR* machine_ir) {
  for (auto bb_it = machine_ir->bb_list().begin(); bb_it != machine_ir->bb_list().end();) {
    auto bb = *bb_it;
    if (bb->in_edges().size() != 1) {
      bb_it = std::next(bb_it);
      continue;
    }
    auto prev_bb = bb->in_edges().at(0)->src();
    if (prev_bb->out_edges().size() != 1) {
      bb_it = std::next(bb_it);
      continue;
    }
    // Current bb only has one predecessor. The predecessor only only has one successor.
    // These basic blocks can be merged.
    ApplyMerge(prev_bb, bb);
    bb_it = machine_ir->bb_list().erase(bb_it);
  }
}

}  // namespace berberis::x86_64
