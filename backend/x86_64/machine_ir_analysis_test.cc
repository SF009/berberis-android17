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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <vector>

#include "berberis/backend/x86_64/machine_ir_analysis.h"

#include "berberis/backend/code_emitter.h"
#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/backend/x86_64/machine_ir_check.h"
#include "berberis/base/algorithm.h"
#include "berberis/base/logging.h"
#include "berberis/guest_state/guest_addr.h"

namespace berberis {

namespace {

void CheckLoopContent(x86_64::Loop* loop, std::vector<MachineBasicBlock*> body) {
  EXPECT_EQ(loop->size(), body.size());
  // Loop head must be the first basic block in the loop.
  EXPECT_EQ(loop->at(0), body[0]);
  EXPECT_THAT(loop->body(), testing::UnorderedElementsAreArray(body));
}

void CheckLoopHeaders(x86_64::Loop* loop, std::vector<MachineBasicBlock*> headers) {
  EXPECT_EQ(loop->entry_blocks().size(), headers.size());
  // Check that the first bb in loop->body_ is one of the headers.
  EXPECT_TRUE(Contains(loop->entry_blocks(), loop->body().at(0)));
  EXPECT_THAT(loop->entry_blocks(), testing::UnorderedElementsAreArray(headers));
}

TEST(MachineIRAnalysis, SelfLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  // bb1 -- bb2 -- bb3
  //        | |
  //        ---
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb2);
  machine_ir.AddEdge(bb2, bb3);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb3, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb3);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loops = x86_64::FindLoops(&machine_ir);
  EXPECT_EQ(loops.size(), 1UL);
  auto loop = loops[0];
  CheckLoopContent(loop, {bb2});
}

TEST(MachineIRAnalysis, SingleLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  // bb1 -- bb2 -- bb3 ---- bb4
  //         |      |
  //         --------
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb2);
  machine_ir.AddEdge(bb3, bb4);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<Branch>(bb3);

  builder.StartBasicBlock(bb3);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb4);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loops = x86_64::FindLoops(&machine_ir);
  EXPECT_EQ(loops.size(), 1UL);
  auto loop = loops[0];
  CheckLoopContent(loop, {bb2, bb3});
}

TEST(MachineIRAnalysis, MultipleBackEdges) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //         -----------------
  //         |               |
  // bb1 -- bb2 -- bb3 ---- bb4
  //         |      |
  //         --------
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb2);
  machine_ir.AddEdge(bb3, bb4);
  machine_ir.AddEdge(bb4, bb2);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<Branch>(bb3);

  builder.StartBasicBlock(bb3);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb4);
  builder.Gen<Branch>(bb2);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loops = x86_64::FindLoops(&machine_ir);
  EXPECT_EQ(loops.size(), 1UL);
  auto loop = loops[0];
  CheckLoopContent(loop, {bb2, bb3, bb4});
}

TEST(MachineIRAnalysis, TwoLoops) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //         ------------------------
  //         |                      |
  // bb0---bb1 -- bb2 -- bb3 ---- bb4
  //               |      |
  //               --------
  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb2);
  machine_ir.AddEdge(bb3, bb4);
  machine_ir.AddEdge(bb4, bb1);

  builder.StartBasicBlock(bb0);
  builder.Gen<Branch>(bb1);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<Branch>(bb3);

  builder.StartBasicBlock(bb3);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb4);
  builder.Gen<Branch>(bb1);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loops = x86_64::FindLoops(&machine_ir);
  EXPECT_EQ(loops.size(), 2UL);
  auto loop1 = loops[0];
  CheckLoopContent(loop1, {bb1, bb2, bb3, bb4});
  auto loop2 = loops[1];
  CheckLoopContent(loop2, {bb2, bb3});
}

TEST(MachineIRAnalysis, LoopTreeInsertLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  x86_64::Loop loop1(&arena);
  loop1.AddToBody(bb1);

  x86_64::LoopTree tree(&machine_ir);
  tree.InsertLoop(&loop1);

  EXPECT_EQ(tree.root()->loop(), nullptr);
  EXPECT_EQ(tree.root()->NumInnerloops(), 1UL);

  auto* node = tree.root()->GetInnerloopNode(0);
  CheckLoopContent(node->loop(), {bb1});
  EXPECT_EQ(node->NumInnerloops(), 0UL);
}

