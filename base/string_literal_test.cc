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

#include "berberis/base/string_literal.h"

#include <array>
#include <string>
#include <tuple>

#include "berberis/base/tuple_processing.h"

namespace berberis {

namespace {

static_assert(std::char_traits<char>::compare(StringLiteral("Test"), "Test", 5) == 0);

static_assert(std::char_traits<char>::compare(StringLiteral(std::to_array("Test")), "Test\0", 6) ==
              0);

static_assert(ToArray(tuple_cat(kGetTemplateName<bool>, std::array{'\0'})) ==
              std::to_array("bool"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123>>, std::array{'\0'})) ==
              std::to_array("123"));

#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wundefined-internal"

void TestFunction(void);

#ifdef __clang__
static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123U>>, std::array{'\0'})) ==
              std::to_array("123U"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123L>>, std::array{'\0'})) ==
              std::to_array("123L"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123UL>>, std::array{'\0'})) ==
              std::to_array("123UL"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<TestFunction>>, std::array{'\0'})) ==
              std::to_array("berberis::(anonymous namespace)::TestFunction"));
#else
static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123U>>, std::array{'\0'})) ==
              std::to_array("123"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123L>>, std::array{'\0'})) ==
              std::to_array("123"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<123UL>>, std::array{'\0'})) ==
              std::to_array("123"));

static_assert(ToArray(tuple_cat(kGetTemplateName<MetaValue<TestFunction>>, std::array{'\0'})) ==
              std::to_array("{anonymous}::TestFunction"));
#endif

}  // namespace

}  // namespace berberis
