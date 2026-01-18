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

// Machine IR public interface.

#ifndef BERBERIS_BACKEND_COMMON_MACHINE_IR_H_
#define BERBERIS_BACKEND_COMMON_MACHINE_IR_H_

#include <climits>  // CHAR_BIT
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "berberis/backend/code_emitter.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/arena_list.h"
#include "berberis/base/arena_vector.h"
#include "berberis/base/checks.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

// MachineReg is a machine instruction argument meaningful for optimizations and
// register allocation. It can be:
// - virtual register:  [1024, +inf)
// - hard register:     [1, 1024)
// - invalid/undefined: 0
// - (reserved):        (-1024, -1]
// - spilled register:  (-inf, -1024]
class MachineReg {
 public:
  // Creates an invalid machine register.
  constexpr MachineReg() : reg_{kInvalidMachineVRegNumber} {}
  constexpr explicit MachineReg(int reg) : reg_{reg} {}
  constexpr MachineReg(const MachineReg&) = default;
  constexpr MachineReg& operator=(const MachineReg&) = default;

  constexpr MachineReg(MachineReg&&) = default;
  constexpr MachineReg& operator=(MachineReg&&) = default;

  [[nodiscard]] constexpr int reg() const { return reg_; }

  [[nodiscard]] constexpr bool IsSpilledReg() const { return reg_ <= kLastSpilledRegNumber; }

  [[nodiscard]] constexpr bool IsHardReg() const {
    return reg_ > kInvalidMachineVRegNumber && reg_ < kFirstVRegNumber;
  }

  [[nodiscard]] constexpr bool IsInvalidReg() const { return reg_ == kInvalidMachineVRegNumber; }

  [[nodiscard]] constexpr bool IsVReg() const { return reg_ >= kFirstVRegNumber; }

  [[nodiscard]] constexpr uint32_t GetVRegIndex() const {
    CHECK_GE(reg_, kFirstVRegNumber);
    return reg_ - kFirstVRegNumber;
  }

  [[nodiscard]] constexpr uint32_t GetSpilledRegIndex() const {
    CHECK_LE(reg_, kLastSpilledRegNumber);
    return kLastSpilledRegNumber - reg_;
  }

  constexpr friend bool operator==(MachineReg left, MachineReg right) {
    return left.reg_ == right.reg_;
  }

  constexpr friend bool operator!=(MachineReg left, MachineReg right) { return !(left == right); }

  constexpr friend bool operator<(MachineReg left, MachineReg right) {
    return left.reg_ < right.reg_;
  }

  [[nodiscard]] static constexpr MachineReg CreateVRegFromIndex(uint32_t index) {
    CHECK_LE(index, std::numeric_limits<int>::max() - kFirstVRegNumber);
    return MachineReg{kFirstVRegNumber + static_cast<int>(index)};
  }

  // Normally, hard registers are predefined by the hardware setup and come from
  // MachineRegClass. This function allows creating them programmatically for
  // testing purposes.
  [[nodiscard]] static constexpr MachineReg CreateHardRegFromIndexForTesting(uint32_t index) {
    // Hard registers are in the range [1, kFirstVRegNumber - 1].
    // Index 0 maps to reg 1, index 1 maps to reg 2, etc.
    CHECK_LT(index, static_cast<uint32_t>(kFirstVRegNumber - 1));
    return MachineReg{1 + static_cast<int>(index)};
  }

  [[nodiscard]] static constexpr MachineReg CreateSpilledRegFromIndex(uint32_t index) {
    CHECK_LE(index, -(std::numeric_limits<int>::min() - kLastSpilledRegNumber));
    return MachineReg{kLastSpilledRegNumber - static_cast<int>(index)};
  }

  [[nodiscard]] static constexpr int GetFirstVRegNumberForTesting() { return kFirstVRegNumber; }

  [[nodiscard]] static constexpr int GetLastSpilledRegNumberForTesting() {
    return kLastSpilledRegNumber;
  }

