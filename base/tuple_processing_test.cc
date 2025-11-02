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

#include "gtest/gtest.h"

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>

#include "berberis/base/tuple_processing.h"

namespace berberis {

namespace {

static_assert(std::is_same_v<TupleTypes<std::tuple<char, int&>>::Enumerate,
                             std::tuple<std::tuple<Value<std::size_t{0}>, char>,
                                        std::tuple<Value<std::size_t{1}>, int&>>>);

static_assert(std::is_same_v<TupleTypes<std::tuple<char, int&>, std::tuple<long&, float>>::Zip,
                             std::tuple<std::tuple<char, long&>, std::tuple<int&, float>>>);

static_assert(TupleValues::Enumerate(std::tuple{'a', 42}) ==
              std::tuple{std::tuple{0, 'a'}, std::tuple{1, 42}});

static_assert(TupleValues::Zip(std::tuple{'a', 42}, std::tuple{1ULL, 3.00}) ==
              std::tuple{std::tuple{'a', 1ULL}, std::tuple{42, 3.00}});

}  // namespace

}  // namespace berberis
