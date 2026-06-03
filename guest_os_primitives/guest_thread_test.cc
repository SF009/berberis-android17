/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "gtest/gtest.h"

#include <pthread.h>
#include <sys/mman.h>

#include <bit>
#include <csetjmp>
#include <csignal>
#include <cstdlib>

#include "berberis/base/bit_util.h"
#include "berberis/base/page_size.h"
#include "berberis/guest_os_primitives/guest_thread.h"
#include "berberis/guest_state/guest_state_opaque.h"
#include "berberis/runtime_primitives/host_stack.h"

namespace berberis {

namespace {

// Test that host pthread_exit runs destructors of local objects.
// This should happen at pthread_cleanup (before pthread_specific destructors).

int g_count = 0;

struct ScopedCount {
  ScopedCount() { ++g_count; }
  ~ScopedCount() { --g_count; }
};

void RunPthreadExit() {
  ASSERT_EQ(1, g_count);
  pthread_exit(nullptr);
  FAIL();
}

void* ThreadFunc(void* /* arg */) {
  ScopedCount c;
  RunPthreadExit();  // does not return
  return nullptr;
}

TEST(GuestThreadTest, PthreadExitRunsLocalDtors) {
  ASSERT_EQ(0, g_count);
  pthread_t thread;
  ASSERT_EQ(0, pthread_create(&thread, nullptr, ThreadFunc, nullptr));
  ASSERT_EQ(0, pthread_join(thread, nullptr));
  // TODO(b/27860783): it turned out that on bionic pthread_exit doesn't run
  // destructors for local objects. If that gets fixed, change the code
  // accordingly (see other TODOs for this bug).
  // ASSERT_EQ(0, g_count);
  ASSERT_EQ(1, g_count);
}

sigjmp_buf g_jmp_buf;
void* g_fault_addr = nullptr;

void SigsegvHandler(int /* sig */, siginfo_t* info, void* /* context */) {
  g_fault_addr = info->si_addr;
  siglongjmp(g_jmp_buf, 1);
}

volatile bool g_always_true = true;

void OverflowFunc() {
  volatile char buffer[1024];
  buffer[0] = 0;
  if (g_always_true) {
    OverflowFunc();
  }
  buffer[0] = 1;
}

#if defined(__x86_64__)
__attribute__((naked, noinline)) void RunOnStack(void* stack_top, void (*func)()) {
  asm volatile(
      "mov %%rsp, %%rax\n"
      "mov %%rdi, %%rsp\n"
      "push %%rax\n"
      "push %%rax\n"
      "call *%%rsi\n"
      "pop %%rax\n"
      "pop %%rsp\n"
      "ret\n"
      :
      :
      : "memory");
}
#elif defined(__i386__)
__attribute__((naked, noinline)) void RunOnStack(void* stack_top, void (*func)()) {
  asm volatile(
      "mov 4(%%esp), %%edx\n"
      "mov 8(%%esp), %%ecx\n"
      "mov %%esp, %%eax\n"
      "mov %%edx, %%esp\n"
      "push %%eax\n"
      "sub $12, %%esp\n"
      "call *%%ecx\n"
      "add $12, %%esp\n"
      "pop %%esp\n"
      "ret\n"
      :
      :
      : "memory");
}
#else
#error "Unsupported host architecture for RunOnStack"
#endif

TEST(GuestThreadTest, HostStackGuardPage) {
  ThreadState* state = CreateThreadState();
  ASSERT_NE(nullptr, state);
  GuestThread* parent = GuestThread::CreateForTest(state);
  ASSERT_NE(nullptr, parent);

  GuestThread* child = GuestThread::CreateClone(parent, true);
  ASSERT_NE(nullptr, child);

  void* stack_top = child->GetHostStackTop();
  ASSERT_TRUE(IsAligned<16>(stack_top));
  size_t stack_size = GetStackSizeForTranslation();

  void* usable_stack_base = std::bit_cast<char*>(stack_top) - stack_size;
  void* bottom_guard_page = std::bit_cast<char*>(usable_stack_base) - kPageSize;

  g_fault_addr = nullptr;

  struct sigaction sa, old_sa;
  sa.sa_sigaction = SigsegvHandler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(0, sigaction(SIGSEGV, &sa, &old_sa));

  stack_t ss, old_ss;
  ss.ss_sp = malloc(SIGSTKSZ);
  ss.ss_size = SIGSTKSZ;
  ss.ss_flags = 0;
  ASSERT_NE(nullptr, ss.ss_sp);
  ASSERT_EQ(0, sigaltstack(&ss, &old_ss));

  if (sigsetjmp(g_jmp_buf, 1) == 0) {
    RunOnStack(stack_top, OverflowFunc);
  }

  EXPECT_NE(g_fault_addr, nullptr);
  EXPECT_GE(g_fault_addr, bottom_guard_page);
  EXPECT_LT(g_fault_addr, usable_stack_base);

  sigaltstack(&old_ss, nullptr);
  free(ss.ss_sp);
  sigaction(SIGSEGV, &old_sa, nullptr);

  GuestThread::Destroy(child);
  GuestThread::Destroy(parent);
}

}  // namespace

}  // namespace berberis
