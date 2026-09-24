// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface texture shader tests
// Authored: GL-free verification of addTexture GLSL output
//           (itwinjs-core tests this live; DanQing verifies the assembled source
//           string without GL). s_texture is a sampler — bound via the legacy
//           params.setInt + driver.bindTexture path, not GraphicUniform cache.

#include "render/ShaderBuilder.h"
#include "render/SurfaceFlags.h"
#include "render/SurfaceTexture.h"

#include <gtest/gtest.h>

#include <string>

using namespace dqRender;

// Ported from: itwinjs-core Surface.ts addTexture() (line 571-687)
// Verifies the assembled GLSL contains the texture system (attribute path).
TEST(SurfaceTextureShaderTest, ProducesCorrectGlsl)
{
    ProgramBuilder pb;
    addSurfaceFlags(pb, /*withFeatureOverrides*/true, /*withFeatureColor*/true);
    addTexture(pb);
    // addTexture sets ComputeBaseColor → frag main() is emitted.
    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    // Vertex: a_texCoord + v_texCoord.
    EXPECT_NE(vert.find("a_texCoord"), std::string::npos);
    EXPECT_NE(vert.find("v_texCoord"), std::string::npos);

    // Attribute path (§3.4 deviation): a_texCoord, NOT LUT g_vertLutData.
    EXPECT_EQ(vert.find("g_vertLutData"), std::string::npos);

    // Fragment: s_texture sampler + sampleSurfaceTexture + g_surfaceTexel +
    // getSurfaceColor + TEXTURE macro usage.
    EXPECT_NE(frag.find("uniform sampler2D s_texture"), std::string::npos);
    EXPECT_NE(frag.find("sampleSurfaceTexture"), std::string::npos);
    EXPECT_NE(frag.find("g_surfaceTexel"), std::string::npos);
    EXPECT_NE(frag.find("getSurfaceColor"), std::string::npos);
    // TEXTURE macro is defined as `texture` — the source uses TEXTURE() which
    // expands to texture() at GL compile time.
    EXPECT_NE(frag.find("TEXTURE(s_texture, v_texCoord)"), std::string::npos);

    // ComputeBaseColor body (inlined into main): g_surfaceTexel = sampleSurfaceTexture();
    // vec4 surfaceColor = getSurfaceColor(); (glyph path uses surfaceColor, not baseColor)
    EXPECT_NE(frag.find("g_surfaceTexel = sampleSurfaceTexture()"), std::string::npos);
    EXPECT_NE(frag.find("getSurfaceColor()"), std::string::npos);
}
