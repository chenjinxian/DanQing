// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/ModelSelectorState.ts
// DanQing dqCommon — ModelSelectorState unit tests
//
// Tests based on itwinjs-core ModelSelectorState API (addModels / dropModels /
// containsModel / equalState) + full-stack-tests/core/src/frontend/standalone/
// ModelState.test.ts:32-49 (plural add + set-dedupe).
#include "dqCommon/ModelSelectorState.h"
#include <dqBase/DqId.h>

#include <gtest/gtest.h>
#include <vector>

using namespace dqCommon;

// Ported from: itwinjs-core ModelSelectorState.addModels / dropModels (plural, :74-83).
TEST(ModelSelectorStateTest, AddDropModelsPlural)
{
    ModelSelectorState sel;
    sel.addModels({dqBase::DqId(1), dqBase::DqId(2), dqBase::DqId(3)});
    EXPECT_EQ(sel.getCount(), 3u);
    EXPECT_TRUE(sel.containsModel(dqBase::DqId(2)));

    sel.dropModels({dqBase::DqId(1), dqBase::DqId(2)});
    EXPECT_EQ(sel.getCount(), 1u);
    EXPECT_TRUE(sel.containsModel(dqBase::DqId(3)));
}

// Ported from: full-stack-tests ModelState.test.ts:32-49 — addModels with a duplicate in
// the arg list dedupes (set semantics).
TEST(ModelSelectorStateTest, AddModelsDedupes)
{
    ModelSelectorState sel;
    sel.addModel(dqBase::DqId(1));
    sel.addModels({dqBase::DqId(2), dqBase::DqId(2), dqBase::DqId(3)});  // (2) duplicated
    EXPECT_EQ(sel.getCount(), 3u);  // {1, 2, 3}
}

// Ported from: itwinjs-core ModelSelectorState.equalState (:62-71).
TEST(ModelSelectorStateTest, EqualState)
{
    ModelSelectorState a, b, c;
    a.addModels({dqBase::DqId(1), dqBase::DqId(2)});
    b.addModels({dqBase::DqId(2), dqBase::DqId(1)});  // same set, different order
    c.addModels({dqBase::DqId(1)});
    EXPECT_TRUE(a.equalState(b));     // same set
    EXPECT_FALSE(a.equalState(c));    // size differs
    c.addModel(dqBase::DqId(3));      // same size, different element
    EXPECT_FALSE(a.equalState(c));
}