 private:
  static constexpr int kFirstVRegNumber = 1024;
  static constexpr int kInvalidMachineVRegNumber = 0;
  static constexpr int kLastSpilledRegNumber = -1024;

  int reg_;
};

constexpr MachineReg kInvalidMachineReg{0};

[[nodiscard]] const char* GetMachineHardRegDebugName(MachineReg r);
[[nodiscard]] std::string GetMachineRegDebugString(MachineReg r);

using MachineRegVector = ArenaVector<MachineReg>;

// Set of registers, ordered by allocation preference.
// This is a struct to avoid static initializers.
// TODO(b/232598137) See if there's a way to use a class here. const array init
// (regs member) in constexpr context is the main challenge.
struct MachineRegClass {
  const char* debug_name;
  int reg_size;
  uint64_t reg_mask;
  int num_regs;
  const MachineReg regs[sizeof(reg_mask) * CHAR_BIT];

  [[nodiscard]] constexpr int RegSize() const { return reg_size; }

  [[nodiscard]] bool HasReg(MachineReg r) const { return reg_mask & (uint64_t{1} << r.reg()); }

  [[nodiscard]] bool IsSubsetOf(const MachineRegClass* other) const {
    return (reg_mask & other->reg_mask) == reg_mask;
  }

  [[nodiscard]] const MachineRegClass* GetIntersection(const MachineRegClass* other) const {
    // At the moment, only handle the case when one class is a subset of other.
    // In most real-life cases reg classes form a tree, so this is good enough.
    auto mask = reg_mask & other->reg_mask;
    if (mask == reg_mask) {
      return this;
    }
    if (mask == other->reg_mask) {
      return other;
    }
    return nullptr;
  }

  [[nodiscard]] constexpr int NumRegs() const { return num_regs; }

  [[nodiscard]] MachineReg RegAt(int i) const { return regs[i]; }

  [[nodiscard]] const MachineReg* begin() const { return &regs[0]; }
  [[nodiscard]] const MachineReg* end() const { return &regs[num_regs]; }

  [[nodiscard]] const char* GetDebugName() const { return debug_name; }
};

class MachineRegKind {
 public:
  enum StandardAccess {
    kNone = 0,
    kUse,
    kDef,
    kUseDef,
    kDefEarlyClobber,
  };

  // We need default constructor to initialize arrays
  constexpr MachineRegKind() : reg_class_(nullptr), access_(StandardAccess(kNone)) {}
  constexpr MachineRegKind(const MachineRegClass* reg_class, StandardAccess access)
      : reg_class_(reg_class), access_(access) {}

  [[nodiscard]] constexpr const MachineRegClass* RegClass() const { return reg_class_; }

  [[nodiscard]] constexpr bool IsDefEarlyClobber() const {
    return access_ == kDefEarlyClobber;
  }

  [[nodiscard]] constexpr bool IsDef() const {
    return access_ == kDef || access_ == kUseDef || access_ == kDefEarlyClobber;
  }

  // TODO(b/232598137): Rename to IsUse.
  [[nodiscard]] constexpr bool IsInput() const { return access_ == kUse || access_ == kUseDef; }

 private:
  const MachineRegClass* reg_class_;
  enum StandardAccess access_;
};

class MachineBasicBlock;

// Machine insn kind meaningful for optimizations and register allocation.
enum MachineInsnKind {
  kMachineInsnDefault = 0,
  kMachineInsnSideEffects,  // never dead
  kMachineInsnCopy,         // can be deleted if dst == src
};

enum MachineOpcode : int;

class MachineIR;

class MachineInsn {
 public:
  virtual ~MachineInsn() {
    // No code here - will never be called!
  }

  [[nodiscard]] virtual std::string GetDebugString() const = 0;
  virtual void Emit(CodeEmitter* as) const = 0;

  [[nodiscard]] MachineOpcode opcode() const { return opcode_; };

  [[nodiscard]] int NumRegOperands() const { return num_reg_operands_; }

  [[nodiscard]] const MachineRegKind& RegKindAt(int i) const { return reg_kinds_[i]; }

