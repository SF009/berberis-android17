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

#ifndef BERBERIS_BACKEND_COMMON_REG_ALLOC_INTERNAL_H_
#define BERBERIS_BACKEND_COMMON_REG_ALLOC_INTERNAL_H_

#include <limits>

#include "berberis/backend/common/lifetime.h"
#include "berberis/backend/common/machine_ir.h"
#include "berberis/base/arena_list.h"
#include "berberis/base/arena_vector.h"

namespace berberis {

// Lifetimes themselves are owned by lifetime list populated by lifetime
// analysis. Use list of pointers to track lifetimes currently allocated
// to particular hard register.
using VRegLifetimePtrList = ArenaList<VRegLifetime*>;

// How to spill one virtual register.
struct VRegLifetimeSpill {
  VRegLifetimePtrList::iterator lifetime;
  SplitPos realloc_pos;

  VRegLifetimeSpill(VRegLifetimePtrList::iterator i, const SplitPos& p);
};

// Every possible spill should have some smaller weight (CHECKed).
// Use half of the maximum int value to avoid overflow if accidentally used
// in arithmetic.
inline const int kInfiniteSpillWeight = std::numeric_limits<int>::max() / 2;

// Track what virtual registers are currently allocated to this particular
// hard register, and how to spill them.
class HardRegAllocation {
 public:
  explicit HardRegAllocation(Arena* arena);

  HardRegAllocation(const HardRegAllocation& other) = default;

  // If new_lifetime doesn't interfere with lifetimes currently allocated
  // to this hard register, allocate new_lifetime to this register as well.
  bool TryAssign(VRegLifetime* new_lifetime);

  // If TryAssign returned false:
  // Check if it is possible to spill all lifetimes allocated to this hard
  // register that interfere with new_lifetime, and return spill weight.
  int ConsiderSpill(VRegLifetime* new_lifetime);

  // If ConsiderSpill returned non-infinite weight:
  // Given spill is possible, actually spill lifetimes that interfere with
  // new_lifetime to spill_slot. Insert newly created tiny lifetimes into
  // 'lifetimes' list, starting at position 'pos'.
  void SpillAndAssign(VRegLifetime* new_lifetime,
                      int spill_slot,
                      VRegLifetimeList* lifetimes,
                      VRegLifetimeList::iterator pos);

 private:
  // Arena for allocations.
  Arena* arena_;
  // Lifetimes currently allocated to this hard register.
  VRegLifetimePtrList lifetimes_;

  // Last lifetime being allocated, for CHECKing.
  // TODO(b/232598137): probably use this for ConsiderSpill and SpillAndAssign?
  // This looks more natural...
  VRegLifetime* new_lifetime_;

  // How to free this register for last considered new lifetime.
  // This is here for the following reasons:
  // - it is highly coupled with lifetimes_
  // - to avoid reallocating this for every spill consideration
  ArenaVector<VRegLifetimeSpill> spills_;
};

// Simple register allocator.
// Walk list of lifetimes sorted by begin and allocates in order.
// Modifies lifetimes that have been spilled and adds tiny lifetimes split
// from spilled lifetimes to the same list.
class VRegLifetimeAllocator {
 public:
  VRegLifetimeAllocator(MachineIR* machine_ir, VRegLifetimeList* lifetimes);

  void Allocate();

 private:
  void AllocateLifetime(VRegLifetimeList::iterator lifetime_it);

  bool TryAssignHardReg(VRegLifetime* lifetime, MachineReg hard_reg);

  int ConsiderSpillHardReg(MachineReg hard_reg, VRegLifetime* lifetime);

  void SpillAndAssignHardReg(MachineReg hard_reg, VRegLifetimeList::iterator curr);

  void RewriteAllocatedLifetimes();

  MachineIR* machine_ir_;

  VRegLifetimeList* lifetimes_;

  ArenaVector<HardRegAllocation> allocations_;
};

}  // namespace berberis

#endif  // BERBERIS_BACKEND_COMMON_REG_ALLOC_INTERNAL_H_
