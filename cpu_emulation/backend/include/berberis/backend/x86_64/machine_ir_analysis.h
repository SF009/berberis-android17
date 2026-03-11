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

#ifndef BERBERIS_BACKEND_X86_64_MACHINE_IR_ANALYSIS_H_
#define BERBERIS_BACKEND_X86_64_MACHINE_IR_ANALYSIS_H_

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/arena_vector.h"
#include "berberis/base/checks.h"

namespace berberis::x86_64 {

class Loop {
 public:
  explicit Loop(Arena* arena) : body_(arena), entry_blocks_(arena) {}

  Loop(const Loop&) = delete;
  Loop& operator=(const Loop&) = delete;

  auto begin() { return body_.begin(); }
  auto end() { return body_.end(); }
  auto begin() const { return body_.begin(); }
  auto end() const { return body_.end(); }
  auto size() const { return body_.size(); }

  void reserve(size_t size) { body_.reserve(size); }

  void AddToBody(MachineBasicBlock* bb) { body_.push_back(bb); }
  void AddEntryBlock(MachineBasicBlock* bb) {
    if (Contains(entry_blocks_, bb)) {
      return;
    }
    entry_blocks_.push_back(bb);
  }

  MachineBasicBlock* at(size_t index) const { return body_.at(index); }

  [[nodiscard]] bool is_reducible() const {
    CHECK_GT(entry_blocks_.size(), 0u);
    CHECK_GT(body_.size(), 0u);
    return entry_blocks_.size() == 1;
  }

  const ArenaVector<MachineBasicBlock*>& body() const { return body_; }
  const ArenaVector<MachineBasicBlock*>& entry_blocks() const { return entry_blocks_; }

 private:
  ArenaVector<MachineBasicBlock*> body_;
  ArenaVector<MachineBasicBlock*> entry_blocks_;
};

using LoopVector = ArenaVector<Loop*>;

class LoopTreeNode {
 public:
  LoopTreeNode(MachineIR* ir, Loop* loop = nullptr) : loop_(loop), innerloop_nodes_(ir->arena()) {}

  Loop* loop() const { return loop_; }
  size_t NumInnerloops() const { return innerloop_nodes_.size(); };
  LoopTreeNode* GetInnerloopNode(size_t i) const { return innerloop_nodes_.at(i); }

  void AddInnerloopNode(LoopTreeNode* node) { innerloop_nodes_.push_back(node); }

 private:
  Loop* loop_;  // null if the node is the root of the tree.
  ArenaVector<LoopTreeNode*> innerloop_nodes_;
};

class LoopTree {
 public:
  LoopTree(MachineIR* ir) : ir_(ir), root_(NewInArena<LoopTreeNode>(ir->arena(), ir)) {}

  LoopTreeNode* root() const { return root_; }

  // This function requires that loops are inserted in the order of
  // non-increasing loop size, because the function assumes the loop in which
  // the current loop is nested in is already inserted.
  void InsertLoop(Loop* loop) {
    bool success = TryInsertLoopAtNode(root(), loop);
    CHECK(success);
  }

 private:
  bool TryInsertLoopAtNode(LoopTreeNode* node, Loop* loop);

  MachineIR* ir_;
  LoopTreeNode* root_;
};

LoopVector FindLoops(MachineIR* ir);
LoopTree BuildLoopTree(MachineIR* ir);

MachineBasicBlockList FindNonloopNodes(MachineIR* ir);
MachineBasicBlockList GetReversePostOrderBBList(MachineIR* ir);

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_MACHINE_IR_ANALYSIS_H_
