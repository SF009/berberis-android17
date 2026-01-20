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

#include "berberis/backend/x86_64/machine_ir_analysis.h"

#include <algorithm>

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/arena_alloc.h"
#include "berberis/base/arena_vector.h"
#include "berberis/base/checks.h"

namespace berberis::x86_64 {

namespace {

class LoopBuilder {
 public:
  LoopBuilder(MachineIR* ir, Loop* loop, MachineBasicBlock* loop_head)
      : loop_(loop), is_bb_in_loop_(ir->NumBasicBlocks(), false, ir->arena()) {
    CHECK_EQ(loop_->size(), 0u);
    loop_->reserve(ir->NumBasicBlocks());
    loop_->AddToBody(loop_head);
    is_bb_in_loop_.at(loop_head->id()) = true;
  }

  // Appends bb to loop (bb-vector) unless bb is already in loop.
  // Returns whether bb is appended.
  bool PushBackIfNotInLoop(MachineBasicBlock* bb) {
    if (is_bb_in_loop_.at(bb->id())) {
      return false;
    }
    loop_->AddToBody(bb);
    is_bb_in_loop_.at(bb->id()) = true;
    return true;
  }

 private:
  Loop* loop_;
  ArenaVector<bool> is_bb_in_loop_;
};

// We create an order of bb's using the DFS described in
// "Nesting of Reducible and Irreducible Loops" by Paul Havlak
// ACM Transactions on Programming Languages and Systems (TOPLAS), 1997.
// See: https://doi.org/10.1145/262004.262005
class HavlakDFS {
 public:
  HavlakDFS(MachineIR* ir)
      : preorder_counter_(1),
        preorder_pos_(ir->NumBasicBlocks(), kUnvisited, ir->arena()),
        max_descendant_preorder_pos_(ir->NumBasicBlocks(), kUnvisited, ir->arena()),
        ordered_bb_vector_(ir->arena()),
        analysis_finished_(false) {
    ordered_bb_vector_.reserve(ir->NumBasicBlocks());
  }

  // This relies on the property that DFS traversal intervals (defined by entry
  // and exit times) are either disjoint or fully nested.
  // Therefore, checking if potential_descendant's start time (preorder_pos) lies within
  // potential_ancestor's range is sufficient to evaluate if the descendant's entire range is
  // contained within the ancestor's range.
  [[nodiscard]] bool IsAncestor(const MachineBasicBlock* potential_ancestor,
                                const MachineBasicBlock* potential_descendant) const {
    CHECK_NE(kUnvisited, max_descendant_preorder_pos_.at(potential_ancestor->id()));
    CHECK_NE(kUnvisited, max_descendant_preorder_pos_.at(potential_descendant->id()));
    return preorder_pos_.at(potential_ancestor->id()) <=
               preorder_pos_.at(potential_descendant->id()) &&
           preorder_pos_.at(potential_descendant->id()) <=
               max_descendant_preorder_pos_.at(potential_ancestor->id());
  }

  ArenaVector<const MachineBasicBlock*> nodes_ordered_by_dfn() {
    CHECK(analysis_finished_);
    return ordered_bb_vector_;
  };

  void RunDfs(const MachineBasicBlock* entry_bb) {
    DfsRecursive(entry_bb);
    analysis_finished_ = true;
  }

  void DfsRecursive(const MachineBasicBlock* bb) {
    preorder_pos_.at(bb->id()) = preorder_counter_++;
    ordered_bb_vector_.push_back(bb);
    for (auto* outward_edge : bb->out_edges()) {
      auto* succ_bb = outward_edge->dst();
      // If unvisited, recurse
      if (preorder_pos_.at(succ_bb->id()) == kUnvisited) {
        DfsRecursive(succ_bb);
      }
    }
    max_descendant_preorder_pos_.at(bb->id()) = preorder_counter_ - 1;
  }

 private:
  static constexpr uint32_t kUnvisited = 0;

