// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contours (per-batch feature→group LUT) tests（M-X X-a3）
//
// 锚定：webgl/Contours.ts:56-152（_initialize/_update/buildLookupTable/bind*
//   /matchesTargetAndFeatureCount/matchesSubCategories）+ BatchUniforms.ts:36/
//   :91/:110-128（_contours/_setCurrentBatch 消费/wantContourLines 门）+
//   Graphic.ts:135-146（PerTargetBatchData.getContours 缓存形）。
//
// Authored: no reference test exists in itwinjs-core for webgl Contours
//           （core/frontend 无 Contours 单测面——行为锚=Contours.ts 逐行；
//           打包字节序=buildLookupTable :92-108 的 4-bit nibble 布局钉值）。
#include <gtest/gtest.h>

#include "NullDriver.h"
#include "NullTargetFixture.h"
#include "render/Batch.h"
#include "render/Contours.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/TargetImpl.h"
#include "render/TechniqueImpl.h"
#include "render/Uniforms.h"

#include <dqBase/CompressedId64Set.h>
#include <dqCommon/ContourDisplay.h>
#include <dqCommon/PackedFeatureTable.h>

using namespace dqRender;

namespace {

// 3 特征 / 2 子类的 packed 特征表（PackedFeatureTable 线格式：3×u32/feature +
// 2×u32/unique subcat 尾——PackedFeatureTable.ts:35-58）：
//   f0: elementId=1, subcatIdx=0(→0x10)；f1: elementId=2, subcatIdx=1(→0x20)；
//   f2: elementId=3, subcatIdx=0(→0x10)。
dqCommon::PackedFeatureTable makePackedTable()
{
    std::vector<uint32_t> words = {
        0x00000001u, 0x00000000u, 0x00000000u,  // f0: elem 1, subcatIdx 0, class Primary
        0x00000002u, 0x00000000u, 0x00000001u,  // f1: elem 2, subcatIdx 1, class Primary
        0x00000003u, 0x00000000u, 0x00000000u,  // f2: elem 3, subcatIdx 0, class Primary
        0x00000010u, 0x00000000u,               // subcat[0] = 0x10
        0x00000020u, 0x00000000u,               // subcat[1] = 0x20
    };
    return dqCommon::PackedFeatureTable(std::move(words), /*modelId*/0,
                                        /*numFeatures*/3, dqCommon::BatchType::Primary);
}

dqCommon::ContourGroup makeGroupForSubcat(uint64_t subCat)
{
    dqCommon::ContourGroup g;
    dqBase::DqIdSet ids;
    ids.insert(dqBase::DqId(subCat));
    g.subCategories = dqBase::CompressedId64Set::SortAndCompress(ids);
    return g;
}

void setTargetContours(TargetImpl& target, dqCommon::ContourDisplay const& contours)
{
    dqRender::ViewFlags vf;
    target.changeRenderPlan(vf, true, nullptr, nullptr, nullptr, &contours);
}

}  // namespace

