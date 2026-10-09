// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for batch rendering (Phase B)
// DanQing dqRender — Batch B tests (GLSL shader builders)
//
// Tests for all GLSL shader builder modules.

#include "render/CompositingShaderBuilders.h"
#include "render/FeatureEffectShaderBuilders.h"
#include "render/SkyShaderBuilders.h"
#include "shader/AmbientOcclusionShaders.h"  // M-T T-d：kAmbientOcclusionFrag
#include "shader/BlurShaders.h"              // M-T T-d：kBlurCommonPrefix/...

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// ============================================================================
// Compositing shader builder tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, FullscreenQuadVertex)
{
    char const* src = getFullscreenQuadVertexShader();
    EXPECT_NE(std::string(src).find("a_position"), std::string::npos);
    EXPECT_NE(std::string(src).find("v_texCoord"), std::string::npos);
}

// M-T T-d：SsaoFragment/BlurFragment 两桩测试替换为参考移植源的源锁——
// 旧断言钉的是自创 SSAO 的发明 uniform（s_depthTexture/u_radius/u_texelSize
// 等），与参考 HBAO/高斯模糊语义背离；桩件随本件清退（TechniqueId 注册面
// 已撤——AO/Blur 程序归合成器内全屏 pass 直持）。
// Authored: no reference test exists in itwinjs-core for batch rendering
//          （锁移植后 shader 源的参考标识符——glsl/AmbientOcclusion.ts /
//           glsl/Blur.ts）。
TEST(CompositingShaderBuildersTest, AmbientOcclusionFragmentMatchesReference)
{
    std::string const s{std::string(kAmbientOcclusionFrag)};
    // PB 变体链令牌（参考 createAmbientOcclusionProgram 的 PB 臂）：
    EXPECT_NE(s.find("u_pickDepthAndOrder"), std::string::npos);
    EXPECT_NE(s.find("readDepthAndOrder"), std::string::npos);
    EXPECT_NE(s.find("decodeDepthRgb"), std::string::npos);
    EXPECT_NE(s.find("computePositionFromDepth"), std::string::npos);
    EXPECT_NE(s.find("computeNormalFromDepth"), std::string::npos);
    EXPECT_NE(s.find("u_hbaoSettings"), std::string::npos);
    EXPECT_NE(s.find("u_maxDistance"), std::string::npos);
    EXPECT_NE(s.find("u_noise"), std::string::npos);
    // 主循环结构（4 方向 × 6 步——参考 :93-121）。
    EXPECT_NE(s.find("for (int i = 0; i < 4; i++)"), std::string::npos);
    EXPECT_NE(s.find("for (int j = 0; j < 6; j++)"), std::string::npos);
    EXPECT_NE(s.find("kRenderOrder_LitSurface"), std::string::npos);
}