TEST(MachineIRAnalysis, LoopTreeInsertParallelLoops) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  auto* bb3 = machine_ir.NewBasicBlock();
  x86_64::Loop loop1(&arena);
  loop1.AddToBody(bb1);
  loop1.AddToBody(bb2);
  x86_64::Loop loop2(&arena);
  loop2.AddToBody(bb3);

  x86_64::LoopTree tree(&machine_ir);
  tree.InsertLoop(&loop1);
  tree.InsertLoop(&loop2);

  EXPECT_EQ(tree.root()->loop(), nullptr);
  EXPECT_EQ(tree.root()->NumInnerloops(), 2UL);

  auto* node1 = tree.root()->GetInnerloopNode(0);
  CheckLoopContent(node1->loop(), {bb1, bb2});
  EXPECT_EQ(node1->NumInnerloops(), 0UL);

  auto* node2 = tree.root()->GetInnerloopNode(1);
  CheckLoopContent(node2->loop(), {bb3});
  EXPECT_EQ(node2->NumInnerloops(), 0UL);
}

TEST(MachineIRAnalysis, LoopTreeInsertNestedLoops) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  auto* bb1 = machine_ir.NewBasicBlock();
  auto* bb2 = machine_ir.NewBasicBlock();
  x86_64::Loop loop1(&arena);
  loop1.AddToBody(bb1);
  loop1.AddToBody(bb2);
  x86_64::Loop loop2(&arena);
  loop2.AddToBody(bb2);

  x86_64::LoopTree tree(&machine_ir);
  tree.InsertLoop(&loop1);
  tree.InsertLoop(&loop2);

  EXPECT_EQ(tree.root()->loop(), nullptr);
  EXPECT_EQ(tree.root()->NumInnerloops(), 1UL);

  auto* node1 = tree.root()->GetInnerloopNode(0);
  CheckLoopContent(node1->loop(), {bb1, bb2});
  EXPECT_EQ(node1->NumInnerloops(), 1UL);

  auto* node2 = node1->GetInnerloopNode(0);
  CheckLoopContent(node2->loop(), {bb2});
  EXPECT_EQ(node2->NumInnerloops(), 0UL);
}

TEST(MachineIRAnalysis, FindSingleLoopTree) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  // bb1 -- bb2 -- bb3
  //        | |
  //        ---
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb2);
  machine_ir.AddEdge(bb2, bb3);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb3, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb3);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* loop_node = root->GetInnerloopNode(0);
  CheckLoopContent(loop_node->loop(), {bb2});
}

TEST(MachineIRAnalysis, FindNestedLoopTree) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //         ------------------------
  //         |                      |
  // bb0---bb1 -- bb2 -- bb3 ---- bb4
  //               |      |
  //               --------
  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb2);
  machine_ir.AddEdge(bb3, bb4);
  machine_ir.AddEdge(bb4, bb1);

  builder.StartBasicBlock(bb0);
  builder.Gen<Branch>(bb1);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<Branch>(bb3);

  builder.StartBasicBlock(bb3);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb4);
  builder.Gen<Branch>(bb1);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* outerloop_node = root->GetInnerloopNode(0);
  CheckLoopContent(outerloop_node->loop(), {bb1, bb2, bb3, bb4});

  EXPECT_EQ(outerloop_node->NumInnerloops(), 1UL);
  auto* innerloop_node = outerloop_node->GetInnerloopNode(0);
  CheckLoopContent(innerloop_node->loop(), {bb2, bb3});
}

