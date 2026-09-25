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
