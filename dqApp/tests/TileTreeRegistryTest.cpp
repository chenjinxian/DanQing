// TileTreeRegistryTest — Tiles 注册表单测（忠实路径 F3）。
//
// Ported from: itwinjs-core core/frontend/src/test/tile/Tiles.test.ts
//   TEST "resetTileTreeOwner disposes only the target tree"（:169-209）
//   TEST "getTileTreeOwner returns the same owner for the same supplier+id"
//   （观察法：onTileTreeLoad 事件——TreeOwner.load() 无 Promise 可 await，
//   DanQing 同步等价）
// DanQing 适配：TestSupplier/TestTree 现场桩（参考 Tiles.test.ts:21-153 的
// TestTree/Supplier in-file mock 模式）；BlankConnection 作注册表宿主
// （Tiles 只持有引用、不触碰——传 props 最小构造）。
#include <gtest/gtest.h>

#include "dqApp/BlankConnection.h"
#include "dqApp/ViewState.h"
#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型
#include <dqRender/RenderGraphic.h>
#include "dqApp/tile/TileTreeOwner.h"
#include "dqApp/tile/SpatialTileTreeReferences.h"
#include "dqApp/tile/Tiles.h"
#include <dqRender/tile/TileAdmin.h>
#include <dqRender/tile/TileDrawArgs.h>

#include <memory>
#include <set>
#include <vector>

namespace {

// TestTree — 桩树（Tiles.test.ts TestTree :26-44 对应物）。
class TestTree : public dqRender::TileTree {
public:
    TestTree() : TileTree(nullptr) {}

    dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                               dqRender::Tile*) override
    {
        return dqRender::TileVisibility::Visible;
    }
};

// NullRangeTree — 根范围 null（→ NotFound，Tiles.ts:56 语义）。
class NullRangeTree : public dqRender::TileTree {
public:
    NullRangeTree()
        : TileTree(nullptr, dqRender::TileLoadPriority::Primary, 0.0f)
    {
        setRootTile(std::make_unique<NullRangeTile>(*this));
    }

    dqRender::TileVisibility computeVisibility(dqRender::TileDrawArgs&,
                                               dqRender::Tile*) override
    {
        return dqRender::TileVisibility::Visible;
    }

    class NullRangeTile : public dqRender::Tile {
    public:
        explicit NullRangeTile(dqRender::TileTree& tree)
            : dqRender::Tile(tree, nullptr, dqGeom::Range3d())  // null range
        {
        }
        bool requestContent() override { return false; }
        dqRender::TileContent readContent(uint8_t const*, size_t) override { return {}; }
        void loadChildren() override {}
    };
};

// TestSupplier — 计数 + 可配置产物的桩（Tiles.test.ts Supplier :46-64）。
class TestSupplier : public dqApp::TileTreeSupplier {
public:
    int compareTileTreeIds(dqApp::TileTreeId const& lhs,
                           dqApp::TileTreeId const& rhs) const override
    {
        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
    }

    std::unique_ptr<dqRender::TileTree> createTileTree(
        dqApp::TileTreeId const& id, dqApp::IModelConnection&) override
    {
        ++createCount;
        if (failIds.count(id) > 0)
            return nullptr;
        if (nullRangeIds.count(id) > 0)
            return std::make_unique<NullRangeTree>();
        return makeLoadedTree();
    }

    static std::unique_ptr<dqRender::TileTree> makeLoadedTree()
    {
        auto tree = std::make_unique<TestTree>();
        // Root tile with a non-null range → Loaded (Tiles.ts:56).
        struct RangeTile : dqRender::Tile {
            explicit RangeTile(dqRender::TileTree& t)
                : dqRender::Tile(t, nullptr,
                                 dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1))
            {
            }
            bool requestContent() override { return false; }
            dqRender::TileContent readContent(uint8_t const*, size_t) override { return {}; }
            void loadChildren() override {}
        };
        tree->setRootTile(std::make_unique<RangeTile>(*tree));
        return tree;
    }

    int createCount = 0;
    std::set<std::string> failIds;
    std::set<std::string> nullRangeIds;
};

}  // namespace

// 同 (supplier, id) → 同一 owner；不同 id/supplier → 不同 owner。
// Ported from: Tiles.test.ts（owner 缓存语义——getTileTreeOwner 的 Map 键）。
namespace {
dqBase::RefPtr<dqApp::BlankConnection> makeBlank()
{
    dqApp::BlankConnectionProps props;
    return dqApp::BlankConnection::create(props);
}
}  // namespace

