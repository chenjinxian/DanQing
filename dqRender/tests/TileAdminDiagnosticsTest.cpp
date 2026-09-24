// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileAdmin diagnostics-surface tests
//
// Authored: no reference test exists in itwinjs-core for TileAdmin's C++
// diagnostics surface (the TS TileAdmin.Statistics are exercised only through
// the display-test-app DiagnosticsPanel, which has no unit tests); the
// counters/semantics are ported 1:1 from TileAdmin.ts:983-1016 +
// TileRequestChannel.ts:28-53 and locked here.
//
// Covers the Debug Info panel data sources (2026-09-21 Debug Info 全量任务):
//   Statistics extended counters + record hooks + decoding + resetStatistics
//   getTilesForUser / getNumRequestsForUser / addExternalTilesForUser
//   selected/unselected loaded tiles + totalTileContentBytes (LRU wiring)
//   purgeUnselectedTileContents

#include "dqRender/RenderGraphic.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/TileTree.h"

#include <dqCommon/FeatureTable.h>

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;
using namespace dqGeom;

namespace {

// Minimal RenderGraphic stub (unionRange no-op — Tile content only needs a
// non-null graphic to count as loaded).
class StubGraphic : public RenderGraphic {
public:
    void unionRange(dqGeom::Range3d&) const override {}
};

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

    // Drive the reference's setIsReady() path: content lands (graphic + bytes)
    // → TileAdmin.onTileContentLoaded → LRU entry.
    void loadContent(size_t bytesUsed)
    {
        setBytesUsed(bytesUsed);
        TileContent content;
        content.graphic = std::make_unique<StubGraphic>();
        content.isLeaf = true;
        setContent(std::move(content));
    }
};

class MockTileTree : public TileTree {
public:
    MockTileTree()
        : TileTree(std::make_unique<MockTile>(*this, 0))
    {
    }

    TileVisibility computeVisibility(TileDrawArgs&, Tile*) override { return TileVisibility::Visible; }
};

class MockTileUser : public TileUser {
public:
    uint32_t getTileUserId() const override { return 1; }
    void discloseTileTrees(std::vector<TileTree*>&) override {}
};

// Fresh TileAdmin per test (the singleton carries session statistics across
// tests — construct a local instance; TileAdmin::instance() itself is covered
// by the accessor smoke test at the bottom).
struct AdminFixture {
    TileAdmin admin;
};

}  // namespace

// Authored: 见文件头 — record hooks drive every cumulative counter.
TEST(TileAdminDiagnostics, RecordHooksDriveCounters)
{
    AdminFixture f;
    f.admin.recordDispatched();
    f.admin.recordDispatched();
    f.admin.recordCompleted();
    f.admin.recordFailed();
    f.admin.recordCanceled();
    f.admin.recordAborted();
    f.admin.recordTimedOut();
    f.admin.recordEmptyTile();
    f.admin.recordUndisplayableTile();
    f.admin.recordElidedTile();
    f.admin.recordCacheMiss();

    auto const& s = f.admin.statistics();
    EXPECT_EQ(s.totalDispatchedRequests, 2u);
    EXPECT_EQ(s.totalCompletedRequests, 1u);
    EXPECT_EQ(s.totalFailedRequests, 1u);
    EXPECT_EQ(s.numCanceled, 1u);
    EXPECT_EQ(s.totalAbortedRequests, 1u);
    EXPECT_EQ(s.totalTimedOutRequests, 1u);
    EXPECT_EQ(s.totalEmptyTiles, 1u);
    EXPECT_EQ(s.totalUndisplayableTiles, 1u);
    EXPECT_EQ(s.totalElidedTiles, 1u);
    EXPECT_EQ(s.totalCacheMisses, 1u);
}

// Authored: 见文件头 — decoding statistics accumulate mean/max/min
// (TileContentDecodingStatistics semantics, TileRequestChannel.ts:28-37).
TEST(TileAdminDiagnostics, DecodingStatistics)
{
    AdminFixture f;
    f.admin.recordDecodingTime(10.0);
    f.admin.recordDecodingTime(30.0);
    f.admin.recordDecodingTime(20.0);

    auto const& d = f.admin.statistics().decoding;
    EXPECT_DOUBLE_EQ(d.total, 60.0);
    EXPECT_DOUBLE_EQ(d.mean, 20.0);
    EXPECT_DOUBLE_EQ(d.max, 30.0);
    EXPECT_DOUBLE_EQ(d.min, 10.0);
}

// Authored: 见文件头 — resetStatistics clears cumulative totals
// (TileAdmin.ts:228-232).
TEST(TileAdminDiagnostics, ResetStatisticsClearsCumulative)
{
    AdminFixture f;
    f.admin.recordCompleted();
    f.admin.recordElidedTile();
    f.admin.recordDecodingTime(5.0);

    f.admin.resetStatistics();

    auto const& s = f.admin.statistics();
    EXPECT_EQ(s.totalCompletedRequests, 0u);
    EXPECT_EQ(s.totalElidedTiles, 0u);
    EXPECT_DOUBLE_EQ(s.decoding.total, 0.0);
    EXPECT_DOUBLE_EQ(s.decoding.mean, 0.0);
}

