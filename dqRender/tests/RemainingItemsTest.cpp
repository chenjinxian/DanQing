// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for remaining render items
// DanQing dqRender — Remaining items tests
//
// Tests for Matrix, RenderGeometry, RenderPlan, GraphicBranchFrustum,
// AnimationBranchState,
// RenderAreaPattern, RenderTerrain, PrimitiveBuilder,
// SceneVolumeClassifier, UpsampleRealityMeshParams, MeshMapLayerGraphicParams,
// GraphicTemplateImpl.

#include "render/Matrix.h"
#include "render/RenderGeometry.h"
#include <dqBase/DqId.h>
#include <dqRender/RenderPlan.h>
#include "render/GraphicBranchFrustum.h"
#include "render/AnimationBranchState.h"
#include "render/RenderAreaPattern.h"
#include "render/RenderTerrain.h"
#include "render/PrimitiveBuilder.h"
#include "render/SceneVolumeClassifier.h"
#include "render/UpsampleRealityMeshParams.h"
#include "render/MeshMapLayerGraphicParams.h"
#include "render/GraphicTemplateImpl.h"

#include "NullDriver.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace dqRender;

// ============================================================================
// Matrix tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix3Test, identity)
{
    Matrix3 m = Matrix3::identity();
    EXPECT_FLOAT_EQ(m.data[0], 1.0f);
    EXPECT_FLOAT_EQ(m.data[4], 1.0f);
    EXPECT_FLOAT_EQ(m.data[8], 1.0f);
    EXPECT_FLOAT_EQ(m.data[1], 0.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix3Test, FromMatrix4)
{
    Matrix4 m4 = Matrix4::identity();
    m4.data[0] = 2.0f;
    m4.data[5] = 3.0f;
    m4.data[10] = 4.0f;

    Matrix3 m3 = Matrix3::fromMatrix4(m4.ptr());
    EXPECT_FLOAT_EQ(m3.data[0], 2.0f);
    EXPECT_FLOAT_EQ(m3.data[4], 3.0f);
    EXPECT_FLOAT_EQ(m3.data[8], 4.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix4Test, identity)
{
    Matrix4 m = Matrix4::identity();
    EXPECT_FLOAT_EQ(m[0], 1.0f);
    EXPECT_FLOAT_EQ(m[5], 1.0f);
    EXPECT_FLOAT_EQ(m[10], 1.0f);
    EXPECT_FLOAT_EQ(m[15], 1.0f);
    EXPECT_FLOAT_EQ(m[1], 0.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix4Test, Translation)
{
    Matrix4 m = Matrix4::translation(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(m[12], 10.0f);
    EXPECT_FLOAT_EQ(m[13], 20.0f);
    EXPECT_FLOAT_EQ(m[14], 30.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix4Test, Scale)
{
    Matrix4 m = Matrix4::scale(2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(m[0], 2.0f);
    EXPECT_FLOAT_EQ(m[5], 3.0f);
    EXPECT_FLOAT_EQ(m[10], 4.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix4Test, multiply)
{
    Matrix4 a = Matrix4::translation(1.0f, 2.0f, 3.0f);
    Matrix4 b = Matrix4::scale(2.0f, 2.0f, 2.0f);
    Matrix4 c = Matrix4::multiply(a, b);

    // Translation * Scale: translation stays, scale applies
    EXPECT_FLOAT_EQ(c[12], 1.0f);
    EXPECT_FLOAT_EQ(c[13], 2.0f);
    EXPECT_FLOAT_EQ(c[14], 3.0f);
    EXPECT_FLOAT_EQ(c[0], 2.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix4Test, Transpose)
{
    Matrix4 m = Matrix4::identity();
    m.data[1] = 5.0f;  // row 0, col 1
    Matrix4 t = m.transpose();
    EXPECT_FLOAT_EQ(t.data[4], 5.0f);  // row 1, col 0
    EXPECT_FLOAT_EQ(t.data[1], 0.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(Matrix4Test, GetTranslation)
{
    Matrix4 m = Matrix4::translation(10.0f, 20.0f, 30.0f);
    float x, y, z;
    m.getTranslation(x, y, z);
    EXPECT_FLOAT_EQ(x, 10.0f);
    EXPECT_FLOAT_EQ(y, 20.0f);
    EXPECT_FLOAT_EQ(z, 30.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(MatrixHelpersTest, FromSumOf)
{
    float p[3] = {1, 2, 3};
    float v[3] = {0, 0, 1};
    float result[3];
    fromSumOf(p, v, 5.0f, result);
    EXPECT_FLOAT_EQ(result[0], 1.0f);
    EXPECT_FLOAT_EQ(result[1], 2.0f);
    EXPECT_FLOAT_EQ(result[2], 8.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(MatrixHelpersTest, NormalizedDifference)
{
    float p0[3] = {1, 0, 0};
    float p1[3] = {0, 0, 0};
    float result[3];
    normalizedDifference(p0, p1, result);
    EXPECT_FLOAT_EQ(result[0], 1.0f);
    EXPECT_FLOAT_EQ(result[1], 0.0f);
    EXPECT_FLOAT_EQ(result[2], 0.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(MatrixHelpersTest, Ortho)
{
    float result[16];
    ortho(-1, 1, -1, 1, 0, 100, result);
    EXPECT_FLOAT_EQ(result[0], 1.0f);   // 2/(right-left)
    EXPECT_FLOAT_EQ(result[5], 1.0f);   // 2/(top-bottom)
    EXPECT_FLOAT_EQ(result[10], -0.02f); // -2/(far-near)
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(MatrixHelpersTest, Frustum)
{
    float result[16];
    frustum(-1, 1, -1, 1, 1, 100, result);
    EXPECT_FLOAT_EQ(result[0], 1.0f);   // 2*near/(right-left)
    EXPECT_FLOAT_EQ(result[5], 1.0f);   // 2*near/(top-bottom)
    EXPECT_FLOAT_EQ(result[11], -1.0f);
}

// ============================================================================
// RenderPlan tests (public SDK struct — Ported from internal/render/RenderPlan.ts)
// ============================================================================

// Authored: no reference test exists in itwinjs-core for the stub field defaults
TEST(RenderPlanTest, DefaultState)
{
    RenderPlan plan;
    EXPECT_EQ(plan.backgroundColor, 0xFFFFFFFFu);  // white
    EXPECT_FLOAT_EQ(plan.sunIntensity, 0.7f);
    // M-O(1)：bool → dqCommon::MonochromeMode（默认 Scaled，RenderPlan.ts:83）。
    EXPECT_EQ(plan.monochromeMode, dqCommon::MonochromeMode::Scaled);
    EXPECT_TRUE(plan.whiteOnWhiteReversal);
    EXPECT_EQ(plan.featureOverrides, nullptr);
}

// Authored: no reference test exists in itwinjs-core for the stub field defaults
TEST(RenderPlanTest, ConfigureFields)
{
    RenderPlan plan;
    plan.backgroundColor = 0x000000FFu;  // red
    plan.sunDirection[0] = 0.5f;
    plan.sunDirection[1] = 0.5f;
    plan.sunDirection[2] = 0.7071f;
    plan.sunIntensity = 0.8f;
    plan.monochromeMode = dqCommon::MonochromeMode::Flat;

    EXPECT_EQ(plan.backgroundColor, 0x000000FFu);
    EXPECT_FLOAT_EQ(plan.sunDirection[2], 0.7071f);
    EXPECT_FLOAT_EQ(plan.sunIntensity, 0.8f);
    EXPECT_EQ(plan.monochromeMode, dqCommon::MonochromeMode::Flat);
}

// Authored: Equals drives Viewport change detection
TEST(RenderPlanTest, EqualsChangeDetection)
{
    RenderPlan a;
    RenderPlan b;
    EXPECT_TRUE(a == b);
    b.backgroundColor = 0x000000FFu;
    EXPECT_FALSE(a == b);
}

// Authored: no reference test exists in itwinjs-core for RenderPlan.equals
// thematic coverage —— plan.equals 驱动 Viewport 变化检测（RenderPlan.ts:127
// thematic 段（is3d && viewFlags.thematicDisplay 门）；M-S S-b）。
TEST(RenderPlanTest, ThematicEqualityAndPresence)
{
    RenderPlan a;
    RenderPlan b;
    // 双缺席相等
    EXPECT_TRUE(a == b);
    // 缺席 → 在场：不等
    dqCommon::ThematicDisplay td;
    b.thematic = td;
    EXPECT_FALSE(a == b);
    // 同值在场：相等
    a.thematic = td;
    EXPECT_TRUE(a == b);
    // 在场字段翻：不等
    b.thematic->displayMode = dqCommon::ThematicDisplayMode::Slope;
    EXPECT_FALSE(a == b);
    b.thematic->displayMode = dqCommon::ThematicDisplayMode::Height;
    EXPECT_TRUE(a == b);
    // range 段翻：不等
    b.thematic->range = dqGeom::Range1d(0.0, 100.0);
    EXPECT_FALSE(a == b);
    // 在场 → 缺席：不等
    b.thematic = std::nullopt;
    EXPECT_FALSE(a == b);
    a.thematic = std::nullopt;
    EXPECT_TRUE(a == b);
}

// Authored: 同上（RenderPlan.ts:60 ao 段——M-T T-b；AmbientOcclusion.Settings.
//           equals 为 DanQing 增补面[参考 Settings 无 equals——T-b 登记]）。
TEST(RenderPlanTest, AoEqualityAndPresence)
{
    RenderPlan a;
    RenderPlan b;
    // 双缺席相等。
    EXPECT_TRUE(a == b);
    // 缺席 → 在场：不等。
    dqCommon::AmbientOcclusion::Settings ao;
    b.ao = ao;
    EXPECT_FALSE(a == b);
    // 同值在场：相等。
    a.ao = ao;
    EXPECT_TRUE(a == b);
    // 在场字段翻：不等。
    b.ao->bias = 0.9;
    EXPECT_FALSE(a == b);
    b.ao->bias = 0.25;
    EXPECT_TRUE(a == b);
    b.ao->blurSigma = 4.0;
    EXPECT_FALSE(a == b);
    // 在场 → 缺席：不等。
    b.ao = std::nullopt;
    EXPECT_FALSE(a == b);
    a.ao = std::nullopt;
    EXPECT_TRUE(a == b);
}

// ============================================================================
// GraphicBranchFrustum tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(GraphicBranchFrustumTest, ComputeFrustumScale)
{
    float transform[16] = {2,0,0,0, 0,3,0,0, 0,0,1,0, 0,0,0,1};
    float scaleX, scaleY;
    GraphicBranchFrustum::computeFrustumScale(transform, scaleX, scaleY);
    EXPECT_FLOAT_EQ(scaleX, 2.0f);
    EXPECT_FLOAT_EQ(scaleY, 3.0f);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(GraphicBranchFrustumTest, MultiplyTransforms)
{
    float a[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};
    float b[16] = {2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,1};
    float result[16];
    GraphicBranchFrustum::multiplyTransforms(a, b, result);
    EXPECT_FLOAT_EQ(result[12], 10.0f);  // translation preserved
    EXPECT_FLOAT_EQ(result[0], 2.0f);    // scale applied
}

// ============================================================================
// AnimationBranchState tests（M-U U-e：参考形重写——早期自创桩三例
// [setTransform/hasTransform/isVisible 等非参考 API] 随 AnimationBranchState.h
// 全文重写清退；新例锁 formatAnimationBranchId 与 fromScript 的参考语义）。
// ============================================================================

// Ported from: itwinjs-core AnimationBranchState.ts formatAnimationBranchId (:21-26)
TEST(AnimationBranchStateTest, FormatAnimationBranchId)
{
    using namespace dqRender;
    EXPECT_EQ(formatAnimationBranchId("0x123", -1), "0x123");
    EXPECT_EQ(formatAnimationBranchId("0x123", 0), "0x123_Node_0");
    EXPECT_EQ(formatAnimationBranchId("0x123", 7), "0x123_Node_7");
}

// Ported from: itwinjs-core AnimationBranchState.ts AnimationBranchStates.fromScript
//              (:44-63——requiresBatching[element 级 visibility×batchId 腿]的
//               omit 分支态；模型级 cutting plane 的 clip 分支态)。
TEST(AnimationBranchStateTest, FromScriptProducesOmitAndClipStates)
{
    using namespace dqRender;
    using namespace dqCommon::RenderSchedule;

    ScriptBuilder builder;
    auto& model = builder.addModelTimeline("0x1");
    auto& hidden = model.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xa")});
    hidden.addVisibility(0.0, 0.0);  // 0% 可见性 → omit
    auto& visible = model.addElementTimeline(dqBase::DqIdVector{dqBase::DqId::FromString("0xb")});
    visible.addVisibility(0.0, 100.0);
    auto props = builder.finish();
    // element 级 visibility + 非 0 batchId → requiresBatching（:835）——
    // fromScript 的前置门由此开（containsModelClipping 假 + requiresBatching 真）。
    ASSERT_EQ(props[0].elementTimelines->size(), 2u);

    auto script = Script::fromJSON(props);
    ASSERT_TRUE(script);

    auto states = animationBranchStatesFromScript(*script, 0.0);
    ASSERT_TRUE(states.has_value());

    // batchId 1（首个 element timeline）的 visibility(0)=0 → omit。
    auto const omitIt = states->branchStates.find("0x1_Node_1");
    ASSERT_TRUE(omitIt != states->branchStates.end());
    EXPECT_TRUE(omitIt->second.omit);
    EXPECT_TRUE(omitIt->second.clip.IsNull());

    // batchId 2 可见且无 cutting plane → 无分支态条目。
    EXPECT_EQ(states->branchStates.find("0x1_Node_2"), states->branchStates.end());
}

// Ported from: itwinjs-core AnimationBranchState.ts AnimationBranchStates.fromScript
//              (:45——无裁剪且无批处理需求时返回 undefined[nullopt]）。
TEST(AnimationBranchStateTest, FromScriptReturnsNulloptWithoutAnimation)
{
    using namespace dqRender;
    using namespace dqCommon::RenderSchedule;

    ScriptBuilder builder;
    auto& model = builder.addModelTimeline("0x1");
    model.addVisibility(0.0, 50.0);  // 仅模型级 visibility（requiresBatching 假：
                                     // 无 element timeline、无 cutting plane）
    auto script = Script::fromJSON(builder.finish());
    ASSERT_TRUE(script);

    EXPECT_FALSE(animationBranchStatesFromScript(*script, 0.0).has_value());
}

// ============================================================================
// RenderAreaPattern tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(RenderAreaPatternTest, DefaultState)
{
    RenderAreaPattern pattern;
    EXPECT_EQ(pattern.getType(), AreaPatternType::None);
    EXPECT_FALSE(pattern.isActive());
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(RenderAreaPatternTest, Configure)
{
    RenderAreaPattern pattern;
    pattern.setType(AreaPatternType::CrossHatch);
    pattern.setScale(2.0f);
    pattern.setColor(0xFF0000);
    pattern.setWeight(1.5f);

    EXPECT_TRUE(pattern.isActive());
    EXPECT_EQ(pattern.getType(), AreaPatternType::CrossHatch);
    EXPECT_FLOAT_EQ(pattern.getScale(), 2.0f);
    EXPECT_EQ(pattern.getColor(), 0xFF0000u);
    EXPECT_FLOAT_EQ(pattern.getWeight(), 1.5f);
}

// ============================================================================
// RenderTerrain tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(RenderTerrainTest, DefaultState)
{
    RenderTerrain terrain;
    EXPECT_FALSE(terrain.isEnabled());
    EXPECT_EQ(terrain.getResolution(), 256u);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(RenderTerrainTest, Configure)
{
    RenderTerrain terrain;
    terrain.setEnabled(true);
    terrain.setHeightRange(0.0f, 500.0f);
    terrain.setResolution(512);

    EXPECT_TRUE(terrain.isEnabled());
    EXPECT_FLOAT_EQ(terrain.getHeightMin(), 0.0f);
    EXPECT_FLOAT_EQ(terrain.getHeightMax(), 500.0f);
    EXPECT_EQ(terrain.getResolution(), 512u);
}

// ============================================================================
// SceneVolumeClassifier tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(SceneVolumeClassifierTest, DefaultState)
{
    SceneVolumeClassifier classifier;
    EXPECT_FALSE(classifier.isActive());
    EXPECT_EQ(classifier.getModelId(), 0u);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(SceneVolumeClassifierTest, Configure)
{
    SceneVolumeClassifier classifier;
    classifier.setModelId(42);
    classifier.setType(1);

    EXPECT_TRUE(classifier.isActive());
    EXPECT_EQ(classifier.getModelId(), 42u);
    EXPECT_EQ(classifier.getType(), 1u);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(SceneVolumeClassifierTest, AddRealityData)
{
    SceneVolumeClassifier classifier;
    VolumeClassifiedRealityData data;
    data.modelId = 10;
    data.geometricError = 0.5f;
    classifier.addRealityData(data);

    EXPECT_EQ(classifier.getRealityData().size(), 1u);
    EXPECT_EQ(classifier.getRealityData()[0].modelId, 10u);
}

// ============================================================================
// UpsampleRealityMeshParams tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(UpsampleRealityMeshParamsTest, DefaultState)
{
    UpsampleRealityMeshParams params;
    EXPECT_FLOAT_EQ(params.getTargetGeometricError(), 0.5f);
    EXPECT_EQ(params.getMaxSubdivisionLevel(), 20u);
    EXPECT_FLOAT_EQ(params.getScreenSpaceError(), 16.0f);
}

// ============================================================================
// MeshMapLayerGraphicParams tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(MeshMapLayerGraphicParamsTest, DefaultState)
{
    MeshMapLayerGraphicParams params;
    EXPECT_EQ(params.getLayerId(), 0u);
    EXPECT_EQ(params.getLod(), 0u);
    EXPECT_EQ(params.getRow(), 0u);
    EXPECT_EQ(params.getColumn(), 0u);
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(MeshMapLayerGraphicParamsTest, Configure)
{
    MeshMapLayerGraphicParams params;
    params.setLayerId(5);
    params.setLod(10);
    params.setRow(100);
    params.setColumn(200);
    params.setTileUrl("http://example.com/tile");

    EXPECT_EQ(params.getLayerId(), 5u);
    EXPECT_EQ(params.getLod(), 10u);
    EXPECT_EQ(params.getRow(), 100u);
    EXPECT_EQ(params.getColumn(), 200u);
    EXPECT_EQ(params.getTileUrl(), "http://example.com/tile");
}

// ============================================================================
// GraphicTemplateImpl tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(GraphicTemplateImplTest, DefaultState)
{
    GraphicTemplateImpl tmpl;
    EXPECT_EQ(tmpl.getMeshGraphic(), nullptr);
    // isInstanceable：参考 GraphicTemplateImpl.ts:57-66 默认 true（DanQing 简化
    // 恒 true——无不可实例化几何源接入）。
    EXPECT_TRUE(tmpl.isInstanceable());
}

// Authored: no reference test exists in itwinjs-core for remaining render items
TEST(GraphicTemplateImplTest, SetMeshGraphic)
{
    rhi::NullDriver driver;
    GraphicTemplateImpl tmpl;
    auto mesh = std::make_unique<MeshGraphic>(driver);
    tmpl.setMeshGraphic(std::move(mesh));
    EXPECT_NE(tmpl.getMeshGraphic(), nullptr);
}