  [[nodiscard]] MachineReg RegAt(int i) const {
    CHECK_LT(i, num_reg_operands_);
    return regs_[i];
  }

  void SetRegAt(int i, MachineReg reg) {
    CHECK_LT(i, num_reg_operands_);
    regs_[i] = reg;
  }

  [[nodiscard]] bool has_side_effects() const {
    return (kind_ == kMachineInsnSideEffects) || recovery_info_.bb ||
           (recovery_info_.pc != kNullGuestAddr) ||
           // Instructions not touching registers are always only used for their other side effects.
           NumRegOperands() == 0;
  }

  [[nodiscard]] bool is_copy() const { return kind_ == kMachineInsnCopy; }

  [[nodiscard]] const MachineBasicBlock* recovery_bb() const { return recovery_info_.bb; }

  void set_recovery_bb(const MachineBasicBlock* bb) { recovery_info_.bb = bb; }

  [[nodiscard]] GuestAddr recovery_pc() const { return recovery_info_.pc; }

  void set_recovery_pc(GuestAddr pc) { recovery_info_.pc = pc; }

 protected:
  MachineInsn(const MachineInsn&) = default;
  MachineInsn(const MachineInsn& insn, MachineReg* regs) : MachineInsn(insn) { regs_ = regs; }
  MachineInsn* operator=(const MachineInsn&) = delete;
  MachineInsn(MachineOpcode opcode,
              int num_reg_operands,
              const MachineRegKind* reg_kinds,
              MachineReg* regs,
              MachineInsnKind kind)
      : opcode_(opcode),
        num_reg_operands_(num_reg_operands),
        reg_kinds_(reg_kinds),
        regs_(regs),
        kind_(kind),
        recovery_info_{nullptr, kNullGuestAddr} {}

 private:
  friend class MachineIR;
  virtual MachineInsn* Clone(Arena* arena) const = 0;
  virtual ArenaList<MachineInsn*> Lower(Arena* arena) const = 0;
  // We either recover by building explicit recovery blocks or by storing recovery pc.
  // TODO(b/200327919): Convert this to union? We'll need to know which one is used during
  // initialization and in has_side_effects.
  struct RecoveryInfo {
    const MachineBasicBlock* bb;
    GuestAddr pc;
  };
  const MachineOpcode opcode_;
  const int num_reg_operands_;
  const MachineRegKind* reg_kinds_;
  MachineReg* regs_;
  MachineInsnKind kind_;
  RecoveryInfo recovery_info_;
};

std::string GetRegOperandDebugString(const MachineInsn* insn, int i);

using MachineInsnList = ArenaList<MachineInsn*>;

class MachineInsnListPosition {
 public:
  MachineInsnListPosition(MachineInsnList* list, MachineInsnList::iterator iterator)
      : list_(list), iterator_(iterator) {}

  [[nodiscard]] MachineInsn* insn() const { return *iterator_; }

  void InsertBefore(MachineInsn* insn) const { list_->insert(iterator_, insn); }

  void InsertAfter(MachineInsn* insn) const {
    MachineInsnList::iterator next_iterator = iterator_;
    list_->insert(++next_iterator, insn);
  }

 private:
  MachineInsnList* list_;
  const MachineInsnList::iterator iterator_;
};

class MachineEdge {
 public:
  MachineEdge(Arena* arena, MachineBasicBlock* src, MachineBasicBlock* dst)
      : src_(src), dst_(dst), insn_list_(arena) {}

  void set_src(MachineBasicBlock* bb) { src_ = bb; }
  void set_dst(MachineBasicBlock* bb) { dst_ = bb; }

  [[nodiscard]] MachineBasicBlock* src() const { return src_; }
  [[nodiscard]] MachineBasicBlock* dst() const { return dst_; }

  [[nodiscard]] const MachineInsnList& insn_list() const { return insn_list_; }
  [[nodiscard]] MachineInsnList& insn_list() { return insn_list_; }

 private:
  MachineBasicBlock* src_;
  MachineBasicBlock* dst_;
  MachineInsnList insn_list_;
};

