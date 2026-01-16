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

#ifndef BERBERIS_BACKEND_X86_64_REG_LIFETIME_H_
#define BERBERIS_BACKEND_X86_64_REG_LIFETIME_H_

#include <optional>
#include <variant>

#include "berberis/backend/common/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/arena_map.h"
#include "berberis/base/arena_vector.h"

namespace berberis::x86_64 {

enum class RegType {
  kUnknown,
  kGeneral,
  kXmm,
};

struct LiveIn {};
struct LiveOut {};
// Note that we treat each instruction as one unit but if we wanted to more
// accurately mimic what the register allocator does, we should separate
// the instruction into two parts: read and write.
// If a register's lifetime ends on an instruction where it's only read, we can
// actually reuse that register for the write portion so we are overcounting.
struct RegLifetime {
  std::variant<LiveIn, MachineInsnList::iterator> start;
  std::variant<LiveOut, MachineInsnList::iterator> end;
  // start_pos is first instruction where lifetime begins.
  size_t start_pos;
  // end_pos is one past last instruction where lifetime ends.
  size_t end_pos;
  RegType reg_type;
};

struct RegLifetimeCount {
  size_t general;
  size_t xmm;
  size_t Increment(RegType reg_type);
  size_t Decrement(RegType reg_type);
  bool operator==(const RegLifetimeCount&) const = default;
};

using RegLifetimeMap = ArenaVector<std::optional<RegLifetime>>;
using RegLifetimeCounts = ArenaVector<RegLifetimeCount>;

class RegLifetimeCounter {
 public:
  RegLifetimeCounter(MachineIR* machine_ir)
      : machine_ir_(machine_ir),
        lifetime_map_(machine_ir->NumVReg(), machine_ir->arena()),
        lifetime_counts_(machine_ir->arena()) {}

  void Count(MachineBasicBlock* bb);
  const RegLifetime& GetLifetimeAt(const MachineReg& reg) const {
    CHECK(LifetimeAt(reg).has_value());
    return LifetimeAt(reg).value();
  }
  const std::optional<RegLifetime>& LifetimeAt(const MachineReg& reg) const {
    return GetMap().at(reg.GetVRegIndex());
  }
  size_t RegCountAt(size_t pos, RegType reg_type) const;
  // Sets last use of reg to end and end_pos, updating both the map and counts.
  std::optional<size_t> UpdateLastUse(MachineReg reg,
                                      MachineInsnList::iterator end,
                                      size_t end_pos,
                                      const size_t kLimit);

  const RegLifetimeMap& GetMap() const { return lifetime_map_; }
  const RegLifetimeCounts& GetCounts() const { return lifetime_counts_; }

 private:
  void CountRegLifetimeMap(MachineBasicBlock* bb);
  void CountRegLifetimes(MachineBasicBlock* bb);

  MachineIR* machine_ir_;
  RegLifetimeMap lifetime_map_;
  RegLifetimeCounts lifetime_counts_;
};

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_REG_LIFETIME_H_
