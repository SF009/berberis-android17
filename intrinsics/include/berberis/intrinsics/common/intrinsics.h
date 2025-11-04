/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef BERBERIS_INTRINSICS_COMMON_INTRINSICS_H_
#define BERBERIS_INTRINSICS_COMMON_INTRINSICS_H_

#include <bit>
#include <cstdint>
#include <type_traits>

#include "berberis/base/checks.h"
#include "berberis/base/dependent_false.h"
#include "berberis/base/tuple_processing.h"
#include "berberis/intrinsics/common/intrinsics_float.h"  // Float16/Float32/Float64

namespace berberis {

class SIMD128Register;

namespace intrinsics {

enum TemplateTypeId : uint8_t {
  kInt8T = 1,
  kUInt8T = 0,
  kInt16T = 3,
  kUInt16T = 2,
  kInt32T = 5,
  kUInt32T = 4,
  kInt64T = 7,
  kUInt64T = 6,
  kFloat16 = 10,
  kFloat32 = 12,
  kFloat64 = 14,
  kSIMD128Register = 16,
};

constexpr TemplateTypeId TemplateTypeIdToFloat(TemplateTypeId value) {
  CHECK(value >= kUInt16T && value <= kInt64T);
  return TemplateTypeId{static_cast<uint8_t>((value & 0x6) + 8)};
}

constexpr TemplateTypeId TemplateTypeIdToInt(TemplateTypeId value) {
  CHECK((value >= kFloat16 && value <= kFloat64) && !(value & 1));
  return TemplateTypeId{static_cast<uint8_t>(value - 8)};
}

constexpr TemplateTypeId TemplateTypeIdToNarrow(TemplateTypeId value) {
  CHECK((value >= kUInt16T && value <= kInt64T) ||
        ((value >= kFloat32 && value <= kFloat64) && !(value & 1)));
  return TemplateTypeId{static_cast<uint8_t>(value - 2)};
}

constexpr TemplateTypeId TemplateTypeIdToSigned(TemplateTypeId value) {
  CHECK_LE(value, kInt64T);
  return TemplateTypeId{static_cast<uint8_t>(value | 1)};
}

constexpr int TemplateTypeIdSizeOf(TemplateTypeId value) {
  if (value == kSIMD128Register) {
    return 16;
  }
  return 1 << ((value & 0b110) >> 1);
}

constexpr TemplateTypeId TemplateTypeIdToUnsigned(TemplateTypeId value) {
  CHECK_LE(value, kInt64T);
  return TemplateTypeId{static_cast<uint8_t>(value & ~1)};
}

constexpr TemplateTypeId TemplateTypeIdToWide(TemplateTypeId value) {
  CHECK(value <= kInt32T || ((value >= kFloat16 && value <= kFloat32) && !(value & 1)));
  return TemplateTypeId{static_cast<uint8_t>(value + 2)};
}

template <typename Type>
constexpr TemplateTypeId IdFromType() {
  if constexpr (std::is_same_v<int8_t, std::decay_t<Type>> ||
                std::is_same_v<Int8, std::decay_t<Type>>) {
    return TemplateTypeId::kInt8T;
  } else if constexpr (std::is_same_v<uint8_t, std::decay_t<Type>> ||
                       std::is_same_v<UInt8, std::decay_t<Type>>) {
    return TemplateTypeId::kUInt8T;
  } else if constexpr (std::is_same_v<int16_t, std::decay_t<Type>> ||
                       std::is_same_v<Int16, std::decay_t<Type>>) {
    return TemplateTypeId::kInt16T;
  } else if constexpr (std::is_same_v<uint16_t, std::decay_t<Type>> ||
                       std::is_same_v<UInt16, std::decay_t<Type>>) {
    return TemplateTypeId::kUInt16T;
  } else if constexpr (std::is_same_v<int32_t, std::decay_t<Type>> ||
                       std::is_same_v<Int32, std::decay_t<Type>>) {
    return TemplateTypeId::kInt32T;
  } else if constexpr (std::is_same_v<uint32_t, std::decay_t<Type>> ||
                       std::is_same_v<UInt32, std::decay_t<Type>>) {
    return TemplateTypeId::kUInt32T;
  } else if constexpr (std::is_same_v<int64_t, std::decay_t<Type>> ||
                       std::is_same_v<Int64, std::decay_t<Type>>) {
    return TemplateTypeId::kInt64T;
  } else if constexpr (std::is_same_v<uint64_t, std::decay_t<Type>> ||
                       std::is_same_v<UInt64, std::decay_t<Type>>) {
    return TemplateTypeId::kUInt64T;
  } else if constexpr (std::is_same_v<Float16, std::decay_t<Type>>) {
    return TemplateTypeId::kFloat16;
  } else if constexpr (std::is_same_v<Float32, std::decay_t<Type>>) {
    return TemplateTypeId::kFloat32;
  } else if constexpr (std::is_same_v<Float64, std::decay_t<Type>>) {
    return TemplateTypeId::kFloat64;
  } else if constexpr (std::is_same_v<SIMD128Register, std::decay_t<Type>>) {
    return TemplateTypeId::kSIMD128Register;
  } else {
    static_assert(kDependentTypeFalse<Type>);
  }
}

template <typename Type>
constexpr TemplateTypeId kIdFromType = IdFromType<Type>();

constexpr TemplateTypeId IntSizeToTemplateTypeId(uint8_t size, bool is_signed = false) {
  CHECK(std::has_single_bit(size));
  CHECK_LT(size, 16);
  return static_cast<TemplateTypeId>((std::countr_zero(size) << 1) + is_signed);
}

template <enum TemplateTypeId>
class TypeFromIdHelper;

template <enum TemplateTypeId>
class WrappedTypeFromIdHelper;

#pragma push_macro("DEFINE_TEMPLATE_TYPE_FROM_ENUM")
#undef DEFINE_TEMPLATE_TYPE_FROM_ENUM
#define DEFINE_TEMPLATE_TYPE_FROM_ENUM(kEnumValue, TemplateType, WrappedTemplateType) \
  template <>                                                                         \
  class TypeFromIdHelper<kEnumValue> {                                                \
   public:                                                                            \
    using Type = TemplateType;                                                        \
  };                                                                                  \
  template <>                                                                         \
  class WrappedTypeFromIdHelper<kEnumValue> {                                         \
   public:                                                                            \
    using Type = WrappedTemplateType;                                                 \
  }

DEFINE_TEMPLATE_TYPE_FROM_ENUM(kInt8T, int8_t, Int8);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kUInt8T, uint8_t, UInt8);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kInt16T, int16_t, Int16);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kUInt16T, uint16_t, UInt16);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kInt32T, int32_t, Int32);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kUInt32T, uint32_t, UInt32);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kInt64T, int64_t, Int64);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kUInt64T, uint64_t, UInt64);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kFloat16, Float16, Float16);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kFloat32, Float32, Float32);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kFloat64, Float64, Float64);
DEFINE_TEMPLATE_TYPE_FROM_ENUM(kSIMD128Register, SIMD128Register, SIMD128Register);

