// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/bvector.h
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/WString.h
// DanQing dqBase — 容器与字符串 thin wrapper
//
// 对齐 imodel-native 的 bvector/bmap/bset 模式：类型别名指向标准库，
// 消除 Qt 依赖，同时保留未来替换底层实现的能力。
#pragma once

#include "Export.h"
#include "bmap.h"
#include "bset.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

// --- 容器（对齐 imodel-native bvector / bmap / bset） ---
// §7.3: 默认有序容器用 btree（bmap/bset），不得用 std::map/std::set 替代。
template<typename T>                      using DqVector  = std::vector<T>;
template<typename K, typename V>          using DqMap     = bmap<K, V>;     // 有序 B-tree（对齐 imodel-native bmap）
template<typename K, typename V>          using DqHashMap = std::unordered_map<K, V>; // 显式哈希 map
template<typename T>                      using DqSet     = bset<T>;        // 有序 B-tree（对齐 imodel-native bset）

// --- 字符串（对齐 imodel-native Utf8String） ---
                                          using DqString     = std::string;    // UTF-8 编码
                                          using DqStringView = std::string_view;

// --- 整型（对齐 imodel-native，消除 Qt quint*/qint*） ---
                                          // uint8_t, uint16_t, uint32_t, uint64_t 来自 <cstdint>
                                          // int8_t,  int16_t,  int32_t,  int64_t  来自 <cstdint>

END_DQ_BASE_NAMESPACE
