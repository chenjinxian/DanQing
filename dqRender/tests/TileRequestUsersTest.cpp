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
// M-C Task 3 adds the TileAdminTest suite here: forgetUser's immediate cancel
// of sole-user requests (TileAdmin.ts:925-940 via :560-563), the shared-request
// non-cancel face, and the end-to-end lock that a still-wanted queued request
// survives the frame boundary through the clearAll → swapPending → re-enqueue
// chain (TileAdmin.ts:827-841).
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

// Read probe for the cancel-drop path (M-C Task 3): a canceled request's late
// response must be discarded before decode (TileRequest.ts:109-110).
int g_readContentCount = 0;

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
    TileContent readContent(uint8_t const*, size_t) override
    {
        ++g_readContentCount;
        return {};
    }
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

// Ported from: TileAdmin.ts:913-917（isQueued && users 空 → channel.append 重
// 入队 + addUser）——重入队不得复制所有权/重复 dispatch。
// Authored: 参考无直接单测；锁 DanQing unique_ptr 所有权下的重入队接缝。
//           M-C Task 3 更新：swapPending 落地后取消面激活——users 空的排队
//           请求会被取消（TileRequestChannel.ts:242-245），故重入队之后必须
//           像参考调用方一样 addUser（TileAdmin.ts:917），dispatch 才会发生。
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

    // 共享请求门的重入队分支（TileAdmin.ts:913-915）+ 调用方回填 user (:917)。
    channel.append(*raw);
    raw->addUser(user);
    channel.process(0);

    EXPECT_EQ(g_requestContentCount, 1);  // 一次 dispatch——重入队未复制请求
    EXPECT_EQ(channel.getPendingCount(), 0u);
    EXPECT_EQ(channel.getActiveCount(), 1u);
    EXPECT_FALSE(raw->isQueued());
    EXPECT_FALSE(raw->isCanceled());
}

// Ported from: TileAdmin.forgetUser (TileAdmin.ts:560-563 —
//              onUserIModelClosed → _users.delete) + onUserIModelClosed
//              (:925-940 — 独占请求立即取消) + TileRequest.cancel
//              (TileRequest.ts:122-131) + TileRequest.dispatch 的数据到达
//              Loading 迁移（TileRequest.ts:92-93 "Set this now, so our
//              `isCanceled` check can see it"——迁移落在丢弃门 :109-110 之前，
//              使 isCanceled 的 Loading 豁免 :58-60 "After we've received the
//              raw tile data, always finish processing it - otherwise tile may
//              end up in limbo" 放行迟到数据 → handleResponse :156-195 交付）。
// Authored: 参考无 TileAdmin 级取消单测（TileAdmin.test.ts 只测统计面）；
//           场景按 onUserIModelClosed 的 `1 === request.users.length` 分支
//           自写；数据到达交付面按 dispatch 的迁移序（:92-93 → :109-110 →
//           handleResponse）自写。
TEST(TileAdminTest, ForgetUserCancelsSoleRequestImmediately)
{
    g_requestContentCount = 0;
    g_readContentCount = 0;
    AdminFixture f;
    StubTree tree;
    ProbeTile tile(tree);
    StubUser user(1);

    f.admin.registerUser(user);
    f.admin.requestTiles(user, {&tile});
    f.admin.process();  // 请求建立并 dispatch（channel 并发 10，槽位充足）
    TileRequest* raw = tile.getRequest();
    ASSERT_NE(raw, nullptr);
    EXPECT_TRUE(raw->isActive());  // 飞行中

    // 独占请求：唯一 user 离开 → 立即取消，不等下帧（TileAdmin.ts:928-940）。
    f.admin.forgetUser(user);
    EXPECT_EQ(raw->getState(), TileRequest::State::Failed);  // cancel → Failed (:130)
    EXPECT_TRUE(raw->isCanceled());
    EXPECT_EQ(tile.getRequest(), nullptr);  // 钩子已释放（TileRequest.ts:145）

    // 取消只标记，不中止 fetch（TileRequestChannel.ts:250 NB）——数据迟到时
    // 交付而非丢弃：数据到达点无条件迁移 Loading（TileRequest.ts:92-93）→
    // isCanceled 的 Loading 豁免（:58-60）使 :109-110 门放行 → readContent +
    // setContent（handleResponse :156-195）。无迁移则字节被丢弃、瓦钉死
    // Loading（cancel 已释放瓦钩 + processRequests 只为 NotLoaded 瓦建请求
    // —— limbo，:58-60 注释所防）。NB：settle 即把请求移出活动集（unique_ptr
    // 所有权 → 销毁），settle 后断言走活动集计数/统计面，不解引用 raw。
    f.admin.deliverTileContent(tile, {0x11, 0x22, 0x33, 0x44});
    EXPECT_EQ(g_readContentCount, 1);  // 数据已到 → 解码交付（不丢弃）
    EXPECT_EQ(g_requestContentCount, 1);  // 不重新取数
    EXPECT_EQ(tile.getLoadStatus(), TileLoadStatus::Ready);  // setContent（Tile.ts:210-216）
    EXPECT_EQ(f.admin.getActiveRequestCount(), 0u);  // 请求 settle（离开活动集）
    EXPECT_EQ(f.admin.statistics().totalCompletedRequests, 1u);  // complete（recordCompletion）
}

