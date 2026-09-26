// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileDrawArgs reference-surface tests
//
// Ported from: itwinjs-core TileDrawArgs.ts insertMissing/markChildrenLoading/
//              markUsed/markReady (:402-421) + SceneContext missing-set
//              semantics (ViewContext.ts:421-434) + getPixelSize sphere path
//              (:138-148) + computePixelSizeInMetersAtClosestPoint (:190-211).
// Authored: no direct reference unit test exists for these faces (the TS tests
//           exercise them only through tile-tree integration — frontend/src/
//           test/tile/ has no TileDrawArgs suite); the scenario matrix and the
//           numeric assertions below are derived from the reference formulas
//           (§5(f)), values pinned per the implementation plan (task 2 step 2).

#include "dqRender/RenderGraphic.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/TileDrawArgs.h"
#include "dqRender/tile/TileTree.h"

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

using namespace dqRender;

namespace {

// Minimal RenderGraphic stub (Tile content only needs a non-null graphic).
class StubGraphic : public RenderGraphic {
public:
    void unionRange(dqGeom::Range3d&) const override {}
};

// Stub tile: the marker/pixel-size faces never touch content machinery.
class StubTile : public Tile {
public:
    StubTile(TileTree& tree, dqGeom::Range3d const& range)
        : Tile(tree, nullptr, range)
    {
    }
    bool requestContent() override { return false; }
    TileContent readContent(uint8_t const*, size_t) override { return {}; }
    void loadChildren() override {}

    void loadContent(size_t bytesUsed)
    {
        setBytesUsed(bytesUsed);
        TileContent content;
        content.graphic = std::make_unique<StubGraphic>();
        content.isLeaf = true;
        setContent(std::move(content));
    }
};

// Stub tree: computeVisibility is never reached by these tests; null root
// (TileTree tolerates it — selectTiles guards, TileTree.cpp:33).
class StubTree : public TileTree {
public:
    StubTree()
        : TileTree(nullptr)
    {
    }
    TileVisibility computeVisibility(TileDrawArgs&, Tile*) override
    {
        return TileVisibility::Visible;
    }
};

}  // namespace

// Ported from: itwinjs-core TileDrawArgs.ts insertMissing/markReady/markUsed
//              (:402-419) + SceneContext missing 集语义（ViewContext.ts:421-434）。
// 标记语义不触 Tile 内容：insertMissing/markReady 只操作集合（参考 insertMissing
// → SceneContext.insertMissingTile 的 Set 语义，去重由集合承担；load-status 门
// 在 SceneContext 侧，ViewContext.ts:421-429，不在 TileDrawArgs 侧）。
TEST(TileDrawArgsTest, MissingAndReadyMarkers)
{
    TileDrawArgs args;
    Tile* t = reinterpret_cast<Tile*>(0x10);  // 标记语义不触 Tile 内容
    args.insertMissing(t);
    args.insertMissing(t);  // 去重
    EXPECT_EQ(args.getMissingTiles().size(), 1u);
    EXPECT_TRUE(args.hasMissingTiles());
    args.markReady(t);
    EXPECT_TRUE(args.isTileReady(t));
    args.markChildrenLoading();
    EXPECT_TRUE(args.areChildrenLoading());
}