// Authored: 同上（Blur.ts——高斯 7 步 + u_blurSettings/u_blurDir 令牌 +
//           TestOrder 臂的 order 跳读）。
TEST(CompositingShaderBuildersTest, BlurFragmentMatchesReference)
{
    std::string const common{std::string(kBlurCommonPrefix)};
    EXPECT_NE(common.find("u_blurSettings"), std::string::npos);
    EXPECT_NE(common.find("u_blurDir"), std::string::npos);
    EXPECT_NE(common.find("u_textureToBlur"), std::string::npos);
    EXPECT_NE(common.find("gaussian"), std::string::npos);
    std::string const testOrder{std::string(kBlurTestOrderPrefix) +
                                std::string(kBlurTestOrderFrag)};
    EXPECT_NE(testOrder.find("u_pickDepthAndOrder"), std::string::npos);
    EXPECT_NE(testOrder.find("kRenderOrder_Silhouette"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, EdlFragment)
{
    char const* src = getEdlFragmentShader();
    std::string s(src);
    EXPECT_NE(s.find("u_edlStrength"), std::string::npos);
    EXPECT_NE(s.find("u_edlRadius"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, EvsmFragment)
{
    char const* src = getEvsmFromDepthFragmentShader();
    std::string s(src);
    EXPECT_NE(s.find("u_evsmExponent"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, SolarShadowMapSampling)
{
    char const* src = getSolarShadowMapSampling();
    std::string s(src);
    EXPECT_NE(s.find("s_shadowMap"), std::string::npos);
    EXPECT_NE(s.find("u_shadowMapMatrix"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, CompositeFragment)
{
    char const* src = getCompositeFragmentShader();
    std::string s(src);
    EXPECT_NE(s.find("s_colorTexture"), std::string::npos);
    EXPECT_NE(s.find("s_occlusionTexture"), std::string::npos);
}

// ============================================================================
// Feature/effect shader builder tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, FeatureSymbologyVertex)
{
    char const* src = getFeatureSymbologyVertexCode();
    std::string s(src);
    EXPECT_NE(s.find("computeFeatureSymbology"), std::string::npos);
    EXPECT_NE(s.find("u_batchId"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, FeatureSymbologyFragment)
{
    char const* src = getFeatureSymbologyFragmentCode();
    std::string s(src);
    EXPECT_NE(s.find("applyFeatureSymbology"), std::string::npos);
    EXPECT_NE(s.find("u_hiliteColor"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, PlanarClassification)
{
    char const* src = getPlanarClassificationCode();
    std::string s(src);
    EXPECT_NE(s.find("applyPlanarClassification"), std::string::npos);
    EXPECT_NE(s.find("s_classifierTexture"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, ThematicDisplay)
{
    char const* src = getThematicDisplayCode();
    std::string s(src);
    EXPECT_NE(s.find("applyThematicDisplay"), std::string::npos);
    EXPECT_NE(s.find("s_thematicLut"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, ContourLines)
{
    char const* src = getContourLinesCode();
    std::string s(src);
    EXPECT_NE(s.find("applyContours"), std::string::npos);
    EXPECT_NE(s.find("u_contourDefs"), std::string::npos);
    EXPECT_NE(s.find("u_contourLUT"), std::string::npos);
    EXPECT_NE(s.find("unpackAndNormalize2BytesVec4"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, MonochromeMode)
{
    char const* src = getMonochromeCode();
    std::string s(src);
    EXPECT_NE(s.find("applyMonochromeMode"), std::string::npos);
    EXPECT_NE(s.find("u_monochromeEnabled"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, Translucency)
{
    char const* src = getTranslucencyCode();
    std::string s(src);
    EXPECT_NE(s.find("writeOitOutput"), std::string::npos);
    EXPECT_NE(s.find("o_accum"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(FeatureEffectShaderBuildersTest, Wiremesh)
{
    char const* src = getWiremeshCode();
    std::string s(src);
    EXPECT_NE(s.find("applyWiremesh"), std::string::npos);
    EXPECT_NE(s.find("u_wiremeshEnabled"), std::string::npos);
}

// ============================================================================
// Sky shader builder tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, SkyBoxVertex)
{
    std::string src = buildSkyBoxVertexShader();
    EXPECT_NE(src.find("a_position"), std::string::npos);
    EXPECT_NE(src.find("v_texCoord"), std::string::npos);
    EXPECT_NE(src.find("pos.xyww"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, SkyBoxFragment)
{
    std::string src = buildSkyBoxFragmentShader();
    EXPECT_NE(src.find("s_skybox"), std::string::npos);
    EXPECT_NE(src.find("texture(s_skybox"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, SkySphereGradientVertex)
{
    std::string src = buildSkySphereGradientVertexShader();
    EXPECT_NE(src.find("u_skyZenithColor"), std::string::npos);
    EXPECT_NE(src.find("u_skyGroundColor"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, SkySphereGradientFragment)
{
    std::string src = buildSkySphereGradientFragmentShader();
    EXPECT_NE(src.find("v_skyColor"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, SkySphereTextureVertex)
{
    std::string src = buildSkySphereTextureVertexShader();
    EXPECT_NE(src.find("a_texCoord"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, SkySphereTextureFragment)
{
    std::string src = buildSkySphereTextureFragmentShader();
    EXPECT_NE(src.find("s_skyTexture"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, ScreenSpaceEffectVertex)
{
    std::string src = buildScreenSpaceEffectVertexShader();
    EXPECT_NE(src.find("a_position"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(SkyShaderBuildersTest, ScreenSpaceEffectFragment)
{
    std::string effectCode = "vec4 applyEffect(vec4 color, vec2 uv) { return color * 0.5; }";
    std::string src = buildScreenSpaceEffectFragmentShader(effectCode);
    EXPECT_NE(src.find("applyEffect"), std::string::npos);
    EXPECT_NE(src.find("s_inputTexture"), std::string::npos);
}
