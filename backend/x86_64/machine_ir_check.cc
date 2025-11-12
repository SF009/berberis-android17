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

#include "berberis/backend/x86_64/machine_ir_check.h"

#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/algorithm.h"

namespace berberis::x86_64 {

namespace {

bool CheckEdgeInVector(const MachineEdge* target_edge, const MachineEdgeVector& edge_vector) {
  return Contains(edge_vector, target_edge);
}

bool CheckBasicBlockInIR(const MachineBasicBlock* bb, const MachineIR& machine_ir) {
  auto& bb_list = machine_ir.bb_list();
  return Contains(bb_list, bb);
}

MachineIRCheckStatus CheckNoDanglingEdgesOrBasicBlocks(const MachineIR& machine_ir,
                                                       const MachineBasicBlock* bb) {
  if (bb->out_edges().size() == 0 && bb->in_edges().size() == 0) {
    if (machine_ir.bb_list().size() != 1) {
      return kMachineIRCheckDanglingBasicBlock;
    }
    return kMachineIRCheckSuccess;
  }

  for (auto* edge : bb->out_edges()) {
    if (!CheckEdgeInVector(edge, edge->dst()->in_edges())) {
      return kMachineIRCheckDanglingEdge;
    }
    if (!CheckBasicBlockInIR(edge->dst(), machine_ir)) {
      return kMachineIRCheckDanglingBasicBlock;
    }
  }
  for (auto* edge : bb->in_edges()) {
    if (!CheckEdgeInVector(edge, edge->src()->out_edges())) {
      return kMachineIRCheckDanglingEdge;
    }
    if (!CheckBasicBlockInIR(edge->src(), machine_ir)) {
      return kMachineIRCheckDanglingBasicBlock;
    }
  }
  return kMachineIRCheckSuccess;
}

bool CheckEdgesAndInterLiveVRegsConsistency(const MachineBasicBlock* bb) {
  if (bb->in_edges().empty() && !bb->live_in().empty()) {
    return false;
  }
  if (bb->out_edges().empty() && !bb->live_out().empty()) {
    return false;
  }
  return true;
}

bool CheckInOutEdgesLinksToBasicBlock(const MachineBasicBlock* bb) {
  for (auto* edge : bb->in_edges()) {
    if (edge->dst() != bb) {
      return false;
    }
  }
  for (auto* edge : bb->out_edges()) {
    if (edge->src() != bb) {
      return false;
    }
  }
  return true;
}

bool IsBasicBlockSuccessor(const MachineBasicBlock* src, const MachineBasicBlock* dst) {
  for (auto* edge : src->out_edges()) {
    if (edge->dst() == dst) {
      return true;
    }
  }
  return false;
}

MachineIRCheckStatus CheckInsnListIntegrity(const MachineIR* ir, const MachineBasicBlock* bb) {
  const berberis::MachineInsn* enter = nullptr;
  if (ir->abi() == MachineIR::ABI::kOptimizedEnabled ||
      ir->abi() == MachineIR::ABI::kOptimizedDisabled) {
    enter = ir->bb_list().front()->insn_list().front();
    // Enter must be the very first instruction in the IR.
    if (enter->opcode() != kMachineOpEnter) {
      return kMachineIRWrongEnterInsnLocation;
    }
  }
  for (auto* insn : bb->insn_list()) {
    switch (insn->opcode()) {
      case MachineOpcode::kMachineOpEnter:
        if (ir->abi() == MachineIR::ABI::kRegular) {
          return kMachineIRWrongEnterInsnLocation;
        }
        if (insn != enter) {
          return kMachineIRWrongEnterInsnLocation;
        };
        break;
      case MachineOpcode::kMachineOpPseudoIndirectJump:
      case MachineOpcode::kMachineOpPseudoJump:
        if (insn != bb->insn_list().back()) {
          return kMachineIRWrongControlFlowInsnLocation;
        }
        return bb->out_edges().empty() ? kMachineIRCheckSuccess
                                       : kMachineIRControlFlowInsnSuccessorMismatch;
      case MachineOpcode::kMachineOpBranch: {
        if (insn != bb->insn_list().back()) {
          return kMachineIRWrongControlFlowInsnLocation;
        }
        const Branch* branch = reinterpret_cast<const Branch*>(insn);
        return IsBasicBlockSuccessor(bb, branch->then_bb())
                   ? kMachineIRCheckSuccess
                   : kMachineIRControlFlowInsnSuccessorMismatch;
      }
      case MachineOpcode::kMachineOpCondBranch: {
        if (insn != bb->insn_list().back()) {
          return kMachineIRWrongControlFlowInsnLocation;
        }
        const CondBranch* cond_branch = reinterpret_cast<const CondBranch*>(insn);
        return (IsBasicBlockSuccessor(bb, cond_branch->then_bb()) &&
                IsBasicBlockSuccessor(bb, cond_branch->else_bb()))
                   ? kMachineIRCheckSuccess
                   : kMachineIRControlFlowInsnSuccessorMismatch;
      }
      default:
        continue;
    }
  }
  // Didn't reach any control flow instruction at the end of the block.
  return kMachineIRWrongControlFlowInsnLocation;
}

MachineIRCheckStatus CheckCFG(const MachineIR& machine_ir) {
  for (auto* bb : machine_ir.bb_list()) {
    if (!CheckEdgesAndInterLiveVRegsConsistency(bb)) {
      return kMachineIRInconsistentEdgesAndInterLiveVRegs;
    }
    if (!CheckInOutEdgesLinksToBasicBlock(bb)) {
      return kMachineIRMislinkedEdge;
    }
    auto status = CheckNoDanglingEdgesOrBasicBlocks(machine_ir, bb);
    if (status != kMachineIRCheckSuccess) {
      return status;
    }
    status = CheckInsnListIntegrity(&machine_ir, bb);
    if (status != kMachineIRCheckSuccess) {
      return status;
    }
  }
  return kMachineIRCheckSuccess;
}

}  // namespace

MachineIRCheckStatus CheckMachineIR(const MachineIR& machine_ir) {
  return CheckCFG(machine_ir);
}

}  // namespace berberis::x86_64
