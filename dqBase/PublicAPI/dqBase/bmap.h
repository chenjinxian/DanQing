// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/bmap.h
// DanQing dqBase — bmap 容器
//
// 1:1 对齐 imodel-native bmap。
// 使用 Google cpp-btree 实现，比 std::map 更高效（缓存友好）。
#pragma once

#include "Export.h"
#include "bpair.h"
#include "btree/btree_map.h"