// buildLookupTable 4-bit 打包钉值（Contours.ts:92-108——`[0] R/G/B/A = index
// pair - lower 4 bits = ndx n, upper 4 bits = ndx n+1`）+ 默认组语义
//（:81/:85-86——空 subCategories 组 = defaultNdx 切换；无默认组时未匹配特征
// =0xf[15——computeContourNdx 的 "无等高线" 哨兵]）。
TEST(ContoursTest, LookupTablePacksNibblesPerFeaturePair)
{
    NullTargetFixture fixture;

    // Case A：group0=默认组（空 subCategories）+ group1={0x10}。
    dqCommon::ContourDisplay withDefault;
    withDefault.displayContours = true;
    withDefault.groups.push_back(dqCommon::ContourGroup{});  // 默认组
    withDefault.groups.push_back(makeGroupForSubcat(0x10));
    setTargetContours(*fixture.target, withDefault);

    auto table = makePackedTable();
    auto contours = Contours::createFromTarget(*fixture.target);
    contours->initFromMap(table);

    // dims：computeDimensions(3, 1/8, 0, maxSize)——nRgba=ceil(3/8)=1 → 1×1
    //（VertexTable.ts:53-81）。数据=w*h*4=4 字节。
    EXPECT_EQ(1u, contours->getLutWidth());
    ASSERT_EQ(4u, contours->getLutData().size());
    // f0(0x10)→组1 低 nibble=1；f1(0x20)→默认组0 高 nibble=0 → byte0=0x01。
    // f2(0x10)→组1（偶索引 staged，尾写——:107-108）→ byte1=0x01。
    EXPECT_EQ(0x01, contours->getLutData()[0]);
    EXPECT_EQ(0x01, contours->getLutData()[1]);
    EXPECT_EQ(0x00, contours->getLutData()[2]);
    EXPECT_EQ(0x00, contours->getLutData()[3]);
    EXPECT_EQ(1u, contours->getLutWidthForBind());  // bindContourLUTWidth 源值

    // Case B：无默认组（{0x10} 组独占**下标 0**）→ f0/f2→组0；f1(0x20)
    // 未匹配→0xf 哨兵（computeContourNdx 的"无等高线"值——Contours.ts:81）。
    dqCommon::ContourDisplay noDefault;
    noDefault.displayContours = true;
    noDefault.groups.push_back(makeGroupForSubcat(0x10));
    setTargetContours(*fixture.target, noDefault);

    auto contours2 = Contours::createFromTarget(*fixture.target);
    contours2->initFromMap(table);
    ASSERT_EQ(4u, contours2->getLutData().size());
    // f0→0 低 nibble；f1→15 高 nibble → byte0 = 0 | (15<<4) = 0xF0。
    EXPECT_EQ(0xF0, contours2->getLutData()[0]);
    EXPECT_EQ(0x00, contours2->getLutData()[1]);
}

// matchesTargetAndFeatureCount（Contours.ts:37-40——target 同一性 +
// numFeatures 等；DanQing 值语义下同 target 重用于不同 batch 的失效面）。
TEST(ContoursTest, MatchesTargetAndFeatureCount)
{
    NullTargetFixture fixture;
    dqCommon::ContourDisplay d;
    d.displayContours = true;
    d.groups.push_back(dqCommon::ContourGroup{});
    setTargetContours(*fixture.target, d);

    auto table = makePackedTable();
    auto contours = Contours::createFromTarget(*fixture.target);
    contours->initFromMap(table);

    EXPECT_TRUE(contours->matchesTargetAndFeatureCount(*fixture.target, table));

    // 特征数变 → 不匹配（重建面）。
    std::vector<uint32_t> words2 = {
        0x00000001u, 0x00000000u, 0x00000000u,  // f0
        0x00000010u, 0x00000000u,               // subcat[0]
    };
    dqCommon::PackedFeatureTable smaller(std::move(words2), 0, 1,
                                         dqCommon::BatchType::Primary);
    EXPECT_FALSE(contours->matchesTargetAndFeatureCount(*fixture.target, smaller));
}

// update 的 subCategories 失效腿（Contours.ts:42-54 matchesSubCategories +
// :133-142 update）——组子类集变更 → LUT 重打包内容变；未变 → 保持。
TEST(ContoursTest, UpdateRebuildsWhenSubCategoriesChange)
{
    NullTargetFixture fixture;
    dqCommon::ContourDisplay d;
    d.displayContours = true;
    d.groups.push_back(dqCommon::ContourGroup{});      // 默认组
    d.groups.push_back(makeGroupForSubcat(0x10));      // 组1={0x10}
    setTargetContours(*fixture.target, d);

    auto table = makePackedTable();
    auto contours = Contours::createFromTarget(*fixture.target);
    contours->initFromMap(table);
    ASSERT_EQ(0x01, contours->getLutData()[0]);

    // 同 subCategories 的 update → 内容不变（matchesSubCategories 早退）。
    contours->update(table);
    EXPECT_EQ(0x01, contours->getLutData()[0]);

    // 组1 改挂 0x20 → f1 自默认组（组0）升入组1，f0/f2(0x10) 落回默认组0：
    // byte0 = 0 | (1<<4) = 0x10；byte1 = 0。
    dqCommon::ContourDisplay changed;
    changed.displayContours = true;
    changed.groups.push_back(dqCommon::ContourGroup{});
    changed.groups.push_back(makeGroupForSubcat(0x20));
    setTargetContours(*fixture.target, changed);
    contours->update(table);
    EXPECT_EQ(0x10, contours->getLutData()[0]);
    EXPECT_EQ(0x00, contours->getLutData()[1]);
}

