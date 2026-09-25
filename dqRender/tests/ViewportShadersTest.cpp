// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ViewportShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
#include "GlslCompileHarness.h"

#include "render/ViewportShaders.h"
#include "render/ShaderBuilder.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/TechniqueImpl.h"

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// modelToWindowCoordinates: references MAT_MV, u_proj, u_viewportTransformation,
// u_frustum, the overlay/background render-pass constants, and the segment-drop
// front-clip logic.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, ModelToWindowCoordinatesGlslFaithful)
TEST(ViewportShadersTest, ModelToWindowCoordinatesGlslFaithful)
{
    std::string const s(kModelToWindowCoordinates);
    EXPECT_NE(s.find("modelToWindowCoordinates"), std::string::npos);
    EXPECT_NE(s.find("MAT_MV"), std::string::npos);
    EXPECT_NE(s.find("u_proj"), std::string::npos);
    EXPECT_NE(s.find("u_viewportTransformation"), std::string::npos);
    EXPECT_NE(s.find("u_frustum"), std::string::npos);
    EXPECT_NE(s.find("kRenderPass_ViewOverlay"), std::string::npos);
    EXPECT_NE(s.find("kRenderPass_Background"), std::string::npos);
    EXPECT_NE(s.find("s_maxZ"), std::string::npos);
}

// addViewport registers u_viewport.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, AddViewportWiring)
TEST(ViewportShadersTest, AddViewportWiring)
{
    ProgramBuilder builder;
    addViewport(builder.getVertexBuilder());
    SUCCEED();
}

// addViewportTransformation registers u_viewportTransformation.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, AddViewportTransformationWiring)
TEST(ViewportShadersTest, AddViewportTransformationWiring)
{
    ProgramBuilder builder;
    addViewportTransformation(builder.getVertexBuilder());
    SUCCEED();
}

// addModelToWindowCoordinates wires model-view + projection + viewport transform
// + render pass + the function (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Viewport.ts
//              TEST(ViewportShadersTest, AddModelToWindowCoordinatesWiring)
TEST(ViewportShadersTest, AddModelToWindowCoordinatesWiring)
{
    ProgramBuilder builder;
    addModelToWindowCoordinates(builder.getVertexBuilder());
    SUCCEED();
}

// 量化 Surface 变体（usesQuantizedPositions + overrides）：VS 的
// 位置/法线/UV/颜色/featureIndex 五通道全部从 g_vertLutData* 读取，
// 不再声明 a_position/a_normal/a_texCoord/a_color/a_featureId attribute。
// 参考：Surface.ts addNormal(:529-566, getComputeNormal :396-406)、
//       addTexture(:571-687, getComputeTexCoord :460-467)、
//       Color.ts addColor(:51-62, getComputeElementColor :16-26)、
//       FeatureSymbology.ts getFeatureIndex(:79-86)、
//       Vertex.ts computeVertexPositionFromLUT(:35-41)。
// Ported from: itwinjs-core glsl/Surface.ts createSurfaceBuilder 量化变体
//              （VertexTable 量化顶点表消费）；测试形态对齐既有
//              SurfaceVariantCompilerTest 的"编译变体→断言源码包含关键片段"模式。
TEST(ViewportShadersTest, SurfaceQuantizedVariantReadsAllChannelsFromLUT)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Quantized;
    flags.featureMode = FeatureMode::Overrides;  // 覆盖 feature 索引源分支
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);

    std::string const& vs = prog.getVertSource();
    ASSERT_FALSE(vs.empty());

    // 位置：LUT 解码（addVertexTable 已供，computeVertexPositionFromLUT
    // Vertex.ts:35-41：qpos=g_vertLutData0/1 + g_featureAndMaterialIndex=g_vertLutData2）
    EXPECT_NE(vs.find("computeVertexPosition"), std::string::npos);
    // 法线：texel3.xy / texel1.zw → octDecodeNormal（Surface.ts:396-406）
    EXPECT_NE(vs.find("g_vertLutData3"), std::string::npos);
    EXPECT_NE(vs.find("octDecodeNormal"), std::string::npos);
    EXPECT_NE(vs.find("kSurfaceBitIndex_HasColorAndNormal"), std::string::npos);
    // UV：texel3 两对 u16 → decodeUInt16 + unquantize2d(u_qTexCoordParams)
    // （Surface.ts:460-467）
    EXPECT_NE(vs.find("unquantize2d"), std::string::npos);
    EXPECT_NE(vs.find("u_qTexCoordParams"), std::string::npos);
    // 颜色：uniform 色路径（Color.ts:51-62 u_color；颜色表采样 TODO）
    EXPECT_NE(vs.find("u_color"), std::string::npos);
    // feature 索引：texel2 → decodeUInt24(g_featureAndMaterialIndex.xyz)
    // （FeatureSymbology.ts:79-86 getFeatureIndex）
    EXPECT_NE(vs.find("decodeUInt24(g_featureAndMaterialIndex.xyz)"), std::string::npos);
    // 量化变体不再有 attribute 通道声明（位置/法线/UV/颜色/featureIndex）
    EXPECT_EQ(vs.find("in vec3 a_position"), std::string::npos);
    EXPECT_EQ(vs.find("in vec3 a_normal"), std::string::npos);
    EXPECT_EQ(vs.find("a_texCoord"), std::string::npos);
    EXPECT_EQ(vs.find("in vec4 a_color"), std::string::npos);
    EXPECT_EQ(vs.find("a_featureId"), std::string::npos);

    // 量化变体必须真实 GL 编译 + 链接通过（离线 harness）。
    test::GlslCompileResult r = test::compileAndLinkProgram(vs, prog.getFragSource());
    if (r.contextAvailable) {
        EXPECT_TRUE(r.vertCompiled)
            << "vertex shader compile failed:\n" << r.vertLog
            << "\n--- vertex source ---\n" << vs;
        EXPECT_TRUE(r.fragCompiled)
            << "fragment shader compile failed:\n" << r.fragLog;
        EXPECT_TRUE(r.linked) << "program link failed:\n" << r.linkLog;
    }
}
