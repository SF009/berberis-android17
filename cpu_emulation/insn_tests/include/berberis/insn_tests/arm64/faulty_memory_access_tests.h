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

#ifndef BERBERIS_INSN_TESTS_ARM64_FAULTY_MEMORY_ACCESS_TESTS_H_
#define BERBERIS_INSN_TESTS_ARM64_FAULTY_MEMORY_ACCESS_TESTS_H_

#include <cstdint>

#if !defined(FAULTY_MEM_NAMESPACE)
#error "FAULTY_MEM_NAMESPACE macro must be defined"
#endif

// Extra namespace to avoid name collision with interpreter.
namespace berberis::faulty_memory_access_tests::FAULTY_MEM_NAMESPACE {

struct FaultyLoadResult {
  uint64_t value;
  uint64_t is_fault;
};

struct FaultySimdLoadResult {
  __uint128_t value;
  uint64_t is_fault;
};

// These are the APIs that the client needs to implement to make this test run.

FaultyLoadResult FaultyLoad(void* addr, uint8_t data_bytes);
bool FaultyStore(void* addr, uint8_t data_bytes, uint64_t value);

FaultySimdLoadResult FaultySimdLoad(void* addr, uint8_t data_bytes);
bool FaultySimdStore(void* addr, uint8_t data_bytes, __uint128_t value);

void* FindFaultyMemoryAccessRecoveryAddrForTesting(void* fault_addr);

}  // namespace berberis::faulty_memory_access_tests::FAULTY_MEM_NAMESPACE

#endif  // BERBERIS_INSN_TESTS_ARM64_FAULTY_MEMORY_ACCESS_TESTS_H_
