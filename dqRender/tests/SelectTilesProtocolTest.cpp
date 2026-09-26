// SelectTilesProtocolTest — ImdlTile::selectTiles 的 SelectParent 协议场景矩阵。
//
// Ported from: IModelTile.selectTiles (IModelTile.ts:205-334) —— 场景矩阵。
// Authored: itwinjs-core 无 IModelTile.selectTiles 直接单测（覆盖在集成层，
//           frontend/src/test/tile/ 无对应文件——已检索）；本矩阵按参考代码
//           分支自写（§5(f)）。
// 桩：ImdlTileTreeTest 的建树模式 + setContent/setNotFound 现有 API，无 GL。
//
// 场景构造的两根杠杆（均为在仓实现的确定性行为）：
// ① contentId 置空 → ImdlTile::hasContent()==false → ImdlTileTree::
//    computeVisibility 恒回 TooCoarse（ImdlTileTree.cpp:355-356 的
//    hasContent 门，先于叶/SSE 分支）；contentId 非空时叶走 Visible 早退
//   （Tile.ts:445-449 对应），非叶走 SSE 判定式（radius ≤ maximumSize →
//    Visible，Tile.ts:459-463）。
// ② 树的 contentRange 放到远处 → loadChildren 的模型域拒绝（TileMetadata.ts
//    :841-845）令 computeImdlChildTileProps 返回空 → ImdlTile::loadChildren
//    早退（ImdlTileTree.cpp:312-315，参考 _childrenLoadStatus 门的同步等价，
//    Tile.ts:354-355）→ 手工 setChildren 的层级不被替换。
//
// isDisplayable 语义登记（Tile.ts:231 = `0 < maximumSize`）：协议体内所有
// isDisplayable 位点（:235/:237/:270）取参考语义——DanQing 预存的
// Tile::isDisplayable()（Ready && graphic，Tile.h）语义漂移，不得用于协议
// （见 ImdlTile::selectTiles 的 EQUIVALENCE 登记）。场景 3 的 numSkipped
// 计数路径（:270-279）只有在参考语义下可达，即本矩阵对该登记的锁定。
#include <gtest/gtest.h>

#include <dqRender/RenderGraphic.h>
#include <dqRender/tile/ImdlTileTree.h>

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable 析构需完整类型

#include <memory>
#include <vector>

using namespace dqRender;

namespace {

// 最小 RenderGraphic 桩（tile 内容只需非空 graphic）——照 TileDrawArgsTest。
class StubGraphic : public RenderGraphic {
public:
    void unionRange(dqGeom::Range3d&) const override {}
};

dqGeom::Range3d box(double x0, double y0, double z0, double x1, double y1, double z1)
{
    return dqGeom::Range3d::CreateXYZXYZ(x0, y0, z0, x1, y1, z1);
}

// Ready + 有 graphic 的内容（isLeaf=true——leaf 分支给非空 contentId 的瓦
// 一个确定的 Visible；对空 contentId 的瓦无影响，hasContent 门先行）。
TileContent readyContent()
{
    TileContent content;
    content.graphic = std::make_unique<StubGraphic>();
    content.isLeaf = true;
    return content;
}

// 协议测试树：contentRange 放到远处（①②两根杠杆的第二根）。
// tileScreenSize 缺省 512 只影响树自带根（场景不用它）。
struct ProtocolTree : ImdlTileTree
{
    ProtocolTree()
        : ImdlTileTree("select-protocol-tree", "0/0/0/0",
                       box(0, 0, 0, 8, 8, 8), treeMeta())
    {
    }
    static ImdlTreeMetadata treeMeta()
    {
        ImdlTreeMetadata meta;
        meta.contentRange = box(1.0e6, 1.0e6, 1.0e6, 2.0e6, 2.0e6, 2.0e6);
        return meta;
    }
};

bool contains(std::vector<Tile*> const& selected, Tile const* tile)
{
    for (auto* t : selected)
        if (t == tile)
            return true;
    return false;
}

}  // namespace

// 叶 Ready → selected 含它，markReady，无 missing（:214-222 + :258-259）。
TEST(SelectTilesProtocolTest, VisibleReadyLeafIsSelected)
{
    ProtocolTree tree;
    ImdlTile root(tree, nullptr, "0/0/0/0", box(0, 0, 0, 8, 8, 8), 0.0, 512.0);
    root.setContent(readyContent());  // Ready + graphic + leaf → Visible

    TileDrawArgs args;
    std::vector<Tile*> selected;
    auto const result = root.selectTiles(selected, args, /*numSkipped=*/0);

    EXPECT_EQ(result, SelectParent::No);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected.front(), &root);
    EXPECT_TRUE(args.isTileReady(&root));   // :221 markReady
    EXPECT_FALSE(args.hasMissingTiles());   // Ready → 不进 missing（:216-217 不触发）
}

