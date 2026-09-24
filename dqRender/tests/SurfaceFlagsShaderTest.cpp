// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface flags shader tests
// Authored: GL-free verification of addSurfaceFlags GLSL output + binding
//           (itwinjs-core tests this live; DanQing verifies the assembled source
//           string + GraphicUniform registration without GL).

#include "render/DrawParams.h"
#include "render/ShaderBindings.h"
#include "render/ShaderBuilder.h"
#include "render/ShaderProgramImpl.h"
#include "render/SurfaceFlags.h"
#include "render/SurfaceGeometry.h"
#include "render/UniformHandle.h"

#include "NullDriver.h"

#include <gtest/gtest.h>

#include <string>

#include <dqCommon/ViewFlags.h>

using namespace dqRender;

// Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 507-527)
// Verifies the assembled GLSL contains the u_surfaceFlags system.
TEST(SurfaceFlagsShaderTest, ProducesCorrectGlsl)
{
    ProgramBuilder pb;
    addSurfaceFlags(pb, /*withFeatureOverrides*/true, /*withFeatureColor*/true);
    // addSurfaceFlags registers a fragment initializer but no FragmentShaderComponent;
    // add a dummy AssignFragData so buildSourceWithComponents emits a fragment main()
    // that contains the registered initializer.
    pb.getFragmentBuilder().setFragmentComponent(
        FragmentShaderComponent::AssignFragData, "    fragColor = vec4(0.0);\n");
    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    // Vertex: uniform array + computed varying + index constants.
    EXPECT_NE(vert.find("uniform bool u_surfaceFlags[12]"), std::string::npos);
    EXPECT_NE(vert.find("v_surfaceFlags"), std::string::npos);
    EXPECT_NE(vert.find("computeSurfaceFlags"), std::string::npos);
    EXPECT_NE(vert.find("kSurfaceBitIndex_HasTexture"), std::string::npos);

    // Fragment: global + initializer unpacking v_surfaceFlags.
    EXPECT_NE(frag.find("uint surfaceFlags"), std::string::npos);
    EXPECT_NE(frag.find("surfaceFlags = uint(floor(v_surfaceFlags + 0.5));"), std::string::npos);
}

TEST(SurfaceFlagsShaderTest, WithoutFeatureOverridesOmitsComputeBase)
{
    ProgramBuilder pb;
    addSurfaceFlags(pb, /*withFeatureOverrides*/false, /*withFeatureColor*/false);
    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    EXPECT_NE(vert.find("computeSurfaceFlags"), std::string::npos);
    // No feature_ignore_material branch when overrides disabled.
    EXPECT_EQ(vert.find("feature_ignore_material"), std::string::npos);
}

// Authored: GL-free binding registration (no itwinjs equivalent).
TEST(SurfaceFlagsShaderTest, WireSurfaceFlagsRegistersGraphicUniform)
{
    ShaderBuilder vert;
    vert.setStage(ShaderStage::Vertex);
    wireSurfaceFlags(vert);

    ShaderProgram prog;
    vert.addBindings(prog);
    EXPECT_TRUE(prog.hasGraphicUniform("u_surfaceFlags"));
}

TEST(SurfaceFlagsShaderTest, BindingWritesFlagsToCache)
{
    ShaderBuilder vert;
    vert.setStage(ShaderStage::Vertex);
    wireSurfaceFlags(vert);

    ShaderProgram prog;
    vert.addBindings(prog);

    DrawParams dp;
    int flags[12] = {1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
    dp.setSurfaceFlags(flags);

    UniformHandle handle;
    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_surfaceFlags", handle, dp));
    int const* cached = handle.getIntData();
    for (int i = 0; i < 12; ++i)
        EXPECT_EQ(cached[i], flags[i]);
    EXPECT_EQ(handle.getType(), UniformHandle::UniformType::IntArray);  // -2 = int-array sentinel
}

TEST(SurfaceFlagsShaderTest, SetUniform1ivCacheBehavior)
{
    UniformHandle h;
    int const a[12] = {1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0};
    EXPECT_TRUE(h.setUniform1iv(a, 12));   // first write → changed
    EXPECT_FALSE(h.setUniform1iv(a, 12));  // identical → not changed
}

// Ported from: itwinjs-core MeshGeometry.computeSurfaceFlags()
TEST(SurfaceFlagsShaderTest, ComputeSurfaceFlagsDefaultMesh)
{
    // Opaque lit surface, no textures, no material info → IgnoreMaterial set.
    rhi::NullDriver driver;
    SurfaceGeometry geom(driver, rhi::IndexBufferHandle{}, 6u, SurfaceType::Opaque, /*isPlanar*/false, /*hasTextures*/false);
    // Production gates (wantNormalMaps, SurfaceGeometry.ts :416-428): the
    // blank-connection view sets renderMode=SmoothShade (ViewPicker.ts :161).
    dqCommon::ViewFlagsProperties vp;
    vp.renderMode = dqCommon::RenderMode::SmoothShade;
    int const* f = SurfaceGeometry::computeSurfaceFlags(geom, /*displayNormalMaps*/true, vp);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f[static_cast<int>(GL::SurfaceBitIndex::ApplyLighting)], 1);
    EXPECT_EQ(f[static_cast<int>(GL::SurfaceBitIndex::HasNormals)], 1);
    EXPECT_EQ(f[static_cast<int>(GL::SurfaceBitIndex::HasColorAndNormal)], 1);
    EXPECT_EQ(f[static_cast<int>(GL::SurfaceBitIndex::hasTexture)], 0);
    EXPECT_EQ(f[static_cast<int>(GL::SurfaceBitIndex::IgnoreMaterial)], 1);
    // No normal map on this geometry → HasNormalMap stays 0 even with all
    // gates open (wantNormalMaps' normalMapExists term, :417).
    EXPECT_EQ(f[static_cast<int>(GL::SurfaceBitIndex::HasNormalMap)], 0);
}