#pragma pop_macro("DEFINE_TEMPLATE_TYPE_FROM_ENUM")

template <enum TemplateTypeId kEnumValue>
using TypeFromId = TypeFromIdHelper<kEnumValue>::Type;

template <enum TemplateTypeId kEnumValue>
using WrappedTypeFromId = WrappedTypeFromIdHelper<kEnumValue>::Type;

#pragma push_macro("DEFINE_VALUE_FUNCTION")
#undef DEFINE_VALUE_FUNCTION
#define DEFINE_VALUE_FUNCTION(FunctionName)                                   \
  template <TemplateTypeId ValueParam>                                        \
  constexpr Value<FunctionName(ValueParam)> FunctionName(Value<ValueParam>) { \
    return {};                                                                \
  }

DEFINE_VALUE_FUNCTION(TemplateTypeIdToFloat)
DEFINE_VALUE_FUNCTION(TemplateTypeIdToInt)
DEFINE_VALUE_FUNCTION(TemplateTypeIdToNarrow)
DEFINE_VALUE_FUNCTION(TemplateTypeIdToSigned)
DEFINE_VALUE_FUNCTION(TemplateTypeIdSizeOf)
DEFINE_VALUE_FUNCTION(TemplateTypeIdToUnsigned)
DEFINE_VALUE_FUNCTION(TemplateTypeIdToWide)

