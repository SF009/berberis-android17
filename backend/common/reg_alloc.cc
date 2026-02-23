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

// At the moment, this implements more-or-less traditional linear scan register
// allocation.
//
// Input is virtual register lifetimes list, sorted by begin. Each lifetime is
// a list of continuous live ranges with lifetime holes in between. Each live
// range tracks insns that actually use the virtual register.
//
// Allocator walks sorted lifetime list and allocates lifetimes to hard
// registers. When lifetimes do not interfere, so live ranges of one lifetime
// fit into holes of another lifetime, both lifetimes can be allocated to the
// same hard register.
//
// If there is no available hard register, allocator selects hard register to
// free. All lifetimes allocated to that hard register that interfere with
// lifetime being allocated are spilled.
//
// Lifetime being spilled is split into tiny lifetimes, each for one insn
// where that virtual register is used. If register is read by insn, reload
// insn is added before, and if register is written by insn, spill insn is
// added after.
//
// Tiny lifetimes originated from spilling still need to be allocated. For tiny
// lifetimes that end before lifetime in favor of which they were spilled,
// previously allocated hard register is used. Tiny lifetimes that begin after
// are merged into a list of not yet allocated lifetimes.
//
// The problematic case is when tiny lifetime overlaps with begin of lifetime
// in favor of which it was spilled. In this case previously allocated hard
// register can't be used (otherwise it doesn't become free) and tiny lifetime
// can't be allocated later according to order by begin. In this case spill is
// considered impossible.
//
// The above is the most significant difference from classic linear scan
// register allocation algorithms, which usually solve tiny lifetimes
// allocation either by backtracking or using reserved registers. Hopefully
// this new approach works if there are more suitable hard registers than can
// be used in one insn, so there is always a suitable register that is not used
// at the point of spill.
//
// TODO(b/232598137): this might blow up when lifetimes compete for some single
// specific register (ecx for x86 shift insn)! Can be solved by generating code
// that minimizes lifetimes of such registers - moves in right before and moves
// out right after the insn using such register.
//
// TODO(b/232598137): investigate how code quality is affected when there are few
// available hard registers (x86_32)!

#include "berberis/backend/common/reg_alloc.h"

#include <iterator>  // std::next()

#include "berberis/backend/common/lifetime.h"
#include "berberis/backend/common/lifetime_analysis.h"
#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/config.h"
#include "berberis/base/tracing.h"

#include "reg_alloc_internal.h"

// #define LOG_REG_ALLOC(...) TRACE(__VA_ARGS__)
#define LOG_REG_ALLOC(...) ((void)0)

