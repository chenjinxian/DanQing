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
#include <dqRender/RenderSystem.h>
#include <dqRender/tile/ImdlHeader.h>

#include "render/Batch.h"
#include "render/Graphic.h"

#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"

#include <memory>
#include <vector>

using namespace dqRender::fixtures;

namespace {

dqGeom::Range3d box(double x0, double y0, double z0, double x1, double y1, double z1)
{
    return dqGeom::Range3d::CreateXYZXYZ(x0, y0, z0, x1, y1, z1);
}

// 桩 RenderSystem：dqRenderTest 无 GL 环境，承接 readContent 的
// createGraphicFromPolyface/createGraphicList/createBatch 三调用。
// createBatch 镜像 OpenGLRenderSystem::createBatch 的真 Batch 路径
//（System.ts:445-463——graphic 判型 asBatch 与 feature table 复制均走真
// 实现，本测试锁的是 readContent 的接线而非桩行为）。
class ReadContentStubGraphic final : public dqRender::Graphic {
public:
    void addCommands(dqRender::RenderCommands&) override {}
};

class ReadContentStubSystem final : public dqRender::RenderSystem {
public:
    bool isValid() const noexcept override { return true; }
    std::unique_ptr<dqRender::RenderTarget> createTarget(void*, uint32_t, uint32_t) override
    {
        return nullptr;
    }
    std::unique_ptr<dqRender::GraphicBuilder> createGraphicBuilder(
        const dqRender::GraphicBuilderOptions&) override
    {
        return nullptr;
    }
    dqRender::GraphicBranch* createBranch(bool = true) override { return nullptr; }
    dqRender::RenderGraphic* createBranchGraphic(dqRender::GraphicBranch*) override
    {
        return nullptr;
    }
    dqRender::RenderGraphic* createGraphicList(
        std::vector<dqRender::RenderGraphic*> graphics) override
    {
        if (graphics.empty())
            return nullptr;
        if (graphics.size() == 1)
            return graphics[0];
        auto* arr = new dqRender::GraphicsArray();
        for (auto* g : graphics)
            arr->add(std::unique_ptr<dqRender::Graphic>(static_cast<dqRender::Graphic*>(g)));
        return arr;
    }
    dqRender::RenderGraphicOwner* createGraphicOwner(dqRender::RenderGraphic* owned) override
    {
        return owned ? new dqRender::RenderGraphicOwner(owned) : nullptr;
    }
    dqRender::RenderGraphic* createGraphicFromPolyface(void const* polyface, uint32_t,
                                                       uint32_t,
                                                       dqRender::rhi::TextureHandle) override
    {
        if (!polyface)
            return nullptr;
        return new ReadContentStubGraphic();
    }
    // 镜像 OpenGLRenderSystem::createBatch（System.ts:445-463）：真 Batch +
    // feature table 复制 + 范围记录。
    dqRender::RenderGraphic* createBatch(dqRender::RenderGraphic* graphic,
                                         dqCommon::FeatureTable const* featureTable,
                                         dqGeom::Range3d const& range) override
    {
        if (!graphic)
            return nullptr;
        std::unique_ptr<dqCommon::FeatureTable> table;
        uint32_t featureCount = 1;
        if (featureTable) {
            auto t = std::make_unique<dqCommon::FeatureTable>(
                featureTable->getMaxFeatures(), featureTable->getModelId(),
                featureTable->getType());
            for (int i = 0; i < featureTable->getArraySize(); ++i) {
                auto const& indexed = featureTable->getArray()[i];
                t->insertWithIndex(indexed.value, indexed.index);
            }
            featureCount = static_cast<uint32_t>(t->getSize());
            table = std::move(t);
        }
        auto* batch = new dqRender::Batch(featureCount, std::move(table));
        batch->setChild(std::unique_ptr<dqRender::Graphic>(
            static_cast<dqRender::Graphic*>(graphic)));
        batch->setRange(range);
        return batch;
    }
};

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

// 模型域拒绝：子范围与 contentRange 不相交 → 跳过（:836-840）。
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

// readContent 的 FeatureTable 链接 + Batch 包裹（U8 归位）。
// Ported from: itwinjs-core ImdlReader.ts:110-123（readContent 的
//              featureTable 产出与 Batch 包裹链——decodeImdlGraphics 后
//              convertFeatureTable + system.createBatch）+
//              ParseImdlDocument.ts:1259-1266（convertFeatureTable 的
//              PackedFeatureTable 构造）+ :1278-1284（FT 头读后 seek 到
//              ftStartPos+length 的定位语义）。
//              测试形态 Authored：参考侧对应覆盖在 IModelTileReader 集成
//              测试（依赖 RPC），DanQing 用录制 fixture 直读（§5(f)）。
TEST(ImdlTileTreeTest, ReadContentPopulatesFeatureTable)
{
    // 建树模式照 ImdlTileTree.LoadChildrenCreatesImdlTiles（本文件——
    // ImdlTileTree(treeId, rootContentId, rootRange, ImdlTreeMetadata)）。
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-1, -1, -1, 9, 9, 9);
    dqRender::ImdlTileTree tree("fixture://minimal-imdl", "0/0/0/0",
                                box(0, 0, 0, 8, 8, 8), meta);
    // readContent 从树取 RenderSystem（TileTree.h 注入约定）——桩系统承接
    // 三个工厂调用（真 Batch 路径，见文件头桩类注释）。
    ReadContentStubSystem system;
    tree.setRenderSystem(&system);

