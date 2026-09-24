// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — uniform-binding wiring tests (GL-free)
// Authored: itwinjs-core has no GL-free binding-registration test path (it
// validates live); these verify the addBindings -> ProgramUniform mechanism
// and the u_frustum binding lambda without a driver/GL context.
#include "render/CommonShaders.h"
#include "render/LightingShaders.h"
#include "render/MonochromeShaders.h"
#include "render/ShaderBuilder.h"
#include "render/ShaderProgramImpl.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/VertexShaders.h"
#include "render/ViewportShaders.h"

#include <gtest/gtest.h>

using namespace dqRender;

// addFrustum registers a ProgramUniform binding via addBindings (the linchpin
// mechanism that is otherwise never called). GL-free: default-construct
// ShaderProgram (no compile/driver).
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, AddFrustumRegistersProgramUniform)
{
    ProgramBuilder builder;
    addFrustum(builder);
    ShaderProgram prog;
    builder.getFragmentBuilder().addBindings(prog);
    builder.getVertexBuilder().addBindings(prog);
    EXPECT_TRUE(prog.hasProgramUniform("u_frustum"));
}

// The registered u_frustum bind callback null-guards when no target is set
// (legacy/test path) -- leaves the handle at its default value, no crash.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, FrustumBindingNullTargetIsNoOp)
{
    ProgramBuilder builder;
    addFrustum(builder);
    ShaderProgram prog;
    builder.getFragmentBuilder().addBindings(prog);

    ShaderProgramParams params;  // target == nullptr
    UniformHandle h;
    ASSERT_TRUE(prog.invokeProgramUniformForTest("u_frustum", h, params));
    // Null target -> lambda null-guards -> handle stays default (0,0,0).
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[2], 0.0f);
}

// The LIVE Surface path: SurfaceVariantCompiler::buildProgram sets the GLSL
// source AND registers the u_frustum ProgramUniform via addBindings (the
// linchpin that was previously never called).  GL-free: no compile/driver.
// Authored: itwinjs-core has no GL-free binding-registration test path.
TEST(UniformBindingTest, SurfaceBuildProgramRegistersFrustumBinding)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;  // defaults: opaque, unquantized, no features
    flags.positionType = PositionType::Unquantized;

    ShaderProgram prog;
    compiler.buildProgram(prog, flags);

    // addBindings fired during buildProgram -> u_frustum ProgramUniform exists.
    EXPECT_TRUE(prog.hasProgramUniform("u_frustum"));
    // GLSL source was set (declares u_frustum).
    EXPECT_NE(prog.getVertSource().find("u_frustum"), std::string::npos);
}

// The u_frustum binding registered through the live buildProgram path still
// null-guards when no target is set (legacy/test path) -- no crash.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, SurfaceBuildProgramFrustumBindingNullTargetIsNoOp)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;

    ShaderProgram prog;
    compiler.buildProgram(prog, flags);

    ShaderProgramParams params;  // target == nullptr
    UniformHandle h;
    ASSERT_TRUE(prog.invokeProgramUniformForTest("u_frustum", h, params));
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[2], 0.0f);
}

// ==========================================================================
// S4 ProgramUniform wiring tests (GL-free, verify addBindings registration)
// ==========================================================================

// addProjectionMatrix registers u_proj ProgramUniform via wireProjectionMatrix.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, ProjectionMatrixBindingRegistered)
{
    ProgramBuilder builder;
    addProjectionMatrix(builder.getVertexBuilder());
    ShaderProgram prog;
    builder.getVertexBuilder().addBindings(prog);
    EXPECT_TRUE(prog.hasProgramUniform("u_proj"));
}

// addLighting registers u_sunDir + u_lightSettings[0] + u_upVector.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, LightingBindingsRegistered)
{
    ProgramBuilder builder;
    addLighting(builder);
    ShaderProgram prog;
    builder.getFragmentBuilder().addBindings(prog);
    EXPECT_TRUE(prog.hasProgramUniform("u_sunDir"));
    EXPECT_TRUE(prog.hasProgramUniform("u_lightSettings[0]"));
    EXPECT_TRUE(prog.hasProgramUniform("u_upVector"));
}

// addViewport registers u_viewport.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, ViewportBindingRegistered)
{
    ProgramBuilder builder;
    addViewport(builder.getFragmentBuilder());
    ShaderProgram prog;
    builder.getFragmentBuilder().addBindings(prog);
    EXPECT_TRUE(prog.hasProgramUniform("u_viewport"));
}

// addViewportTransformation registers u_viewportTransformation.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, ViewportTransformationBindingRegistered)
{
    ProgramBuilder builder;
    addViewportTransformation(builder.getFragmentBuilder());
    ShaderProgram prog;
    builder.getFragmentBuilder().addBindings(prog);
    EXPECT_TRUE(prog.hasProgramUniform("u_viewportTransformation"));
}

// addSurfaceMonochrome registers u_mixMonoColor.
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, MonochromeMixBindingRegistered)
{
    ProgramBuilder builder;
    addSurfaceMonochrome(builder);
    ShaderProgram prog;
    builder.getFragmentBuilder().addBindings(prog);
    EXPECT_TRUE(prog.hasProgramUniform("u_mixMonoColor"));
}

// All wired ProgramUniforms null-guard when no target is set (no crash).
// Authored: itwinjs-core has no GL-free binding-registration test path (it
TEST(UniformBindingTest, WiredBindingsNullTargetIsNoOp)
{
    ProgramBuilder builder;
    addProjectionMatrix(builder.getVertexBuilder());
    addLighting(builder);
    addViewport(builder.getFragmentBuilder());
    addViewportTransformation(builder.getFragmentBuilder());

    ShaderProgram prog;
    builder.getVertexBuilder().addBindings(prog);
    builder.getFragmentBuilder().addBindings(prog);

    ShaderProgramParams params;  // target == nullptr
    UniformHandle h;
    // Each should return true (registered) and not crash with null target.
    EXPECT_TRUE(prog.invokeProgramUniformForTest("u_proj", h, params));
    EXPECT_TRUE(prog.invokeProgramUniformForTest("u_sunDir", h, params));
    EXPECT_TRUE(prog.invokeProgramUniformForTest("u_lightSettings[0]", h, params));
    EXPECT_TRUE(prog.invokeProgramUniformForTest("u_upVector", h, params));
    EXPECT_TRUE(prog.invokeProgramUniformForTest("u_viewport", h, params));
    EXPECT_TRUE(prog.invokeProgramUniformForTest("u_viewportTransformation", h, params));
}

// GraphicUniform registrations (u_mv, u_materialColor) are verified via the
// live draw path (DrawParams + shader->draw() in SceneCompositorImpl).
// GL-free tests deferred: hasProgramUniform checks ProgramUniforms only;
// addMaterial test blocked by SurfaceMaterialShaders/CommonShaders ODR conflict.
