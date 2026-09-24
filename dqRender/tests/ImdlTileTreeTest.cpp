// ImdlTileTreeTest — per-model 树生产核心逻辑（H-5）。
//
// Ported from: itwinjs-core computeChildTileProps 的语义（TileMetadata.ts
//              :777-853——子分计算无独立测试文件，行为锚在其唯一调用者
//              IModelTile._loadChildren :153-169 与 ContentIdProvider 方案
//              :665-674；断言值从参考算法逐分支推导）。
// Authored: 参考无 computeChildTileProps 专项测试（行为经 TileIO 集成测试
//           间接覆盖）；本测试按参考算法分支构造断言（§5(f)）。
#include <gtest/gtest.h>

#include <dqRender/tile/ImdlTileTree.h>

#include <dqCommon/FeatureTable.h>
#include <dqRender/RenderGraphic.h>

namespace {

dqGeom::Range3d box(double x0, double y0, double z0, double x1, double y1, double z1)
{
    return dqGeom::Range3d::CreateXYZXYZ(x0, y0, z0, x1, y1, z1);
}

}  // namespace

// 3d 树：非叶、无 multiplier → 8 子，contentId 深度 +1、ijk 翻倍 + 位。
TEST(ImdlTileTree, Subdivides3dIntoEightChildren)
{
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0";
    parent.range = box(0, 0, 0, 8, 8, 8);
    parent.isLeaf = false;

    dqRender::ImdlTreeMetadata root;
    root.contentRange = box(-1, -1, -1, 9, 9, 9);  // 完全包含父 → 不做模型域拒绝
    root.is2d = false;

    auto children = dqRender::computeImdlChildTileProps(parent, root);
    ASSERT_EQ(children.size(), 8u);

    // 首子（i=j=k=0）：低半轴三分，contentId "1/0/0/0"。
    EXPECT_EQ(children[0].contentId, "1/0/0/0");
    EXPECT_NEAR(children[0].range.low.x, 0.0, 1e-12);
    EXPECT_NEAR(children[0].range.high.x, 4.0, 1e-12);
    EXPECT_NEAR(children[0].range.high.z, 4.0, 1e-12);

    // 末子（i=j=k=1）：高半轴，contentId "1/1/1/1"。
    EXPECT_EQ(children[7].contentId, "1/1/1/1");
    EXPECT_NEAR(children[7].range.low.x, 4.0, 1e-12);
    EXPECT_NEAR(children[7].range.high.z, 8.0, 1e-12);
}

// 2d 树：4 子。
TEST(ImdlTileTree, Subdivides2dIntoFourChildren)
{
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0";
    parent.range = box(0, 0, 0, 4, 4, 0);
    parent.isLeaf = false;

    dqRender::ImdlTreeMetadata root;
    root.is2d = true;

    auto children = dqRender::computeImdlChildTileProps(parent, root);
    EXPECT_EQ(children.size(), 4u);
}

// emptySubRangeMask：置位子体积跳过（TileMetadata.ts:826-830——位序
// 1<<(i + j*2 + k*4)）。
TEST(ImdlTileTree, EmptyMaskSkipsSubVolumes)
{
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0";
    parent.range = box(0, 0, 0, 8, 8, 8);
    parent.isLeaf = false;
    parent.emptySubRangeMask = 0x1 | 0x80;  // 子 0（i=j=k=0）与子 7（i=j=k=1）

    dqRender::ImdlTreeMetadata root;
    root.is2d = false;

    auto children = dqRender::computeImdlChildTileProps(parent, root);
    EXPECT_EQ(children.size(), 6u);
    for (auto const& c : children) {
        EXPECT_NE(c.contentId, "1/0/0/0");
        EXPECT_NE(c.contentId, "1/1/1/1");
    }
}

// 模型域拒绝：子范围与 contentRange 不相交 → 跳过（:841-845）。
TEST(ImdlTileTree, RejectsChildrenOutsideModelRange)
{
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0";
    parent.range = box(0, 0, 0, 8, 8, 8);
    parent.isLeaf = false;

    dqRender::ImdlTreeMetadata root;
    root.contentRange = box(0, 0, 0, 3, 8, 8);   // 只覆盖低 x 半边（父不被完全包含）
    root.is2d = false;

    auto children = dqRender::computeImdlChildTileProps(parent, root);
    // 低 x 的 4 子保留；高 x 的 4 子被拒。
    EXPECT_EQ(children.size(), 4u);
    for (auto const& c : children)
        EXPECT_NEAR(c.range.high.x, 4.0, 1e-12);
}

// magnification 分支：有 sizeMultiplier → 单子同体积、倍率×2（:785-799）。
TEST(ImdlTileTree, MagnificationDoublesMultiplier)
{
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0/1";
    parent.range = box(0, 0, 0, 5, 5, 5);
    parent.isLeaf = false;
    parent.sizeMultiplier = 1.0;

    dqRender::ImdlTreeMetadata root;
    auto children = dqRender::computeImdlChildTileProps(parent, root);
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(children[0].contentId, "0/0/0/0/2");
    EXPECT_DOUBLE_EQ(children[0].sizeMultiplier, 2.0);
    EXPECT_FALSE(children[0].isLeaf);
    EXPECT_NEAR(children[0].range.low.x, 0.0, 1e-12);
    EXPECT_NEAR(children[0].range.high.x, 5.0, 1e-12);
}

// 叶节点：无子（:781-783）。
TEST(ImdlTileTree, LeafHasNoChildren)
{
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0";
    parent.range = box(0, 0, 0, 1, 1, 1);
    parent.isLeaf = true;

    dqRender::ImdlTreeMetadata root;
    EXPECT_TRUE(dqRender::computeImdlChildTileProps(parent, root).empty());
}

// loadChildren：树根 → 8 ImdlTile 子（IModelTile._loadChildren :153-169）。
TEST(ImdlTileTree, LoadChildrenCreatesImdlTiles)
{
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-1, -1, -1, 9, 9, 9);
    dqRender::ImdlTileTree tree("test-model-tree", "0/0/0/0",
                                box(0, 0, 0, 8, 8, 8), meta);
    ASSERT_NE(tree.getRootTile(), nullptr);

    tree.getRootTile()->loadChildren();
    ASSERT_EQ(tree.getRootTile()->getChildren().size(), 8u);
    auto const* first = tree.getRootTile()->getChildren().front();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(static_cast<dqRender::ImdlTile const*>(first)->getContentId(), "1/0/0/0");
    EXPECT_EQ(first->getDepth(), 1u);
}

// contentId 解析/格式化往返（V1 方案 :665-674）。
TEST(ImdlTileTree, ContentIdRoundtrip)
{
    dqRender::ImdlContentIdSpec spec{2, 3, 1, 0, 0};
    EXPECT_EQ(dqRender::formatImdlContentId(spec), "2/3/1/0");
    auto parsed = dqRender::parseImdlContentId("2/3/1/0");
    EXPECT_EQ(parsed.depth, 2u);
    EXPECT_EQ(parsed.i, 3u);
    EXPECT_EQ(parsed.j, 1u);
    EXPECT_EQ(parsed.k, 0u);

    spec.mult = 4;
    EXPECT_EQ(dqRender::formatImdlContentId(spec), "2/3/1/0/4");
    parsed = dqRender::parseImdlContentId("2/3/1/0/4");
    EXPECT_EQ(parsed.mult, 4u);
}
