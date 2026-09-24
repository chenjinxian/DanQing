// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionSet and HiliteSet tests
// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
#include <gtest/gtest.h>

#include <dqApp/SelectionSet.h>

using namespace dqApp;

// ---------------------------------------------------------------------------
// SelectionSet tests
// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
// ---------------------------------------------------------------------------
TEST(SelectionSetTest, DefaultIsEmpty)
{
    SelectionSet sel;
    EXPECT_TRUE(sel.isEmpty());
    EXPECT_EQ(sel.size(), 0);
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, AddAndContains)
TEST(SelectionSetTest, AddAndContains)
{
    SelectionSet sel;
    QSet<uint32_t> ids;
    ids.insert(1);
    ids.insert(2);
    ids.insert(3);

    sel.add(ids);

    EXPECT_EQ(sel.size(), 3);
    EXPECT_TRUE(sel.Contains(1));
    EXPECT_TRUE(sel.Contains(2));
    EXPECT_TRUE(sel.Contains(3));
    EXPECT_FALSE(sel.Contains(4));
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, AddRaisesOnChanged)
TEST(SelectionSetTest, AddRaisesOnChanged)
{
    SelectionSet sel;
    int changeCount = 0;
    dqBase::DqEventScope scope;  // RAII 断连（同 BlankConnectionTest 修正）
    scope.add(sel.OnChanged.AddListener(
        [&changeCount](void*) { changeCount++; }));

    QSet<uint32_t> ids;
    ids.insert(1);
    sel.add(ids);

    EXPECT_EQ(changeCount, 1);
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, RemoveAndContains)
TEST(SelectionSetTest, RemoveAndContains)
{
    SelectionSet sel;
    QSet<uint32_t> ids;
    ids.insert(1);
    ids.insert(2);
    ids.insert(3);
    sel.add(ids);

    QSet<uint32_t> toRemove;
    toRemove.insert(2);
    sel.Remove(toRemove);

    EXPECT_EQ(sel.size(), 2);
    EXPECT_TRUE(sel.Contains(1));
    EXPECT_FALSE(sel.Contains(2));
    EXPECT_TRUE(sel.Contains(3));
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, ReplaceClearsOld)
TEST(SelectionSetTest, ReplaceClearsOld)
{
    SelectionSet sel;
    QSet<uint32_t> ids1;
    ids1.insert(1);
    ids1.insert(2);
    sel.add(ids1);

    QSet<uint32_t> ids2;
    ids2.insert(10);
    ids2.insert(20);
    sel.Replace(ids2);

    EXPECT_EQ(sel.size(), 2);
    EXPECT_FALSE(sel.Contains(1));
    EXPECT_FALSE(sel.Contains(2));
    EXPECT_TRUE(sel.Contains(10));
    EXPECT_TRUE(sel.Contains(20));
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, EmptyAllClears)
TEST(SelectionSetTest, EmptyAllClears)
{
    SelectionSet sel;
    QSet<uint32_t> ids;
    ids.insert(1);
    ids.insert(2);
    sel.add(ids);

    sel.EmptyAll();

    EXPECT_TRUE(sel.isEmpty());
    EXPECT_EQ(sel.size(), 0);
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, EmptyAllNoOpOnEmpty)
TEST(SelectionSetTest, EmptyAllNoOpOnEmpty)
{
    SelectionSet sel;
    int changeCount = 0;
    dqBase::DqEventScope scope;  // RAII 断连（同 BlankConnectionTest 修正）
    scope.add(sel.OnChanged.AddListener(
        [&changeCount](void*) { changeCount++; }));

    sel.EmptyAll();

    // Should not raise event if already empty
    EXPECT_EQ(changeCount, 0);
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(SelectionSetTest, RemoveRaisesOnChanged)
TEST(SelectionSetTest, RemoveRaisesOnChanged)
{
    SelectionSet sel;
    QSet<uint32_t> ids;
    ids.insert(1);
    sel.add(ids);

    int changeCount = 0;
    dqBase::DqEventScope scope;  // RAII 断连（同 BlankConnectionTest 修正）
    scope.add(sel.OnChanged.AddListener(
        [&changeCount](void*) { changeCount++; }));

    QSet<uint32_t> toRemove;
    toRemove.insert(1);
    sel.Remove(toRemove);

    EXPECT_EQ(changeCount, 1);
}

// ---------------------------------------------------------------------------
// HiliteSet tests
// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
// ---------------------------------------------------------------------------
TEST(HiliteSetTest, DefaultIsEmpty)
{
    HiliteSet hilite;
    EXPECT_TRUE(hilite.GetElements().isEmpty());
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(HiliteSetTest, SyncWithSelectionSet)
TEST(HiliteSetTest, SyncWithSelectionSet)
{
    SelectionSet sel;
    QSet<uint32_t> ids;
    ids.insert(1);
    ids.insert(2);
    sel.add(ids);

    HiliteSet hilite;
    hilite.SyncWith(sel);

    EXPECT_EQ(hilite.GetElements().size(), 2);
    EXPECT_TRUE(hilite.Contains(1));
    EXPECT_TRUE(hilite.Contains(2));
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(HiliteSetTest, SyncReflectsChanges)
TEST(HiliteSetTest, SyncReflectsChanges)
{
    SelectionSet sel;
    HiliteSet hilite;

    QSet<uint32_t> ids;
    ids.insert(1);
    sel.add(ids);
    hilite.SyncWith(sel);

    EXPECT_TRUE(hilite.Contains(1));

    QSet<uint32_t> toRemove;
    toRemove.insert(1);
    sel.Remove(toRemove);
    hilite.SyncWith(sel);

    EXPECT_FALSE(hilite.Contains(1));
}

// Ported from: itwinjs-core core/frontend/src/test/SelectionSet.test.ts
//              TEST(HiliteSetTest, ClearRemovesAll)
TEST(HiliteSetTest, ClearRemovesAll)
{
    SelectionSet sel;
    QSet<uint32_t> ids;
    ids.insert(1);
    ids.insert(2);
    sel.add(ids);

    HiliteSet hilite;
    hilite.SyncWith(sel);
    hilite.clear();

    EXPECT_TRUE(hilite.GetElements().isEmpty());
}