// wantContourLines 门（BatchUniforms.ts:110-113——currentBranch.contourLine
// 在场 && displayContours && groups.length>0）。
TEST(ContoursTest, WantContourLinesGate)
{
    NullTargetFixture fixture;
    auto& batch = fixture.target->getUniforms().batch;

    // 无 contours → false。
    EXPECT_FALSE(batch.wantContourLines());

    // displayContours=false → false。
    dqCommon::ContourDisplay off;
    off.displayContours = false;
    off.groups.push_back(dqCommon::ContourGroup{});
    setTargetContours(*fixture.target, off);
    EXPECT_FALSE(batch.wantContourLines());

    // displayContours=true + 组空 → false。
    dqCommon::ContourDisplay emptyGroups;
    emptyGroups.displayContours = true;
    setTargetContours(*fixture.target, emptyGroups);
    EXPECT_FALSE(batch.wantContourLines());

    // displayContours=true + 组非空 → true。
    dqCommon::ContourDisplay on;
    on.displayContours = true;
    on.groups.push_back(dqCommon::ContourGroup{});
    setTargetContours(*fixture.target, on);
    EXPECT_TRUE(batch.wantContourLines());
}

// Batch.getContours 缓存形（Graphic.ts:135-146 PerTargetBatchData.getContours——
// 同一 batch/target 复用 + 特征数变重建）。
TEST(ContoursTest, BatchGetContoursCachesPerFeatureCount)
{
    NullTargetFixture fixture;
    dqCommon::ContourDisplay d;
    d.displayContours = true;
    d.groups.push_back(dqCommon::ContourGroup{});
    setTargetContours(*fixture.target, d);

    auto table = std::make_unique<dqCommon::PackedFeatureTable>(makePackedTable());
    auto* tablePtr = table.get();
    Batch batch(3, std::move(table));

    auto& c1 = batch.getContours(*fixture.target);
    EXPECT_EQ(3u, c1.getNumFeatures());
    // 复用（同 target 同表 → 同一对象）。
    auto& c2 = batch.getContours(*fixture.target);
    EXPECT_EQ(&c1, &c2);
    (void)tablePtr;
}

// shader 槽激活（Contours.ts:102-151 addApplyContours + Surface.ts:776 的
// **无条件**编译面——无 TechniqueFlags 变体位，运行时 u_contourLUTWidth==0u
// 早退门[:17-18]）：Surface 变体源恒含 applyContours 调用 + u_contourDefs[8]
// 数组 + v_contourNdx/v_height 计算通道 + u_contourLUT/u_contourLUTWidth/
// u_modelToWorldC 注册。
TEST(ContoursTest, SurfaceVariantAlwaysCompilesApplyContours)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Quantized;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    // 顶点侧：computeContourNdx（Contours.ts:16-29）+ computeWorldHeight
    //（:105-111）+ v_contourNdx/v_height computed-varying 赋值 +
    // u_contourLUT/u_contourLUTWidth/u_modelToWorldC uniform 声明。
    EXPECT_NE(vert.find("computeContourNdx"), std::string::npos);
    EXPECT_NE(vert.find("computeWorldHeight"), std::string::npos);
    EXPECT_NE(vert.find("v_contourNdx"), std::string::npos);
    EXPECT_NE(vert.find("v_height"), std::string::npos);
    EXPECT_NE(vert.find("u_contourLUT"), std::string::npos);
    EXPECT_NE(vert.find("u_contourLUTWidth"), std::string::npos);
    EXPECT_NE(vert.find("u_modelToWorldC"), std::string::npos);
    // 早退门原样（Contours.ts:17-18）。
    EXPECT_NE(vert.find("u_contourLUTWidth == 0u"), std::string::npos);

    // 片元侧：applyContours 函数 + u_contourDefs[8]（ContourUniforms 的
    // ceil(5*1.5)=8——Contours.ts:141）+ main 调用（ShaderBuilder.ts:1097-1101
    // 序——reverseWoW 之后 applyFlash 之前）。
    EXPECT_NE(frag.find("u_contourDefs[8]"), std::string::npos);
    EXPECT_NE(frag.find("applyContours"), std::string::npos);
    EXPECT_NE(frag.find("baseColor = applyContours(baseColor);"), std::string::npos);
}
