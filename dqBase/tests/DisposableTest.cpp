// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Disposable.ts
//              DisposableList.dispose() (lines 185-187) — forward iteration.
#include "dqBase/Disposable.h"

#include <gtest/gtest.h>

#include <vector>

using namespace dqBase;

// Ref Disposable.ts:185-187:
//   public dispose(): void {
//     for (const disposable of this._disposables)
//       disposable.dispose();
//   }
// Iterates the backing array FORWARD (registration order). DanQing previously
// iterated LIFO (rbegin/rend) — a §5 behavioral divergence. This test pins the
// reference forward order.
// Ported from: itwinjs-core core/bentley/src/Disposable.ts
//              TEST(DisposableTest, DisposeRunsInForwardRegistrationOrder)
TEST(DisposableTest, DisposeRunsInForwardRegistrationOrder) {
    std::vector<int> order;
    DisposableList list;
    list.add([&order] { order.push_back(1); });
    list.add([&order] { order.push_back(2); });
    list.add([&order] { order.push_back(3); });

    list.Dispose();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);  // first-registered disposed first (ref forward order)
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// Ported from: itwinjs-core core/bentley/src/Disposable.ts
//              TEST(DisposableTest, DisposeClearsListAndIsEmptyReportsCorrectly)
TEST(DisposableTest, DisposeClearsListAndIsEmptyReportsCorrectly) {
    DisposableList list;
    EXPECT_TRUE(list.isEmpty());
    list.add([] {});
    list.add([] {});
    EXPECT_FALSE(list.isEmpty());
    list.Dispose();
    EXPECT_TRUE(list.isEmpty());
}

// Re-disposing an already-disposed list is a no-op (clear() leaves it empty).
// Ported from: itwinjs-core core/bentley/src/Disposable.ts
//              TEST(DisposableTest, RedisposeIsNoOp)
TEST(DisposableTest, RedisposeIsNoOp) {
    int count = 0;
    DisposableList list;
    list.add([&count] { ++count; });
    list.Dispose();
    list.Dispose();  // must not double-invoke
    EXPECT_EQ(count, 1);
}
