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

#ifndef BERBERIS_BACKEND_X86_64_READ_FLAGS_VARIANTS_TEST_HELPER_H
#define BERBERIS_BACKEND_X86_64_READ_FLAGS_VARIANTS_TEST_HELPER_H

#include "gtest/gtest.h"

#include "berberis/backend/x86_64/machine_ir.h"
#include "berberis/backend/x86_64/machine_ir_builder.h"
#include "berberis/base/checks.h"
#include "berberis/device_arch_info/x86_64/device_arch_info.h"

namespace berberis::x86_64 {

class ReadFlagsVariantsTest : public testing::TestWithParam<MachineOpcode> {
 public:
  static berberis::MachineInsn* GenReadFlags(MachineIRBuilder& builder,
                                             MachineReg reg,
                                             MachineReg flag) {
    if (GetReadOpcode() == kMachineOpReadFlagsWithOverflow) {
      return builder.Gen<ReadFlagsWithOverflow>(reg, flag);
    } else if (GetReadOpcode() == kMachineOpReadFlagsWithoutOverflow) {
      return builder.Gen<ReadFlagsWithoutOverflow>(reg, flag);
    } else {
      FATAL("Unsupported opcode");
    }
  }
  static const MachineOpcode& GetReadOpcode() { return GetParam(); }
};

#define INSTANTIATE_READ_FLAGS_VARIANTS_TEST(TestName) \
  INSTANTIATE_TEST_SUITE_P(                            \
      TestName,                                        \
      ReadFlagsVariantsTest,                           \
      testing::Values(kMachineOpReadFlagsWithOverflow, kMachineOpReadFlagsWithoutOverflow));

}  // namespace berberis::x86_64

#endif  // BERBERIS_BACKEND_X86_64_READ_FLAGS_VARIANTS_TEST_HELPER_H
