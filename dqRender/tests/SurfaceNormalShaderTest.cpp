// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface normal shader tests
// Authored: GL-free verification of addNormal GLSL output + binding
//           (itwinjs-core tests this live; DanQing verifies the assembled source
//           string + GraphicUniform registration without GL).

#include "render/DrawParams.h"
#include "render/ShaderBindings.h"
#include "render/ShaderBuilder.h"
#include "render/ShaderProgramImpl.h"
#include "render/SurfaceFlags.h"
#include "render/SurfaceNormal.h"
#include "render/UniformHandle.h"

#include <gtest/gtest.h>

#include <string>

using namespace dqRender;

// Ported from: itwinjs-core Surface.ts addNormal() (line 529-566)
// Verifies the assembled GLSL contains the normal system (attribute path).
TEST(SurfaceNormalShaderTest, ProducesCorrectGlsl)
{
    ProgramBuilder pb;
    addSurfaceFlags(pb, /*withFeatureOverrides*/true, /*withFeatureColor*/true);
    addNormal(pb);
    // addNormal sets FinalizeNormal → frag main() is emitted.
    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    // Vertex: u_normalMatrix + MAT_NORM macro + computeSurfaceNormal + v_n + a_normal.
    EXPECT_NE(vert.find("uniform mat3 u_normalMatrix"), std::string::npos);
    EXPECT_NE(vert.find("MAT_NORM"), std::string::npos);
    EXPECT_NE(vert.find("computeSurfaceNormal"), std::string::npos);
    EXPECT_NE(vert.find("v_n"), std::string::npos);
    EXPECT_NE(vert.find("a_normal"), std::string::npos);

    // Attribute path (§3.4 deviation): MAT_NORM * a_normal, NOT LUT g_vertLutData.
    EXPECT_NE(vert.find("MAT_NORM * a_normal"), std::string::npos);
    EXPECT_EQ(vert.find("g_vertLutData"), std::string::npos);

    // Fragment: g_normal + finalizeNormal (flip on gl_FrontFacing).
    EXPECT_NE(frag.find("g_normal"), std::string::npos);
    EXPECT_NE(frag.find("normalize(v_n)"), std::string::npos);
    EXPECT_NE(frag.find("gl_FrontFacing"), std::string::npos);
}

// Authored: GL-free binding registration (no itwinjs equivalent).
TEST(SurfaceNormalShaderTest, WireNormalMatrixRegistersGraphicUniform)
{
    ShaderBuilder vert;
    vert.setStage(ShaderStage::Vertex);
    wireNormalMatrix(vert);

    ShaderProgram prog;
    vert.addBindings(prog);
    EXPECT_TRUE(prog.hasGraphicUniform("u_normalMatrix"));
}

TEST(SurfaceNormalShaderTest, BindingWritesMatrixToCache)
{
    ShaderBuilder vert;
    vert.setStage(ShaderStage::Vertex);
    wireNormalMatrix(vert);

    ShaderProgram prog;
    vert.addBindings(prog);

    DrawParams dp;
    float const nm[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    dp.setNormalMatrix(nm);

    UniformHandle handle;
    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_normalMatrix", handle, dp));
    EXPECT_EQ(handle.getType(), UniformHandle::UniformType::Mat3);  // 9 = mat3 sentinel (positive = float count)
    float const* cached = handle.getData();
    for (int i = 0; i < 9; ++i)
        EXPECT_FLOAT_EQ(cached[i], nm[i]);
}
