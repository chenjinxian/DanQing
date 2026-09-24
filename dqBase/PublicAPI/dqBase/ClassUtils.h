// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/ClassUtils.ts
// DanQing dqBase — 类工具函数
//
// 1:1 对齐 itwinjs-core ClassUtils。
#pragma once

#include "Export.h"

#include <type_traits>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 类型检查工具
// Ported from: itwinjs-core ClassUtils.ts
// ---------------------------------------------------------------------------

/// 检查 Sub 是否为 Base 的真子类（不包括自身）
/// Ported from: itwinjs-core isProperSubclassOf
template<typename Sub, typename Base>
constexpr bool IsProperSubclassOf() noexcept {
    return std::is_base_of_v<Base, Sub> && !std::is_same_v<Sub, Base>;
}

/// 检查 Sub 是否为 Base 的子类（包括自身）
/// Ported from: itwinjs-core isSubclassOf
template<typename Sub, typename Base>
constexpr bool IsSubclassOf() noexcept {
    return std::is_base_of_v<Base, Sub>;
}

END_DQ_BASE_NAMESPACE
