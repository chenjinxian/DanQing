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
//
// M-C Task 3 extends this file with the cancel face: swapPending double
// buffer (TileRequestChannel.ts:215-221), the users-empty cancel cycles in
// process (:236-253), the dispatch-time canceled guard (TileRequest.ts:81-82)
// and the shared-request gate's cross-queue re-enqueue (TileAdmin.ts:913-917).

#include "dqRender/RenderGraphic.h"  // TileContent::graphic unique_ptr 析构需完整类型
#include "dqRender/tile/TileRequestChannel.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileAdmin.h"  // TileUser（TileRequest 构造需 user）
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

// TileUser stub — the reference constructor seeds the request's user set with
// the requesting user (TileRequest.ts:34-39), so a user is required to build
// one. Id is arbitrary here (no sharing asserted by these ordering tests);
// the optional id keeps multi-user cancel tests unambiguous.
class StubUser : public TileUser {
public:
    explicit StubUser(uint32_t id = 1)
        : m_id(id)
    {
    }
    uint32_t getTileUserId() const override { return m_id; }
    void discloseTileTrees(std::vector<TileTree*>&) override {}

private:
    uint32_t m_id;
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
    StubUser user;

    auto requestA = std::make_unique<TileRequest>(tileA, channel, user);
    requestA->setPriority(0);
    auto requestB = std::make_unique<TileRequest>(tileB, channel, user);
    requestB->setPriority(9);
    channel.append(std::move(requestA));
    channel.append(std::move(requestB));

    channel.process(0);

    // Tree key 5 (Dynamic) < 40 (Context) → B dispatches first; A stays
    // pending (concurrency 1, and B's initiated fetch completes（探针同步交付）
    // frees the slot only after this process pass has popped its single
    // request).
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
    StubUser user;

