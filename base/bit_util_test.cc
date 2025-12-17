/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "berberis/base/bit_util.h"

#include "berberis/base/int.h"

namespace berberis {

namespace {

static_assert(IsPowerOf2(sizeof(void*)));
static_assert(!IsPowerOf2(sizeof(void*) + 1));

static_assert(IsPowerOf2(RawInt8(4)));
static_assert(IsPowerOf2(SatInt8(4)));
static_assert(IsPowerOf2(Int8(4)));

static_assert(AlignUp(6, 4) == 8);
static_assert(AlignUp<4>(6) == 8);
static_assert(AlignUp<4>(RawInt8(6)) == RawInt8(8));
static_assert(AlignUp<4>(SatInt8(6)) == SatInt8(8));
static_assert(AlignUp<4>(Int8(6)) == Int8(8));

static_assert(AlignDown(6, 4) == 4);
static_assert(AlignDown<4>(6) == 4);
static_assert(AlignDown<4>(RawInt8(6)) == RawInt8(4));
static_assert(AlignDown<4>(SatInt8(6)) == SatInt8(4));
static_assert(AlignDown<4>(Int8(6)) == Int8(4));

static_assert(IsAligned(6, 2));
static_assert(!IsAligned(6, 4));
static_assert(IsAligned<2>(6));
static_assert(!IsAligned<4>(6));
static_assert(IsAligned<2>(RawInt8(6)));
static_assert(!IsAligned<4>(RawInt8(6)));
static_assert(IsAligned<2>(SatInt8(6)));
static_assert(!IsAligned<4>(SatInt8(6)));
static_assert(IsAligned<2>(Int8(6)));
static_assert(!IsAligned<4>(Int8(6)));

static_assert(BitUtilLog2(1) == 0);
static_assert(BitUtilLog2(16) == 4);
static_assert(BitUtilLog2(sizeof(void*)) > 0);

static_assert(CountRZero(~uint32_t{1}) == 1);
static_assert(CountRZero(RawInt32{~UInt32{1}}) == RawInt32{1});
static_assert(CountRZero(SatUInt32{~Int32{1}}) == SatUInt32{1});
static_assert(CountRZero(~UInt32{1}) == UInt32{1});
static_assert(CountRZero(~uint64_t{1}) == 1);
static_assert(CountRZero(RawInt64{~UInt64{1}}) == RawInt64{1});
static_assert(CountRZero(SatUInt64{~Int64{1}}) == SatUInt64{1});
static_assert(CountRZero(~UInt64{1}) == UInt64{1});
#if defined(__x86_64__)
static_assert(CountRZero(~static_cast<unsigned __int128>(1) << 64) == 65);
static_assert(CountRZero(RawInt128{~UInt128{1}}) == RawInt128{1});
static_assert(CountRZero(SatUInt128{~Int128{1}}) == SatUInt128{1});
static_assert(CountRZero(~UInt128{1} << UInt128{64}) == UInt128{65});
#endif

static_assert(Popcount(~uint32_t{1}) == 31);
static_assert(Popcount(RawInt32{~UInt32{1}}) == RawInt32{31});
static_assert(Popcount(SatUInt32{~Int32{1}}) == SatUInt32{31});
static_assert(Popcount(~UInt32{1}) == UInt32{31});
static_assert(Popcount(~uint64_t{1}) == 63);
static_assert(Popcount(RawInt64{~UInt64{1}}) == RawInt64{63});
static_assert(Popcount(SatUInt64{~Int64{1}}) == SatUInt64{63});
static_assert(Popcount(~UInt64{1}) == UInt64{63});
#if defined(__x86_64__)
static_assert(Popcount(~static_cast<unsigned __int128>(1)) == 127);
static_assert(Popcount(RawInt128{~UInt128{1}}) == RawInt128{127});
static_assert(Popcount(SatUInt128{~Int128{1}}) == SatUInt128{127});
static_assert(Popcount(~UInt128{1}) == UInt128{127});
#endif

}  // namespace

}  // namespace berberis