// Visible 但 !isReady 且 !hasGraphics：insertMissing（:216-217）；孩子全
// Ready 可画 → 孩子入选（:243-253 路径：所有可见直接孩子可画则画它们——
// push 不 markReady，TileDrawArgs.ts:419-421 的 markReady 只在 :221）。
TEST(SelectTilesProtocolTest, NotReadyVisibleInsertsMissingAndTriesChildren)
{
    ProtocolTree tree;
    // 非叶根：maximumSize 512 ≥ radius(≈6.93) → Visible（SSE 判定式）。
    ImdlTile root(tree, nullptr, "0/0/0/0", box(0, 0, 0, 8, 8, 8), 0.0, 512.0);

    // 两个孩子：可见、Ready、有 graphic。
    auto kid0 = std::make_unique<ImdlTile>(tree, &root, "1/0/0/0",
                                           box(0, 0, 0, 4, 4, 4), 0.0, 512.0);
    auto kid1 = std::make_unique<ImdlTile>(tree, &root, "1/1/1/1",
                                           box(4, 4, 4, 8, 8, 8), 0.0, 512.0);
    Tile* kid0Ptr = kid0.get();
    Tile* kid1Ptr = kid1.get();
    kid0->setContent(readyContent());
    kid1->setContent(readyContent());
    std::vector<std::unique_ptr<Tile>> kids;
    kids.push_back(std::move(kid0));
    kids.push_back(std::move(kid1));
    root.setChildren(std::move(kids));

    TileDrawArgs args;
    std::vector<Tile*> selected;
    auto const result = root.selectTiles(selected, args, /*numSkipped=*/0);

    EXPECT_EQ(result, SelectParent::No);
    // 根进 missing（Visible && !isReady，:216-217），孩子入选（:250 push）。
    ASSERT_EQ(args.getMissingTiles().size(), 1u);
    EXPECT_EQ(args.getMissingTiles().front(), &root);
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], kid0Ptr);
    EXPECT_EQ(selected[1], kid1Ptr);
    // :243-253 只 push 不 markReady——ready 集保持空。
    EXPECT_FALSE(args.isTileReady(kid0Ptr));
    EXPECT_FALSE(args.isTileReady(kid1Ptr));
    EXPECT_FALSE(args.isTileReady(&root));
}

// TooCoarse：深度 ≥ maxInitialTilesToSkip 的未就绪瓦跳级——numSkipped 达
// maxTilesToSkip(1) 后 canSkipThisTile=false → insertMissing（:265-280 计数
// 路径 + :330-331），父链回退画根（:296-314 独占回滚 + :317-326）。
TEST(SelectTilesProtocolTest, TooCoarseSkipsWithinMaxTilesToSkip)
{
    ProtocolTree tree;
    // 根：空 contentId → 恒 TooCoarse；Ready + graphic（回退的落点）。
    ImdlTile root(tree, nullptr, "", box(0, 0, 0, 8, 8, 8), 0.0, 512.0);
    root.setContent(readyContent());  // isLeaf=true → loadChildren 早退，孩子保留

    // 二级（kid）/三级（grand）：未就绪、无 graphic、无 multiplier——计数路径
    // 的 isNotReady 形态（:272）。maximumSize=1 → isDisplayable（:231 语义
    // 0 < maximumSize）为真 → 跳级计数生效（:270 门）。
    auto kid = std::make_unique<ImdlTile>(tree, &root, "", box(0, 0, 0, 8, 8, 8),
                                          0.0, 1.0);
    auto grand = std::make_unique<ImdlTile>(tree, kid.get(), "",
                                            box(0, 0, 0, 4, 4, 4), 0.0, 1.0);
    Tile* grandPtr = grand.get();
    std::vector<std::unique_ptr<Tile>> grandKids;
    grandKids.push_back(std::move(grand));
    kid->setChildren(std::move(grandKids));
    std::vector<std::unique_ptr<Tile>> rootKids;
    rootKids.push_back(std::move(kid));
    root.setChildren(std::move(rootKids));

    TileDrawArgs args;  // maxTilesToSkip = TileAdmin.maximumLevelsToSkip 缺省 1
    std::vector<Tile*> selected;
    auto const result = root.selectTiles(selected, args, /*numSkipped=*/0);

    // 计数：kid 第一跳（numSkipped 0→1，:277）；grand 第二跳遇
    // numSkipped(1) >= maxTilesToSkip(1) → canSkipThisTile=false（:274-275）
    // → grand insertMissing（:330-331）。
    ASSERT_EQ(args.getMissingTiles().size(), 1u);
    EXPECT_EQ(args.getMissingTiles().front(), grandPtr);
    // grand 返回 Yes（isParentDisplayable，:333）→ kid 独占回滚（:313-314）
    // → kid 也返回 Yes → 非根 → drawChildren=false → 根画自己（:317-326）。
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected.front(), &root);
    EXPECT_EQ(result, SelectParent::No);
    // 根 canSkip 为真 → 不 markReady（:320-323 的 !canSkipThisTile 门）。
    EXPECT_FALSE(args.isTileReady(&root));
}