TEST(MachineIRAnalysis, FindLoopTreeWithMultipleInnerloops) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //         -------------------------------
  //         |                     |       |
  // bb0---bb1 -- bb2 -- bb3 ---- bb4-----bb5
  //               |      |
  //               --------
  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  auto bb5 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb2);
  machine_ir.AddEdge(bb3, bb4);
  machine_ir.AddEdge(bb4, bb5);
  machine_ir.AddEdge(bb5, bb4);
  machine_ir.AddEdge(bb5, bb1);

  builder.StartBasicBlock(bb0);
  builder.Gen<Branch>(bb1);

  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);

  builder.StartBasicBlock(bb2);
  builder.Gen<Branch>(bb3);

  builder.StartBasicBlock(bb3);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb4, x86_64::kMachineRegFLAGS);

  builder.StartBasicBlock(bb4);
  builder.Gen<Branch>(bb5);

  builder.StartBasicBlock(bb5);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb1, bb4, x86_64::kMachineRegFLAGS);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* outerloop_node = root->GetInnerloopNode(0);
  CheckLoopContent(outerloop_node->loop(), {bb1, bb2, bb3, bb4, bb5});
  CheckLoopHeaders(outerloop_node->loop(), {bb1});

  EXPECT_EQ(outerloop_node->NumInnerloops(), 2UL);
  auto* innerloop_node1 = outerloop_node->GetInnerloopNode(0);
  CheckLoopContent(innerloop_node1->loop(), {bb2, bb3});
  auto* innerloop_node2 = outerloop_node->GetInnerloopNode(1);
  CheckLoopContent(innerloop_node2->loop(), {bb4, bb5});
}

TEST(MachineIRAnalysis, FindNonloopNodes) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  // bb1, bb2, bb6, bb7, and bb8 are loop nodes.
  // bb0 -> bb1 <-> bb2 -> bb3 -> bb4 -> bb5 -> bb6 -> bb7 -> bb8 -> bb9
  //                                             ^-------------|
  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  auto bb5 = machine_ir.NewBasicBlock();
  auto bb6 = machine_ir.NewBasicBlock();
  auto bb7 = machine_ir.NewBasicBlock();
  auto bb8 = machine_ir.NewBasicBlock();
  auto bb9 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb2, bb1);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb4);
  machine_ir.AddEdge(bb4, bb5);
  machine_ir.AddEdge(bb5, bb6);
  machine_ir.AddEdge(bb6, bb7);
  machine_ir.AddEdge(bb7, bb8);
  machine_ir.AddEdge(bb8, bb6);
  machine_ir.AddEdge(bb8, bb9);

  builder.StartBasicBlock(bb0);
  builder.Gen<Branch>(bb1);
  builder.StartBasicBlock(bb1);
  builder.Gen<Branch>(bb2);
  builder.StartBasicBlock(bb2);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb1, bb3, x86_64::kMachineRegFLAGS);
  builder.StartBasicBlock(bb3);
  builder.Gen<Branch>(bb4);
  builder.StartBasicBlock(bb4);
  builder.Gen<Branch>(bb5);
  builder.StartBasicBlock(bb5);
  builder.Gen<Branch>(bb6);
  builder.StartBasicBlock(bb6);
  builder.Gen<Branch>(bb7);
  builder.StartBasicBlock(bb7);
  builder.Gen<Branch>(bb8);
  builder.StartBasicBlock(bb8);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb6, bb9, x86_64::kMachineRegFLAGS);
  builder.StartBasicBlock(bb9);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);

  auto nonloop_nodes = FindNonloopNodes(&machine_ir);
  EXPECT_THAT(nonloop_nodes, testing::UnorderedElementsAre(bb0, bb3, bb4, bb5, bb9));
}

TEST(MachineIRAnalysis, FindNonloopNodesIrreducible) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //      bb0
  //       |
  //       v
  //      bb1 ----+
  //       |      |
  //       v      v
  //      bb2<-->bb3
  //       |
  //       v
  //      bb4
  //
  // Loop is bb2-bb3, but there is an edge from bb1 to bb3,
  // which makes the loop irreducible because bb2 doesn't dominate bb3.
  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb1, bb3);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb2, bb4);
  machine_ir.AddEdge(bb3, bb2);

  builder.StartBasicBlock(bb0);
  builder.Gen<Branch>(bb1);
  builder.StartBasicBlock(bb1);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb3, x86_64::kMachineRegFLAGS);
  builder.StartBasicBlock(bb2);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);
  builder.StartBasicBlock(bb3);
  builder.Gen<Branch>(bb2);
  builder.StartBasicBlock(bb4);
  builder.Gen<Jump>(kNullGuestAddr);

  auto nonloop_nodes = FindNonloopNodes(&machine_ir);
  EXPECT_THAT(nonloop_nodes, testing::UnorderedElementsAre(bb0, bb1, bb4));
}

