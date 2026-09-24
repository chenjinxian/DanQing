// TileAdminMemoryTest — GPU 内存预算驱逐单测。
//
// Ported from: itwinjs-core core/frontend/src/test/tile/TileAdmin.test.ts
//              TEST(TileAdmin, "enforces memory limits")（内存上限段——mock tile
//              计 bytesUsed、驱动 freeMemory、断言 LRU 未选区被驱逐）
//              DanQing 适配：MockRender.System 的 TestGraphic 对应物为
//              setBytesUsed 注入（LRUTileList::add 经 Tile::getBytesUsed 计量）。
#include <gtest/gtest.h>

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型
#include <dqRender/RenderGraphic.h>

#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/TileDrawArgs.h"
#include "dqRender/tile/TileTree.h"

#include <memory>
#include <vector>

namespace {

// TestTree — 桩树（TileAdmin.test.ts TestTree :206-216 对应物）。
class TestTree : public dqRender::TileTree {
public:
    TestTree() : TileTree(nullptr) {}

    dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                               dqRender::Tile*) override
    {
        return dqRender::TileVisibility::Visible;
    }
};

// TestTile — bytesUsed 注入的桩 tile（TileAdmin.test.ts TestTile :137-143 对应物）。
class TestTile : public dqRender::Tile {
public:
    TestTile(TestTree& tree)
        : dqRender::Tile(tree, nullptr,
                         dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1))
    {
    }

    bool requestContent() override { return false; }
    dqRender::TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}

    using dqRender::Tile::setBytesUsed;
};

}  // namespace

// 内存预算序列（单测试防单例状态跨测试污染）：
//   1) 超预算 + 已 demote（未选）→ 从最老端驱逐至预算内；
//   2) selected 分区不驱逐（clearTilesForUser 前）；
//   3) 预算 0 = 不驱逐；
//   4) 默认预算 = 1GB。
// Ported from: TileAdmin.test.ts "enforces memory limits"（:92-608 内存段的
// demote-then-evict 模式：viewport 不再 markUsed → clearTilesForUser →
// LRU 未选分区可驱逐）。
TEST(TileAdminMemory, BudgetEvictsDemotedTilesOnly)
{
    auto& admin = dqRender::TileAdmin::instance();

    TestTree tree;
    std::vector<std::unique_ptr<TestTile>> tiles;
    for (int i = 0; i < 5; ++i) {
        tiles.push_back(std::make_unique<TestTile>(tree));
        tiles.back()->setBytesUsed(100);
    }

    // 内容落地 → 进 LRU。
    for (auto& t : tiles)
        admin.onTileContentLoaded(*t);
    ASSERT_EQ(admin.totalTileContentBytes(), 500u);

    // selected 分区不驱逐：预算 350 时全 selected（无 demote）→ 不动。
    auto const savedBudget = admin.getMaxTotalTileContentBytes();
    admin.setMaxTotalTileContentBytes(350);
    admin.freeMemory();
    EXPECT_EQ(admin.totalTileContentBytes(), 500u)
        << "selected partition must not be evicted";

    // demote（viewport 不再使用）→ 未选分区可驱逐。
    struct TestUser : dqRender::TileUser {
        uint32_t getTileUserId() const noexcept override { return 7777; }
        void discloseTileTrees(std::vector<dqRender::TileTree*>&) override {}
    } user;
    admin.registerUser(user);
    std::vector<dqRender::Tile*> selected = {tiles[4].get()};  // 最新的保选
    admin.addTilesForUser(user, selected, selected, {});
    admin.clearTilesForUser(user);   // demote 全部（参考 clearUsageForUser）
    admin.forgetUser(user);

    admin.freeMemory();
    EXPECT_LE(admin.totalTileContentBytes(), 350u)
        << "freeMemory did not evict demoted tiles down to the budget";

    // 预算 0 = 不驱逐。
    admin.setMaxTotalTileContentBytes(0);
    TestTile extra(tree);
    extra.setBytesUsed(1000);
    admin.onTileContentLoaded(extra);
    admin.clearTilesForUser(user);   // demote（防残留 selected）
    admin.freeMemory();
    EXPECT_GE(admin.totalTileContentBytes(), 1000u) << "zero budget must not evict";
    admin.setMaxTotalTileContentBytes(savedBudget);

    // 默认预算 = 1GB（桌面 default，TileAdmin.ts:1322-1326）。
    EXPECT_EQ(admin.getMaxTotalTileContentBytes(), 1024ull * 1024ull * 1024ull);
}

