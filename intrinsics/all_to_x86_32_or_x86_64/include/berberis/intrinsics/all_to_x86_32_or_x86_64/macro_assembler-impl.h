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

#ifndef BERBERIS_INTRINSICS_ALL_TO_X86_32_OR_X86_64_MACRO_ASSEMBLER_IMPL_H_
#define BERBERIS_INTRINSICS_ALL_TO_X86_32_OR_X86_64_MACRO_ASSEMBLER_IMPL_H_

namespace berberis {

template <typename Assembler, typename AssemblerBase, typename SpecificMacroAssembler>
template <typename Ti>
constexpr void
MacroAssemblerX86GuestAgnostic<Assembler, AssemblerBase, SpecificMacroAssembler>::ReverseBits(
    Register dst,
    Register src) {
  static_assert(std::is_integral_v<Ti> && std::is_unsigned_v<Ti> && sizeof(Ti) <= sizeof(uint32_t));
  using ImmType = std::make_signed_t<Ti>;
  Mov<Ti>(dst, src);
  Shr<Ti>(src, int8_t{1});
  Shl<Ti>(dst, int8_t{1});
  And<Ti>(src, static_cast<ImmType>(0x5555'5555));
  And<Ti>(dst, static_cast<ImmType>(0xaaaa'aaaa));
  Or<Ti>(dst, src);
  Mov<Ti>(src, dst);
  Shr<Ti>(src, int8_t{2});
  Shl<Ti>(dst, int8_t{2});
  And<Ti>(src, static_cast<ImmType>(0x3333'3333));
  And<Ti>(dst, static_cast<ImmType>(0xcccc'cccc));
  Or<Ti>(dst, src);
  Mov<Ti>(src, dst);
  Shr<Ti>(src, int8_t{4});
  Shl<Ti>(dst, int8_t{4});
  And<Ti>(src, static_cast<ImmType>(0x0f0f'0f0f));
  And<Ti>(dst, static_cast<ImmType>(0xf0f0'f0f0));
  Or<Ti>(dst, src);
  if constexpr (sizeof(Ti) == sizeof(uint16_t)) {
    Ror<Ti>(dst, 8);
  } else if constexpr (sizeof(Ti) == sizeof(uint32_t)) {
    Bswap<Ti>(dst);
  }
}

}  // namespace berberis

#endif  // BERBERIS_INTRINSICS_ALL_TO_X86_32_OR_X86_64_MACRO_ASSEMBLER_IMPL_H_