namespace berberis {

namespace {

// Same as std::list::merge, but starting from a 'pos' position.
void MergeVRegLifetimeList(VRegLifetimeList* dst,
                           VRegLifetimeList::iterator dst_pos,
                           VRegLifetimeList* src) {
  while (!src->empty()) {
    auto curr = src->begin();
    for (; dst_pos != dst->end(); ++dst_pos) {
      if (curr->begin() < dst_pos->begin()) {
        break;
      }
    }
    dst->splice(dst_pos, *src, curr);
  }
}

}  // namespace

HardRegAllocation::HardRegAllocation(Arena* arena)
    : arena_(arena),
      lifetimes_(arena),
      active_lifetimes_begin_(0),
      new_lifetime_(nullptr),
      spills_(arena) {}

bool HardRegAllocation::TryAssign(VRegLifetime* new_lifetime) {
  new_lifetime_ = new_lifetime;

  // Make sure we still have all the active lifetimes needed to correctly check the interference
  // with new_lifetime.
  CHECK_LE(active_lifetimes_begin_, new_lifetime->begin());
  // For interference test efficiency we release lifetimes ending before new_lifetime, assuming the
  // assignments are happening in sorted order for linear scan. There is one corner case though when
  // to allocate new_lifetime we need to spill an access within the instruction where
  // new_lifetime begins. In this case we would need to reallocate the spilled access
  // that starts before new_lifetime. Since it's within the same instruction it can only be 1 tick
  // behind.
  // Technically active_lifetimes_begin_ may become -1, but it's fine, since it's signed int and
  // lifetime begin positions are at least 0.
  static_assert(std::is_signed_v<decltype(active_lifetimes_begin_)>);
  active_lifetimes_begin_ = new_lifetime->begin() - 1;

  for (auto curr = lifetimes_.begin(); curr != lifetimes_.end();) {
    VRegLifetime* curr_lifetime = *curr;

    if (curr_lifetime->end() <= active_lifetimes_begin_) {
      curr = lifetimes_.erase(curr);
    } else if (curr_lifetime->TestInterference(*new_lifetime)) {
      // Lifetimes interfere, can't assign.
      return false;
    } else {
      ++curr;
    }
  }

  // No lifetimes interfere with new, can assign.
  lifetimes_.push_back(new_lifetime);
  return true;
}

std::tuple<int, SplitKind> HardRegAllocation::ConsiderSpill(VRegLifetime* new_lifetime) {
  CHECK_EQ(new_lifetime_, new_lifetime);

  spills_.clear();
  int weight = 0;
  SplitKind combined_split_kind = SPLIT_OK;
  const int kNewRegClassNumRegs = new_lifetime->GetRegClass()->NumRegs();

  for (auto curr = lifetimes_.begin(); curr != lifetimes_.end(); ++curr) {
    VRegLifetime* curr_lifetime = *curr;

    if (!curr_lifetime->TestInterference(*new_lifetime)) {
      // No interference, no need to spill.
      continue;
    }

    SplitPos split_pos;
    SplitKind split_kind = curr_lifetime->FindSplitPos(new_lifetime->begin(), &split_pos);
    if (split_kind == SPLIT_CONFLICT) {
      // Only one lifetime assigned to this hard register can conflict with the beginning of
      // new_lifetime.
      CHECK_EQ(combined_split_kind, SPLIT_OK);
      // If we split curr_lifetime we would still have to reallocate its access that
      // conflicts with the beginning of new_lifetime. This access may evict new_lifetime back.
      // To avoid such double spill and potentially infinite loop of them evicting each other, we
      // only allow this split if it'll create a lifetime that would have a wider register class
      // than new_lifetime.
      if (kNewRegClassNumRegs < curr_lifetime->GetRegClass()->NumRegs()) {
        // If the whole curr_lifetime is wider, no need to separate the conflicting access.
      } else if (kNewRegClassNumRegs < split_pos.access_it->GetRegClass()->NumRegs()) {
        // If only the conflicting access from curr_lifetime is wider than new_lifetime, we can
        // split it out to a separate lifetime and be sure it won't spill new_lifetime back.
        split_kind = SPLIT_CONFLICT_WITH_ACCESS_SEPARATION;
      } else {
        // Otherwise spill something else.
        return {kInfiniteSpillWeight, SPLIT_IMPOSSIBLE};
      }
      combined_split_kind = split_kind;
    }

    // Record spill.
    spills_.push_back(VRegLifetimeSpill(curr, split_pos, split_kind));
    // Evicting spilled lifetime is free.
    if (curr_lifetime->GetSpill() == -1) {
      weight += curr_lifetime->spill_weight();
    }
  }

  CHECK_LT(weight, kInfiniteSpillWeight);
  return {weight, combined_split_kind};
}


void HardRegAllocation::SpillAndAssign(VRegLifetime* new_lifetime,
                                       int spill_slot,
                                       VRegLifetimeList* lifetimes,
                                       VRegLifetimeList::iterator pos) {
  CHECK_EQ(new_lifetime_, new_lifetime);
  CHECK(!spills_.empty());

  for (const auto& spill : spills_) {
    VRegLifetime* spill_lifetime = *spill.lifetime;

    // Assign spill slot.
    // Lifetimes being spilled do not interfere, can share spill slot!
    // TODO(b/232598137): evicted tiny lifetime have spill slot already.
    // If we only evict tiny lifetimes, we might not need new spill_slot!
    // Allocate spill slot here when needed.
    if (spill_lifetime->GetSpill() == -1) {
      spill_lifetime->SetSpill(spill_slot);
    }

    // Split spilled lifetime into tiny lifetimes, enqueue them for allocation.
    VRegLifetimeList split(arena_);
    spill_lifetime->Split(spill.realloc_pos, spill.split_kind, &split);
    MergeVRegLifetimeList(lifetimes, pos, &split);

    // Expire spilled lifetime.
    lifetimes_.erase(spill.lifetime);
  }

  // Spilled all interfering lifetimes, can assign.
  lifetimes_.push_back(new_lifetime);
}

VRegLifetimeAllocator::VRegLifetimeAllocator(MachineIR* machine_ir, VRegLifetimeList* lifetimes)
    : machine_ir_(machine_ir),
      lifetimes_(lifetimes),
      allocations_(
          config::kMaxHardRegs, HardRegAllocation(machine_ir->arena()), machine_ir->arena()) {}

int VRegLifetimeAllocator::ConsiderSpillHardReg(MachineReg hard_reg, VRegLifetime* lifetime) {
  return std::get<0>(allocations_[hard_reg.reg()].ConsiderSpill(lifetime));
}

bool VRegLifetimeAllocator::TryAssignHardReg(VRegLifetime* curr_lifetime, MachineReg hard_reg) {
  if (allocations_[hard_reg.reg()].TryAssign(curr_lifetime)) {
    curr_lifetime->set_hard_reg(hard_reg);
    LOG_REG_ALLOC(".. to %s\n", GetMachineHardRegDebugName(hard_reg));
    return true;
  }
  return false;
}

void VRegLifetimeAllocator::SpillAndAssignHardReg(MachineReg hard_reg,
                                                  VRegLifetimeList::iterator curr) {
  auto next = std::next(curr);
  allocations_[hard_reg.reg()].SpillAndAssign(&*curr, machine_ir_->AllocSpill(), lifetimes_, next);
  curr->set_hard_reg(hard_reg);
  LOG_REG_ALLOC(".. to %s (after spill)\n", GetMachineHardRegDebugName(hard_reg));
}

void VRegLifetimeAllocator::AllocateLifetime(VRegLifetimeList::iterator lifetime_it) {
  VRegLifetime* lifetime = &*lifetime_it;
  const MachineRegClass* reg_class = lifetime->GetRegClass();

  LOG_REG_ALLOC(
      "allocating lifetime %s:\n%s", reg_class->GetDebugName(), lifetime->GetDebugString().c_str());

  // Walk registers from reg class.
  for (MachineReg hard_reg : *reg_class) {
    if (TryAssignHardReg(lifetime, hard_reg)) {
      return;
    }
  }

  LOG_REG_ALLOC("... failed to find free hard reg, will try spilling");

  // Walk registers again, consider each for spilling.
  int best_spill_weight = kInfiniteSpillWeight;
  MachineReg best_reg{0};
  for (MachineReg hard_reg : *reg_class) {
    int spill_weight = ConsiderSpillHardReg(hard_reg, lifetime);
    LOG_REG_ALLOC(
        "... consider spilling %s, weight %d", GetMachineHardRegDebugName(hard_reg), spill_weight);
    if (best_spill_weight > spill_weight) {
      best_spill_weight = spill_weight;
      best_reg = hard_reg;
    }
  }

  // Spill register with best spill weight.
  CHECK_LT(best_spill_weight, kInfiniteSpillWeight);
  SpillAndAssignHardReg(best_reg, lifetime_it);
}

void VRegLifetimeAllocator::RewriteAllocatedLifetimes() {
  for (auto& lifetime : *lifetimes_) {
    lifetime.Rewrite(machine_ir_);
  }
}

void VRegLifetimeAllocator::Allocate() {
  for (auto lifetime_it = lifetimes_->begin(); lifetime_it != lifetimes_->end(); ++lifetime_it) {
    CHECK(!lifetime_it->IsEmpty());
    AllocateLifetime(lifetime_it);
  }
  RewriteAllocatedLifetimes();
}

void CoalesceLifetimes(VRegLifetimeList* lifetimes) {
  for (auto& lifetime : *lifetimes) {
    if (lifetime.IsEmpty()) {
      // It's already merged and emptied.
      continue;
    }

    const auto& candidates = lifetime.coalescing_candidates();
    // Attention: candidates.size() may grow after Merge()! Iterating by index
    // to avoid iterator invalidation.
    for (size_t i = 0; i < candidates.size(); ++i) {
      VRegLifetime* other = candidates.at(i);
      CHECK_NE(&lifetime, other);
      if (!other->IsEmpty() &&
          // Narrowing reg class may lead to difficult to handle spills.
          // TODO(b/459067902): support such spill and allow coalescing.
          lifetime.GetRegClass() == other->GetRegClass() &&
          // We can only merge non-interfering lifetimes.
          !lifetime.TestInterference(*other)) {
        lifetime.Merge(other);
      }
    }
  }
  lifetimes->remove_if([](const VRegLifetime& lifetime) { return lifetime.IsEmpty(); });
}

void CollectLifetimes(const MachineIR* machine_ir, VRegLifetimeList* lifetimes) {
  VRegLifetimeAnalysis lifetime_analysis(machine_ir->arena(), machine_ir->NumVReg(), lifetimes);

  // Not 'const' because we need pointer to modifiable bb->insn_list().
  for (auto* bb : machine_ir->bb_list()) {
    for (const auto reg : bb->live_in()) {
      lifetime_analysis.SetLiveIn(reg);
    }

    for (auto insn_it = bb->insn_list().begin(); insn_it != bb->insn_list().end(); ++insn_it) {
      lifetime_analysis.AddInsn(MachineInsnListPosition(&(bb->insn_list()), insn_it));
    }

    for (const auto reg : bb->live_out()) {
      lifetime_analysis.SetLiveOut(reg);
    }
    lifetime_analysis.EndBasicBlock();
  }
}

void AllocRegs(MachineIR* machine_ir) {
  LOG_REG_ALLOC("Machine IR before reg alloc:\n%s\n", machine_ir->GetDebugString().c_str());
  VRegLifetimeList lifetimes(machine_ir->arena());
  CollectLifetimes(machine_ir, &lifetimes);
  CoalesceLifetimes(&lifetimes);

  VRegLifetimeAllocator allocator(machine_ir, &lifetimes);
  allocator.Allocate();
}

}  // namespace berberis