// Authored: 见文件头 — getTilesForUser: absent user → false; present → the
// selected/ready vectors registered by addTilesForUser (TileAdmin.ts:506).
TEST(TileAdminDiagnostics, GetTilesForUser)
{
    AdminFixture f;
    MockTileUser user;
    f.admin.registerUser(user);

    TileAdmin::SelectedAndReadyTiles tiles;
    EXPECT_FALSE(f.admin.getTilesForUser(user, tiles));

    MockTileTree tree;
    MockTile t1(tree, 100);
    MockTile t2(tree, 200);
    std::vector<Tile*> selected{ &t1 };
    std::vector<Tile*> ready{ &t2 };
    f.admin.addTilesForUser(user, selected, ready, {});

    EXPECT_TRUE(f.admin.getTilesForUser(user, tiles));
    ASSERT_NE(tiles.selected, nullptr);
    ASSERT_NE(tiles.ready, nullptr);
    EXPECT_EQ(tiles.selected->size(), 1u);
    EXPECT_EQ((*tiles.selected)[0], &t1);
    EXPECT_EQ(tiles.ready->size(), 1u);
    EXPECT_EQ((*tiles.ready)[0], &t2);
}

// Authored: 见文件头 — getNumRequestsForUser counts queued requests plus
// external.requested (TileAdmin.ts:474-482).
TEST(TileAdminDiagnostics, GetNumRequestsForUser)
{
    AdminFixture f;
    MockTileUser user;
    f.admin.registerUser(user);

    EXPECT_EQ(f.admin.getNumRequestsForUser(user), 0u);

    MockTileTree tree;
    MockTile t1(tree, 100);  // NotLoaded → queued by addTilesForUser
    MockTile t2(tree, 200);
    std::vector<Tile*> selected{ &t1, &t2 };
    f.admin.addTilesForUser(user, selected, {}, {});

    TileAdmin::ExternalTileStatistics external;
    external.requested = 3;
    f.admin.addExternalTilesForUser(user, external);

    // NOTE: addTilesForUser queues only NotLoaded tiles (2 here — both start
    // NotLoaded) + external.requested (3).
    EXPECT_EQ(f.admin.getNumRequestsForUser(user), 5u);
}

// Authored: 见文件头 — LRU wiring: content-loaded tiles enter the LRU via
// onTileContentLoaded; markUsed (addTilesForUser) selects them;
// totalTileContentBytes + the selected/unselected visitors reflect it
// (TileAdmin.ts:383/458/465 — the TileMemoryBreakdown left-pane source).
// Uses TileAdmin::instance() — Tile::setContent registers with the singleton.
TEST(TileAdminDiagnostics, LoadedTileAccounting)
{
    auto& admin = TileAdmin::instance();
    MockTileUser user;
    admin.registerUser(user);

    MockTileTree tree;
    auto loaded = std::make_unique<MockTile>(tree, 0);
    loaded->loadContent(500);  // setIsReady path → LRU entry
    std::vector<Tile*> selected{ loaded.get() };
    admin.addTilesForUser(user, selected, {}, {});

    // The tile has content → marked used → "selected" partition.
    uint32_t numSelected = 0;
    admin.forEachSelectedLoadedTile([&numSelected](Tile&) { ++numSelected; });
    EXPECT_EQ(numSelected, 1u);
    EXPECT_GE(admin.totalTileContentBytes(), 500u);

    // Unload the selection → the reference flow (clearTilesForUser then
    // addTilesForUser, TileAdmin.ts:550-555) demotes the tile to unselected.
    admin.clearTilesForUser(user);
    std::vector<Tile*> none;
    admin.addTilesForUser(user, none, {}, {});
    uint32_t numUnselected = 0;
    admin.forEachUnselectedLoadedTile([&numUnselected](Tile&) { ++numUnselected; });
    EXPECT_EQ(numUnselected, 1u);

    admin.forgetUser(user);
}

// Authored: 见文件头 — purgeUnselectedTileContents frees unselected content
// and drops it from the accounting (selected tiles untouched).
TEST(TileAdminDiagnostics, PurgeUnselectedTileContents)
{
    auto& admin = TileAdmin::instance();
    MockTileUser user;
    admin.registerUser(user);

    MockTileTree tree;
    auto keep = std::make_unique<MockTile>(tree, 0);
    auto drop = std::make_unique<MockTile>(tree, 0);
    keep->loadContent(700);
    drop->loadContent(300);

    // Select both, then the reference flow (clear → add) keeps only `keep`.
    std::vector<Tile*> both{ keep.get(), drop.get() };
    admin.addTilesForUser(user, both, {}, {});
    admin.clearTilesForUser(user);
    std::vector<Tile*> keepOnly{ keep.get() };
    admin.addTilesForUser(user, keepOnly, {}, {});

    admin.purgeUnselectedTileContents();

    uint32_t numSelected = 0;
    admin.forEachSelectedLoadedTile([&numSelected](Tile&) { ++numSelected; });
    EXPECT_EQ(numSelected, 1u);  // keep survives
    EXPECT_EQ(drop->getLoadStatus(), TileLoadStatus::Abandoned);  // freeMemory transition
    EXPECT_EQ(drop->getBytesUsed(), 0u);

    admin.forgetUser(user);
}
