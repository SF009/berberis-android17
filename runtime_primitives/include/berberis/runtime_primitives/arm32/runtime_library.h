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

#ifndef BERBERIS_RUNTIME_PRIMITIVES_ARM32_RUNTIME_LIBRARY_H_
#define BERBERIS_RUNTIME_PRIMITIVES_ARM32_RUNTIME_LIBRARY_H_

#include "berberis/runtime_primitives/host_code.h"
#include "berberis/runtime_primitives/runtime_library.h"

extern "C" {

void berberis_entry_Unpredictable();

__attribute__((used, __visibility__("hidden"))) void berberis_HandleUnpredictable(
    berberis::ThreadState* state);

}  // extern "C"

namespace berberis {

// Syntax sugar, cannot use constexpr because of reinterpret_cast.
inline const auto kEntryUnpredictable = AsHostCodeAddr(AsHostCode(berberis_entry_Unpredictable));

uint32_t GetFPEnvironment(uint32_t fpflags);
uint32_t SetFPEnvironment(uint32_t val);

}  // namespace berberis

#endif  // BERBERIS_RUNTIME_PRIMITIVES_ARM32_RUNTIME_LIBRARY_H_
