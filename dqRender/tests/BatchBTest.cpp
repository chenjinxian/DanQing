// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for batch rendering (Phase B)
// DanQing dqRender — Batch B tests (GLSL shader builders)
//
// Tests for all GLSL shader builder modules.

#include "render/CompositingShaderBuilders.h"
#include "render/FeatureEffectShaderBuilders.h"
#include "render/SkyShaderBuilders.h"

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

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, SsaoFragment)
{
    char const* src = getSsaoFragmentShader();
    std::string s(src);
    EXPECT_NE(s.find("s_depthTexture"), std::string::npos);
    EXPECT_NE(s.find("u_radius"), std::string::npos);
    EXPECT_NE(s.find("occlusion"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(CompositingShaderBuildersTest, BlurFragment)
{
    char const* src = getBlurFragmentShader();
    std::string s(src);
    EXPECT_NE(s.find("u_texelSize"), std::string::npos);
    EXPECT_NE(s.find("u_blurRadius"), std::string::npos);
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
