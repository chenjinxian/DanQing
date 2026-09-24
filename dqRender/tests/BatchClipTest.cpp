// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Batch, ClipVolume, ClipStack, BranchStack tests
// Authored: no reference test exists in itwinjs-core for batch clip rendering

#include "render/Batch.h"
#include "render/BranchStack.h"
#include "render/ClipStack.h"
#include "render/ClipVolume.h"
#include "render/Graphic.h"
#include "render/ShaderBuilder.h"
#include "render/SurfaceVariantCompiler.h"

#include <dqGeom/Transform.h>

#include <gtest/gtest.h>
#include <cmath>

using namespace dqRender;

// ============================================================================
// Batch tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BatchTest, createBatch)
{
    Batch batch(100);
    EXPECT_EQ(batch.getFeatureCount(), 100u);
    EXPECT_EQ(batch.getBatchId(), 0u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BatchTest, SetBatchId)
{
    Batch batch(50);
    batch.setBatchId(42);
    EXPECT_EQ(batch.getBatchId(), 42u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
// Ported from: itwinjs-core BatchState.ts assignBatchId
// Each batch gets a contiguous range of IDs based on feature count.
TEST(BatchStateTest, AssignBatchIds)
{
    BatchState state;
    Batch batch1(10);
    Batch batch2(20);

    state.assignBatchId(batch1);
    state.assignBatchId(batch2);

    EXPECT_EQ(batch1.getBatchId(), 1u);
    // batch2 starts after batch1's 10 features: 1 + 10 = 11
    EXPECT_EQ(batch2.getBatchId(), 11u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BatchStateTest, PushPop)
{
    BatchState state;
    Batch batch1(10);
    Batch batch2(20);

    batch1.setBatchId(1);
    batch2.setBatchId(2);

    state.push(batch1);
    EXPECT_EQ(state.getCurrentBatchId(), 1u);

    state.push(batch2);
    EXPECT_EQ(state.getCurrentBatchId(), 2u);

    state.pop();
    EXPECT_EQ(state.getCurrentBatchId(), 1u);

    state.pop();
    EXPECT_EQ(state.getCurrentBatchId(), 0u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BatchStateTest, Reset)
{
    BatchState state;
    Batch batch(10);
    state.assignBatchId(batch);
    state.push(batch);

    state.reset();
    EXPECT_EQ(state.getCurrentBatchId(), 0u);
}

// ============================================================================
// ClipVolume tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipVolumeTest, EmptyVolume)
{
    ClipVolume vol;
    EXPECT_TRUE(vol.isEmpty());
    EXPECT_EQ(vol.getPlaneCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipVolumeTest, SetPlanes)
{
    ClipVolume vol;
    std::vector<ClipPlane> planes = {
        ClipPlane(1.0f, 0.0f, 0.0f, -5.0f),
        ClipPlane(0.0f, 1.0f, 0.0f, -3.0f),
    };
    vol.setPlanes(planes);

    EXPECT_FALSE(vol.isEmpty());
    EXPECT_EQ(vol.getPlaneCount(), 2u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipVolumeTest, buildTextureData)
{
    ClipVolume vol;
    std::vector<ClipPlane> planes = {
        ClipPlane(1.0f, 0.0f, 0.0f, -5.0f),
        ClipPlane(0.0f, 1.0f, 0.0f, -3.0f),
    };
    vol.setPlanes(planes);

    auto data = vol.buildTextureData();
    EXPECT_EQ(data.size(), 8u);  // 2 planes × 4 floats

    // First plane
    EXPECT_FLOAT_EQ(data[0], 1.0f);
    EXPECT_FLOAT_EQ(data[1], 0.0f);
    EXPECT_FLOAT_EQ(data[2], 0.0f);
    EXPECT_FLOAT_EQ(data[3], -5.0f);

    // Second plane
    EXPECT_FLOAT_EQ(data[4], 0.0f);
    EXPECT_FLOAT_EQ(data[5], 1.0f);
    EXPECT_FLOAT_EQ(data[6], 0.0f);
    EXPECT_FLOAT_EQ(data[7], -3.0f);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipVolumeTest, EmptyTextureData)
{
    ClipVolume vol;
    auto data = vol.buildTextureData();
    EXPECT_EQ(data.size(), 4u);  // padded to minimum
}

// ============================================================================
// ClipStack tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipStackTest, EmptyStack)
{
    ClipStack stack;
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.getActivePlaneCount(), 0u);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipStackTest, PushPop)
{
    ClipStack stack;
    ClipVolume vol1;
    vol1.setPlanes({ClipPlane(1, 0, 0, -5)});
    ClipVolume vol2;
    vol2.setPlanes({ClipPlane(0, 1, 0, -3), ClipPlane(0, 0, 1, -1)});

    stack.push(vol1);
    EXPECT_FALSE(stack.isEmpty());
    EXPECT_EQ(stack.getActivePlaneCount(), 1u);

    stack.push(vol2);
    EXPECT_EQ(stack.getActivePlaneCount(), 3u);

    stack.pop();
    EXPECT_EQ(stack.getActivePlaneCount(), 1u);

    stack.pop();
    EXPECT_TRUE(stack.isEmpty());
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipStackTest, CombinedPlanes)
{
    ClipStack stack;
    ClipVolume vol1;
    vol1.setPlanes({ClipPlane(1, 0, 0, -5)});
    ClipVolume vol2;
    vol2.setPlanes({ClipPlane(0, 1, 0, -3)});

    stack.push(vol1);
    stack.push(vol2);

    auto combined = stack.getCombinedPlanes();
    EXPECT_EQ(combined.size(), 2u);
    EXPECT_FLOAT_EQ(combined[0].nx, 1.0f);
    EXPECT_FLOAT_EQ(combined[1].ny, 1.0f);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ClipStackTest, CombinedTextureData)
{
    ClipStack stack;
    ClipVolume vol1;
    vol1.setPlanes({ClipPlane(1, 0, 0, -5)});
    ClipVolume vol2;
    vol2.setPlanes({ClipPlane(0, 1, 0, -3)});

    stack.push(vol1);
    stack.push(vol2);

    auto data = stack.buildCombinedTextureData();
    EXPECT_EQ(data.size(), 8u);  // 2 planes × 4 floats
}

// ============================================================================
// BranchStack tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BranchStackTest, IdentityDefault)
{
    BranchStack stack;
    auto const& mv = stack.getCurrentMv();

    // identity matrix: diagonal = 1, rest = 0
    EXPECT_FLOAT_EQ(mv[0], 1.0f);
    EXPECT_FLOAT_EQ(mv[5], 1.0f);
    EXPECT_FLOAT_EQ(mv[10], 1.0f);
    EXPECT_FLOAT_EQ(mv[15], 1.0f);
    EXPECT_FLOAT_EQ(mv[1], 0.0f);
    EXPECT_FLOAT_EQ(mv[12], 0.0f);

    // MVP should also be identity
    auto const& mvp = stack.getCurrentMvp();
    EXPECT_FLOAT_EQ(mvp[0], 1.0f);
    EXPECT_FLOAT_EQ(mvp[15], 1.0f);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BranchStackTest, PushPop)
{
    BranchStack stack;

    // Translation matrix (translate by 10, 20, 30)
    std::array<float, 16> translate = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};

    stack.pushTransform(translate, translate);

    auto const& mv = stack.getCurrentMv();
    EXPECT_FLOAT_EQ(mv[12], 10.0f);
    EXPECT_FLOAT_EQ(mv[13], 20.0f);
    EXPECT_FLOAT_EQ(mv[14], 30.0f);

    stack.pop();

    auto const& mv2 = stack.getCurrentMv();
    EXPECT_FLOAT_EQ(mv2[12], 0.0f);
    EXPECT_FLOAT_EQ(mv2[13], 0.0f);
    EXPECT_FLOAT_EQ(mv2[14], 0.0f);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BranchStackTest, TransformMultiply)
{
    BranchStack stack;

    // First translation: (10, 0, 0)
    std::array<float, 16> t1 = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,0,0,1};
    stack.pushTransform(t1, t1);

    // Second translation: (5, 0, 0)
    std::array<float, 16> t2 = {1,0,0,0, 0,1,0,0, 0,0,1,0, 5,0,0,1};
    stack.pushTransform(t2, t2);

    // Combined: (15, 0, 0)
    auto const& mv = stack.getCurrentMv();
    EXPECT_FLOAT_EQ(mv[12], 15.0f);

    stack.pop();
    auto const& mv2 = stack.getCurrentMv();
    EXPECT_FLOAT_EQ(mv2[12], 10.0f);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BranchStackTest, ViewFlags)
{
    BranchStack stack;

    ViewFlags flags;
    flags.visibleEdges = false;
    flags.textures = false;

    stack.pushViewFlags(flags);

    auto const& vf = stack.getCurrentViewFlags();
    EXPECT_FALSE(vf.visibleEdges);
    EXPECT_FALSE(vf.textures);
    EXPECT_TRUE(vf.fill);  // inherited default

    stack.pop();
    auto const& vf2 = stack.getCurrentViewFlags();
    EXPECT_FALSE(vf2.visibleEdges);  // restored to default (visibleEdges defaults to false)
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BranchStackTest, Clear)
{
    BranchStack stack;
    // Constructor initializes with a default state (depth=1, matching itwinjs-core).
    EXPECT_EQ(stack.getDepth(), 1u);

    std::array<float, 16> t = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};
    stack.pushTransform(t, t);
    EXPECT_EQ(stack.getDepth(), 2u);

    // clear() resets to a single default state.
    stack.clear();
    EXPECT_EQ(stack.getDepth(), 1u);
    EXPECT_FLOAT_EQ(stack.getCurrentMv()[12], 0.0f);
}

// fromBranch composes the branch's localToWorld onto the parent's.
// Ported from: itwinjs-core BranchState.fromBranch (BranchState.ts line 104):
//   transform: prev.transform.multiplyTransformTransform(branch.localToWorldTransform)
// Authored: no reference unit test exists in itwinjs-core for BranchState.fromBranch.
TEST(BranchStackTest, FromBranchComposesLocalToWorld)
{
    BranchState prev;
    prev.setLocalToWorld(dqGeom::Transform::CreateTranslation(10.0, 0.0, 0.0));

    Branch branch;
    branch.setLocalToWorld(dqGeom::Transform::CreateTranslation(5.0, 0.0, 0.0));

    BranchState const state = BranchState::fromBranch(prev, branch);
    // prev * branch = translation(10,0,0) * translation(5,0,0) = translation(15,0,0).
    EXPECT_NEAR(state.getLocalToWorld().GetOrigin().x, 15.0, 1e-9);
    EXPECT_NEAR(state.getLocalToWorld().GetOrigin().y, 0.0, 1e-9);
}

// Backward compat: a Branch with default (identity) localToWorld yields the
// parent's localToWorld (prev * identity = prev) — preserves the old inherit.
// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(BranchStackTest, FromBranchIdentityLocalToWorldInheritsParent)
{
    BranchState prev;
    prev.setLocalToWorld(dqGeom::Transform::CreateTranslation(10.0, 0.0, 0.0));

    Branch branch;  // localToWorld defaults to identity

    BranchState const state = BranchState::fromBranch(prev, branch);
    EXPECT_NEAR(state.getLocalToWorld().GetOrigin().x, 10.0, 1e-9);
}

// ============================================================================
// ShaderBuilder component tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ShaderBuilderTest, VertexComponents)
{
    ShaderBuilder builder;
    builder.setVersion("410 core");

    builder.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
    builder.addVariable({"u_mvp", VariableType::Mat4, VariableScope::Uniform, 0});
    builder.addVariable({"v_color", VariableType::Vec4, VariableScope::Varying, 0});

    builder.setVertexComponent(VertexShaderComponent::ComputePosition,
        "    gl_Position = u_mvp * vec4(a_position, 1.0);\n");
    builder.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
        "    v_color = vec4(1.0, 0.0, 0.0, 1.0);\n");

    std::string source = builder.buildSourceWithComponents();

    EXPECT_NE(source.find("#version 410 core"), std::string::npos);
    EXPECT_NE(source.find("void main()"), std::string::npos);
    EXPECT_NE(source.find("gl_Position"), std::string::npos);
    EXPECT_NE(source.find("v_color"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ShaderBuilderTest, FragmentComponents)
{
    ShaderBuilder builder;
    builder.setVersion("410 core");

    builder.addVariable({"v_color", VariableType::Vec4, VariableScope::Varying, 0});
    builder.addVariable({"fragColor", VariableType::Vec4, VariableScope::Global, 0});

    builder.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor,
        "    vec4 baseColor = v_color;\n");
    builder.setFragmentComponent(FragmentShaderComponent::ApplyLighting,
        "    fragColor = baseColor;\n");

    std::string source = builder.buildSourceWithComponents();

    EXPECT_NE(source.find("void main()"), std::string::npos);
    EXPECT_NE(source.find("baseColor"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(ShaderBuilderTest, FallbackToCodeSnippets)
{
    ShaderBuilder builder;
    builder.setVersion("410 core");
    builder.addCode("void main() { gl_Position = vec4(0.0); }\n");

    // No components set — should fall back to code snippets
    std::string source = builder.buildSourceWithComponents();
    EXPECT_NE(source.find("gl_Position"), std::string::npos);
}

// ============================================================================
// SurfaceVariantCompiler tests (verify ShaderBuilder integration)
// ============================================================================

// Build Surface variant GLSL via the live buildProgram path and return the
// sources (mirrors the former GetShaderSource positional API for tests).
// Authored: no reference test exists in itwinjs-core for batch clip rendering
static void BuildSurfaceSource(SurfaceVariantCompiler& compiler,
                               bool quantized, bool translucent, FeatureMode featureMode,
                               bool instanced, bool shadowable, bool animated,
                               bool classified, bool thematic,
                               std::string& vert, std::string& frag) {
    TechniqueFlags flags;
    flags.positionType = quantized ? PositionType::Quantized : PositionType::Unquantized;
    flags.isTranslucent = translucent;
    flags.featureMode = featureMode;
    flags.isInstanced = instanced;
    flags.isShadowable = shadowable;
    flags.isAnimated = animated;
    flags.isClassified = classified;
    flags.isThematic = thematic;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    vert = prog.getVertSource();
    frag = prog.getFragSource();
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, BasicVertex)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, false, false, FeatureMode::None, false, false, false, false, false, vert, frag);

    ASSERT_FALSE(vert.empty());
    ASSERT_FALSE(frag.empty());

    EXPECT_NE(vert.find("#version 410 core"), std::string::npos);
    EXPECT_NE(vert.find("void main()"), std::string::npos);
    EXPECT_NE(vert.find("a_position"), std::string::npos);
    EXPECT_NE(vert.find("u_mvp"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, QuantizedVertex)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, true, false, FeatureMode::None, false, false, false, false, false, vert, frag);

    EXPECT_NE(vert.find("a_qPosition"), std::string::npos);
    EXPECT_NE(vert.find("u_qOrigin"), std::string::npos);
    EXPECT_NE(vert.find("u_qScale"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, InstancedVertex)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, false, false, FeatureMode::None, true, false, false, false, false, vert, frag);

    EXPECT_NE(vert.find("u_instanced_modelView"), std::string::npos);
    EXPECT_NE(vert.find("gl_InstanceID"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, FeatureModePick)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, false, false, FeatureMode::Pick, false, false, false, false, false, vert, frag);

    EXPECT_NE(vert.find("a_featureId"), std::string::npos);
    EXPECT_NE(frag.find("v_featureId"), std::string::npos);
    EXPECT_NE(frag.find("out uint fragColor"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, FeatureModeOverrides)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, false, false, FeatureMode::Overrides, false, false, false, false, false, vert, frag);

    EXPECT_NE(frag.find("u_featureOverrides"), std::string::npos);
    EXPECT_NE(frag.find("u_featureOverrideWidth"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, TranslucentFragment)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, false, true, FeatureMode::None, false, false, false, false, false, vert, frag);

    EXPECT_NE(frag.find("u_alphaCutoff"), std::string::npos);
    EXPECT_NE(frag.find("discard"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core for batch clip rendering
TEST(SurfaceVariantCompilerTest, ClipPlanes)
{
    SurfaceVariantCompiler compiler;
    std::string vert, frag;

    BuildSurfaceSource(compiler, false, false, FeatureMode::None, false, false, false, false, false, vert, frag);

    EXPECT_NE(vert.find("u_numClipPlanes"), std::string::npos);
    EXPECT_NE(vert.find("u_clipPlanes"), std::string::npos);
    EXPECT_NE(vert.find("gl_ClipDistance"), std::string::npos);
}
