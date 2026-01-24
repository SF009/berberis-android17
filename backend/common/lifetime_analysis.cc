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

#include "berberis/backend/common/lifetime_analysis.h"

#include "berberis/backend/common/lifetime.h"
#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/checks.h"
#include "berberis/base/tracing.h"

namespace berberis {

VRegLifetime* VRegLifetimeAnalysis::GetVRegLifetime(MachineReg r, int begin, bool is_input) {
  VRegLifetime*& lifetime = vreg_lifetimes_.at(r.GetVRegIndex());
  if (lifetime) {
    // Ensure the lifetime has live range for current basic block.
    // Use last live range begin to check that, as lifetime end might be equal
    // to bb_tick_ both when register lives out of prev basic block and when
    // register lives into current basic block but yet has no accesses (so last
    // live range is [bb_tick_, bb_tick_)).
    if (lifetime->LastLiveRangeBegin() < bb_tick_) {
      if (is_input) {
        TRACE("Error: first lifetime access in basic block must not be input : v%u",
              r.GetVRegIndex());
        return nullptr;
      }
      lifetime->StartLiveRange(begin);
    }
  } else {
    if (is_input) {
      TRACE("Error: first seen lifetime access must not be input : v%u", r.GetVRegIndex());
      return nullptr;
    }
    // Newly created lifetime last live range will start at 'begin'.
    lifetimes_->push_back(VRegLifetime(arena_, begin));
    lifetime = &lifetimes_->back();
  }
  return lifetime;
}

void VRegLifetimeAnalysis::AppendAccess(const VRegAccess& access) {
  VRegLifetime* lifetime = GetVRegLifetime(access.GetVReg(), access.begin(), access.IsInput());
  CHECK(lifetime);
  lifetime->AppendAccess(access);
}

// Set move hint for vreg to vreg move.
void VRegLifetimeAnalysis::TrySetMoveHint(const MachineInsn* insn) {
  if (!insn->is_copy()) {
    return;
  }

  // Copy should have 2 vreg operands.
  CHECK_EQ(insn->NumRegOperands(), 2);
  MachineReg dst = insn->RegAt(0);
  if (!dst.IsVReg()) {
    return;
  }
  MachineReg src = insn->RegAt(1);
  if (!src.IsVReg()) {
    return;
  }

  // Lifetimes must exist.
  auto dst_lifetime = vreg_lifetimes_.at(dst.GetVRegIndex());
  auto src_lifetime = vreg_lifetimes_.at(src.GetVRegIndex());

  dst_lifetime->SetMoveHint(src_lifetime);
  dst_lifetime->LinkCoalescingCandidate(src_lifetime);
}

void VRegLifetimeAnalysis::AddInsn(const MachineInsnListPosition& pos) {
  const MachineInsn* insn = pos.insn();

  // To get lifetimes sorted by begin, first add use and use-def operands,
  // then def-only operands.

  // Walk operands staring at tick_ : use, use-def and def-early-clobber.
  for (int i = 0; i < insn->NumRegOperands(); ++i) {
    if (!insn->RegAt(i).IsVReg()) {
      continue;
    }

    const MachineRegKind& reg_kind = insn->RegKindAt(i);
    if (!reg_kind.IsInput() && !reg_kind.IsDefEarlyClobber()) {
      continue;
    }
    AppendAccess(VRegAccess(pos, i, tick_, tick_ + (reg_kind.IsDef() ? 2 : 1)));
  }

  // Walk def-only register operands.
  for (int i = 0; i < insn->NumRegOperands(); ++i) {
    if (!insn->RegAt(i).IsVReg()) {
      continue;
    }

    // Skip accesses that are already processed.
    const MachineRegKind& reg_kind = insn->RegKindAt(i);
    if (reg_kind.IsInput() || reg_kind.IsDefEarlyClobber()) {
      continue;
    }

    AppendAccess(VRegAccess(pos, i, tick_ + 1, tick_ + 2));
  }

  TrySetMoveHint(insn);

  // Instruction have got 2 ticks:
  // - read inputs ('use' operands)
  // - write outputs ('def' operands)
  tick_ += 2;
}

}  // namespace berberis
