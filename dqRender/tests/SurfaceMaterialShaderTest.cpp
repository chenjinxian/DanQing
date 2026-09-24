// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface material shader tests
// Authored: GL-free verification of addMaterial GLSL output + u_materialParams binding
//           (itwinjs-core tests this live; DanQing verifies the assembled source
//           string + GraphicUniform registration without GL).

#include "render/DrawParams.h"
#include "render/ShaderBindings.h"
#include "render/ShaderBuilder.h"
#include "render/ShaderProgramImpl.h"
#include "render/SurfaceFlags.h"
#include "render/SurfaceMaterial.h"
#include "render/UniformHandle.h"

#include <gtest/gtest.h>

#include <string>

using namespace dqRender;

// Ported from: itwinjs-core Surface.ts addMaterial() (line 183-238)
// Verifies the assembled GLSL contains the material system.
TEST(SurfaceMaterialShaderTest, ProducesCorrectGlsl)
{
    ProgramBuilder pb;
    addSurfaceFlags(pb, /*withFeatureOverrides*/true, /*withFeatureColor*/true);
    addMaterial(pb, /*instanced*/false, /*quantized*/false);
    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    // Vertex: u_materialColor + u_materialParams + mat_rgb/mat_alpha/use_material +
    // g_materialParams + decodeMaterialColor.
    EXPECT_NE(vert.find("u_materialColor"), std::string::npos);
    EXPECT_NE(vert.find("u_materialParams"), std::string::npos);
    EXPECT_NE(vert.find("mat_rgb"), std::string::npos);
    EXPECT_NE(vert.find("mat_alpha"), std::string::npos);
    EXPECT_NE(vert.find("use_material"), std::string::npos);
    EXPECT_NE(vert.find("decodeMaterialColor"), std::string::npos);

    // Fragment: mat_texture_weight/mat_weights/mat_specular + decodeMaterialParams.
    EXPECT_NE(frag.find("mat_texture_weight"), std::string::npos);
    EXPECT_NE(frag.find("mat_weights"), std::string::npos);
    EXPECT_NE(frag.find("mat_specular"), std::string::npos);
    EXPECT_NE(frag.find("decodeMaterialParams"), std::string::npos);
}

// Authored: GL-free binding registration (no itwinjs equivalent).
TEST(SurfaceMaterialShaderTest, WireMaterialParamsRegistersGraphicUniform)
{
    ShaderBuilder vert;
    vert.setStage(ShaderStage::Vertex);
    wireMaterialParams(vert);

    ShaderProgram prog;
    vert.addBindings(prog);
    EXPECT_TRUE(prog.hasGraphicUniform("u_materialParams"));
}

TEST(SurfaceMaterialShaderTest, BindingWritesParamsToCache)
{
    ShaderBuilder vert;
    vert.setStage(ShaderStage::Vertex);
    wireMaterialParams(vert);

    ShaderProgram prog;
    vert.addBindings(prog);

    DrawParams dp;
    float const mp[4] = {26265.0f, 65535.0f, 65535.0f, 13.5f};  // itwinjs defaults
    dp.setMaterialParams(mp);

    UniformHandle handle;
    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_materialParams", handle, dp));
    EXPECT_EQ(handle.getType(), UniformHandle::UniformType::Vec4);  // vec4 = 4 floats
    float const* cached = handle.getData();
    for (int i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(cached[i], mp[i]);
}
