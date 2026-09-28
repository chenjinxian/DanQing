// IModelTileTreeIdTest — tile tree Id 派生面（M-H Task 3）：
// iModelTileTreeIdToString / edgeOptionsToString（TileMetadata.ts:399-409/
// :433-441/:487-532 的移植锁——参考 PrimaryTreeReference.createTreeId
//（PrimaryTileTree.ts:268-290）→ PrimaryTreeSupplier.createTileTree（:65）
// 的 requestTileTreeProps RPC 键派生链）。
#include <gtest/gtest.h>

#include <dqRender/tile/ImdlTileTree.h>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/common/src/test/TileMetadata.test.ts
//              it("stringifies tree Ids")（:70-270 用例表全移植）。
// **参考测试与参考实现漂移登记**：参考测试的期望 flags（kDefaults=0xd 系）
// 写于 TreeFlags.ExpandProjectExtents（TileMetadata.ts:439）加入
// iModelTileTreeIdToString 的 flags 合成（:499-500）之前——参考实现
// （:487-532 逐行）对 defaultTileOptions.expandProjectExtents=true
// （defaultTileOptions :321）**恒置 0x10 位**（volume classifier 分支清位，
// :518）。活体 DTA 采集的 treeId 即为 "25_1d-E:6_…"（flags=0x1d——
// joeshouse-v1/instances60-v1 manifest 11 树全表）。故本测试的期望 =
// **参考实现的真实产出**（= 参考测试期望 | 0x10；volume 用例与参考期望
// 恰好一致——该分支 &= ~ExpandProjectExtents）——实现逐行移植是规范
// （§0），测试文件漂移如实登记。
// ---------------------------------------------------------------------------
TEST(IModelTileTreeIdTest, StringifiesTreeIds)
{
    using dqRender::ClassifierTileTreeId;
    using dqRender::EdgeOptions;
    using dqRender::IModelTileTreeId;
    using dqRender::PrimaryTileTreeId;
    using dqRender::TileEdgeType;
    using dqRender::TileOptions;
    using dqRender::TreeFlags;

    // 参考测试的 primaryId helper（TileMetadata.test.ts:71-78）：
    // edgesRequired → { non-indexed, smooth:false }；否则 false。
    auto primaryId = [](bool edgesRequired = true,
                        bool enforceDisplayPriority = false,
                        std::optional<std::string> sectionCut = std::nullopt,
                        std::optional<std::string> animationId = std::nullopt) {
        PrimaryTileTreeId id;
        if (edgesRequired)
            id.edges = EdgeOptions{TileEdgeType::NonIndexed, false};
        id.animationId = animationId;
        id.enforceDisplayPriority = enforceDisplayPriority;
        id.sectionCut = sectionCut;
        return IModelTileTreeId{id};
    };
    // 参考测试的 classifierId helper（:80-86）。
    auto classifierId = [](double expansion = 1.0, bool planar = true,
                           std::optional<std::string> animationId = std::nullopt) {
        ClassifierTileTreeId id;
        id.type = planar ? dqCommon::BatchType::PlanarClassifier
                         : dqCommon::BatchType::VolumeClassifier;
        id.expansion = expansion;
        id.animationId = animationId;
        return IModelTileTreeId{id};
    };

    struct Case {
        IModelTileTreeId id;
        bool ignoreProjectExtents = false;
        bool noOptimizeBReps = false;
        bool useSmallerTiles = false;
        std::string baseId;
        uint32_t flags;  // 参考实现产出（= 参考测试期望 | 0x10，volume 除外——见锁头登记）
    };

    auto constexpr k = [](TreeFlags f) { return static_cast<uint32_t>(f); };
    uint32_t const kNone = k(TreeFlags::None);
    uint32_t const kExtents = k(TreeFlags::UseProjectExtents);
    uint32_t const kBReps = k(TreeFlags::OptimizeBRepProcessing);
    uint32_t const kPriority = k(TreeFlags::EnforceDisplayPriority);
    uint32_t const kLarger = k(TreeFlags::UseLargerTiles);
    uint32_t const kExpand = k(TreeFlags::ExpandProjectExtents);
    uint32_t const kDefaults = kExtents | kBReps | kLarger;  // 0xd（参考测试原期望）
    uint32_t const kAll = kDefaults | kPriority;             // 0xf
    // 参考实现产出（Expand 恒置位——defaultTileOptions.expandProjectExtents=true）。
    uint32_t const kDefaultsX = kDefaults | kExpand;         // 0x1d
    uint32_t const kAllX = kAll | kExpand;                   // 0x1f

    std::vector<Case> const cases = {
        {primaryId(), false, false, false, "", kDefaultsX},
        {primaryId(false), false, false, false, "E:0_", kDefaultsX},
        {primaryId(true), true, false, false, "", kBReps | kLarger | kExpand},
        {primaryId(true), false, true, false, "", kExtents | kLarger | kExpand},
        {primaryId(true), true, true, false, "", kLarger | kExpand},
        {primaryId(true), true, true, true, "", kNone | kExpand},
        {primaryId(true), false, false, true, "", kExtents | kBReps | kExpand},
        {primaryId(true), false, true, true, "", kExtents | kExpand},
        {primaryId(false), true, false, false, "E:0_", kBReps | kLarger | kExpand},
        {primaryId(false, true), true, false, false, "E:0_",
         kPriority | kBReps | kLarger | kExpand},
        {primaryId(true, true), false, false, false, "", kAllX},
        {primaryId(false, false, "abcxyz"), false, false, false, "E:0_Sabcxyzs",
         kDefaultsX},
        {primaryId(true, true, "fakeclip"), false, false, false, "Sfakeclips", kAllX},
        {primaryId(true, false, std::nullopt, "0x123"), false, false, false,
         "A:0x123_", kDefaultsX},
        {primaryId(false, false, std::nullopt, "0xfde"), true, false, false,
         "A:0xfde_E:0_", kBReps | kLarger | kExpand},
        {primaryId(false, false, "clippy", "0x5c"), false, false, false,
         "A:0x5c_E:0_Sclippys", kDefaultsX},
        {primaryId(true, true, std::nullopt, "0x1a"), false, false, false, "A:0x1a_",
         kDefaultsX},
        {classifierId(), false, false, false, "CP:1.000000_", kDefaultsX},
        {classifierId(0.250000), false, false, false, "CP:0.250000_", kDefaultsX},
        // volume classifier：参考实现强制 UseProjectExtents 置位 + Expand 清位
        //（:515-519）——期望与参考测试原值恰好一致（漂移登记见锁头）。
        {classifierId(2.500000, false), false, false, false, "C:2.500000_", kDefaults},
        {classifierId(3, false, "0xabc"), false, false, false, "C:3.000000_A:0xabc_",
         kDefaults},
        {classifierId(12.00001234), false, false, false, "CP:12.000012_", kDefaultsX},
        {classifierId(123456789.0), false, false, false, "CP:123456789.000000_",
         kDefaultsX},
        {classifierId(), true, false, false, "CP:1.000000_", kBReps | kLarger | kExpand},
        {classifierId(1, false), true, false, false, "C:1.000000_", kDefaults},
    };

    for (auto const& test : cases) {
        // 参考测试的 options 覆写（TileMetadata.test.ts:256-262——
        // defaultTileOptions 继承 + 三布尔覆写）。
        TileOptions options;  // = defaultTileOptions
        options.useProjectExtents = !test.ignoreProjectExtents;
        options.useLargerTiles = !test.useSmallerTiles;
        options.optimizeBRepProcessing = !test.noOptimizeBReps;

        std::string const modelId = "0x1c";
        std::string const actual =
            dqRender::iModelTileTreeIdToString(modelId, test.id, options);

        char flagsHex[8];
        std::snprintf(flagsHex, sizeof(flagsHex), "%x", test.flags);
        std::string const expected =
            std::string("25_") + flagsHex + "-" + test.baseId + modelId;
        EXPECT_EQ(expected, actual) << "baseId=" << test.baseId;
    }
}