// Ported from: TileRequest.dispatch (TileRequest.ts:94-110) — the no-data
//              drop face: the Loading migration (:92-93) sits inside the try
//              AFTER `gotResponse = true`, so a canceled request whose fetch
//              produces no data never reaches it — it stays Failed, and the
//              fetch failure settles through setFailed (:148-153 —
//              notifyAndClear + Failed + tile.setNotFound + recordFailure).
// Authored: 参考无直接单测；对照锁"无数据的取消不交付"——Loading 迁移只在
//           数据到达点发生，失败链（reportTileFetchError，参考 catch 不复查
//           isCanceled → setFailed 无条件跑）不变。
TEST(TileAdminTest, CanceledRequestWithoutDataStaysDropped)
{
    g_requestContentCount = 0;
    g_readContentCount = 0;
    AdminFixture f;
    StubTree tree;
    ProbeTile tile(tree);
    StubUser user(1);

    f.admin.registerUser(user);
    f.admin.requestTiles(user, {&tile});
    f.admin.process();
    TileRequest* raw = tile.getRequest();
    ASSERT_NE(raw, nullptr);
    EXPECT_TRUE(raw->isActive());

    f.admin.forgetUser(user);
    EXPECT_EQ(raw->getState(), TileRequest::State::Failed);  // cancel → Failed
    EXPECT_TRUE(raw->isCanceled());  // 无数据到达 → :92-93 未达 → 无 Loading 豁免

    // fetch 失败（无数据）→ 失败链：settle(failed) + setNotFound（setFailed
    // 形态，TileRequest.ts:148-153）——不得复活动画/交付（settle 销毁请求，
    // 之后断言走统计面）。
    f.admin.reportTileFetchError(tile, "fetch failed");
    EXPECT_EQ(f.admin.getActiveRequestCount(), 0u);  // 失败链 settle
    EXPECT_EQ(f.admin.statistics().totalFailedRequests, 1u);  // fail（recordFailure 形态）
    EXPECT_EQ(g_readContentCount, 0);  // 不 decode
    EXPECT_EQ(tile.getLoadStatus(), TileLoadStatus::NotFound);  // setNotFound（:152）
}

