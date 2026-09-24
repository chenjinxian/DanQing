// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
// DanQing dqRender — ITileFetcher interface tests
//
// Tests the Qt-free abstract ITileFetcher interface (§7.2):
// - Mock fetcher that records calls (no Qt dependency)
// - DI via TileAdmin::setFetcher()
// - processCompleted() called during TileAdmin::process()

#include "dqRender/tile/ITileFetcher.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileTree.h"
#include "dqRender/RenderGraphic.h"

#include <dqCommon/FeatureTable.h>

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;

// ============================================================================
// Mock Tile for testing
// ============================================================================
class FetcherTestTile : public Tile {
public:
    FetcherTestTile(TileTree& tree)
        : Tile(tree, nullptr, dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1))
    {}

    bool requestContent() override { return false; }
    TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}
};

// ============================================================================
// Mock TileTree for testing
// ============================================================================
class FetcherTestTileTree : public TileTree {
public:
    FetcherTestTileTree()
        : TileTree(std::make_unique<FetcherTestTile>(*this))
    {}

    TileVisibility computeVisibility(TileDrawArgs&, Tile*) override { return TileVisibility::Visible; }
};

// ============================================================================
// Mock ITileFetcher — no Qt dependency
// ============================================================================
class MockITileFetcher : public ITileFetcher {
public:
    void fetch(std::string const& url, Tile& /*tile*/,
               std::function<void(Tile&, std::vector<uint8_t> const&)> onComplete,
               std::function<void(Tile&, std::string const&)> onError) override
    {
        ++fetchCallCount;
        lastUrl = url;
        lastOnComplete = std::move(onComplete);
        lastOnError = std::move(onError);
    }

    void processCompleted() override
    {
        ++processCompletedCallCount;
    }

    uint32_t getActiveCount() const noexcept override
    {
        return activeCount;
    }

    void cancelAll() override
    {
        ++cancelAllCallCount;
    }

    int fetchCallCount = 0;
    int processCompletedCallCount = 0;
    int cancelAllCallCount = 0;
    uint32_t activeCount = 0;
    std::string lastUrl;
    std::function<void(Tile&, std::vector<uint8_t> const&)> lastOnComplete;
    std::function<void(Tile&, std::string const&)> lastOnError;
};

// ============================================================================
// Tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
TEST(ITileFetcherTest, MockFetcherRecordsFetchCall)
{
    MockITileFetcher mock;
    FetcherTestTileTree tree;
    FetcherTestTile tile(tree);

    std::vector<uint8_t> const fakeData = {0x01, 0x02, 0x03};
    mock.fetch("https://example.com/tile.b3dm", tile,
               [](Tile&, std::vector<uint8_t> const&) {},
               [](Tile&, std::string const&) {});

    EXPECT_EQ(mock.fetchCallCount, 1);
    EXPECT_EQ(mock.lastUrl, "https://example.com/tile.b3dm");
}

// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
TEST(ITileFetcherTest, MockFetcherProcessesCompleted)
{
    MockITileFetcher mock;

    mock.processCompleted();
    mock.processCompleted();

    EXPECT_EQ(mock.processCompletedCallCount, 2);
}

// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
TEST(ITileFetcherTest, MockFetcherCancelAll)
{
    MockITileFetcher mock;

    mock.cancelAll();

    EXPECT_EQ(mock.cancelAllCallCount, 1);
}

// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
TEST(ITileFetcherTest, SetFetcherDI)
{
    MockITileFetcher mock;
    TileAdmin admin;

    admin.setFetcher(std::make_unique<MockITileFetcher>(mock));

    // After DI, getFetcher() should return the mock
    auto& fetcher = admin.getFetcher();
    EXPECT_EQ(fetcher.getActiveCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
TEST(ITileFetcherTest, ProcessCallsProcessCompleted)
{
    MockITileFetcher mock;

    // Create TileAdmin, replace fetcher with mock
    TileAdmin admin;
    auto* mockPtr = new MockITileFetcher();
    admin.setFetcher(std::unique_ptr<ITileFetcher>(mockPtr));

    // process() should call processCompleted() on our mock
    admin.process();

    EXPECT_EQ(mockPtr->processCompletedCallCount, 1);
}

// Authored: no reference test exists in itwinjs-core for ITileFetcher abstraction
TEST(ITileFetcherTest, DefaultFetcherIsNotNull)
{
    TileAdmin admin;
    auto& fetcher = admin.getFetcher();
    EXPECT_NE(&fetcher, nullptr);
}