using MachineEdgeVector = ArenaVector<MachineEdge*>;

class MachineBasicBlock {
 public:
  MachineBasicBlock(Arena* arena, uint32_t id)
      : id_(id),
        guest_addr_(kNullGuestAddr),
        profile_counter_(0),
        insn_list_(arena),
        in_edges_(arena),
        out_edges_(arena),
        live_in_(arena),
        live_out_(arena),
        is_recovery_(false),
        is_cold_{false} {}

  [[nodiscard]] uint32_t id() const { return id_; }

  [[nodiscard]] GuestAddr guest_addr() const { return guest_addr_; }
  void set_guest_addr(GuestAddr addr) { guest_addr_ = addr; }

  [[nodiscard]] std::optional<uint32_t> profile_counter() const { return profile_counter_; }
  void set_profile_counter(uint32_t counter) { profile_counter_ = counter; }

  [[nodiscard]] const MachineInsnList& insn_list() const { return insn_list_; }
  [[nodiscard]] MachineInsnList& insn_list() { return insn_list_; }

  [[nodiscard]] const MachineEdgeVector& in_edges() const { return in_edges_; }
  [[nodiscard]] MachineEdgeVector& in_edges() { return in_edges_; }

  [[nodiscard]] const MachineEdgeVector& out_edges() const { return out_edges_; }
  [[nodiscard]] MachineEdgeVector& out_edges() { return out_edges_; }

  [[nodiscard]] const MachineRegVector& live_in() const { return live_in_; }
  [[nodiscard]] MachineRegVector& live_in() { return live_in_; }

  [[nodiscard]] const MachineRegVector& live_out() const { return live_out_; }
  [[nodiscard]] MachineRegVector& live_out() { return live_out_; }

  void MarkAsRecovery() { is_recovery_ = true; }
  void MarkAsCold() { is_cold_ = true; }

  [[nodiscard]] bool is_recovery() const { return is_recovery_; }
  [[nodiscard]] bool IsCold() const { return is_recovery_ || is_cold_; }

  [[nodiscard]] std::string GetDebugString() const;

 private:
  const uint32_t id_;
  GuestAddr guest_addr_;
  std::optional<uint32_t> profile_counter_;
  MachineInsnList insn_list_;
  MachineEdgeVector in_edges_;
  MachineEdgeVector out_edges_;
  MachineRegVector live_in_;
  MachineRegVector live_out_;
  bool is_recovery_;
  bool is_cold_;
};

using MachineBasicBlockList = ArenaList<MachineBasicBlock*>;

class MachineIR {
 public:
  // First num_vreg virtual register numbers are reserved for custom use
  // in the derived class, numbers above that can be used for scratches.
  MachineIR(Arena* arena, int num_vreg, uint32_t num_bb)
      : num_bb_(num_bb),
        arena_(arena),
        num_vreg_(num_vreg),
        num_arg_slots_(0),
        num_spill_slots_(0),
        bb_list_(arena),
        contains_calls_(false) {}

  [[nodiscard]] int NumVReg() const { return num_vreg_; }

  [[nodiscard]] MachineReg AllocVReg() { return MachineReg::CreateVRegFromIndex(num_vreg_++); }

  [[nodiscard]] uint32_t ReserveBasicBlockId() { return num_bb_++; }

  [[nodiscard]] MachineBasicBlock* NewBasicBlock() {
    return NewInArena<MachineBasicBlock>(arena(), arena(), ReserveBasicBlockId());
  }

  // Stack frame layout is:
  //     [arg slots][spill slots]
  //     ^--- stack pointer
  //
  // Arg slots are for stack frame part that require a fixed offset from the
  // stack pointer, in particular for call arguments passed on the stack.
  // Spill slots are for spilled registers.
  // Each slot is 16-bytes, and the stack pointer is always 16-bytes aligned.
  //
  // TODO(b/232598137): If we need a custom stack layout for an architecture,
  // implement the following functions specifically for each architecture.

