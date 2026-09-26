// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileRequest.users face + TileAdmin shared-request gate tests
//
// Ported from: itwinjs-core core/frontend/src/tile/TileRequest.ts:25-77
//   (users set + addUser + isCanceled) and core/frontend/src/tile/TileAdmin.ts
//   :897-925 (processRequests' shared-request gate: the first user's pass
//   creates the request and hooks it on the tile; every later user sharing the
//   tile joins the existing request via addUser instead of building a
//   duplicate; a queued, userless request is re-enqueued).
//
// Authored: 参考无 channel/users 级单测（该面覆盖在 TileAdmin 集成与
//   Viewport 生命周期里——TileAdmin.test.ts 只驱动统计面）；场景自写，
//   场景与断言值取自参考源码的分支结构（TileAdmin.ts:899-920）。

#include "dqRender/RenderGraphic.h"  // TileContent::graphic unique_ptr 析构需完整类型
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/TileRequestChannel.h"
#include "dqRender/tile/TileTree.h"

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型

#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace dqRender;
using namespace dqGeom;

namespace {

// Dispatch probe: counts requestContent calls. The shared-request gate's
// discriminating signal — two users on one tile must produce exactly ONE
// fetch (a rebuilt request would fetch twice, TileAdmin.ts:899-905).
int g_requestContentCount = 0;

class ProbeTile : public Tile {
public:
    explicit ProbeTile(TileTree& tree)
        : Tile(tree, nullptr, Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1))
    {
    }

    bool requestContent() override
    {
        ++g_requestContentCount;
        // "Fetch initiated" keeps the dispatched request in the active set
        // (process → setLoadStatus(Loading)) so the tile hook stays live
        // across assertions.
        return true;
    }
    TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}
};

class StubTree : public TileTree {
public:
    StubTree()
        : TileTree(nullptr, TileLoadPriority::Primary)
    {
    }

    TileVisibility computeVisibility(TileDrawArgs&, Tile*) override
    {
        return TileVisibility::Visible;
    }
};

// TileUser stubs with DISTINCT ids — the reference's user sets dedup by
// tileUserId (TileUserSet.ts:17 — sorted by `tileUserId`), so same-id users
// collapse by design.
class StubUser : public TileUser {
public:
    explicit StubUser(uint32_t id)
        : m_id(id)
    {
    }
    uint32_t getTileUserId() const override { return m_id; }
    void discloseTileTrees(std::vector<TileTree*>&) override {}

private:
    uint32_t m_id;
};

bool hasUser(std::vector<TileUser*> const& users, TileUser const* user)
{
    for (TileUser const* entry : users)
        if (entry == user)
            return true;
    return false;
}

// Fresh TileAdmin per test (AdminFixture pattern — the singleton carries
// session statistics across tests).
struct AdminFixture {
    TileAdmin admin;
};

}  // namespace

// Ported from: TileRequest.ts users/addUser/isCanceled (:25-77) + TileAdmin.ts
//              processRequests 共享请求门 (:897-925)。
// Authored: 参考无 channel 级单测（覆盖在集成）；场景自写。
TEST(TileRequestUsers, SharedRequestAddUserInsteadOfRebuild)
{
    AdminFixture f;
    StubTree tree;
    ProbeTile tile(tree);
    StubUser userA(1);
    StubUser userB(2);

    f.admin.registerUser(userA);
    f.admin.registerUser(userB);

    // 同瓦两 user 依次进 processRequestsForUser（经 requestTiles 喂入 —
    // TileAdmin.ts:498-500 → processRequests TileAdmin.ts:837）：
    f.admin.requestTiles(userA, {&tile});
    f.admin.requestTiles(userB, {&tile});
    f.admin.process();

    // 第一遍 → 建请求；第二遍（userB）→ 不新建（tile->getRequest() 同一指针，
    // TileAdmin.ts:909-920 else 分支），users 含 A+B。
    TileRequest* request = tile.getRequest();
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->getUsers().size(), 2u);
    EXPECT_TRUE(hasUser(request->getUsers(), &userA));
    EXPECT_TRUE(hasUser(request->getUsers(), &userB));

    // 恰一次取数：若第二遍重建请求，则同瓦两次 dispatch（Tile.ts:246 钩子
    // 缺失 → 两请求都进队列）。
    EXPECT_EQ(g_requestContentCount, 1);

    // forgetUser(userA) 后 users 只剩 B；再 forgetUser(userB) →
    // isCanceled()==true（users 空 = 取消，TileRequest.ts:63）。
    f.admin.forgetUser(userA);
    ASSERT_NE(tile.getRequest(), nullptr);  // 取消接线归 Task 3 — 请求仍在
    EXPECT_EQ(request->getUsers().size(), 1u);
    EXPECT_TRUE(hasUser(request->getUsers(), &userB));
    EXPECT_FALSE(request->isCanceled());

    f.admin.forgetUser(userB);
    EXPECT_TRUE(request->getUsers().empty());
    EXPECT_TRUE(request->isCanceled());
}

// Ported from: TileRequest.isCanceled 的 Loading 豁免（TileRequest.ts:58-60 —
// "After we've received the raw tile data, always finish processing it"）。
// Authored: 参考无直接单测（同上）。
TEST(TileRequestUsers, LoadingStateOverridesEmptyUsers)
{
    g_requestContentCount = 0;
    StubTree tree;
    ProbeTile tile(tree);
    StubUser user(1);
    TileRequestChannel channel(1);

    TileRequest request(tile, channel, user);
    EXPECT_FALSE(request.isCanceled());  // users = {user}

    request.removeUser(user);
    EXPECT_TRUE(request.getUsers().empty());
    EXPECT_TRUE(request.isQueued());     // TileRequest.ts:50
    EXPECT_TRUE(request.isCanceled());   // Queued + 空 users → 取消

    request.dispatch();                  // Queued → Dispatched（TileRequest.ts:85）
    request.startLoading();              // Dispatched → Loading（TileRequest.ts:93——
                                         //  Loading 仅自 Dispatched 可达）
    EXPECT_FALSE(request.isCanceled());  // Loading 豁免（TileRequest.ts:59-60）
}

// Ported from: TileAdmin.ts:914-915（isQueued && users 空 → channel.append 重
// 入队）——重入队不得复制所有权/重复 dispatch。
// Authored: 参考无直接单测；锁 DanQing unique_ptr 所有权下的重入队接缝
// （swapPending 归 Task 3，届时同一接缝负责跨队列搬运）。
TEST(TileRequestUsers, QueuedUserlessRequestReenqueuedNotDuplicated)
{
    g_requestContentCount = 0;
    StubTree tree;
    ProbeTile tile(tree);
    StubUser user(1);
    TileRequestChannel channel(1);

    auto request = std::make_unique<TileRequest>(tile, channel, user);
    TileRequest* raw = request.get();
    channel.append(std::move(request));  // queued
    EXPECT_TRUE(raw->isQueued());

    channel.forgetUser(user);  // users 空（UniqueTileUserSets.forgetUser 角色）
    EXPECT_TRUE(raw->getUsers().empty());

    // 共享请求门的重入队分支（TileAdmin.ts:914-915）。
    channel.append(*raw);
    channel.process(0);

    EXPECT_EQ(g_requestContentCount, 1);  // 一次 dispatch——重入队未复制请求
    EXPECT_EQ(channel.getPendingCount(), 0u);
    EXPECT_EQ(channel.getActiveCount(), 1u);
    EXPECT_FALSE(raw->isQueued());
}