    // fixture 根 tile 的 FT 头真值（自洽断言基准：feature 数与头 count 一致，
    // 词向量布局 3×u32/feature + 2×u32/subcat tail——PackedFeatureTable.ts）。
    dqRender::ImdlFeatureTableHeader ftHeader;
    {
        dqRender::ImdlByteStream stream(V1_1::rectangleBytes, V1_1::rectangleSize);
        auto const header = dqRender::ImdlHeader::readFrom(stream);
        ASSERT_TRUE(header.isValid());
        ASSERT_TRUE(dqRender::ImdlFeatureTableHeader::readFrom(stream, ftHeader));
    }

    auto* root = tree.getRootTile();
    ASSERT_NE(root, nullptr);
    auto content = root->readContent(V1_1::rectangleBytes, V1_1::rectangleSize);

    // FT 头已由 ImdlHeader/ImdlDocument 测试锁定可读——本测试锁定链接结果：
    // featureTable 非空、feature 数与 fixture FT 头 count 一致（参考
    // expectNumFeatures(batch, 1)——TileIO.test.ts rectangle 单元素场景）、
    // feature0 解析非零 elementId。
    ASSERT_NE(content.featureTable, nullptr);
    EXPECT_EQ(content.featureTable->getSize(), static_cast<int>(ftHeader.count));
    auto f0 = content.featureTable->findFeature(0);
    ASSERT_TRUE(f0.has_value());
    printf("[IMDL-READCONTENT] count=%u feature0 elementId=%s subCategoryId=%s\n",
           ftHeader.count, f0->elementId.ToString().c_str(),
           f0->subCategoryId.ToString().c_str());
    EXPECT_NE(f0->elementId.GetValue(), 0u);
    // 钉死录制夹具真值（v1.1 rectangle = TileIO.data.ts 的绿矩形场景；
    // fixture 字节或 FT 解析链任一变化即红）。
    EXPECT_EQ(f0->elementId.GetValue(), 0x4eu);
    EXPECT_EQ(f0->subCategoryId.GetValue(), 0x18u);

    // graphic 被 Batch 包裹（pickable 的前提，ImdlReader.ts:122-123）：
    // content.graphic 指向 Batch 且 Batch 持有 feature table。判型走
    // RenderGraphic::asBatch()——itwinjs `instanceof Batch`（Graphic.ts）
    // 的 -fno-rtti 等价（PlanarClassifier.ts:374）。
    auto* batch = content.graphic ? content.graphic->asBatch() : nullptr;
    ASSERT_NE(batch, nullptr);
    EXPECT_EQ(batch->getFeatureCount(),
              static_cast<uint32_t>(content.featureTable->getSize()));
    ASSERT_NE(batch->getFeatureTable(), nullptr);
    EXPECT_TRUE(batch->isPickable());
}