  void ReserveArgs(uint32_t size) {
    uint32_t slots = (size + 15) / 16;
    if (num_arg_slots_ < slots) {
      num_arg_slots_ = slots;
    }
  }

  void set_contains_calls() { contains_calls_ = true; }
  [[nodiscard]] bool contains_calls() const { return contains_calls_; }

  [[nodiscard]] uint32_t AllocSpill() { return num_spill_slots_++; }

  [[nodiscard]] uint32_t SpillSlotOffset(uint32_t slot) const {
    return 16 * (num_arg_slots_ + slot);
  }

  [[nodiscard]] uint32_t FrameSize() const { return 16 * (num_arg_slots_ + num_spill_slots_); }

  [[nodiscard]] size_t NumBasicBlocks() const { return num_bb_; }

  [[nodiscard]] const MachineBasicBlockList& bb_list() const { return bb_list_; }

  [[nodiscard]] MachineBasicBlockList& bb_list() { return bb_list_; }

  [[nodiscard]] std::string GetDebugString() const;

  // DOT is a graph description language, which tools like Graphviz can visualize for you.
  [[nodiscard]] std::string GetDebugStringAsDot() const;

  bool Emit(CodeEmitter* as) const;

  [[nodiscard]] Arena* arena() const { return arena_; }

  template <typename T, typename... Args>
  [[nodiscard]] T* NewInsn(Args&&... args) {
    return NewInArena<T>(arena(), std::forward<Args>(args)...);
  }

  MachineInsn* CloneInsn(const MachineInsn* insn) { return insn->Clone(arena()); }
  MachineInsnList LowerInsn(const MachineInsn* insn) { return insn->Lower(arena()); }

 private:
  // Basic block number is useful when allocating analytical data
  // structures indexed by IDs. Note that the return value of this function is
  // not necessarily equal to bb_list().size() since some basic blocks may not
  // be enrolled in this list.
  // This can be set in ctor or managed in the derived class. It's the derived
  // class's responsibility to guarantee that max basic block ID is less than
  // this number.
  uint32_t num_bb_;

  Arena* const arena_;
  int num_vreg_;
  uint32_t num_arg_slots_;    // 16-byte slots for call args/results
  uint32_t num_spill_slots_;  // 16-byte slots for spilled registers
  MachineBasicBlockList bb_list_;
  bool contains_calls_;
};

class Branch final : public MachineInsn {
 public:
  static const MachineOpcode kOpcode;

  explicit Branch(const MachineBasicBlock* then_bb);

  std::string GetDebugString() const override;
  void Emit(CodeEmitter* as) const override;

  const MachineBasicBlock* then_bb() const { return then_bb_; }
  void set_then_bb(const MachineBasicBlock* then_bb) { then_bb_ = then_bb; }

 private:
  friend Branch* NewInArena<Branch, const Branch&>(Arena*, const Branch&);
  Branch(const Branch&) = default;
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
  const MachineBasicBlock* then_bb_;
};

class CondBranch final : public MachineInsn {
 public:
  static const MachineOpcode kOpcode;

  CondBranch(CodeEmitter::Condition cond,
                   const MachineBasicBlock* then_bb,
                   const MachineBasicBlock* else_bb,
                   MachineReg eflags);

  std::string GetDebugString() const override;
  void Emit(CodeEmitter* as) const override;

  CodeEmitter::Condition cond() const { return cond_; }
  void set_cond(CodeEmitter::Condition cond) { cond_ = cond; }
  const MachineBasicBlock* then_bb() const { return then_bb_; }
  const MachineBasicBlock* else_bb() const { return else_bb_; }
  void set_then_bb(const MachineBasicBlock* then_bb) { then_bb_ = then_bb; }
  void set_else_bb(const MachineBasicBlock* else_bb) { else_bb_ = else_bb; }
  MachineReg eflags() const { return eflags_; }

 private:
  friend CondBranch* NewInArena<CondBranch, const CondBranch&>(
      Arena*,
      const CondBranch&);
  CondBranch(const CondBranch&) = default;
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
  CodeEmitter::Condition cond_;
  const MachineBasicBlock* then_bb_;
  const MachineBasicBlock* else_bb_;
  MachineReg eflags_;
};

