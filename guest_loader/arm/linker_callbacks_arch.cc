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

#include "berberis/guest_loader/guest_loader.h"
#include "berberis/tiny_loader/loaded_elf_file.h"

#include <string>

#include "guest_loader_impl.h"

namespace berberis {

bool InitializeLinkerCallbacksArch(LinkerCallbacks* linker_callbacks,
                                   const LoadedElfFile& linker_elf_file,
                                   std::string* error_msg) {
  // This callback is specific to ARM32. ARM32 uses a different unwinding ABI from most other
  // architectures, which rely on standardized DWARF eh_frame.
  return FindSymbol(linker_elf_file,
                    "__loader_dl_unwind_find_exidx",
                    &linker_callbacks->dl_unwind_find_exidx_fn_,
                    error_msg);
}

}  // namespace berberis