// maximumSize 数据链 + 真 SSE 判定式（U9(2)）。
// Ported from: TileMetadata.ts computeChildTileProps (:795/:847 —— 每孩子
//              maximumSize = root.tileScreenSize) + IModelTile.ts:82-83
//              (sizeMultiplier 乘法) + Tile.ts:459-463 判定式
//              (pixelSize <= maximumSize * tileSizeModifier)。
TEST(ImdlTileTreeTest, MaximumSizeChainAndSseTest)
{
    // --- ① computeImdlChildTileProps：每孩子 maximumSize = root.tileScreenSize
    //       （:847 剖分分支 / :795 magnification 分支）。tileScreenSize 取非
    //       缺省值 256——证明取自元数据而非硬编码。
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "0/0/0/0";
    parent.range = box(0, 0, 0, 8, 8, 8);
    parent.isLeaf = false;

    dqRender::ImdlTreeMetadata root;
    root.contentRange = box(-1, -1, -1, 9, 9, 9);
    root.is2d = false;
    root.tileScreenSize = 256;

    auto children = dqRender::computeImdlChildTileProps(parent, root);
    ASSERT_EQ(children.size(), 8u);
    for (auto const& c : children)
        EXPECT_DOUBLE_EQ(c.maximumSize, 256.0);

    parent.sizeMultiplier = 1.0;
    auto mag = dqRender::computeImdlChildTileProps(parent, root);
    ASSERT_EQ(mag.size(), 1u);
    EXPECT_DOUBLE_EQ(mag[0].maximumSize, 256.0);
    parent.sizeMultiplier = 0.0;

    // --- ② ImdlTile::getMaximumSize = base × (sizeMultiplier > 0 ?
    //       sizeMultiplier : 1)（IModelTile.ts:82-83）。根 leg：树构造填
    //       tileScreenSize。
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-1, -1, -1, 9, 9, 9);
    meta.tileScreenSize = 512;
    dqRender::ImdlTileTree tree("test-max-size-tree", "0/0/0/0",
                                box(0, 0, 0, 8, 8, 8), meta);
    ASSERT_NE(tree.getRootTile(), nullptr);
    EXPECT_DOUBLE_EQ(tree.getRootTile()->getMaximumSize(), 512.0);

    dqRender::ImdlTile magnified(tree, tree.getRootTile(), "0/0/0/0/2",
                                 box(0, 0, 0, 8, 8, 8), 2.0, 512.0);
    EXPECT_DOUBLE_EQ(magnified.getMaximumSize(), 1024.0);
    dqRender::ImdlTile plain(tree, tree.getRootTile(), "1/0/0/0",
                             box(0, 0, 0, 8, 8, 8), 0.0, 512.0);
    EXPECT_DOUBLE_EQ(plain.getMaximumSize(), 512.0);

    // --- ③ 判定式数值锁（Tile.ts:459-463）。camera 关（正交段）→
    //       getPixelSize = radius / pixelSizeRatio（TileDrawArgs 球路径）。
    //       pixelSizeRatio=1、radius=512（range 对角 1024）→ pixelSize=512。
    //       非叶 tile（contentId 非空且未 setContent）走 SSE 分支——叶在
    //       Tile.ts:445-449 早退 Visible，不进判定式。
    dqRender::TileDrawArgs args;  // cameraOn=false、ratio=1、双 modifier=1
    dqRender::ImdlTile onBoundary(tree, tree.getRootTile(), "1/0/0/0",
                                  box(0, 0, 0, 1024, 0, 0), 0.0, 512.0);
    // 512 <= 512 → Visible（边界含等号，Tile.ts:461）。
    EXPECT_EQ(tree.computeVisibility(args, &onBoundary),
              dqRender::TileVisibility::Visible);

    // 513 > 512 → TooCoarse。旧近似公式（geometricError = 512/2^depth =
    // 512/2 = 256，sse = 256/513 ≈ 0.499 <= 16）同输入判 Visible——
    // 本断言即"切换真实发生"的证明。
    dqRender::ImdlTile overBoundary(tree, tree.getRootTile(), "1/0/0/0",
                                    box(0, 0, 0, 1026, 0, 0), 0.0, 512.0);
    EXPECT_EQ(tree.computeVisibility(args, &overBoundary),
              dqRender::TileVisibility::TooCoarse);
}