// Authored: 参考无 edgeOptionsToString 专项（TileMetadata.test.ts 的用例只用
// {non-indexed,false}→"" 与 false→"E:0_" 两分支——采集域 treeId "E:6_"
// ={compact,smooth} 是该函数无参考用例的分支族，§5(f)）。全分支覆盖
//（TileMetadata.ts:399-409 逐分支）。
TEST(IModelTileTreeIdTest, EdgeOptionsToStringCoversAllBranches)
{
    using dqRender::EdgeOptions;
    using dqRender::TileEdgeType;
    EXPECT_EQ("E:0_", dqRender::edgeOptionsToString(std::nullopt));
    EXPECT_EQ("",
              dqRender::edgeOptionsToString(EdgeOptions{TileEdgeType::NonIndexed, false}));
    EXPECT_EQ("E:3_",
              dqRender::edgeOptionsToString(EdgeOptions{TileEdgeType::NonIndexed, true}));
    EXPECT_EQ("E:2_",
              dqRender::edgeOptionsToString(EdgeOptions{TileEdgeType::Indexed, false}));
    EXPECT_EQ("E:4_",
              dqRender::edgeOptionsToString(EdgeOptions{TileEdgeType::Indexed, true}));
    EXPECT_EQ("E:5_",
              dqRender::edgeOptionsToString(EdgeOptions{TileEdgeType::Compact, false}));
    EXPECT_EQ("E:6_",
              dqRender::edgeOptionsToString(EdgeOptions{TileEdgeType::Compact, true}));
}