class Jump final : public MachineInsn {
 public:
  enum class Kind {
    kJumpWithPendingSignalsCheck,
    kJumpWithoutPendingSignalsCheck,
    kExitGeneratedCode,
    kSyscall,
  };

  struct WithOptimizedABI {};

  Jump(GuestAddr target, Kind kind = Kind::kJumpWithPendingSignalsCheck);
  Jump(GuestAddr target,
             WithOptimizedABI tag,
             Kind kind = Kind::kJumpWithPendingSignalsCheck);

  std::string GetDebugString() const override;
  void Emit(CodeEmitter* as) const override;

  GuestAddr target() const { return target_; }
  Kind kind() const { return kind_; }

 private:
  friend Jump* NewInArena<Jump, const Jump&>(Arena*, const Jump&);
  Jump(const Jump&) = default;
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
  GuestAddr target_;
  Kind kind_;
  // ABI outputs.
  MachineReg args_[6];
};

class IndirectJump final : public MachineInsn {
 public:
  struct WithOptimizedABI {};

  explicit IndirectJump(MachineReg src);
  IndirectJump(MachineReg src, WithOptimizedABI tag);

  [[nodiscard]] std::string GetDebugString() const override;
  void Emit(CodeEmitter* as) const override;

 private:
  friend IndirectJump* NewInArena<IndirectJump, const IndirectJump&>(
      Arena*,
      const IndirectJump&);
  IndirectJump(const IndirectJump&);
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
  // Target and ABI outputs.
  MachineReg regs_[1 + 6];
};

// Copy the value of given size between registers/memory.
// Register class of operands is anything capable of keeping values of this
// size.
// ATTENTION: this insn has operands with variable register class!
class Copy final : public MachineInsn {
 public:
  static const MachineOpcode kOpcode;

  template <const MachineRegClass* kRegClass>
  Copy(MachineReg dst, MachineReg src, MetaValue<kRegClass>)
      : Copy(dst, src, kCopyRegInfo<kRegClass>) {}
  // For Spill/Reload. Either dst or src have to hard reg, the other one have to be a spill slot.
  Copy(MachineReg dst, MachineReg src, const MachineRegClass* reg_class);
  Copy(MachineReg dst, MachineReg src, int size);

  std::string GetDebugString() const override;
  void Emit(CodeEmitter* as) const override;

 private:
  template <const MachineRegClass* kRegClass>
  static constexpr MachineRegKind kCopyRegInfo[2] = {{kRegClass, MachineRegKind::kDef},
                                                     {kRegClass, MachineRegKind::kUse}};
  Copy(MachineReg dst, MachineReg src, const MachineRegKind reg_info[2]);
  friend Copy* NewInArena<Copy, const Copy&>(Arena*, const Copy&);
  Copy(const Copy&);
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
  MachineReg regs_[2];
};

// Some instructions have use-def operands, but for the semantics of our IR are really def-only,
// so we use this auxiliary instruction to ensure data-flow is integral (required by some phases
// including register allocation), but we do not emit it.
//
// Example: PmovsxwdXRegXReg followed by MovlhpsXRegXReg
// Example: xor rax, rax
class PseudoDefReg final : public MachineInsn {
 public:
  static const MachineOpcode kOpcode;

  explicit PseudoDefReg(MachineReg reg);

  [[nodiscard]] std::string GetDebugString() const override;
  void Emit(CodeEmitter* /*as*/) const override {
    // It's an auxiliary instruction. Does not emit.
  }

 private:
  friend PseudoDefReg* NewInArena<PseudoDefReg, const PseudoDefReg&>(Arena*, const PseudoDefReg&);
  PseudoDefReg(const PseudoDefReg&);
  MachineInsn* Clone(Arena* arena) const override;
  MachineInsnList Lower(Arena* arena) const override;
  MachineReg reg_;
};

}  // namespace berberis

#endif  // BERBERIS_BACKEND_COMMON_MACHINE_IR_H_
