// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/CategorySelectorState.ts
// DanQing dqCommon — CategorySelectorState unit tests
//
// Tests based on itwinjs-core CategorySelectorState API behavior (addCategories /
// dropCategories / has / isCategoryViewed / clear / equalState).
#include "dqCommon/CategorySelectorState.h"
#include <dqBase/DqId.h>

#include <gtest/gtest.h>
#include <vector>

using namespace dqCommon;

// Ported from: itwinjs-core CategorySelectorState (addCategories/dropCategories/has).
TEST(CategorySelectorStateTest, AddDropContains)
{
    CategorySelectorState sel;
    EXPECT_TRUE(sel.isEmpty());

    sel.addCategory(dqBase::DqId(7));
    sel.addCategory(dqBase::DqId(9));
    EXPECT_EQ(sel.getCount(), 2u);
    EXPECT_TRUE(sel.containsCategory(dqBase::DqId(7)));
    EXPECT_TRUE(sel.has(dqBase::DqId(9)));   // itwinjs `has(id)`
    EXPECT_FALSE(sel.containsCategory(dqBase::DqId(11)));

    sel.dropCategory(dqBase::DqId(7));
    EXPECT_FALSE(sel.containsCategory(dqBase::DqId(7)));
    EXPECT_EQ(sel.getCount(), 1u);
}

// Ported from: itwinjs-core CategorySelectorState.categories.clear.
TEST(CategorySelectorStateTest, Clear)
{
    CategorySelectorState sel;
    sel.addCategory(dqBase::DqId(1));
    sel.addCategory(dqBase::DqId(2));
    sel.clear();
    EXPECT_TRUE(sel.isEmpty());
    EXPECT_EQ(sel.getCount(), 0u);
}

// Ported from: itwinjs-core CategorySelectorState.addCategories / dropCategories (plural, :80-89).
TEST(CategorySelectorStateTest, AddDropCategoriesPlural)
{
    CategorySelectorState sel;
    sel.addCategories({dqBase::DqId(1), dqBase::DqId(2), dqBase::DqId(3)});
    EXPECT_EQ(sel.getCount(), 3u);
    EXPECT_TRUE(sel.containsCategory(dqBase::DqId(2)));

    sel.dropCategories({dqBase::DqId(1), dqBase::DqId(2)});
    EXPECT_EQ(sel.getCount(), 1u);
    EXPECT_TRUE(sel.containsCategory(dqBase::DqId(3)));
}

// Ported from: itwinjs-core CategorySelectorState.addCategories (non-batched path raises
// one OnCategoryAdded event per id) — contrasts with AddCategoriesBatched.
TEST(CategorySelectorStateTest, AddCategoriesFiresPerIdEvents)
{
    CategorySelectorState sel;
    int perIdCount = 0;
    sel.OnCategoryAdded.AddListener([&](dqBase::DqId) { ++perIdCount; });
    sel.addCategories({dqBase::DqId(1), dqBase::DqId(2)});
    EXPECT_EQ(perIdCount, 2);
}

// Ported from: itwinjs-core CategorySelectorState.addCategoriesBatched (:94-96) — raises
// a SINGLE batch event (not N per-id events). This is the contract the Batched variant
// exists for (perf: full-stack-tests Viewport.test.ts:219-235 adds 50k categories).
TEST(CategorySelectorStateTest, AddCategoriesBatchedFiresSingleEvent)
{
    CategorySelectorState sel;
    int batchCount = 0;
    int batchSize = 0;
    int perIdCount = 0;
    sel.OnCategoriesBatchAdded.AddListener([&](std::vector<dqBase::DqId> const& added) {
        ++batchCount;
        batchSize = static_cast<int>(added.size());
    });
    sel.OnCategoryAdded.AddListener([&](dqBase::DqId) { ++perIdCount; });

    sel.addCategoriesBatched({dqBase::DqId(1), dqBase::DqId(2), dqBase::DqId(3)});
    EXPECT_EQ(batchCount, 1);   // single batch event
    EXPECT_EQ(batchSize, 3);
    EXPECT_EQ(perIdCount, 0);   // NOT the per-id path
    EXPECT_EQ(sel.getCount(), 3u);
}

// Ported from: itwinjs-core CategorySelectorState.dropCategoriesBatched (:101-103).
TEST(CategorySelectorStateTest, DropCategoriesBatchedFiresSingleEvent)
{
    CategorySelectorState sel;
    sel.addCategories({dqBase::DqId(1), dqBase::DqId(2), dqBase::DqId(3)});
    int batchCount = 0;
    sel.OnCategoriesBatchRemoved.AddListener([&](std::vector<dqBase::DqId> const&) { ++batchCount; });
    sel.dropCategoriesBatched({dqBase::DqId(1), dqBase::DqId(2)});
    EXPECT_EQ(batchCount, 1);
    EXPECT_EQ(sel.getCount(), 1u);
}

// Ported from: itwinjs-core CategorySelectorState.changeCategoryDisplay (:109-114).
TEST(CategorySelectorStateTest, ChangeCategoryDisplay)
{
    CategorySelectorState sel;
    sel.changeCategoryDisplay({dqBase::DqId(1), dqBase::DqId(2)}, /*add=*/true);
    EXPECT_EQ(sel.getCount(), 2u);
    sel.changeCategoryDisplay({dqBase::DqId(1)}, /*add=*/false);
    EXPECT_FALSE(sel.containsCategory(dqBase::DqId(1)));
    EXPECT_TRUE(sel.containsCategory(dqBase::DqId(2)));
}

// Ported from: itwinjs-core CategorySelectorState.equalState (:59-68).
TEST(CategorySelectorStateTest, EqualState)
{
    CategorySelectorState a, b, c;
    a.addCategories({dqBase::DqId(1), dqBase::DqId(2)});
    b.addCategories({dqBase::DqId(2), dqBase::DqId(1)});  // same set, different order
    c.addCategories({dqBase::DqId(1)});
    EXPECT_TRUE(a.equalState(b));     // same set
    EXPECT_FALSE(a.equalState(c));    // size differs
    c.addCategory(dqBase::DqId(3));   // same size, different element
    EXPECT_FALSE(a.equalState(c));
}
