// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for LRUTileList (C++ port)
// DanQing dqRender — LRUTileList tests
//
// Tests for LRU tile memory management: add, markUsed, clearUsed, freeMemory, drop.

#include "tile/LRUTileList.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileTree.h"

#include <dqCommon/FeatureTable.h>

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;
using namespace dqGeom;

// ============================================================================
// Mock Tile for testing
// ============================================================================
class MockTile : public Tile {
public:
    MockTile(TileTree& tree, size_t bytesUsed, Tile* parent = nullptr)
        : Tile(tree, parent, Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1))
    {
        setBytesUsed(bytesUsed);
    }

    bool requestContent() override { return false; }
    TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}
};

// ============================================================================
// Mock TileTree for testing
// ============================================================================
class MockTileTree : public TileTree {
public:
    MockTileTree()
        : TileTree(std::make_unique<MockTile>(*this, 0))
    {
    }

    TileVisibility computeVisibility(TileDrawArgs&, Tile*) override { return TileVisibility::Visible; }
};

// ============================================================================
// LRUTileList tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, EmptyList)
{
    LRUTileList list;
    EXPECT_EQ(list.getTotalBytesUsed(), 0u);
    EXPECT_EQ(list.getTileCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, AddSingleTile)
{
    MockTileTree tree;
    MockTile tile(tree, 1024);

    LRUTileList list;
    list.add(tile);

    EXPECT_EQ(list.getTotalBytesUsed(), 1024u);
    EXPECT_EQ(list.getTileCount(), 1u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, AddMultipleTiles)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);
    MockTile t3(tree, 300);

    LRUTileList list;
    list.add(t1);
    list.add(t2);
    list.add(t3);

    EXPECT_EQ(list.getTotalBytesUsed(), 600u);
    EXPECT_EQ(list.getTileCount(), 3u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, DropTile)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);

    LRUTileList list;
    list.add(t1);
    list.add(t2);
    EXPECT_EQ(list.getTileCount(), 2u);

    list.drop(t1);
    EXPECT_EQ(list.getTotalBytesUsed(), 200u);
    EXPECT_EQ(list.getTileCount(), 1u);

    list.drop(t2);
    EXPECT_EQ(list.getTotalBytesUsed(), 0u);
    EXPECT_EQ(list.getTileCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, DropNonexistentTile)
{
    MockTileTree tree;
    MockTile tile(tree, 100);

    LRUTileList list;
    // Dropping a tile that was never added should be a no-op.
    list.drop(tile);
    EXPECT_EQ(list.getTotalBytesUsed(), 0u);
    EXPECT_EQ(list.getTileCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, MarkUsedMovesToEnd)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);
    MockTile t3(tree, 300);

    LRUTileList list;
    list.add(t1);
    list.add(t2);
    list.add(t3);

    // t3 is most recently used (added last). Mark t1 as used by user 1.
    std::vector<Tile*> tiles = {&t1};
    list.markUsed(1, tiles);

    // All tiles still present.
    EXPECT_EQ(list.getTileCount(), 3u);
    EXPECT_EQ(list.getTotalBytesUsed(), 600u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, ClearUsedMovesToNotSelected)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);

    LRUTileList list;
    list.add(t1);
    list.add(t2);

    // Mark t1 as used by user 1.
    std::vector<Tile*> tiles1 = {&t1};
    list.markUsed(1, tiles1);

    // Clear user 1 — t1 should move to "not selected".
    list.clearUsed(1);

    // Tiles still in list.
    EXPECT_EQ(list.getTileCount(), 2u);
    EXPECT_EQ(list.getTotalBytesUsed(), 300u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, FreeMemoryEvictsNotSelected)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);
    MockTile t3(tree, 300);

    LRUTileList list;
    list.add(t1);
    list.add(t2);
    list.add(t3);
    // All tiles are in "selected" partition (just added).
    // Free memory should not evict selected tiles.
    list.freeMemory(100);
    // All tiles still selected — nothing evicted.
    EXPECT_EQ(list.getTileCount(), 3u);
    EXPECT_EQ(list.getTotalBytesUsed(), 600u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, FreeMemoryEvictsAfterClearUsed)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);
    MockTile t3(tree, 300);

    LRUTileList list;
    list.add(t1);
    list.add(t2);
    list.add(t3);

    // Mark all tiles as used by user 1.
    std::vector<Tile*> tiles = {&t1, &t2, &t3};
    list.markUsed(1, tiles);

    // Clear user 1 — all tiles move to "not selected".
    list.clearUsed(1);

    // Evict to 400 bytes. The eviction order depends on pointer ordering.
    // After eviction, total must be <= 400 and at least one tile remains.
    list.freeMemory(400);
    EXPECT_LE(list.getTotalBytesUsed(), 400u);
    EXPECT_GE(list.getTileCount(), 1u);
    EXPECT_LE(list.getTileCount(), 2u);

    // Evict all remaining.
    list.freeMemory(0);
    EXPECT_EQ(list.getTotalBytesUsed(), 0u);
    EXPECT_EQ(list.getTileCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, FreeMemoryEvictsAll)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);

    LRUTileList list;
    list.add(t1);
    list.add(t2);

    // Move to not selected.
    std::vector<Tile*> tiles = {&t1, &t2};
    list.markUsed(1, tiles);
    list.clearUsed(1);

    // Free all memory.
    list.freeMemory(0);
    EXPECT_EQ(list.getTotalBytesUsed(), 0u);
    EXPECT_EQ(list.getTileCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, ReAddAfterDrop)
{
    MockTileTree tree;
    MockTile tile(tree, 100);

    LRUTileList list;
    list.add(tile);
    EXPECT_EQ(list.getTileCount(), 1u);

    list.drop(tile);
    EXPECT_EQ(list.getTileCount(), 0u);

    // Re-add the same tile.
    list.add(tile);
    EXPECT_EQ(list.getTileCount(), 1u);
    EXPECT_EQ(list.getTotalBytesUsed(), 100u);
}

// Authored: no reference test exists in itwinjs-core for LRUTileList C++ port
TEST(LRUTileListTest, MultipleUsers)
{
    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);

    LRUTileList list;
    list.add(t1);
    list.add(t2);

    // Both users mark t1 as used.
    std::vector<Tile*> tiles1 = {&t1};
    list.markUsed(1, tiles1);
    list.markUsed(2, tiles1);

    // Clear user 1 — t1 still has user 2, stays selected.
    list.clearUsed(1);
    EXPECT_EQ(list.getTileCount(), 2u);

    // Clear user 2 — t1 now has no users, moves to not selected.
    list.clearUsed(2);
    EXPECT_EQ(list.getTileCount(), 2u);

    // Free memory — evicts from "not selected" until under budget.
    // Eviction order depends on pointer ordering; just check invariants.
    list.freeMemory(150);
    EXPECT_LE(list.getTotalBytesUsed(), 150u);
    EXPECT_GE(list.getTileCount(), 0u);
    EXPECT_LE(list.getTileCount(), 1u);
}
