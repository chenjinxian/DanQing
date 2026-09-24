// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/bset.h
// DanQing dqBase — bset 容器
//
// 1:1 对齐 imodel-native bset。
// 使用 Google cpp-btree 实现，比 std::set 更高效（缓存友好）。
#pragma once

#include "Export.h"
#include "btree/btree_set.h"