  uint32_t preorder_counter_;
  ArenaVector<uint32_t> preorder_pos_;
  ArenaVector<uint32_t> max_descendant_preorder_pos_;
  ArenaVector<const MachineBasicBlock*> ordered_bb_vector_;
  bool analysis_finished_;
};

void PostOrderTraverseBBListRecursive(MachineBasicBlock* bb,
                                      ArenaVector<bool>& is_visited,
                                      MachineBasicBlockList& result) {
  is_visited[bb->id()] = true;
  for (auto* edge : bb->out_edges()) {
    auto* dst = edge->dst();
    if (!is_visited[dst->id()]) {
      PostOrderTraverseBBListRecursive(dst, is_visited, result);
    }
  }
  // We push to front so that the post order list is automatically reversed.
  result.push_front(bb);
}

Loop* CollectLoop(MachineIR* ir,
                  const MachineEdgeVector& back_edges,
                  const HavlakDFS* havlak,
                  size_t begin,
                  size_t end) {
  Arena* arena = ir->arena();
  auto* head_bb = back_edges[begin]->dst();
  auto* loop = NewInArena<Loop>(arena, arena);
  loop->AddEntryBlock(head_bb);
  LoopBuilder builder(ir, loop, head_bb);
  for (size_t edge_no = begin; edge_no < end; ++edge_no) {
    auto* back_branch_bb = back_edges[edge_no]->src();
    // All back-edges must be to the same entry.
    CHECK_EQ(back_edges[edge_no]->dst(), head_bb);

    if (!builder.PushBackIfNotInLoop(back_branch_bb)) {
      // We have already processed this basic-block (and consequently
      // all its predecessors) while processing another back-edge.
      continue;
    }

    for (size_t bb_no = loop->size() - 1; bb_no < loop->size(); ++bb_no) {
      auto* bb = loop->at(bb_no);
      CHECK(!bb->in_edges().empty());
      for (auto in_edge : bb->in_edges()) {
        MachineBasicBlock* pred = in_edge->src();
        if (pred == head_bb) {
          continue;
        }
        if (havlak->IsAncestor(head_bb, pred)) {
          builder.PushBackIfNotInLoop(pred);
        } else {
          // 'pred' is NOT a descendant.
          // This means 'pred' is an entry point from OUTSIDE the loop structure.
          // This confirms the loop is irreducible, and 'bb' is another header.
          // We treat this edge as dead, and do not add 'pred' to the loop
          ir->set_contains_irreducible_loops();
          loop->AddEntryBlock(bb);
        }
      }
    }
  }
  return loop;
}

}  // namespace

MachineBasicBlockList FindNonloopNodes(MachineIR* ir) {
  MachineBasicBlockList ret(ir->arena());
  LoopVector loops = FindLoops(ir);
  ArenaVector<bool> is_in_loop(ir->NumBasicBlocks(), false, ir->arena());
  for (auto* loop : loops) {
    for (MachineBasicBlock* bb : *loop) {
      is_in_loop.at(bb->id()) = true;
    }
  }

  for (MachineBasicBlock* bb : ir->bb_list()) {
    if (!is_in_loop.at(bb->id())) {
      ret.push_back(bb);
    }
  }
  return ret;
}

MachineBasicBlockList GetReversePostOrderBBList(MachineIR* ir) {
  if (ir->bb_order() == MachineIR::BasicBlockOrder::kReversePostOrder) {
    return ir->bb_list();
  }
  MachineBasicBlock* entry_bb = ir->bb_list().front();
  CHECK_EQ(entry_bb->in_edges().size(), 0);

  ArenaVector<bool> is_visited(ir->NumBasicBlocks(), false, ir->arena());
  MachineBasicBlockList rpo_list(ir->arena());
  PostOrderTraverseBBListRecursive(entry_bb, is_visited, rpo_list);
  return rpo_list;
}

LoopVector FindLoops(MachineIR* ir) {
  HavlakDFS havlak(ir);
  const MachineBasicBlock* entry_bb = ir->bb_list().front();
  CHECK_EQ(entry_bb->in_edges().size(), 0);
  havlak.RunDfs(entry_bb);

  Arena* arena = ir->arena();

  LoopVector loops_vector(arena);
  const size_t kMaxBackEdgesExpected = 16;
  loops_vector.reserve(kMaxBackEdgesExpected);

  ArenaVector<MachineEdge*> back_edges(arena);
  back_edges.reserve(kMaxBackEdgesExpected);

  // Collects back-edges.
  // We iterate through basic blocks in the order they were discovered by DFS.
  // TODO(b/463953668): The DFS pass can be extended to find back-edges in addition to ancestors.
  // This will remove the need for a separate pass to find back-edges.
  for (auto* bb : havlak.nodes_ordered_by_dfn()) {
    for (auto* edge : bb->out_edges()) {
      // A back-edge is an edge from a node to one of its ancestors in the DFS tree.
      if (havlak.IsAncestor(edge->dst(), bb)) {
        back_edges.push_back(edge);
      }
    }
  }

  // Pull back-edges with the same target (loop head) together.
  std::sort(
      back_edges.begin(), back_edges.end(), [](const MachineEdge* left, const MachineEdge* right) {
        return left->dst()->id() < right->dst()->id();
      });

  // Guard which makes the following loop-body simpler.
  auto empty_edge = MachineEdge(arena, nullptr, nullptr);
  back_edges.push_back(&empty_edge);

  size_t begin_edge_no = 0;
  // Collect loops for back-edges with the same target.
  for (size_t edge_no = 1; edge_no < back_edges.size(); ++edge_no) {
    if (back_edges[begin_edge_no]->dst() == back_edges[edge_no]->dst()) {
      continue;
    }
    // Encountered new head - collect loop for the previous one.
    // Guard (being the last) doesn't require loop collection.
    auto* loop = CollectLoop(ir, back_edges, &havlak, begin_edge_no, edge_no);
    if (loop) {
      loops_vector.push_back(loop);
    }
    begin_edge_no = edge_no;
  }
  return loops_vector;
}

bool LoopTree::TryInsertLoopAtNode(LoopTreeNode* node, Loop* loop) {
  if (node->loop() != nullptr && !Contains(*node->loop(), loop->at(0))) {
    return false;
  }

  for (size_t i = 0; i < node->NumInnerloops(); i++) {
    auto* innerloop_node = node->GetInnerloopNode(i);
    if (TryInsertLoopAtNode(innerloop_node, loop)) {
      return true;
    }
  }

  LoopTreeNode* innerloop_node = NewInArena<LoopTreeNode>(ir_->arena(), ir_, loop);
  node->AddInnerloopNode(innerloop_node);
  return true;
}

LoopTree BuildLoopTree(MachineIR* ir) {
  auto loops = FindLoops(ir);
  std::sort(loops.begin(), loops.end(), [](auto* loop1, auto* loop2) {
    return loop1->size() > loop2->size();
  });

  LoopTree loop_tree(ir);
  for (auto* loop : loops) {
    loop_tree.InsertLoop(loop);
  }

  return loop_tree;
}

}  // namespace berberis::x86_64
