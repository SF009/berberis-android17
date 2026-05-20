/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include <linux/sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <bit>
#include <csignal>

#if !defined(__LP64__)
#error "clone3_test is only supported on 64-bit architectures"
#endif

#ifndef __NR_clone3
#define __NR_clone3 435
#endif

#ifndef CLONE_PIDFD
#define CLONE_PIDFD 0x00001000
#endif

namespace {

struct clone_args {
  uint64_t flags;
  uint64_t pidfd;
  uint64_t child_tid;
  uint64_t parent_tid;
  uint64_t exit_signal;
  uint64_t stack;
  uint64_t stack_size;
  uint64_t tls;
  uint64_t set_tid;
  uint64_t set_tid_size;
  uint64_t cgroup;
};

TEST(Clone3, SameStackVfork) {
  std::atomic<int> child_finished(0);

  struct clone_args args = {};
  args.flags = CLONE_VM | CLONE_VFORK;
  args.exit_signal = SIGCHLD;
  args.stack = 0;
  args.stack_size = 0;

  pid_t pid = syscall(__NR_clone3, &args, sizeof(args));

  if (pid == 0) {
    // Child
    child_finished = 1;
    _exit(42);
  }

  ASSERT_GT(pid, 0);

  int status;
  ASSERT_EQ(pid, waitpid(pid, &status, 0));
  ASSERT_TRUE(WIFEXITED(status)) << "Child "
                                 << (WIFSIGNALED(status) ? "terminated by signal " +
                                                               std::to_string(WTERMSIG(status))
                                                         : "did not exit normally, status: " +
                                                               std::to_string(status));
  ASSERT_EQ(WEXITSTATUS(status), 42);
  ASSERT_EQ(child_finished, 1);
}

TEST(Clone3, NewProcess) {
  struct clone_args args = {};
  args.exit_signal = SIGCHLD;

  pid_t pid = syscall(__NR_clone3, &args, sizeof(args));

  if (pid == 0) {
    // Child
    _exit(43);
  }

  ASSERT_GT(pid, 0);

  int status;
  ASSERT_EQ(pid, waitpid(pid, &status, 0));
  ASSERT_TRUE(WIFEXITED(status)) << "Child "
                                 << (WIFSIGNALED(status) ? "terminated by signal " +
                                                               std::to_string(WTERMSIG(status))
                                                         : "did not exit normally, status: " +
                                                               std::to_string(status));
  ASSERT_EQ(WEXITSTATUS(status), 43);
}

TEST(Clone3, Pidfd) {
  int pidfd = -1;
  struct clone_args args = {};
  args.flags = CLONE_PIDFD;
  args.pidfd = std::bit_cast<uint64_t>(&pidfd);
  args.exit_signal = SIGCHLD;

  pid_t pid = syscall(__NR_clone3, &args, sizeof(args));

  if (pid == 0) {
    // Child
    _exit(44);
  }

  ASSERT_GT(pid, 0);
  ASSERT_GE(pidfd, 0);

  int status;
  ASSERT_EQ(pid, waitpid(pid, &status, 0));
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), 44);

  close(pidfd);
}

}  // namespace
