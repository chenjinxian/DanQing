// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/bvector.h
// DanQing dqBase — bvector 容器别名
//
// 1:1 对齐 imodel-native bvector。
#pragma once

#include <vector>

namespace dqBase {

/// bvector — imodel-native 兼容的 vector 别名
/// Ported from: imodel-native bvector.h
template<typename T, typename Alloc = std::allocator<T>>
using bvector = std::vector<T, Alloc>;

} // namespace dqBase
