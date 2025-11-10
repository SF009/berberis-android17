/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef BERBERIS_BASE_ALGORITHM_H_
#define BERBERIS_BASE_ALGORITHM_H_

#include <algorithm>
#include <iterator>

namespace berberis {

//
// Non-const container versions.
//

template <typename ContainerType>
auto EraseFromReverseIterator(ContainerType& container,
                              typename ContainerType::reverse_iterator rit) {
  return std::reverse_iterator(container.erase(std::prev(rit.base())));
}

template <typename ContainerType, typename ValueType>
auto Find(ContainerType& container, const ValueType& value) {
  return std::find(container.begin(), container.end(), value);
}

//
// Const container versions.
//

template <typename ContainerType, typename ValueType>
auto Find(const ContainerType& container, const ValueType& value) {
  return std::find(container.begin(), container.end(), value);
}

template <typename ContainerType, typename ValueType>
bool Contains(const ContainerType& container, const ValueType& value) {
  return Find(container, value) != container.end();
}

template <typename ContainerType, typename Predicate>
auto FindIf(const ContainerType& container, Predicate predicate) {
  return std::find_if(container.begin(), container.end(), predicate);
}

template <typename ContainerType, typename Predicate>
bool ContainsIf(const ContainerType& container, Predicate predicate) {
  return FindIf(container, predicate) != container.end();
}

}  // namespace berberis

#endif  // BERBERIS_BASE_ALGORITHM_H_
