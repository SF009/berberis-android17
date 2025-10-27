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

#ifndef BERBERIS_BACKEND_X86_64_RENAME_COPY_USES_H_
#define BERBERIS_BACKEND_X86_64_RENAME_COPY_USES_H_

#include <stdint.h>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/arena_vector.h"

namespace berberis::x86_64 {

class RenameCopyUsesMap {
 public:
  explicit RenameCopyUsesMap(MachineIR* machine_ir)
      : map_(machine_ir->NumVReg(), {kInvalidMachineReg, 0, 0}, machine_ir->arena()) {}

  void RenameUseIfMapped(berberis::MachineInsn* insn, int i);
  void ProcessDef(berberis::MachineInsn* insn, int i);
  void ProcessCopy(berberis::MachineInsn* copy);
  void Tick() { time_++; }
  void StartBasicBlock(MachineBasicBlock* bb);
  MachineReg Get(MachineReg reg) const;

 private:
  struct RenameData {
    MachineReg renamed;
    uint64_t renaming_time;
    uint64_t last_def_time;
  };

  RenameData& RenameDataForReg(MachineReg reg) { return map_.at(reg.GetVRegIndex()); }
  const RenameData& RenameDataForReg(MachineReg reg) const { return map_.at(reg.GetVRegIndex()); }

  ArenaVector<RenameData> map_;
  // Since we are not SSA or SSI we keep track of time of definitions to see if
  // mappings are still active.
  uint64_t time_ = 0;
  MachineBasicBlock* bb_;
};

class DuplicateLiveOutsMap {
 public:
  explicit DuplicateLiveOutsMap(MachineIR* machine_ir)
      : duplicate_live_outs_map_(
            machine_ir->NumBasicBlocks(),
            ArenaVector<MachineReg>(machine_ir->NumVReg(), kInvalidMachineReg, machine_ir->arena()),
            machine_ir->arena()),
        registers_defined_map_(machine_ir->NumBasicBlocks(),
                               ArenaVector<bool>(machine_ir->NumVReg(), false, machine_ir->arena()),
                               machine_ir->arena()),
        bb_contains_duplicates_map_(machine_ir->NumBasicBlocks(), false, machine_ir->arena()) {}

  void SaveDuplicates(int bb_id, MachineReg reg1, MachineReg reg2);

  void ProcessDef(const berberis::MachineBasicBlock* bb, const berberis::MachineInsn* insn, int i);

  [[nodiscard]] MachineReg GetDuplicateForTesting(const berberis::MachineBasicBlock* bb,
                                                  MachineReg reg) const {
    return GetDuplicate(bb, reg);
  }

  [[nodiscard]] bool BasicBlockDefinesRegisterForTesting(const berberis::MachineBasicBlock* bb,
                                                         const MachineReg reg) const {
    return BasicBlockDefinesRegister(bb, reg);
  }

  [[nodiscard]] bool BasicBlockContainsDuplicateLiveOuts(
      const berberis::MachineBasicBlock* bb) const {
    return bb_contains_duplicates_map_.at(bb->id());
  }

  void RenameAndRemoveDuplicateLiveIns(berberis::MachineIR* machine_ir,
                                       berberis::MachineBasicBlock* bb,
                                       const berberis::MachineBasicBlock* prev_bb);

 private:
  [[nodiscard]] MachineReg GetDuplicate(const berberis::MachineBasicBlock* bb,
                                        MachineReg reg) const {
    if (!reg.IsVReg()) {
      return kInvalidMachineReg;
    }
    return duplicate_live_outs_map_.at(bb->id()).at(reg.GetVRegIndex());
  }

  [[nodiscard]] bool BasicBlockDefinesRegister(const berberis::MachineBasicBlock* bb,
                                               const MachineReg reg) const {
    return registers_defined_map_.at(bb->id()).at(reg.GetVRegIndex());
  }

  // duplicate_live_outs_map_[x][y] == z means:
  // at the end of basic block x, register y has the same value as z, where both y and z
  // are live_out registers.
  ArenaVector<ArenaVector<MachineReg>> duplicate_live_outs_map_;

  // registers_defined_map_[x][y] == true means:
  // basic block x contains an instruction which defines register y.
  ArenaVector<ArenaVector<bool>> registers_defined_map_;

  // bb_contains_duplicates_map_[x] == true means: bb x contains duplicate live_outs.
  ArenaVector<bool> bb_contains_duplicates_map_;
};

void ComputeDuplicateLiveOuts(MachineIR* machine_ir,
                              MachineBasicBlock* bb,
                              const RenameCopyUsesMap* rename_copy_uses_map,
                              DuplicateLiveOutsMap* duplicate_live_outs_map);

void RenameCopyUsesInBasicBlock(MachineBasicBlock* bb,
                                RenameCopyUsesMap* rename_copy_uses_map,
                                DuplicateLiveOutsMap* duplicate_live_outs_map);
void RenameCopyUses(MachineIR* machine_ir);

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_RENAME_COPY_USES_H_