// setContent 回填 maximumSize（IModelTile.ts:140-142——content.graphic 到达
// 且 maximumSize==0 时取 tree.tileScreenSize；非零不被覆盖）。
// Ported from: itwinjs-core IModelTile.setContent (:140-142)。
// M-F(1) 归位：回填随 IModelTile.setContent 语义落在 ImdlTile::setContent
// （经 Tile& 虚分派——生产路径 TileAdmin::deliverTileContent 的
// readContent → setContent 链）。
TEST(ImdlTileTreeTest, MaximumSizeBackfilledWhenContentLoaded)
{
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-1, -1, -1, 9, 9, 9);
    dqRender::ImdlTileTree tree("fixture://minimal-imdl", "0/0/0/0",
                                box(0, 0, 0, 8, 8, 8), meta);
    ReadContentStubSystem system;
    tree.setRenderSystem(&system);

    // maximumSize=0 构造（参考 tree props 未带 maximumSize 的形态——
    // undisplayable 直到内容到达）。
    dqRender::ImdlTile tile(tree, nullptr, "0/0/0/0", box(0, 0, 0, 8, 8, 8),
                            0.0, 0.0);
    ASSERT_DOUBLE_EQ(tile.getMaximumSize(), 0.0);

    auto content = tile.readContent(V1_1::rectangleBytes, V1_1::rectangleSize);
    ASSERT_NE(content.graphic, nullptr);
    tile.setContent(std::move(content));  // deliverTileContent 的生产链形态
    EXPECT_DOUBLE_EQ(tile.getMaximumSize(),
                     static_cast<double>(tree.metadata().tileScreenSize));

    // 已有非零 maximumSize 不被覆盖（参考 `0 === this.maximumSize` 门）。
    dqRender::ImdlTile preset(tree, nullptr, "0/0/0/0", box(0, 0, 0, 8, 8, 8),
                              0.0, 64.0);
    auto presetContent =
        preset.readContent(V1_1::rectangleBytes, V1_1::rectangleSize);
    ASSERT_NE(presetContent.graphic, nullptr);
    preset.setContent(std::move(presetContent));
    EXPECT_DOUBLE_EQ(preset.getMaximumSize(), 64.0);
}

// ---------------------------------------------------------------------------
// ImdlTile::setContent — IModelTile.setContent 语义锁（M-F(1)）。
//
// Ported from: itwinjs-core IModelTile.setContent (IModelTile.ts:134-148 ——
//              emptySubRangeMask 赋值 (:136) / maximumSize 回填 (:140-142) /
//              sizeMultiplier 升门控 (:145) + contentId 覆写 (:147) /
//              子代 >1 disposeChildren (:148))。
// Authored: 场景自写（参考无 setContent 直接单测——覆盖在树集成）。
// 桩树/provider 复用本文件 V4 键域形态（TreeOverridesRootKeyWhenPropsCarry-
// FormatVersion——formatVersion=37.0 → provider 覆写根键为 "-b-0-0-0-0-1"）。
// ---------------------------------------------------------------------------

