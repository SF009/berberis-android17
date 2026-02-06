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

#include "berberis/runtime_primitives/checks.h"

namespace berberis {

// Guest address for wrapped host function must be valid ARM (aligned on 4)
// address, otherwise guest simply can't encode it to call by immediate. We
// are unlikely affected, as calling external symbol by immediate requires text
// relocation, but still issue an error.
bool IsProgramCounterProperlyAlignedForArch(GuestAddr pc) {
  return (pc & 0b11) == 0b00;
}

}  // namespace berberis