    auto requestLow = std::make_unique<TileRequest>(lowPriorityTile, channel, user);
    requestLow->setPriority(0);
    auto requestHigh = std::make_unique<TileRequest>(highPriorityTile, channel, user);
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

// ---------------------------------------------------------------------------
// Cancel face (M-C Task 3) — swapPending double buffer + users-empty cancel
// cycles + the shared-request gate's cross-queue re-enqueue.
// ---------------------------------------------------------------------------

// Local TileAdmin — the channel records cancellations into TileAdmin
// statistics (the reference counts per channel, TileRequestChannel.ts:327;
// DanQing folds channel statistics into TileAdmin.Statistics — AdminFixture
// pattern from TileRequestUsersTest.cpp).
struct AdminFixture
{
    TileAdmin admin;
};

// Ported from: TileRequestChannel.ts process (:236-253 — users-empty 取消
//              双段) + TileRequest.cancel (:122-131)。
// Authored: 参考无 channel 级取消单测（TileAdmin.test.ts 只测统计面）；
//           场景按参考 process 的双段取消分支结构自写。content 迟到的丢弃
//           路径在 TileAdmin::deliverTileContent（admin 的 channel 注册表），
//           由 TileAdminTest.ForgetUserCancelsSoleRequestImmediately 锁定。
TEST(TileRequestChannelTest, UsersEmptyCancelsPendingAndActive)
{
    AdminFixture f;
    g_requestContentSeq = 0;

    // --- pending 段：并发 0 → 请求留在队列；唯一 user 离开 → 下帧取消 ---
    PriorityStubTree tree(TileLoadPriority::Primary);
    OrderProbeTile pendingTile(tree);
    StubUser user(1);

    TileRequestChannel channel(0);  // concurrency 0 → 不 dispatch
    auto request = std::make_unique<TileRequest>(pendingTile, channel, user);
    channel.append(std::move(request));

    channel.forgetUser(user);  // 唯一 user 离开（UniqueTileUserSets.forgetUser 角色）
    channel.swapPending();     // 帧界（TileAdmin.processQueue → channels.swapPending）
    channel.process(0);        // process :242-245 → 取消 + 清 previouslyPending

    EXPECT_EQ(f.admin.statistics().numCanceled, 1u);  // channel.cancel 计数 (:325-328)
    EXPECT_EQ(channel.getPendingCount(), 0u);         // previouslyPending 已清 (:247)
    EXPECT_EQ(channel.getActiveCount(), 0u);
    EXPECT_EQ(pendingTile.dispatchOrder, 0);          // 从未 dispatch
    EXPECT_EQ(pendingTile.getRequest(), nullptr);     // cancel 释放 tile 钩子 (:144-145)

    // --- active 段：dispatch 后 user 离开 → 标记取消但 active 不摘 ---
    OrderProbeTile activeTile(tree);
    StubUser user2(2);
    TileRequestChannel channel2(1);

    auto request2 = std::make_unique<TileRequest>(activeTile, channel2, user2);
    TileRequest* raw2 = request2.get();
    channel2.append(std::move(request2));
    channel2.process(0);
    ASSERT_EQ(activeTile.dispatchOrder, 1);  // 已 dispatch（fetch 不中止）
    ASSERT_EQ(channel2.getActiveCount(), 1u);

    channel2.forgetUser(user2);  // 飞行中失去全部 user
    channel2.swapPending();
    channel2.process(0);         // :249-253 → cancel，但不摘 active

    EXPECT_EQ(f.admin.statistics().numCanceled, 2u);
    EXPECT_EQ(channel2.getActiveCount(), 1u);      // NB :250 — 完成前不摘
    EXPECT_EQ(activeTile.dispatchOrder, 1);        // fetch 未被中止
    EXPECT_TRUE(raw2->isCanceled());
    EXPECT_EQ(raw2->getState(), TileRequest::State::Failed);  // cancel → Failed (:130)
    EXPECT_EQ(activeTile.getRequest(), nullptr);   // 钩子已释放 (:144-145)

    // cancel 后 content 迟到的丢弃路径经 TileAdmin::deliverTileContent
    // （完成链只能查 TileAdmin 的 channel 注册表），由
    // TileAdminTest.ForgetUserCancelsSoleRequestImmediately 锁定：
    // 不 decode（TileRequest.ts:109-110）+ 槽位释放（:333-336）。
}

// Ported from: TileRequestChannel.append (:224-227) + TileAdmin.ts:913-917
//              （isQueued && users 空 → channel.append 重入队 + addUser）在
//              swapPending 之后的跨队列搬运。
// Authored: 参考按引用持有双队列成员（同一请求可同时存在于
//           _previouslyPending 与 _pending）；DanQing unique_ptr 单槽位
//           所有权 → 重入队等价于把句柄从 previouslyPending 搬入 pending。
TEST(TileRequestChannelTest, ReenqueueMovesRequestFromPreviouslyPending)
{
    AdminFixture f;
    g_requestContentSeq = 0;

    PriorityStubTree tree(TileLoadPriority::Primary);
    OrderProbeTile tile(tree);
    StubUser user(3);

    TileRequestChannel channel(1);
    auto request = std::make_unique<TileRequest>(tile, channel, user);
    TileRequest* raw = request.get();
    channel.append(std::move(request));

    channel.swapPending();  // 帧界：请求随队列进入 previouslyPending
    EXPECT_EQ(channel.getPendingCount(), 0u);

    channel.forgetUser(user);  // clearAll 每帧清空 user 集（TileAdmin.ts:829 等价作用）
    channel.append(*raw);      // 共享门重入队（TileAdmin.ts:913-915）
    raw->addUser(user);        // 调用方回填 user（TileAdmin.ts:917）

    channel.process(0);
    EXPECT_EQ(tile.dispatchOrder, 1);  // 一次 dispatch — 搬运而非重建
    EXPECT_EQ(channel.getPendingCount(), 0u);
    EXPECT_EQ(channel.getActiveCount(), 1u);
    EXPECT_FALSE(raw->isQueued());
    EXPECT_FALSE(raw->isCanceled());
}

// Ported from: TileRequest.dispatch 的取消守卫（TileRequest.ts:81-82 —
//              `if (this.isCanceled) return;`）+ channel.dispatch 统计顺序
//              （TileRequestChannel.ts:313 — 守卫之前计数）。
// Authored: 参考无直接单测；锁 DanQing 拥有型队列下的"弹出即弃"路径
//           （unique_ptr 析构代替参考 GC）。
TEST(TileRequestChannelTest, CanceledQueuedRequestIsNotDispatched)
{
    AdminFixture f;
    g_requestContentSeq = 0;

    PriorityStubTree tree(TileLoadPriority::Primary);
    OrderProbeTile tile(tree);
    StubUser user(4);

    TileRequestChannel channel(1);
    auto request = std::make_unique<TileRequest>(tile, channel, user);
    channel.append(std::move(request));
    channel.forgetUser(user);  // 排队中失去全部 user（未 swap → 仍在 m_pending）

    channel.process(0);
    EXPECT_EQ(tile.dispatchOrder, 0);                 // 取消请求不发起 fetch (:81-82)
    EXPECT_EQ(channel.getPendingCount(), 0u);         // 弹出即弃
    EXPECT_EQ(channel.getActiveCount(), 0u);
    EXPECT_EQ(f.admin.statistics().numCanceled, 0u);  // 非取消循环路径，不计数
    EXPECT_EQ(f.admin.statistics().totalDispatchedRequests,
              1u);                                    // :313 — 守卫之前计数
    EXPECT_EQ(tile.getRequest(), nullptr);  // DanQing 弹出即弃时释放钩子（内存安全适配）
}