// 孩子 NotFound → drawChildren=false → 父（Ready）入选并 markReady（:296-307
// + :317-326：:301 的 canSkipThisTile=false 使 :320-323 的 markReady 生效）。
TEST(SelectTilesProtocolTest, NotFoundChildFallsBackToParent)
{
    ProtocolTree tree;
    ImdlTile root(tree, nullptr, "", box(0, 0, 0, 8, 8, 8), 0.0, 512.0);
    root.setContent(readyContent());

    auto kid = std::make_unique<ImdlTile>(tree, &root, "", box(0, 0, 0, 8, 8, 8),
                                          0.0, 1.0);
    Tile* kidPtr = kid.get();
    kid->setNotFound();  // TileLoadStatus::NotFound（Tile.ts:205-207）
    std::vector<std::unique_ptr<Tile>> rootKids;
    rootKids.push_back(std::move(kid));
    root.setChildren(std::move(rootKids));

    TileDrawArgs args;
    std::vector<Tile*> selected;
    auto const result = root.selectTiles(selected, args, /*numSkipped=*/0);

    // kid：TooCoarse + NotFound，可跳（isParentDisplayable）→ 返回 Yes 且不进
    // missing（canSkip 保持真，:330 不触发——NotFound 瓦不重请求）。
    EXPECT_FALSE(contains(args.getMissingTiles(), kidPtr));
    // root：Yes && kid.loadStatus==NotFound → drawChildren=canSkip=false
    //（:299-301）→ 回退画根 + markReady（canSkip 被置 false → :320-323）。
    EXPECT_EQ(result, SelectParent::No);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected.front(), &root);
    EXPECT_TRUE(args.isTileReady(&root));
}

// drawChildren=false 且 parentsAndChildrenExclusive → selected 回滚到
// initialSize（:295 + :313-314）；非独占（map 类树）保留已入选孩子（:312
// 参考注释）。
TEST(SelectTilesProtocolTest, ParentsAndChildrenExclusiveRollsBackChildren)
{
    ProtocolTree tree;
    ImdlTile root(tree, nullptr, "", box(0, 0, 0, 8, 8, 8), 0.0, 512.0);
    root.setContent(readyContent());

    // kid0：可见、Ready、有 graphic → 递归中先入选（:219-222）。
    auto kid0 = std::make_unique<ImdlTile>(tree, &root, "1/0/0/0",
                                           box(0, 0, 0, 4, 4, 4), 0.0, 512.0);
    Tile* kid0Ptr = kid0.get();
    kid0->setContent(readyContent());
    // kid1：可见但未就绪、无孩子 → 返回 Yes（:226-229）→ drawChildren=false。
    auto kid1 = std::make_unique<ImdlTile>(tree, &root, "1/1/1/1",
                                           box(4, 4, 4, 8, 8, 8), 0.0, 512.0);
    Tile* kid1Ptr = kid1.get();
    std::vector<std::unique_ptr<Tile>> kids;
    kids.push_back(std::move(kid0));
    kids.push_back(std::move(kid1));
    root.setChildren(std::move(kids));

    {
        // 独占（TileDrawArgs 缺省 true，TileDrawArgs.ts:105）→ kid0 被回滚。
        TileDrawArgs args;
        std::vector<Tile*> selected;
        auto const result = root.selectTiles(selected, args, /*numSkipped=*/0);
        EXPECT_EQ(result, SelectParent::No);
        ASSERT_EQ(selected.size(), 1u);
        EXPECT_EQ(selected.front(), &root);
        EXPECT_FALSE(contains(selected, kid0Ptr));
        // kid1 未就绪 → 进 missing（:216-217）；ready 集仍记 kid0（:221，
        // 回滚只作用于 selected——:313-314）。
        EXPECT_TRUE(contains(args.getMissingTiles(), kid1Ptr));
        EXPECT_TRUE(args.isTileReady(kid0Ptr));
    }
    {
        // 非独占 → kid0 的入选保留（:313-314 跳过），根随后仍入选。
        TileDrawArgs args;
        args.parentsAndChildrenExclusive = false;
        std::vector<Tile*> selected;
        auto const result = root.selectTiles(selected, args, /*numSkipped=*/0);
        EXPECT_EQ(result, SelectParent::No);
        ASSERT_EQ(selected.size(), 2u);
        EXPECT_EQ(selected[0], kid0Ptr);
        EXPECT_EQ(selected[1], &root);
    }
}