// Ported from: TileDrawArgs.ts getPixelSize (:138-148) —— 包围球路径：
// pixelSize = (0.5·diag).length / metersPerPixelAtClosestPoint。
// 场景：range=(-1,-1,-1)..(1,1,1) → diag=(2,2,2)，radius=0.5·|diag|=sqrt(3)
//（getTileRadius，TileDrawArgs.ts:319-328）。OBB 角点路径（参考
// getPixelSizeFromProjection :151-176）DanQing 无 OBB 表示——不移植：
// EQUIVALENCE: 参考源=TileDrawArgs.ts:151-176（OBB 角点路径）；发散=DanQing
// Tile 仅含包围球（BoundingSphere，Tile.h:32-35），无 OBB 表示；验证法=球路径
// 两组数值锁 + 像素锁。
TEST(TileDrawArgsTest, GetPixelSizeSpherePath)
{
    StubTree tree;
    StubTile tile(tree, dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1));
    double const r = std::sqrt(3.0);

    // 正交组：cameraOn=false、pixelSizeRatio=0.5（world/px）——参考
    // :190-211 的 worldToViewMap 均匀段 → metersPerPixel = pixelSizeRatio。
    {
        TileDrawArgs args;
        args.cameraOn = false;
        args.pixelSizeRatio = 0.5f;
        EXPECT_NEAR(args.getPixelSize(tile), std::sqrt(3.0) / 0.5, 1e-6);
    }

    // 透视组：cameraOn=true、cameraEye=(0,0,10)、perspectiveScale=s——最近点
    // 距 = 球心距 - 半径 = 10 - r（参考 :198-204 最近球点），metersPerPixel =
    // (10 - r) * s（参考 :208-210 的 worldToViewMap 回程的 pinhole 折算）。
    {
        TileDrawArgs args;
        args.cameraOn = true;
        args.cameraEye[0] = 0.0f;
        args.cameraEye[1] = 0.0f;
        args.cameraEye[2] = 10.0f;
        float const s = 0.25f;
        args.perspectiveScale = s;
        EXPECT_NEAR(args.getPixelSize(tile), r / ((10.0 - r) * s), 1e-6);
    }

    // 近平面夹冲分支（参考 :194-196“球与近平面重叠”守卫的 DanQing pinhole
    // 等价）：眼距 < 半径 → 夹紧生效，值有限（不发散）。
    {
        TileDrawArgs args;
        args.cameraOn = true;
        args.cameraEye[0] = 0.0f;
        args.cameraEye[1] = 0.0f;
        args.cameraEye[2] = 0.5f;  // dist = 0.5 < r
        args.perspectiveScale = 0.25f;
        double const v = args.getPixelSize(tile);
        EXPECT_TRUE(std::isfinite(v)) << v;
        EXPECT_GT(v, 0.0);
    }
}

// Ported from: TileDrawArgs.ts markUsed (:412-414) —— 参考为
// tile.usageMarker.mark(viewport, now)；DanQing 的 usage-marker 时间戳面 =
// Tile.markUsed(nowSeconds)（Tile.h:82-87）+ touched 集（参考 touchedTiles，
// TileDrawArgs.ts:112-115——“内容保留在内存”的标记，经 TileAdmin.addTilesForUser
// 的 LRU markUsed 生效，TileAdmin.ts:519-520）。
TEST(TileDrawArgsTest, MarkUsedRecordsTimestampAndTouchedSet)
{
    StubTree tree;
    StubTile tile(tree, dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1));
    TileDrawArgs args;

    TileAdmin::setNowOverrideForTest(123.0);
    args.markUsed(&tile);
    TileAdmin::clearNowOverrideForTest();

    EXPECT_EQ(args.getTouchedTiles().size(), 1u);
    EXPECT_EQ(args.getTouchedTiles()[0], &tile);
    EXPECT_DOUBLE_EQ(tile.getLastUsedTime(), 123.0);
}

// Ported from: itwinjs-core TileDrawArgs field defaults —— parentsAndChildrenExclusive
//（TileDrawArgs.ts:105，来源 TileTree.parentsAndChildrenExclusive 恒 true，
// TileTree.ts:110-113）+ pixelSizeScaleFactor（:127，computePixelSizeScaleFactor
// :240-265 在无非均匀 model display transform 时 = 1）+ tileSizeModifier（:313）。
TEST(TileDrawArgsTest, ReferenceFieldDefaults)
{
    TileDrawArgs args;
    EXPECT_TRUE(args.parentsAndChildrenExclusive);
    EXPECT_FLOAT_EQ(args.pixelSizeScaleFactor, 1.0f);
    EXPECT_FLOAT_EQ(args.tileSizeModifier, 1.0f);
    EXPECT_FALSE(args.hasMissingTiles());
    EXPECT_FALSE(args.areChildrenLoading());
    EXPECT_TRUE(args.getMissingTiles().empty());
    EXPECT_TRUE(args.getReadyTiles().empty());
}