#pragma pop_macro("DEFINE_VALUE_FUNCTION")

// Note: this is very simple demultiplexer and it's NOT guaranteed to always work (especially if
// someone would use it with more than 8 parameters), but it would start producing collisions then
// we wouldn't really have any runtime issues because we use these values in a switch – and in C++
// an attempt to have to different case's in a switch is a compile-time error.
template <typename... Param>
constexpr int TrivialDemultiplexer(Param... param) {
  int variant_index = 0;
  int index = 0;
  ((variant_index ^= param << index, index += 4), ...);
  return variant_index;
}

constexpr int8_t kDemultiplexerInfoIgnore = -128;
constexpr int8_t kDemultiplexerInfoCountRZero = 32;

template <size_t kVariantSize>
struct DemultiplexerInfoData {
  int data_bits;
  // kDemultiplexerInfoIgnore, kDemultiplexerInfoCountRZero or shift: positive means left shift,
  // negative for right shift.
  int8_t bitop[kVariantSize];
  constexpr int operator()(const int array[kVariantSize]) const {
    int variant_index = 0;
    for (size_t index = 0; index < kVariantSize; ++index) {
      variant_index ^= bitop[index] == kDemultiplexerInfoIgnore ? 0
                       : bitop[index] == kDemultiplexerInfoCountRZero
                           ? std::countr_zero(static_cast<unsigned>(array[index]))
                       : bitop[index] >= 0 ? array[index] << bitop[index]
                                           : array[index] >> -bitop[index];
    }
    return variant_index & ((1 << data_bits) - 1);
  }
};

template <auto kDemultiplexerInfoData>
class DemultiplexerInfo;

template <size_t kVariantSize, const DemultiplexerInfoData<kVariantSize> kDemultiplexerInfoData>
class DemultiplexerInfo<kDemultiplexerInfoData> {
 public:
  template <typename... Param>
  constexpr int operator()(Param... param) const {
    int array[kVariantSize] = {param...};
    return kDemultiplexerInfoData(array);
  }
};

template <auto kDemultiplexerInfoData>
inline constexpr DemultiplexerInfo<kDemultiplexerInfoData> kDemultiplexerInfo{};

// Generates DemultipxexerInfoData for the given variants.
//
// The variants are given as a 2D array of integers, where the first dimension represents the
// variants and the second dimension represents the unique variants possibilities. The function
// will generate a DemultipxexerInfoData that can be used to  demultiplex the variants into a single
// value.
//
// The function will try to generate the smallest possible DemultipxexerInfoData that can
// distinguish all the variants. The function will also try to minimize  the number of bits used in
// the DemultipxexerInfoData.
//
// The function will return a DemultipxexerInfoData that can distinguish all the variants, or it
// will print an error message if it is not possible to distinguish all the variants.
//
// An attempt to print an arror message in a constexpr context (which is the typical use of that
// function) is a compile-time error.
//
// Example:
//
//   constexpr int variants[][3] = {
//     { 1, 1, 1 },
//     { 1, 2, 2 },
//     { 1, 1, 4 },
//     { 1, 2, 8 },
//     { 2, 1, 1 },
//     { 2, 2, 2 },
//     { 2, 1, 4 },
//     { 2, 2, 8 },
//   };
//
//   constexpr DemultiplexerInfoData data = GenerateDemultiplexerCoefficients(variants);
//
//   data.data_bits == 3
//   data.bitop[0] == 1
//   data.bitop[1] == kDemultiplexerInfoIgnore
//   data.bitop[2] == kDemultiplexerInfoCountRZero
//
// This DemultipxexerInfoData can be used to demultiplex the variants into a
// single value. For example:
//
//   int value = kDemultiplexerInfo<data>(1, 2, 8);
//
// This will set value to 1.