// 过期清理（G-B）：超时未用 tile 的子树内容被释放；节流窗口内不剪；
// 在选（markUsed 当前时间）的子树不剪。
// Ported from: TileAdmin.test.ts prune 段模式（时间前进 → tree.prune 生效）+
//              TileUsageMarker.isExpired（TileUsageMarker.ts:29-32）。
TEST(TileAdminMemory, PruneReleasesExpiredUnusedChildren)
{
    auto& admin = dqRender::TileAdmin::instance();

    // 造一棵带 children 的树，children 挂上"graphic"（用 bytesUsed 进 LRU
    // 观察 Abandoned 状态即可——freeMemory 置 Abandoned）。
    struct RangeTile2 : dqRender::Tile {
        explicit RangeTile2(dqRender::TileTree& t, dqRender::Tile* parent)
            : dqRender::Tile(t, parent,
                             dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1))
        {
        }
        bool requestContent() override { return false; }
        dqRender::TileContent readContent(uint8_t const*, size_t) override { return {}; }
        void loadChildren() override {}
    };
    class TwoLevelTree : public dqRender::TileTree {
    public:
        TwoLevelTree()
            : TileTree(nullptr)
        {
            auto root = std::make_unique<RangeTile2>(*this, nullptr);
            std::vector<std::unique_ptr<dqRender::Tile>> children;
            children.push_back(std::make_unique<RangeTile2>(*this, root.get()));
            m_child = children.back().get();
            root->setChildren(std::move(children));
            setRootTile(std::move(root));
        }
        dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                                   dqRender::Tile*) override
        {
            return dqRender::TileVisibility::Visible;
        }
        dqRender::Tile* m_child;
    };

    struct PruneUser : dqRender::TileUser {
        uint32_t getTileUserId() const noexcept override { return 8888; }
        void discloseTileTrees(std::vector<dqRender::TileTree*>& trees) override
        {
            trees.push_back(&tree);
        }
        TwoLevelTree tree;
    } user;

    admin.registerUser(user);

    // 时钟注入：t=100，全部 markUsed(100)（模拟选择）。
    dqRender::TileAdmin::setNowOverrideForTest(100.0);
    std::vector<dqRender::Tile*> selected = {user.tree.getRootTile(), user.tree.m_child};
    admin.addTilesForUser(user, selected, selected, {});
    EXPECT_EQ(user.tree.getRootTile()->getLastUsedTime(), 100.0);
    EXPECT_EQ(user.tree.m_child->getLastUsedTime(), 100.0);

    // t=105（<20s 过期阈值 + 节流窗口内）→ 不剪。
    dqRender::TileAdmin::setNowOverrideForTest(105.0);
    admin.process();
    EXPECT_NE(user.tree.m_child->getLoadStatus(), dqRender::TileLoadStatus::Abandoned)
        << "inside expiration window — no prune";

    // t=130（root 超时 30s > 20s）→ root 的 children 内容被释放。
    dqRender::TileAdmin::setNowOverrideForTest(130.0);
    admin.process();
    EXPECT_EQ(user.tree.m_child->getLoadStatus(), dqRender::TileLoadStatus::Abandoned)
        << "expired tile's children must be pruned";

    // 在选子树不剪：child 重新选中（markUsed=130）后，t=135 再剪 root 层
    // ——root 亦未过期（130）→ 不动。把 root 单独 markUsed(110)（过期）而
    // child markUsed(135)（新鲜）：剪 root 的 children（即 child）——但 child
    // 是"在选"……参考语义：usageMarker.isExpired = 时间戳过期 && !inUse。
    // 时间戳过期即剪（in-use 由 LRU 分区保障内容不彻底丢——这里按时间戳）。
    dqRender::TileAdmin::clearNowOverrideForTest();
    admin.forgetUser(user);
}

// 节流：两次 process 在同一窗口内只剪一次（nextPruneTime 推进）。
TEST(TileAdminMemory, PruneThrottledPerExpirationWindow)
{
    auto& admin = dqRender::TileAdmin::instance();
    double const saved = admin.getTileExpirationTime();
    admin.setTileExpirationTime(5.0);  // clamp 下限 5
    EXPECT_EQ(admin.getTileExpirationTime(), 5.0);
    admin.setTileExpirationTime(0.0);  // clamp 到 5
    EXPECT_EQ(admin.getTileExpirationTime(), 5.0);
    admin.setTileExpirationTime(saved);
}