TEST(TileTreeRegistry, OwnerIsCachedPerSupplierAndId)
{
    auto iModel = makeBlank();
    dqApp::Tiles tiles(*iModel);
    TestSupplier supplier;

    auto& a1 = tiles.getTileTreeOwner("a", supplier);
    auto& a2 = tiles.getTileTreeOwner("a", supplier);
    auto& b = tiles.getTileTreeOwner("b", supplier);
    EXPECT_EQ(&a1, &a2);
    EXPECT_NE(&a1, &b);

    TestSupplier supplier2;
    auto& a3 = tiles.getTileTreeOwner("a", supplier2);
    EXPECT_NE(&a1, &a3);
}

// load()：Loaded 状态 + onTileTreeLoad 事件 + 不重复创建。
// Ported from: Tiles.test.ts（onTileTreeLoad 观察法，:134-141）。
TEST(TileTreeRegistry, LoadCreatesOnceAndRaisesEvent)
{
    auto iModel = makeBlank();
    dqApp::Tiles tiles(*iModel);
    TestSupplier supplier;

    int events = 0;
    auto disconnect = dqRender::TileAdmin::instance().onTileTreeLoad.AddListener(
        [&events](dqRender::TileTree&) { ++events; });

    auto& owner = tiles.getTileTreeOwner("a", supplier);
    EXPECT_EQ(owner.getLoadStatus(), dqRender::TileTreeLoadStatus::NotLoaded);

    dqRender::TileTree* tree = owner.load();
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(owner.getLoadStatus(), dqRender::TileTreeLoadStatus::Loaded);
    EXPECT_EQ(supplier.createCount, 1);
    EXPECT_EQ(events, 1);

    owner.load();
    owner.load();
    EXPECT_EQ(supplier.createCount, 1) << "repeated load must not re-create";

    disconnect();
}

// 创建失败（nullptr）与 null 根范围 → NotFound（Tiles.ts:55-58）。
TEST(TileTreeRegistry, FailureAndNullRangeYieldNotFound)
{
    auto iModel = makeBlank();
    dqApp::Tiles tiles(*iModel);
    TestSupplier supplier;
    supplier.failIds.insert("bad");
    supplier.nullRangeIds.insert("nullish");

    auto& bad = tiles.getTileTreeOwner("bad", supplier);
    EXPECT_EQ(bad.load(), nullptr);
    EXPECT_EQ(bad.getLoadStatus(), dqRender::TileTreeLoadStatus::NotFound);

    auto& nullish = tiles.getTileTreeOwner("nullish", supplier);
    EXPECT_EQ(nullish.load(), nullptr);
    EXPECT_EQ(nullish.getLoadStatus(), dqRender::TileTreeLoadStatus::NotFound);
}

// resetTileTreeOwner 只处置目标树：同 id 再取是新 owner（新树），其它 owner 不动。
// Ported from: Tiles.test.ts "resetTileTreeOwner disposes only the target tree"
// (:169-209)。
TEST(TileTreeRegistry, ResetDisposesOnlyTargetTree)
{
    auto iModel = makeBlank();
    dqApp::Tiles tiles(*iModel);
    TestSupplier supplier;

    auto* ownerA = &tiles.getTileTreeOwner("a", supplier);
    auto* ownerB = &tiles.getTileTreeOwner("b", supplier);
    dqRender::TileTree* treeB = ownerB->load();
    ASSERT_NE(treeB, nullptr);
    ASSERT_NE(ownerA->load(), nullptr);

    tiles.resetTileTreeOwner("a", supplier);

    // b 未被处置（同一棵树）。
    EXPECT_EQ(ownerB->getTileTree(), treeB);
    EXPECT_EQ(ownerB->getLoadStatus(), dqRender::TileTreeLoadStatus::Loaded);

    // a 同 id 再取 → 新 owner（旧树已被处置）。指针同一性在释放后不可靠
    // （堆回收可能复用地址——全量跑堆碎片时 ownerA2==ownerA 间歇恒真，本测试
    // 历史 flake 实锤；TD-12 同类修正见 DropSupplierForgetsAllItsOwners）：
    // 改用状态判定——新 owner 必须是 NotLoaded（fresh）。
    auto* ownerA2 = &tiles.getTileTreeOwner("a", supplier);
    EXPECT_EQ(ownerA2->getLoadStatus(), dqRender::TileTreeLoadStatus::NotLoaded);
    // 旧 owner 经 reset 已不在注册表内——不触碰（参考 erase 后 owner 失效）。
}