TEST(MachineIRAnalysis, FindLoopTreeWithIrreducibleLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //      bb0
  //       |
  //       v
  //      bb1 ----+
  //       |      |
  //       v      v
  //      bb2<-->bb3
  //       |
  //       v
  //      bb4
  //
  // Loop is bb2-bb3, but there is an edge from bb1 to bb3,
  // which makes the loop irreducible because bb2 doesn't dominate bb3.
  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb1, bb3);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb2, bb4);
  machine_ir.AddEdge(bb3, bb2);

  builder.StartBasicBlock(bb0);
  builder.Gen<Branch>(bb1);
  builder.StartBasicBlock(bb1);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb2, bb3, x86_64::kMachineRegFLAGS);
  builder.StartBasicBlock(bb2);
  builder.Gen<CondBranch>(CodeEmitter::Condition::kZero, bb3, bb4, x86_64::kMachineRegFLAGS);
  builder.StartBasicBlock(bb3);
  builder.Gen<Branch>(bb2);
  builder.StartBasicBlock(bb4);
  builder.Gen<Jump>(kNullGuestAddr);

  ASSERT_EQ(x86_64::CheckMachineIR(machine_ir), x86_64::kMachineIRCheckSuccess);
  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* loop_node = root->GetInnerloopNode(0);
  CheckLoopContent(loop_node->loop(), {bb2, bb3});
  CheckLoopHeaders(loop_node->loop(), {bb2, bb3});
}

TEST(MachineIRAnalysis, FindLoopTreeWithIrreducibleLoopNestedInsideReducibleLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //         [ bb0 ]
  //            |
  //            v
  //          ( bb1 ) <-------+   <-- Outer Loop Header (Natural)
  //          /   \         |
  //         v     v        |
  //       ( bb2 )<->( bb3 )    ^  <-- Inner Loop (Irreducible)
  //          \     /       |       (Cycle B-C entered from both A->B and A->C)
  //           \   /        |
  //            v v         |
  //          ( bb4 ) --------+   <-- Outer Loop Latch
  //            |
  //            v
  //         [ bb5 ]

  auto bb0 = machine_ir.NewBasicBlock();
  auto bb1 = machine_ir.NewBasicBlock();
  auto bb2 = machine_ir.NewBasicBlock();
  auto bb3 = machine_ir.NewBasicBlock();
  auto bb4 = machine_ir.NewBasicBlock();
  auto bb5 = machine_ir.NewBasicBlock();
  machine_ir.AddEdge(bb0, bb1);
  machine_ir.AddEdge(bb1, bb2);
  machine_ir.AddEdge(bb1, bb3);
  machine_ir.AddEdge(bb2, bb3);
  machine_ir.AddEdge(bb3, bb2);
  machine_ir.AddEdge(bb2, bb4);
  machine_ir.AddEdge(bb3, bb4);
  machine_ir.AddEdge(bb4, bb1);
  machine_ir.AddEdge(bb4, bb5);

  builder.StartBasicBlock(bb0);
  builder.StartBasicBlock(bb1);
  builder.StartBasicBlock(bb2);
  builder.StartBasicBlock(bb3);
  builder.StartBasicBlock(bb4);
  builder.StartBasicBlock(bb5);
  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* outerloop_node = root->GetInnerloopNode(0);
  CheckLoopContent(outerloop_node->loop(), {bb1, bb2, bb3, bb4});
  CheckLoopHeaders(outerloop_node->loop(), {bb1});

  EXPECT_EQ(outerloop_node->NumInnerloops(), 1UL);
  auto* innerloop_node = outerloop_node->GetInnerloopNode(0);
  CheckLoopContent(innerloop_node->loop(), {bb2, bb3});
  CheckLoopHeaders(innerloop_node->loop(), {bb2, bb3});
}

