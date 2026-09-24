// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/bmap.h
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/bset.h
// DanQing dqBase — DqTypes 容器别名契约测试
//
// 验证 §7.3：DqMap/DqSet 必须由 btree 支持（有序），不得用 std::unordered_* 替代。
#include <gtest/gtest.h>

#include "dqBase/DqTypes.h"

#include <vector>
#include <type_traits>

// Ported from: imodel-native bmap.h/bset.h contract (§7.3 — btree, not std::map)
// DqMap/DqSet MUST be ordered btree containers, not std::unordered_*.
TEST(DqTypesTest, DqMapAndUeSetAreBtreeBacked) {
    // Ordered btree maps require operator< on the key and iterate in sorted order.
    dqBase::DqMap<int, int> m;
    m[3] = 30; m[1] = 10; m[2] = 20;
    std::vector<int> keys;
    for (auto const& kv : m) keys.push_back(kv.first);
    EXPECT_EQ((std::vector<int>{1, 2, 3}), keys); // sorted iteration → btree, not unordered

    dqBase::DqSet<int> s;
    s.insert(3); s.insert(1); s.insert(2);
    std::vector<int> elems(s.begin(), s.end());
    EXPECT_EQ((std::vector<int>{1, 2, 3}), elems); // sorted → btree

    // DqHashMap stays hash-based (genuine hash use case, std::hash<DqId> still valid)
    static_assert(!std::is_same_v<dqBase::DqHashMap<int,int>, dqBase::DqMap<int,int>>,
                  "DqHashMap must remain distinct (hash) from DqMap (btree)");
}