// dropSupplier 处置该 supplier 全部树并移除注册。
TEST(TileTreeRegistry, DropSupplierForgetsAllItsOwners)
{
    auto iModel = makeBlank();
    dqApp::Tiles tiles(*iModel);
    TestSupplier supplier;
    TestSupplier other;

    auto* a = &tiles.getTileTreeOwner("a", supplier);
    auto* b = &tiles.getTileTreeOwner("b", supplier);
    auto* c = &tiles.getTileTreeOwner("c", other);
    a->load();
    b->load();
    c->load();

    dqRender::TileTreeLoadStatus const aStatusBefore = a->getLoadStatus();  // Loaded
    tiles.dropSupplier(supplier);

    // 指针同一性在释放后不可靠（堆回收可能复用地址——全量跑堆碎片时
    // a2==a 恒真）；改用状态判定：新 owner 必须是 NotLoaded（fresh）。
    auto& a2 = tiles.getTileTreeOwner("a", supplier);
    EXPECT_EQ(a2.getLoadStatus(), dqRender::TileTreeLoadStatus::NotLoaded)
        << "owner must be re-created after dropSupplier (fresh NotLoaded, was "
        << static_cast<int>(aStatusBefore) << ")";
    // other supplier 不受影响（其 owner 仍 Loaded）。
    EXPECT_EQ(&tiles.getTileTreeOwner("c", other), c);
}

// ---------------------------------------------------------------------------
// 工厂 seam 覆写（frontend-tiles initializeFrontendTiles 的 DanQing 等价证明：
// FrontendTiles.ts:215 替换 SpatialTileTreeReferences.create 后视图的 refs
// 来自注入实现）。
// Authored: no reference test drives the seam without a browser app harness;
// the semantics port from FrontendTiles.ts:215 + PrimaryTileTree.ts:601-606.
// ---------------------------------------------------------------------------
namespace {

// 占位 owner（OverrideRef 需要 owner；树为空即可）。
class DirectTileTreeOwnerForTest final : public dqApp::TileTreeOwner {
public:
    explicit DirectTileTreeOwnerForTest(dqRender::TileTree* tree) : m_tree(tree) {}
    dqRender::TileTree* getTileTree() const noexcept override { return m_tree; }
    dqRender::TileTreeLoadStatus getLoadStatus() const noexcept override
    {
        return dqRender::TileTreeLoadStatus::NotFound;
    }
    dqRender::TileTree* load() override { return m_tree; }
    void dispose() override {}

private:
    dqRender::TileTree* m_tree;
};

// OverrideRef — 覆写工厂返回的 ref（frontend-tiles BatchedTileTreeReference
// 的最小对应物；只验证 refs 通道，不渲染）。
class OverrideRef final : public dqApp::TileTreeReference {
public:
    dqApp::TileTreeOwner& getTreeOwner() override { return m_owner; }

private:
    DirectTileTreeOwnerForTest m_owner{nullptr};
};

}  // namespace

TEST(SpatialTileTreeReferencesFactory, OverrideReplacesDefaultRefs)
{
    using namespace dqApp;

    // 默认：空 refs（DanQing 无 per-model 树生产）。
    {
        auto view = SpatialViewState::CreateBlank(nullptr, dqGeom::Point3d(0, 0, 0),
                                                  dqGeom::Vector3d(100, 100, 100));
        ASSERT_TRUE(static_cast<bool>(view));
        int count = 0;
        view->ForEachModelTreeRef([&count](TileTreeReference&) { ++count; });
        EXPECT_EQ(count, 0) << "default spatial refs must be empty";
    }

    // 覆写后：refs 来自注入工厂（frontend-tiles 模式）。
    SpatialTileTreeReferences::setCreateOverride(
        [](SpatialViewState&) -> std::unique_ptr<SpatialTileTreeReferences> {
            struct OverrideRefs final : SpatialTileTreeReferences {
                void forEachTileTreeRef(
                    std::function<void(TileTreeReference&)> const& func) const override
                {
                    for (auto& ref : const_cast<std::vector<std::unique_ptr<OverrideRef>>&>(refs))
                        func(*ref);
                }
                std::vector<std::unique_ptr<OverrideRef>> refs;
            };
            auto out = std::make_unique<OverrideRefs>();
            out->refs.push_back(std::make_unique<OverrideRef>());
            out->refs.push_back(std::make_unique<OverrideRef>());
            return out;
        });
    ASSERT_TRUE(SpatialTileTreeReferences::hasCreateOverride());

    {
        auto view = SpatialViewState::CreateBlank(nullptr, dqGeom::Point3d(0, 0, 0),
                                                  dqGeom::Vector3d(100, 100, 100));
        ASSERT_TRUE(static_cast<bool>(view));
        int count = 0;
        view->ForEachModelTreeRef([&count](TileTreeReference&) { ++count; });
        EXPECT_EQ(count, 2) << "overridden factory must supply the view's refs";
    }

    // 清理覆写（测试卫生——影响全局工厂）。
    SpatialTileTreeReferences::clearCreateOverride();
    ASSERT_FALSE(SpatialTileTreeReferences::hasCreateOverride());

    {
        auto view = SpatialViewState::CreateBlank(nullptr, dqGeom::Point3d(0, 0, 0),
                                                  dqGeom::Vector3d(100, 100, 100));
        int count = 0;
        view->ForEachModelTreeRef([&count](TileTreeReference&) { ++count; });
        EXPECT_EQ(count, 0) << "cleared override must restore the default";
    }
}