// A helper function that returns the maximum bucket size for the given index. The maximum bucket
// size is the number of variants where different possible values (at a given index) are merged
// into the same “demultiplexed” value.
//
// Out goal is to ensure that maximum bucket size is one: that means that we have a perfect hash.
template <size_t kVariantsCount, size_t kVariantSize>
constexpr size_t GenerateDemultiplexerCoefficients_GetMaxBucketSize(
    const DemultiplexerInfoData<kVariantSize>& result,
    const int (&kVariants)[kVariantsCount][kVariantSize],
    size_t index) {
  // A helper struct that stores information about a bucket of values.
  struct BucketInfo {
    size_t count;
    int values[kVariantsCount];
  };

  // The maximum number of bits that can be used in a DemultipxexerInfoData.
  constexpr int8_t kMaxDataBits = 8;

  size_t max_bucket_size = 0;
  BucketInfo buckets_info[1 << kMaxDataBits]{};
  bool power_of_two = true;
  for (auto& kVariant : kVariants) {
    int bucket = result(kVariant);
    size_t count = buckets_info[bucket].count;
    int scan_value = kVariant[index];
    size_t id;
    for (id = 0; id < count; ++id) {
      if (buckets_info[bucket].values[id] == scan_value) {
        break;
      }
    }
    if (id == count) {
      buckets_info[bucket].values[count++] = scan_value;
      if (scan_value == 0 || ((scan_value & (scan_value - 1)) != 0)) {
        power_of_two = false;
      }
      if (buckets_info[bucket].count = count; count > max_bucket_size) {
        max_bucket_size = count;
      }
    }
  }
  // Power of two values would take this much if we wouldn't use countr_zero
  if (power_of_two) {
    return (1 << max_bucket_size) - 1;
  }
  return max_bucket_size;
}

template <size_t kVariantsCount, size_t kVariantSize>
constexpr std::optional<size_t> GenerateDemultiplexerCoefficients_GetIndexToSplitIfNotPerfectHash(
    const DemultiplexerInfoData<kVariantSize>& result,
    const int (&kVariants)[kVariantsCount][kVariantSize]) {
  size_t max_bucket_size = 1;
  size_t max_bucket_index = 0;
  for (size_t index = 0; index < kVariantSize; ++index) {
    if (result.bitop[index] != kDemultiplexerInfoIgnore) {
      continue;
    }
    size_t bucket_size =
        GenerateDemultiplexerCoefficients_GetMaxBucketSize(result, kVariants, index);
    if (bucket_size > max_bucket_size) {
      max_bucket_size = bucket_size;
      max_bucket_index = index;
    }
  }
  if (max_bucket_size == 1) {
    return {};
  }
  return max_bucket_index;
}