// Authored: model⇔tree 对账锚（M-H Task 3——10+1 采集 treeId 全表钉死）：
// 采集 manifest trees[].treeId（joeshouse-v1 10 树 + instances60-v1 1 树）
// == iModelTileTreeIdToString(modelId, {Primary, edges={compact,smooth}},
// defaultTileOptions)。参考派生链 = PrimaryTreeReference.createTreeId
//（PrimaryTileTree.ts:283-289——edgesRequired（saved viewflags
// visEdges=true）→ tileAdmin.edgeOptions=defaultTileOptions.edgeOptions
// {compact,smooth}）→ PrimaryTreeSupplier.createTileTree（:65）。
// 无参考测试（§5(f)）——采集域是唯一 ground truth（11 树 manifest 在案）。
TEST(IModelTileTreeIdTest, CapturedTreeIdsMatchModelSelectorDerivation)
{
    dqRender::PrimaryTileTreeId id;
    id.edges = dqRender::EdgeOptions{dqRender::TileEdgeType::Compact, true};
    dqRender::TileOptions const options;  // = defaultTileOptions
    struct Pair {
        char const* modelId;
        char const* treeId;
    };
    Pair const pairs[] = {
        // instances60-v1（modelSelectorProps.models=["0x1c"]）。
        {"0x1c", "25_1d-E:6_0x1c"},
        // joeshouse-v1（modelSelectorProps.models 10 条——imodel.json 原序）。
        {"0x26", "25_1d-E:6_0x26"},
        {"0x3d", "25_1d-E:6_0x3d"},
        {"0x3f", "25_1d-E:6_0x3f"},
        {"0x41", "25_1d-E:6_0x41"},
        {"0x43", "25_1d-E:6_0x43"},
        {"0x45", "25_1d-E:6_0x45"},
        {"0x47", "25_1d-E:6_0x47"},
        {"0x49", "25_1d-E:6_0x49"},
        {"0x4b", "25_1d-E:6_0x4b"},
        {"0x4d", "25_1d-E:6_0x4d"},
    };
    for (auto const& p : pairs) {
        EXPECT_EQ(p.treeId, dqRender::iModelTileTreeIdToString(p.modelId, id, options))
            << "modelId=" << p.modelId;
    }
}