// Ported from: IModelTile.setContent (IModelTile.ts:134-148)。
// Authored: 场景自写（参考无 setContent 直接单测——覆盖在树集成）。
TEST(ImdlTileTreeTest, SetContentConsumesSizeMultiplierAndRewritesContentId)
{
    // 桩树 V4 provider（-b- 键域）+ 根瓦 "-b-0-0-0-0-1"（m_sizeMultiplier=0=undefined 态）。
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-100.005, -100.005, -100.005, -97.505, -97.505, -97.505);
    meta.tileScreenSize = 2048;
    meta.formatVersion = 37u << 0x10;
    dqRender::ImdlTileTree tree("25_1d-E:6_0x1c", "0/0/0/0/1",
                                box(-100.015, -100.015, -100.015, 100.015, 100.015, 100.015),
                                meta);
    auto* root = static_cast<dqRender::ImdlTile*>(tree.getRootTile());
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->getContentId(), "-b-0-0-0-0-1");  // 构造器 provider->rootContentId() 覆写
    ASSERT_DOUBLE_EQ(root->getSizeMultiplier(), 0.0);

    // 预置 2 个假子代（:148 disposeChildren 门的 children.length > 1 判定）。
    std::vector<std::unique_ptr<dqRender::Tile>> kids;
    kids.push_back(std::make_unique<dqRender::ImdlTile>(
        tree, root, "-b-1-0-0-0-1",
        box(-100.015, -100.015, -100.015, 0.0, 0.0, 0.0), 0.0, 2048.0));
    kids.push_back(std::make_unique<dqRender::ImdlTile>(
        tree, root, "-b-1-0-1-0-1",
        box(-100.015, 0.0, -100.015, 0.0, 100.015, 0.0), 0.0, 2048.0));
    root->setChildren(std::move(kids));
    ASSERT_EQ(root->getChildren().size(), 2u);

    // content{graphic!=null, sizeMultiplier=1.0, emptySubRangeMask=0x5}
    // （sizeMultiplier=1.0 = 该模型 header 的描述形态——decode 的
    // TileMetadata.ts:924-927 分支赋值形态）。
    dqRender::TileContent content;
    content.graphic = std::make_unique<ReadContentStubGraphic>();
    content.sizeMultiplier = 1.0;
    content.emptySubRangeMask = 0x5;
    root->setContent(std::move(content));

    // ① 升门控赋值（:145-146）：m_sizeMultiplier 0→1.0
    //   （hasSizeMultiplier = m_sizeMultiplier > 0.0，IModelTile.ts:81）。
    EXPECT_DOUBLE_EQ(root->getSizeMultiplier(), 1.0);
    // ② contentId 覆写 = idFromParentAndMultiplier(root, 1)（:147，V4 形钉死值——
    //    末段替换为 hex(1)；根键末段本就是 1 → 钉同串锁 V4 hex 形态合同）。
    EXPECT_EQ(root->getContentId(), "-b-0-0-0-0-1");
    // ③ emptySubRangeMask 写入可观察（:136 → IModelTile.emptySubRangeMask :78）。
    EXPECT_EQ(root->getEmptySubRangeMask(), 0x5u);
    // ④ 子代 >1 → disposeChildren 等价（:148 → setChildren({}) 释放）→ children 空。
    EXPECT_TRUE(root->getChildren().empty());

    // 链式升级（:145-148 的 magnification 链 1→2 派生形态——覆写机制的非恒等串
    // 可观察锁，亦为本里程碑根因场景的参考行为：根瓦升级后 loadChildren 走
    // magnification 派生 "-b-0-0-0-0-2" 而非细分 "-b-1-0-0-0-1"）。
    dqRender::TileContent upgrade;
    upgrade.graphic = std::make_unique<ReadContentStubGraphic>();
    upgrade.sizeMultiplier = 2.0;
    root->setContent(std::move(upgrade));
    EXPECT_DOUBLE_EQ(root->getSizeMultiplier(), 2.0);
    EXPECT_EQ(root->getContentId(), "-b-0-0-0-0-2");
}

// Ported from: IModelTile.setContent 的 :145 门控（IModelTile.ts:145
//              `sizeMult > this._sizeMultiplier`——降值不触发）。
// Authored: 场景自写（参考无 setContent 直接单测——覆盖在树集成）。
TEST(ImdlTileTreeTest, SetContentSizeMultiplierNeverDowngrades)
{
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-100.005, -100.005, -100.005, -97.505, -97.505, -97.505);
    meta.tileScreenSize = 2048;
    meta.formatVersion = 37u << 0x10;
    dqRender::ImdlTileTree tree("25_1d-E:6_0x1c", "0/0/0/0/1",
                                box(-100.015, -100.015, -100.015, 100.015, 100.015, 100.015),
                                meta);
    auto* root = static_cast<dqRender::ImdlTile*>(tree.getRootTile());
    ASSERT_NE(root, nullptr);

    // 现值 2.0 的瓦（magnification 链中段形态），content.sizeMultiplier=1.0。
    dqRender::ImdlTile tile(tree, root, "-b-0-0-0-0-2",
                            box(-100.015, -100.015, -100.015, 100.015, 100.015, 100.015),
                            2.0, 2048.0);
    dqRender::TileContent content;
    content.graphic = std::make_unique<ReadContentStubGraphic>();
    content.sizeMultiplier = 1.0;
    tile.setContent(std::move(content));

    // :142 门控——不降级、不覆写 contentId。
    EXPECT_DOUBLE_EQ(tile.getSizeMultiplier(), 2.0);
    EXPECT_EQ(tile.getContentId(), "-b-0-0-0-0-2");
}

