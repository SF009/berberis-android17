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

#include "guest_loader_impl.h"

namespace berberis {

// Paths required by guest_loader_impl.h.
const char* kAppProcessRelativePath = "riscv64/app_process64";
const char* kPtInterpRelativePath = "riscv64/linker64";
const char* kVdsoRelativePath = "riscv64/libnative_bridge_vdso.so";
const char* kProxyPrefix = "libberberis_proxy_";

}  // namespace berberis
