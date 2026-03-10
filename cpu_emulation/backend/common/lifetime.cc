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

#include "berberis/backend/common/lifetime.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/arena_list.h"
#include "berberis/base/checks.h"
#include "berberis/base/stringprintf.h"

namespace berberis {

void VRegAccess::Spill(MachineIR* machine_ir,
                       MachineReg hard_reg,
                       int slot,
                       bool needs_value_in_hard_reg) {
  CHECK_NE(slot, -1);
  CHECK(IsDef());
  MachineReg spill = MachineReg::CreateSpilledRegFromIndex(machine_ir->SpillSlotOffset(slot));
  if (!needs_value_in_hard_reg && pos_.insn()->is_copy() && !pos_.insn()->RegAt(1).IsSpilledReg()) {
    // Rewrite the dst of the copy itself, unless the result is mem-to-mem copy.
    CHECK_EQ(0, index_);
    pos_.insn()->SetRegAt(0, spill);
  } else {
    pos_.InsertAfter(machine_ir->NewInsn<Copy>(spill, hard_reg, GetRegClass()));
  }
}

void VRegAccess::Reload(MachineIR* machine_ir,
                        MachineReg hard_reg,
                        int slot,
                        bool needs_value_in_hard_reg) {
  CHECK_NE(slot, -1);
  CHECK(IsInput());
  MachineReg spill = MachineReg::CreateSpilledRegFromIndex(machine_ir->SpillSlotOffset(slot));
  if (!needs_value_in_hard_reg && pos_.insn()->is_copy() && !pos_.insn()->RegAt(0).IsSpilledReg()) {
    // Rewrite the src of the copy itself, unless the result is mem-to-mem copy.
    CHECK_EQ(1, index_);
    pos_.insn()->SetRegAt(1, spill);
  } else {
    pos_.InsertBefore(machine_ir->NewInsn<Copy>(hard_reg, spill, GetRegClass()));
  }
}

void VRegLiveRange::RewriteVReg(MachineIR* machine_ir, MachineReg hard_reg, int spill_slot) {
  std::optional<std::tuple<VRegAccess*, bool>> last_def;

  for (auto it = access_list().begin(); it != access_list().end(); ++it) {
    VRegAccess& access = *it;
    access.RewriteVReg(hard_reg);

    if (spill_slot != -1 && access.IsDef()) {
      last_def = {&access, DoesAccessNeedValueInHardReg(it)};
    }
  }

  // Note that for COPY insns we may overwrite the hard register assigned above with
  // a stack register.
  if (spill_slot != -1) {
    // We only need one reload per range, and only if the first access is an input.
    if (!access_list().empty() && access_list().front().IsInput()) {
      auto first_input = access_list().begin();
      first_input->Reload(
          machine_ir, hard_reg, spill_slot, DoesAccessNeedValueInHardReg(first_input));
    }
    // Only the last def in the range needs to be spilled to pass the value to other ranges.
    if (last_def.has_value()) {
      auto [def_acc, needs_value_in_hard_reg] = last_def.value();
      def_acc->Spill(machine_ir, hard_reg, spill_slot, needs_value_in_hard_reg);
    }
  }
}

std::string VRegLiveRange::GetDebugString() const {
  std::string out(StringPrintf("[%d, %d) {\n", begin(), end()));
  for (const auto& use : access_list_) {
    out += "  ";
    out += use.GetDebugString();
    out += "\n";
  }
  out += "}\n";
  return out;
}

void VRegLifetime::AppendAccess(const VRegAccess& access) {
  if (range_list_.empty()) {
    // This may happen for lifetimes constructed from split accesses.
    range_list_.push_back(VRegLiveRange(arena_, access.begin()));
  }
  if (access.IsDef() && !access.IsInput() && end() < access.begin()) {
    // This is write-only access and there is a gap between it and previous access.
    // Can insert lifetime hole.
    if (range_list_.back().access_list().empty()) {
      // If current live range is still empty, this might be live-in
      // register that gets overwritten, so remove live-in.
      range_list_.back().set_begin(access.begin());
    } else {
      range_list_.push_back(VRegLiveRange(arena_, access.begin()));
    }
  }
  range_list_.back().AppendAccess(access);
  // We assume reg classes are either nested or unrelated (so have no
  // common registers).
  if (reg_class_) {
    reg_class_ = reg_class_->GetIntersection(access.GetRegClass());
    CHECK(reg_class_);
  } else {
    reg_class_ = access.GetRegClass();
  }
}

int VRegLifetime::ComputeWeight() const {
  int weight = 0;
  for (const auto& range : range_list_) {
    const auto& accesses = range.access_list();
    if (accesses.empty()) {
      continue;
    }
    if (accesses.front().IsInput()) {
      ++weight;
    }
    if (range.has_def()) {
      ++weight;
    }
  }
  return weight;
}

std::string VRegLifetime::GetDebugString() const {
  std::string out("lifetime {\n");
  for (const auto& range : range_list_) {
    out += range.GetDebugString();
  }
  out += "}\n";
  return out;
}

bool VRegLifetime::TestInterference(const VRegLifetime& other) const {
  VRegLiveRangeList::const_iterator j = other.range_list_.begin();
  for (VRegLiveRangeList::const_iterator i = range_list_.begin();
       i != range_list_.end() && j != other.range_list_.end();) {
    if (i->end() <= j->begin()) {
      ++i;
    } else if (j->end() <= i->begin()) {
      ++j;
    } else {
      return true;
    }
  }
  return false;
}

SplitKind VRegLifetime::FindSplitPos(int begin, SplitPos* pos) {
  for (auto range_it = range_list_.begin(); range_it != range_list_.end(); ++range_it) {
    if (range_it->end() <= begin) {
      continue;
    }

    for (auto access_it = range_it->access_list().begin();
         access_it != range_it->access_list().end();
         ++access_it) {
      if (access_it->end() <= begin) {
        // Future tiny lifetime ends before 'begin'.
        continue;
      }

      // Future tiny lifetime starts at or after 'begin'.
      pos->range_it = range_it;
      pos->access_it = access_it;
      return access_it->begin() <= begin ? SPLIT_CONFLICT : SPLIT_OK;
    }
  }

  // If we got here, lifetime spans after begin but has no accesses there.
  // It can happen with live-out virtual registers.
  pos->range_it = range_list_.end();
  return SPLIT_OK;
}

void VRegLifetime::Merge(VRegLifetime* other) {
  // Checking lifetimes for emptiness is caller's responsibility.
  CHECK(!IsEmpty() && !other->IsEmpty());
  // Merges must be done before hard-reg allocations which may trigger spilling.
  CHECK(spill_slot_ == -1 && other->spill_slot_ == -1);

  auto it = range_list_.begin();
  auto other_it = other->range_list_.begin();

  while (other_it != other->range_list_.end()) {
    if (it != range_list_.end() && it->begin() < other_it->begin()) {
      ++it;
      continue;
    }
    // Move other_it to before it.
    auto moved_other_it = other_it++;
    range_list_.splice(it, other->range_list_, moved_other_it);
  }

  CHECK(reg_class_ && other->reg_class_);
  reg_class_ = reg_class_->GetIntersection(other->reg_class_);
  CHECK(reg_class_);

  for (auto candidate : other->coalescing_candidates_) {
    if (candidate != this) {
      LinkCoalescingCandidate(candidate);
    }
  }

  other->coalescing_candidates_.clear();
}

void VRegLifetime::Split(const SplitPos& split_pos,
                         SplitKind split_kind,
                         ArenaList<VRegLifetime>* new_lifetimes) {
  if (split_pos.range_it == range_list_.end()) {
    return;
  }
  auto first_range_to_split = split_pos.range_it;
  auto first_access_to_split = split_pos.access_it;
  auto& access_list = split_pos.range_it->access_list();

  // Separate lifetime for the conflicting access if needed.
  if (split_kind == SPLIT_CONFLICT_WITH_ACCESS_SEPARATION) {
    CHECK(split_pos.access_it != split_pos.range_it->access_list().end());
    auto next_access_it = std::next(split_pos.access_it);
    AddLifetimeFromAccessList(
        new_lifetimes, split_pos.access_it, next_access_it, GetSpill(), arena_);
    access_list.erase(split_pos.access_it, next_access_it);
    split_pos.range_it->UpdateHasDef();
    first_access_to_split = next_access_it;
  }
  // If we split the list of accesses in the first range, create a separate lifetime for them.
  if (first_access_to_split != access_list.begin() &&
      // May happen if first access is separated above.
      first_access_to_split != access_list.end()) {
    AddLifetimeFromAccessList(
        new_lifetimes, first_access_to_split, access_list.end(), GetSpill(), arena_);

    // Erase the accesses that were split off.
    access_list.erase(first_access_to_split, access_list.end());
    split_pos.range_it->UpdateHasDef();
    // Recompute the end of the split range.
    int new_end = 0;
    // Since we erase after the begin, there must be at least one access
    // left in the front.
    CHECK(!access_list.empty());
    for (auto access : access_list) {
      new_end = std::max(new_end, access.end());
    }
    split_pos.range_it->set_end</* kAllowShrink */ true>(new_end);
    ++first_range_to_split;
  }
  // Create new lifetimes from ranges after split pos.
  // Note: live ranges for live-ins/outs are shrunk to the actual range of the accesses.
  for (auto range_it = first_range_to_split; range_it != range_list_.end(); ++range_it) {
    AddLifetimeFromAccessList(new_lifetimes,
                              range_it->access_list().begin(),
                              range_it->access_list().end(),
                              GetSpill(),
                              arena_);
  }
  range_list_.erase(first_range_to_split, range_list_.end());
}

void VRegLifetime::Rewrite(MachineIR* machine_ir) {
  for (auto& range : range_list_) {
    range.RewriteVReg(machine_ir, hard_reg_, spill_slot_);
  }
}

void VRegLifetime::AddLifetimeFromAccessList(ArenaList<VRegLifetime>* lifetimes,
                                             VRegAccessList::iterator start_it,
                                             VRegAccessList::iterator end_it,
                                             int spill_slot,
                                             Arena* arena) {
  if (start_it == end_it) {
    return;
  }
  lifetimes->emplace_back(arena);
  lifetimes->back().SetSpill(spill_slot);
  for (auto it = start_it; it != end_it; ++it) {
    lifetimes->back().AppendAccess(*it);
  }
}

}  // namespace berberis