// ---------------------------------------------------------------------------
// ContentIdProvider — 内容 Id 方案机制（M-D(3)：RPC dump 请求键形态的来源）。
//
// Ported from: itwinjs-core core/common/src/test/TileMetadata.test.ts
//              ("parses TileTreeId and ContentId strings" :330-336 的 flags
//              语义字面量 "-3-0"/"-c-5"/"-F-2" + "round trips tree Id and
//              content Id" :341 的 {depth:2,i:5,j:400,k:16,multiplier:8} spec
//              + getMaximumMajorTileFormatVersion TileMetadata.ts:412-427 的
//              钳制分支)。
//              适配登记：参考测试的 tree-Id 解析半边（iModelTileTreeIdToString
//              /parseTileTreeIdAndContentId）DanQing 无载体，不移植——只移植
//              content-Id 半边；flags 字面量取参考值，id 形态按 computeId 展
//              开（hex join，TileMetadata.ts:633-636）。"-b-0-0-0-0-1" 键兼为
//              采集资产 manifest 的根请求键（compatseed-v1，事实双锚）。
// ---------------------------------------------------------------------------
// Ported from: TileMetadata.ts:412-427 的钳制分支逐条（TileMetadata.test.ts
// 无本函数专项 it——断言值自参考实现逐分支推导，§5(f) 标注）。
TEST(ImdlContentIdProvider, GetMaximumMajorTileFormatVersionClamps)
{
    using dqRender::getMaximumMajorTileFormatVersion;
    // (maxMajorVersion, formatVersion) → major（TileMetadata.ts:412-427 分支）。
    EXPECT_EQ(37u, getMaximumMajorTileFormatVersion(37, 0));            // undefined → max
    EXPECT_EQ(37u, getMaximumMajorTileFormatVersion(37, 37u << 0x10));  // 37.0（dump 域）
    EXPECT_EQ(37u, getMaximumMajorTileFormatVersion(37, (37u << 0x10) | 5));  // minor 忽略
    EXPECT_EQ(4u, getMaximumMajorTileFormatVersion(4, 50u << 0x10));    // 后端超 app 上限 → app 上限
    EXPECT_EQ(4u, getMaximumMajorTileFormatVersion(37, 4u << 0x10));    // 后端低于已知版本
    EXPECT_EQ(1u, getMaximumMajorTileFormatVersion(1, 0));              // <1 无效 → 1
}

// Authored: 参考无对应测试（键兼为采集资产 compatseed-v1 manifest 的 depth-0
// 瓦键——事实双锚）；flags 组合语义锚在 defaultTileOptions（TileMetadata.ts
// :314-331）+ ContentIdV4Provider 构造（:703-710）。
TEST(ImdlContentIdProvider, V4RootContentIdMatchesCapturedKeyDomain)
{
    // defaultTileOptions + allowInstancing=true → flags 0xb（TileMetadata.ts
    // :314-331 缺省 + :703-710 组合——AllowInstancing|ImprovedElision|
    // ExternalTextures；ignoreAreaPatterns 缺省 false）。
    dqRender::TileOptions options;
    auto provider = dqRender::ContentIdProvider::create(
        /*allowInstancing=*/true, options, 37u << 0x10);
    ASSERT_NE(provider, nullptr);
    EXPECT_EQ(37u, provider->majorFormatVersion);
    EXPECT_EQ(static_cast<uint32_t>(dqRender::ContentFlags::AllowInstancing)
                  | static_cast<uint32_t>(dqRender::ContentFlags::ImprovedElision)
                  | static_cast<uint32_t>(dqRender::ContentFlags::ExternalTextures),
              static_cast<uint32_t>(provider->contentFlags));
    // 根请求键 = 采集 manifest 的 depth-0 键（compatseed-v1）。
    EXPECT_EQ("-b-0-0-0-0-1", provider->rootContentId());
}

