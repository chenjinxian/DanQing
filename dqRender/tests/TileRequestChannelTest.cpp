// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileRequestChannel scheduling-order tests
//
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts:13-20 —
//   TileRequestQueue's comparator is two-level: the tile tree's loadPriority
//   dominates; the per-request priority breaks ties. Both keys ascend (lower =
//   dispatched first): `const diff = lhs.tile.tree.loadPriority -
//   rhs.tile.tree.loadPriority; return 0 !== diff ? diff : lhs.priority -
//   rhs.priority;`
//
// Authored: no reference test exists in itwinjs-core for the queue comparator
// directly (ordering is exercised only through TileAdmin integration —
// TileAdmin.test.ts drives channel statistics, not dispatch order); the
// scenarios below lock the comparator's contract with stub trees whose
// priorities come from the reference enum values (Tile.ts:626-639).

#include "dqRender/RenderGraphic.h"  // TileContent::graphic unique_ptr 析构需完整类型
#include "dqRender/tile/TileRequestChannel.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileTree.h"
#include "dqRender/tile/RealityTileTree.h"

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;
using namespace dqGeom;

namespace {

// Dispatch-order probe: the channel dispatches by calling tile.requestContent()
// (TileRequestChannel.cpp process → tile.requestContent, the reference's
// channel.requestContent → tile.requestContent forwarding,
// TileRequestChannel.ts:305-307). Recording the call sequence pins which
// request the scheduler picked first.
int g_requestContentSeq = 0;

class OrderProbeTile : public Tile {
public:
    OrderProbeTile(TileTree& tree)
        : Tile(tree, nullptr, Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1))
    {
    }

    bool requestContent() override
    {
        dispatchOrder = ++g_requestContentSeq;
        // Returning true ("fetch initiated") keeps the dispatched request in
        // the active set (process → setLoadStatus(Loading)), so with
        // concurrency 1 exactly one request dispatches per process() — the
        // scheduler's first pick is the only dispatch.
        return true;
    }
    TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}

    int dispatchOrder = 0;  // 0 = never dispatched
};

// Stub tree carrying an explicit reference load priority (Tile.ts:626-639).
class PriorityStubTree : public TileTree {
public:
    explicit PriorityStubTree(TileLoadPriority priority)
        : TileTree(nullptr, priority)
    {
    }

    TileVisibility computeVisibility(TileDrawArgs&, Tile*) override
    {
        return TileVisibility::Visible;
    }
};

}  // namespace

// Ported from: TileRequestChannel.ts:13-20 —— 树优先级先于请求优先级。
// Authored: 参考无 channel 比较器直接单测（覆盖在 TileAdmin 集成）。
// A = Context(40) with request priority 0; B = Dynamic(5) with request
// priority 9. The tree key must dominate: B dispatches first despite its
// larger request key.
TEST(TileRequestChannel, TreePriorityDominatesRequestPriority)
{
    g_requestContentSeq = 0;

    PriorityStubTree contextTree(TileLoadPriority::Context);  // 40
    PriorityStubTree dynamicTree(TileLoadPriority::Dynamic);  // 5

    OrderProbeTile tileA(contextTree);
    OrderProbeTile tileB(dynamicTree);

    // Concurrency 1 → exactly one dispatch per process(); whichever request
    // sorts first is the only one dispatched.
    TileRequestChannel channel(1);

    auto requestA = std::make_unique<TileRequest>(tileA, channel);
    requestA->setPriority(0);
    auto requestB = std::make_unique<TileRequest>(tileB, channel);
    requestB->setPriority(9);
    channel.append(std::move(requestA));
    channel.append(std::move(requestB));

    channel.process(0);

    // Tree key 5 (Dynamic) < 40 (Context) → B dispatches first; A stays
    // pending (concurrency 1, and B's failed fetch frees the slot only after
    // this process pass has popped its single request).
    EXPECT_EQ(tileB.dispatchOrder, 1);
    EXPECT_EQ(tileA.dispatchOrder, 0);
    EXPECT_EQ(channel.getPendingCount(), 1u);
}

// Ported from: TileRequestChannel.ts:16-17 —— 树键相等时请求键决胜（同为升序，
// 小者先调度）。
// Authored: 参考无 channel 比较器直接单测（覆盖在 TileAdmin 集成）。
TEST(TileRequestChannel, RequestPriorityBreaksTiesWithinTree)
{
    g_requestContentSeq = 0;

    PriorityStubTree tree(TileLoadPriority::Primary);  // 20 — both tiles

    OrderProbeTile lowPriorityTile(tree);
    OrderProbeTile highPriorityTile(tree);

    TileRequestChannel channel(1);

    auto requestLow = std::make_unique<TileRequest>(lowPriorityTile, channel);
    requestLow->setPriority(0);
    auto requestHigh = std::make_unique<TileRequest>(highPriorityTile, channel);
    requestHigh->setPriority(9);
    // Append the higher request key first — append order is ignored by the
    // reference ("Ordering is ignored - the queue will be re-sorted later",
    // TileRequestChannel.ts:221), the sort decides.
    channel.append(std::move(requestHigh));
    channel.append(std::move(requestLow));

    channel.process(0);

    EXPECT_EQ(lowPriorityTile.dispatchOrder, 1);
    EXPECT_EQ(highPriorityTile.dispatchOrder, 0);
    EXPECT_EQ(channel.getPendingCount(), 1u);
}

// Authored: no reference test pins the reality-tree priority constant (the
// value lives in RealityModelTileTree's loader — RealityModelTileTree.ts:464 —
// and flows to TileTree params at :299 → TileTree.ts:129); lock the DanQing
// wiring so the tree key the scheduler reads is the reference value.
TEST(TileRequestChannel, RealityTreeDefaultsToContextPriority)
{
    RealityTileTree tree(nullptr, "fixture://tileset.json");
    EXPECT_EQ(tree.getLoadPriority(), TileLoadPriority::Context);
}
