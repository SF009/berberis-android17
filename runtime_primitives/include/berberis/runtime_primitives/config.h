/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef BERBERIS_RUNTIME_PRIMITIVES_CONFIG_H_
#define BERBERIS_RUNTIME_PRIMITIVES_CONFIG_H_

#include <cstdint>

#include "berberis/base/config.h"
#include "berberis/runtime_primitives/platform.h"

// Compile time configuration variables.
namespace berberis::config {

#ifdef NDEBUG
inline constexpr bool kIsDebug = false;
#else
inline constexpr bool kIsDebug = true;
#endif

// Optimize.
inline constexpr bool kOptimize = true;
// This is a hint for maximum number of concurrently live registers.
// TODO(levarum): It may be profitable to have different hints for different
// host register types.
// TODO(levarum): Hint for x86_32 needs tuning.
// Now its value is based on a single benchmark.
inline constexpr uint32_t kMaxLiveVRegsHint = host_platform::kIsX86_64 ? 14u : 8u;
// Checking IR may be quite expensive. So doing it only in debug-mode.
inline constexpr bool kCheckIR = kIsDebug;
// Emulate floating point behavior precisely.
inline constexpr bool kPreciseFloatingPointEmulation = false;
// Handle input NaNs precisely, according to Guest ISA.
inline constexpr bool kPreciseNaNOperationsHandling = kPreciseFloatingPointEmulation;
// Host page size, 4K on all platforms so far.
inline constexpr int32_t kHostPageSize = 4096;

}  // namespace berberis::config

#endif  // BERBERIS_RUNTIME_PRIMITIVES_CONFIG_H_