// Ported from: TileMetadata.test.ts "parses TileTreeId and ContentId strings"
//              :330-336 的 flags 语义字面量（elision+instancing→3、
//              noPatterns+externalTextures→c、四开→F；树-Id 解析半边不移植，
//              字面量落在 rootContentId 前缀形态上）。
TEST(ImdlContentIdProvider, V4FlagPermutationsFromReferenceLiterals)
{
    // flags 语义字面量（TileMetadata.test.ts:330-336：elision+instancing → 3；
    // noPatterns+externalTextures → c；四者全开 → F）——经 rootContentId 的
    // 前缀形态断言（"-<flags>-0-0-0-0-1"）。
    dqRender::TileOptions elisionInstancing;  // 缺省即 instancing+elision+textures
    elisionInstancing.enableExternalTextures = false;
    EXPECT_EQ("-3-0-0-0-0-1",
              dqRender::ContentIdProvider::create(true, elisionInstancing, 4u << 0x10)
                  ->rootContentId());

    dqRender::TileOptions noPatternsTextures;
    noPatternsTextures.enableInstancing = false;   // allowInstancing=true 但 options 关断
    noPatternsTextures.enableImprovedElision = false;
    noPatternsTextures.ignoreAreaPatterns = true;
    EXPECT_EQ("-c-0-0-0-0-1",
              dqRender::ContentIdProvider::create(true, noPatternsTextures, 4u << 0x10)
                  ->rootContentId());

    dqRender::TileOptions allFlags;
    allFlags.ignoreAreaPatterns = true;
    EXPECT_EQ("-f-0-0-0-0-1",
              dqRender::ContentIdProvider::create(true, allFlags, 4u << 0x10)
                  ->rootContentId());

    // allowInstancing=false 关断 instancing 位（:704 的合取门）。
    dqRender::TileOptions defaults;
    EXPECT_EQ("-a-0-0-0-0-1",
              dqRender::ContentIdProvider::create(false, defaults, 4u << 0x10)
                  ->rootContentId());
}

// Ported from: TileMetadata.ts 方案注释（:663-665 V1 "depth/i/j/k/multiplier"、
//              :677-679 V2 "_majorVersion_flags_depth_i_j_k_multiplier"）+
//              create 的 majorVersion 分派（:644-661）；spec 取
//              TileMetadata.test.ts:341 的 round-trip 值。
TEST(ImdlContentIdProvider, V1AndV2SchemesByMajorVersion)
{
    // major 0/1 → V1 "depth/i/j/k/multiplier"（TileMetadata.ts:663-665 注释）。
    auto v1 = dqRender::ContentIdProvider::create(true, dqRender::TileOptions{}, 1u << 0x10);
    ASSERT_NE(v1, nullptr);
    EXPECT_EQ("2/5/190/10/8",
              v1->idFromSpec(dqRender::ImdlContentIdSpec{2, 5, 400, 16, 8}));

    // major 2/3 → V2 "_majorVersion_flags_depth_i_j_k_multiplier"
    //（TileMetadata.ts:677-679 注释；flags 只有 instancing 位——:683）。
    auto v2 = dqRender::ContentIdProvider::create(true, dqRender::TileOptions{}, 2u << 0x10);
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ("_2_1_2_5_190_10_8",
              v2->idFromSpec(dqRender::ImdlContentIdSpec{2, 5, 400, 16, 8}));
}