TEST(MachineIRAnalysis, FindLoopTreeWithReducibleLoopNestedInsideIrreducibleLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //           [ a ]
  //           /   \
  //          /     v
  //         |    ( c ) <---------------------+
  //         |      |                         |
  //         |      |                         |
  //         v      v                         |
  //       ( b ) <--+                         |
  //         |                                |
  //         v                                |
  //       ( d ) <----------+                 |
  //         |              |                 |
  //         v              |                 |
  //       ( e ) -----------+                 |
  //         |                                |
  //         v                                |
  //       ( f ) -----------------------------+
  //         |
  //         v
  //       [ g ]

  auto a = machine_ir.NewBasicBlock();
  auto b = machine_ir.NewBasicBlock();
  auto c = machine_ir.NewBasicBlock();
  auto d = machine_ir.NewBasicBlock();
  auto e = machine_ir.NewBasicBlock();
  auto f = machine_ir.NewBasicBlock();
  auto g = machine_ir.NewBasicBlock();

  machine_ir.AddEdge(a, b);
  machine_ir.AddEdge(a, c);
  machine_ir.AddEdge(c, b);
  machine_ir.AddEdge(b, d);
  machine_ir.AddEdge(d, e);
  machine_ir.AddEdge(e, d);
  machine_ir.AddEdge(e, f);
  machine_ir.AddEdge(f, c);
  machine_ir.AddEdge(f, g);

  builder.StartBasicBlock(a);
  builder.StartBasicBlock(b);
  builder.StartBasicBlock(c);
  builder.StartBasicBlock(d);
  builder.StartBasicBlock(e);
  builder.StartBasicBlock(f);
  builder.StartBasicBlock(g);

  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* outerloop_node = root->GetInnerloopNode(0);
  CheckLoopContent(outerloop_node->loop(), {b, c, d, e, f});
  CheckLoopHeaders(outerloop_node->loop(), {b, c});

  EXPECT_EQ(outerloop_node->NumInnerloops(), 1UL);
  auto* innerloop_node = outerloop_node->GetInnerloopNode(0);
  CheckLoopContent(innerloop_node->loop(), {d, e});
  CheckLoopHeaders(innerloop_node->loop(), {d});
}

TEST(MachineIRAnalysis, FindLoopTreeWithIrreducibleLoopNestedInsideIrreducibleLoop) {
  Arena arena;
  x86_64::MachineIR machine_ir(&arena);

  x86_64::MachineIRBuilder builder(&machine_ir);

  //           [ a ]
  //           /   \
  //          /     v
  //         |    ( c ) <---------------------+
  //         |      |                         |
  //         |      |                         |
  //         v      v                         |
  // +--<--( b ) <--+                         |
  // |       |                                |
  // |       v                                |
  // |     ( d ) <----------+                 |
  // |       |              |                 |
  // |       v              |                 |
  // +-->--( e ) ---->------+                 |
  //         |                                |
  //         v                                |
  //       ( f ) ------------>----------------+
  //         |
  //         v
  //       [ g ]

  auto a = machine_ir.NewBasicBlock();
  auto b = machine_ir.NewBasicBlock();
  auto c = machine_ir.NewBasicBlock();
  auto d = machine_ir.NewBasicBlock();
  auto e = machine_ir.NewBasicBlock();
  auto f = machine_ir.NewBasicBlock();
  auto g = machine_ir.NewBasicBlock();

  machine_ir.AddEdge(a, b);
  machine_ir.AddEdge(a, c);
  machine_ir.AddEdge(c, b);
  machine_ir.AddEdge(b, d);
  machine_ir.AddEdge(b, e);
  machine_ir.AddEdge(d, e);
  machine_ir.AddEdge(e, d);
  machine_ir.AddEdge(e, f);
  machine_ir.AddEdge(f, c);
  machine_ir.AddEdge(f, g);

  builder.StartBasicBlock(a);
  builder.StartBasicBlock(b);
  builder.StartBasicBlock(c);
  builder.StartBasicBlock(d);
  builder.StartBasicBlock(e);
  builder.StartBasicBlock(f);
  builder.StartBasicBlock(g);

  auto loop_tree = x86_64::BuildLoopTree(&machine_ir);
  auto* root = loop_tree.root();

  EXPECT_EQ(root->NumInnerloops(), 1UL);
  auto* outerloop_node = root->GetInnerloopNode(0);
  CheckLoopContent(outerloop_node->loop(), {b, c, d, e, f});
  CheckLoopHeaders(outerloop_node->loop(), {b, c});

  EXPECT_EQ(outerloop_node->NumInnerloops(), 1UL);
  auto* innerloop_node = outerloop_node->GetInnerloopNode(0);
  CheckLoopContent(innerloop_node->loop(), {d, e});
  CheckLoopHeaders(innerloop_node->loop(), {d, e});
}

}  // namespace

}  // namespace berberis