template <size_t kVariantsCount, size_t kVariantSize>
constexpr std::optional<int8_t> GenerateDemultiplexerCoefficients_GetBitOperationForPerfectHash(
    DemultiplexerInfoData<kVariantSize> result,
    const int (&kVariants)[kVariantsCount][kVariantSize],
    size_t index) {
  int8_t bitop;
  // Start with no shift, then try positive ones.
  for (bitop = 0; bitop < 32; ++bitop) {
    result.bitop[index] = bitop;
    if (GenerateDemultiplexerCoefficients_GetMaxBucketSize(result, kVariants, index) == 1) {
      return bitop;
    }
  }

  // Positive bitop haven't worked.
  for (bitop = -1; bitop > -32; --bitop) {
    result.bitop[index] = bitop;
    if (GenerateDemultiplexerCoefficients_GetMaxBucketSize(result, kVariants, index) == 1) {
      return bitop;
    }
  }

  // Last resort. Try countr_zero.
  result.bitop[index] = kDemultiplexerInfoCountRZero;
  if (GenerateDemultiplexerCoefficients_GetMaxBucketSize(result, kVariants, index) == 1) {
    return kDemultiplexerInfoCountRZero;
  }
  return {};
}

template <size_t kVariantsCount, size_t kVariantSize>
constexpr DemultiplexerInfoData<kVariantSize> GenerateDemultiplexerCoefficients(
    const int (&kVariants)[kVariantsCount][kVariantSize]) {
  // Initialize the DemultipxexerInfoData with all bitop set to
  // kDemultipxexerInfoIgnore.
  DemultiplexerInfoData<kVariantSize> result;
  result.data_bits = 0;
  for (size_t index = 0; index < kVariantSize; ++index) {
    result.bitop[index] = kDemultiplexerInfoIgnore;
  }

  // The maximum number of bits that can be used in a DemultipxexerInfoData.
  constexpr int8_t kMaxDataBits = 8;

  for (result.data_bits = 0; result.data_bits <= kMaxDataBits; result.data_bits++) {
    bool perfect_hash = false;
    for (;;) {
      std::optional<size_t> index_to_split =
          GenerateDemultiplexerCoefficients_GetIndexToSplitIfNotPerfectHash(result, kVariants);
      if (!index_to_split.has_value()) {
        perfect_hash = true;
        break;
      }
      std::optional<int8_t> bitop = GenerateDemultiplexerCoefficients_GetBitOperationForPerfectHash(
          result, kVariants, *index_to_split);
      if (!bitop.has_value()) {
        break;
      }
      result.bitop[*index_to_split] = *bitop;
    }
    if (perfect_hash) {
      return result;
    }
  }
  printf("Couldn't find strategy without unsing more than %d bits.", kMaxDataBits);
  // Note: that code is never actually executed.
  return result;
}

// A solution for the inability to call generic implementation from specialization.
// Declaration:
//   template <typename Type,
//             int size,
//             enum PreferredIntrinsicsImplementation = kUseAssemblerImplementationIfPossible>
//   inline std::tuple<SIMD128Register> VectorMultiplyByScalarInt(SIMD128Register op1,
//                                                                SIMD128Register op2);
// Normal use only specifies two arguments, e.g. VectorMultiplyByScalarInt<uint32_t, 2>,
// but assembler implementation can (if SSE 4.1 is not available) do the following call:
//   return VectorMultiplyByScalarInt<uint32_t, 2, kUseCppImplementation>(in0, in1);
//
// Because PreferredIntrinsicsImplementation argument has non-default value we have call to the
// generic C-based implementation here.

enum PreferredIntrinsicsImplementation {
  kUseAssemblerImplementationIfPossible,
  kUseCppImplementation
};

}  // namespace intrinsics

// If we carry TemplateTypeId then we can do the exact same manipulations with it as with
// the normal value, but also can get the actual type from it and do the appropriate operations:
// make signed, make unsigned, widen, narrow, etc.
template <intrinsics::TemplateTypeId ValueParam>
class MetaValue<ValueParam> {
 public:
  using Type = intrinsics::TypeFromId<ValueParam>;
  using WrappedType = intrinsics::WrappedTypeFromId<ValueParam>;
  using ValueType = intrinsics::TemplateTypeId;
  static constexpr auto kValue = ValueParam;
  constexpr operator ValueType() const { return kValue; }
};

}  // namespace berberis

#endif  // BERBERIS_INTRINSICS_COMMON_INTRINSICS_H_