// Ported from: TileMetadata.test.ts "round trips tree Id and content Id"
//              :341-347（{depth:2,i:5,j:400,k:16,multiplier:8} 的
//              idFromSpec→specFromId 往返；tree-Id 半边不移植）。
TEST(ImdlContentIdProvider, SpecRoundtripHex)
{
    // TileMetadata.test.ts:341-347 的 round-trip spec（{2,5,400,16,8}——hex
    // 字段 190/10 走 parse/再组仍等）。
    dqRender::TileOptions options;
    auto provider = dqRender::ContentIdProvider::create(true, options, 37u << 0x10);
    dqRender::ImdlContentIdSpec const spec{2, 5, 400, 16, 8};
    EXPECT_EQ("-b-2-5-190-10-8", provider->idFromSpec(spec));
    dqRender::ImdlContentIdSpec const parsed =
        provider->specFromId(provider->idFromSpec(spec));
    EXPECT_EQ(spec.depth, parsed.depth);
    EXPECT_EQ(spec.i, parsed.i);
    EXPECT_EQ(spec.j, parsed.j);
    EXPECT_EQ(spec.k, parsed.k);
    EXPECT_EQ(spec.mult, parsed.mult);
}

// Authored: 参考无 computeChildTileProps 专项测试（行为锚在唯一调用者
// IModelTile._loadChildren :153-169 的 idProvider 接线 + 采集资产
// compatseed-v1 manifest 第 2 瓦键——事实双锚，§5(f) 标注）。
TEST(ImdlContentIdProvider, ChildIdsFollowCapturedKeyDomain)
{
    // 参考链：IModelTile._loadChildren（:153-169）→ computeChildTileProps
    //（:777-853）→ idProvider.idFromSpec（:846）——compatseed 域：根 ±100.015、
    // model range 在低角 → 唯一存活子 = depth1/i=j=k=0，键 = manifest 第 2 瓦。
    dqRender::ImdlTileMetadata parent;
    parent.contentId = "-b-0-0-0-0-1";
    parent.range = box(-100.015, -100.015, -100.015, 100.015, 100.015, 100.015);
    parent.contentRange = parent.range;

    dqRender::ImdlTreeMetadata root;
    root.contentRange = box(-100.005, -100.005, -100.005, -97.505, -97.505, -97.505);
    root.tileScreenSize = 2048;

    dqRender::TileOptions options;
    auto provider = dqRender::ContentIdProvider::create(true, options, 37u << 0x10);
    auto children = dqRender::computeImdlChildTileProps(parent, *provider, root);
    ASSERT_EQ(1u, children.size());
    EXPECT_EQ("-b-1-0-0-0-1", children[0].contentId);
    EXPECT_DOUBLE_EQ(2048.0, children[0].maximumSize);
}

// Authored: 参考无对应测试（行为锚在 IModelTileTree.ts:396-398——构造器内
// contentIdProvider.create + params.rootTile.contentId 覆写；§5(f) 标注）。
TEST(ImdlContentIdProvider, TreeOverridesRootKeyWhenPropsCarryFormatVersion)
{
    // IModelTileTree.ts:398 — params.rootTile.contentId =
    // contentIdProvider.rootContentId（props 的 V1 形根 id 被覆写为协商方案
    // 的根请求键）；props 无 formatVersion → legacy 路径不覆写（登记见
    // ImdlTreeMetadata::formatVersion）。
    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = box(-100.005, -100.005, -100.005, -97.505, -97.505, -97.505);
    meta.tileScreenSize = 2048;
    meta.formatVersion = 37u << 0x10;
    dqRender::ImdlTileTree tree("25_1d-E:6_0x1c", "0/0/0/0/1",
                                box(-100.015, -100.015, -100.015, 100.015, 100.015, 100.015),
                                meta);
    auto* root = static_cast<dqRender::ImdlTile*>(tree.getRootTile());
    ASSERT_NE(root, nullptr);
    EXPECT_EQ("-b-0-0-0-0-1", root->getContentId());
    EXPECT_EQ("25_1d-E:6_0x1c/-b-0-0-0-0-1",
              tree.contentUrl(root->getContentId()));

    // 无 formatVersion → 根键保持传入原文（legacy V1 id 合同）。
    dqRender::ImdlTreeMetadata legacyMeta = meta;
    legacyMeta.formatVersion = 0;
    dqRender::ImdlTileTree legacyTree("fixture://tree", "0/0/0/0",
                                      box(0, 0, 0, 8, 8, 8), legacyMeta);
    auto* legacyRoot = static_cast<dqRender::ImdlTile*>(legacyTree.getRootTile());
    ASSERT_NE(legacyRoot, nullptr);
    EXPECT_EQ("0/0/0/0", legacyRoot->getContentId());
}
