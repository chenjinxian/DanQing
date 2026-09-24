// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/bpair.h
// DanQing dqBase — bpair 别名
//
// 1:1 对齐 imodel-native bpair。
#pragma once

#include <utility>

namespace dqBase {

/// bpair — imodel-native 兼容的 pair 别名
/// Ported from: imodel-native bpair.h
template<typename T1, typename T2>
using bpair = std::pair<T1, T2>;

/// make_bpair — imodel-native 兼容的 make_pair
template<typename T1, typename T2>
auto make_bpair(T1 t1, T2 t2) { return std::make_pair(t1, t2); }

} // namespace dqBase