// Ported from: TileAdmin.onUserIModelClosed (TileAdmin.ts:928-940) —
//              `1 === request.users.length` 门：多 user 共享的请求不取消。
// Authored: 参考无直接单测；锁"不误伤仍有关注者的共享请求"。
TEST(TileAdminTest, ForgetUserKeepsSharedRequestForOtherUsers)
{
    g_requestContentCount = 0;
    g_readContentCount = 0;
    AdminFixture f;
    StubTree tree;
    ProbeTile tile(tree);
    StubUser userA(1);
    StubUser userB(2);

    f.admin.registerUser(userA);
    f.admin.registerUser(userB);
    f.admin.requestTiles(userA, {&tile});
    f.admin.requestTiles(userB, {&tile});
    f.admin.process();
    TileRequest* raw = tile.getRequest();
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->getUsers().size(), 2u);

    f.admin.forgetUser(userA);
    EXPECT_FALSE(raw->isCanceled());        // B 仍关注 → 不取消
    EXPECT_EQ(tile.getRequest(), raw);      // 钩子原样
    EXPECT_EQ(raw->getUsers().size(), 1u);

    // content 到达 → 正常交付（settle + decode + setContent）。
    f.admin.deliverTileContent(tile, {0x01, 0x02, 0x03, 0x04});
    EXPECT_EQ(g_readContentCount, 1);
    EXPECT_EQ(tile.getLoadStatus(), TileLoadStatus::Ready);
}

// Ported from: TileAdmin.processQueue (TileAdmin.ts:827-841 —
//              clearAll → channels.swapPending → processRequests) +
//              processRequests 的重入队分支 (:913-915)。
// Authored: 参考无直接单测；端到端锁"跨帧仍被关注的排队请求经
//           swapPending/重入队链存活"——若重入队搬运缺失，请求会在
//           previouslyPending 清空时被销毁、tile 重建新请求（指针不同）。
TEST(TileAdminTest, QueuedRequestSurvivesFrameBoundaryViaReenqueue)
{
    g_requestContentCount = 0;
    AdminFixture f;
    StubTree tree;
    StubUser user(1);

    // channel 并发 10（getChannel("default") 默认）：12 块瓦 → 10 dispatched
    // + 2 queued，制造"跨帧排队"状态。
    std::vector<std::unique_ptr<ProbeTile>> tiles;
    std::vector<Tile*> raws;
    for (int i = 0; i < 12; ++i) {
        tiles.push_back(std::make_unique<ProbeTile>(tree));
        raws.push_back(tiles.back().get());
    }
    f.admin.registerUser(user);
    f.admin.requestTiles(user, raws);
    f.admin.process();
    ASSERT_EQ(g_requestContentCount, 10);
    ASSERT_EQ(f.admin.getPendingRequestCount(), 2u);

    // 选一块仍排队的瓦作观察对象。
    ProbeTile* subject = nullptr;
    for (auto& t : tiles)
        if (t->getRequest() && t->getRequest()->isQueued()) {
            subject = t.get();
            break;
        }
    ASSERT_NE(subject, nullptr);
    TileRequest* const subjectAddr = subject->getRequest();  // 仅比对地址值

    // 帧 2：clearAll 清空 user 集 → swapPending（请求随队列进入
    // previouslyPending）→ 重入队分支把它搬回 m_pending（TileAdmin.ts:913-915）。
    f.admin.process();
    EXPECT_EQ(f.admin.getPendingRequestCount(), 2u);  // 槽位满，仍排队
    EXPECT_EQ(subject->getRequest(), subjectAddr);    // 同一请求存活（非重建）
    TileRequest* live = subject->getRequest();
    ASSERT_EQ(live, subjectAddr);  // 仅在同址时解引用（搬运缺失时不解悬垂指针）
    EXPECT_FALSE(live->isCanceled());

    // 释放两个并发槽（正常交付两块飞行中的瓦）。
    int settled = 0;
    for (auto& t : tiles) {
        if (settled == 2)
            break;
        TileRequest* r = t->getRequest();
        if (r && r->isActive() && !r->isQueued()) {
            f.admin.deliverTileContent(*t, {0x01, 0x02, 0x03, 0x04});
            ++settled;
        }
    }
    ASSERT_EQ(settled, 2);

    // 帧 3：槽位释放 → 排队请求（含观察对象）获得 dispatch。
    f.admin.process();
    EXPECT_EQ(g_requestContentCount, 12);
    EXPECT_EQ(subject->getRequest(), subjectAddr);
    EXPECT_FALSE(subject->getRequest()->isQueued());
}
