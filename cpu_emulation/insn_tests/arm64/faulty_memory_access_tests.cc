/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <csignal>
#include <cstddef>  // size_t
#include <cstdint>

#include "berberis/base/checks.h"
#include "berberis/insn_tests/arm64/faulty_memory_access_tests.h"

#if !defined(FAULTY_MEM_NAMESPACE)
#error "FAULTY_MEM_NAMESPACE macro must be defined"
#endif

namespace berberis::faulty_memory_access_tests::FAULTY_MEM_NAMESPACE {

namespace {

#if defined(__i386__)
constexpr size_t kRegIP = REG_EIP;
#elif defined(__x86_64__) || defined(__riscv__)
constexpr size_t kRegIP = REG_RIP;
#else
#error "Unsupported arch"
#endif

void FaultHandler(int /* sig */, siginfo_t* /* info */, void* ctx) {
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(ctx);
  static_assert(sizeof(void*) == sizeof(greg_t), "Unsupported type sizes");
  void* fault_addr = reinterpret_cast<void*>(ucontext->uc_mcontext.gregs[kRegIP]);
  void* recovery_addr = FindFaultyMemoryAccessRecoveryAddrForTesting(fault_addr);
  CHECK(recovery_addr);
  ucontext->uc_mcontext.gregs[kRegIP] = reinterpret_cast<greg_t>(recovery_addr);
}

class ScopedFaultySigaction {
 public:
  ScopedFaultySigaction() {
    struct sigaction sa{};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = FaultHandler;

    CHECK_EQ(0, sigaction(SIGSEGV, &sa, &old_sa_));
  }

  ~ScopedFaultySigaction() { CHECK_EQ(0, sigaction(SIGSEGV, &old_sa_, nullptr)); }

 private:
  struct sigaction old_sa_;
};

#if !defined(FAULTY_MEM_TESTSUITE)
#error "FAULTY_MEM_TESTSUITE macro must be defined"
#endif

TEST(FAULTY_MEM_TESTSUITE, FaultyLoadZeroExtension) {
  ScopedFaultySigaction scoped_sa;
  FaultyLoadResult result;

  int8_t data_8bit = 0x80;
  result = FaultyLoad(&data_8bit, 1);
  EXPECT_EQ(result.value, 0x0000'0000'0000'0080ULL);
  EXPECT_FALSE(result.is_fault);

  int16_t data_16bit = 0x8000;
  result = FaultyLoad(&data_16bit, 2);
  EXPECT_EQ(result.value, 0x0000'0000'0000'8000ULL);
  EXPECT_FALSE(result.is_fault);

  int32_t data_32bit = 0x8000'0000;
  result = FaultyLoad(&data_32bit, 4);
  EXPECT_EQ(result.value, 0x0000'0000'8000'0000ULL);
  EXPECT_FALSE(result.is_fault);
}

TEST(FAULTY_MEM_TESTSUITE, FaultyLoadSuccess) {
  ScopedFaultySigaction scoped_sa;
  uint64_t data = 0xffff'eeee'cccc'bbaaULL;
  FaultyLoadResult result;

  for (uint8_t data_bytes : {1, 2, 4, 8}) {
    result = FaultyLoad(&data, data_bytes);
    uint64_t mask = static_cast<uint64_t>((__uint128_t{1} << (8 * data_bytes)) - 1);
    EXPECT_EQ(result.value, data & mask);
    EXPECT_FALSE(result.is_fault);
  }
}

TEST(FAULTY_MEM_TESTSUITE, FaultyLoadFault) {
  ScopedFaultySigaction scoped_sa;
  FaultyLoadResult result;

  for (uint8_t data_bytes : {1, 2, 4, 8}) {
    result = FaultyLoad(nullptr, data_bytes);
    EXPECT_TRUE(result.is_fault);
  }
}

TEST(FAULTY_MEM_TESTSUITE, FaultyStoreSuccess) {
  ScopedFaultySigaction scoped_sa;
  uint64_t data = 0xffff'eeee'cccc'bbaaULL;
  uint64_t storage = 0;

  for (uint8_t data_bytes : {1, 2, 4, 8}) {
    bool is_fault = FaultyStore(&storage, data_bytes, data);
    uint64_t mask = static_cast<uint64_t>((__uint128_t{1} << (8 * data_bytes)) - 1);
    EXPECT_EQ(storage & mask, data & mask);
    EXPECT_FALSE(is_fault);
  }
}

TEST(FAULTY_MEM_TESTSUITE, FaultyStoreFault) {
  ScopedFaultySigaction scoped_sa;

  for (uint8_t data_bytes : {1, 2, 4, 8}) {
    bool is_fault = FaultyStore(nullptr, data_bytes, 0);
    EXPECT_TRUE(is_fault);
  }
}

// TODO(b/456536516): Add SIMD tests where the first half is no-fault but the second is fault.

TEST(FAULTY_MEM_TESTSUITE, FaultySimdLoadSuccess) {
  ScopedFaultySigaction scoped_sa;
  __uint128_t data = 0xffff'eeee'cccc'bbaaULL;
  data = (data << 64) | data;
  FaultySimdLoadResult result;

  for (uint8_t data_bytes : {1, 2, 4, 8, 16}) {
    result = FaultySimdLoad(&data, data_bytes);
    __uint128_t mask;
    if (data_bytes == 16) {
      mask = ~__int128_t{0};
    } else {
      mask = static_cast<uint64_t>((__uint128_t{1} << (8 * data_bytes)) - 1);
    }
    EXPECT_EQ(result.value, data & mask);
    EXPECT_FALSE(result.is_fault);
  }
}

TEST(FAULTY_MEM_TESTSUITE, FaultySimdLoadFault) {
  ScopedFaultySigaction scoped_sa;
  FaultySimdLoadResult result;

  for (uint8_t data_bytes : {1, 2, 4, 8, 16}) {
    result = FaultySimdLoad(nullptr, data_bytes);
    EXPECT_TRUE(result.is_fault);
  }
}

TEST(FAULTY_MEM_TESTSUITE, FaultySimdStoreSuccess) {
  ScopedFaultySigaction scoped_sa;
  __uint128_t data = 0xffff'eeee'cccc'bbaaULL;
  data = (data << 64) | data;
  __uint128_t storage = 0;

  for (uint8_t data_bytes : {1, 2, 4, 8, 16}) {
    bool is_fault = FaultySimdStore(&storage, data_bytes, data);
    __uint128_t mask;
    if (data_bytes == 16) {
      mask = ~__int128_t{0};
    } else {
      mask = static_cast<uint64_t>((__uint128_t{1} << (8 * data_bytes)) - 1);
    }
    EXPECT_EQ(storage & mask, data & mask);
    EXPECT_FALSE(is_fault);
  }
}

TEST(FAULTY_MEM_TESTSUITE, FaultySimdStoreFault) {
  ScopedFaultySigaction scoped_sa;

  for (uint8_t data_bytes : {1, 2, 4, 8, 16}) {
    bool is_fault = FaultySimdStore(nullptr, data_bytes, 0);
    EXPECT_TRUE(is_fault);
  }
}

}  // namespace

}  // namespace berberis::faulty_memory_access_tests::FAULTY_MEM_NAMESPACE
