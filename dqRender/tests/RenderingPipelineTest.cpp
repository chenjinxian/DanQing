// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
// DanQing dqRender — Rendering pipeline tests (P0/P1)
//
// Tests for DrawParams, Uniforms, Primitive, geometry types,
// FeatureSymbology shaders, Pixel API, EdgeSettings, ColorInfo, FloatRGBA.

#include "render/Uniforms.h"
#include "render/Graphic.h"
#include "render/SurfaceGeometry.h"
#include "render/ViewportQuadGeometry.h"
#include "render/EdgeSettings.h"
#include "render/ColorInfo.h"
#include "render/FloatRGBA.h"
#include "render/VertexShaderModules.h"
#include "render/FeatureSymbologyShaders.h"
#include "dqRender/Pixel.h"

#include "NullDriver.h"

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

using namespace dqRender;

// ============================================================================
// Uniform tests
// ----------------------------------------------------------------------------
// FrustumUniforms / ViewRectUniforms / LightingUniforms / HiliteUniforms /
// StyleUniforms have been extracted to dedicated 1:1 files (see *UniformsTest.cpp).
// The remaining BatchUniforms / BranchUniforms are still stubs; their tests
// live here until those modules are extracted too.
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(BatchUniformsTest, BatchId)
{
    BatchUniforms batch;
    batch.setBatchId(42);
    EXPECT_EQ(batch.getBatchId(), 42u);
    batch.setFeatureMode(2);
    EXPECT_EQ(batch.getFeatureMode(), 2);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(BranchUniformsTest, Matrices)
{
    BranchUniforms branch;

    float mv[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};
    branch.setModelViewMatrix(mv);

    float const* result = branch.getModelViewMatrix();
    EXPECT_FLOAT_EQ(result[12], 10.0f);
    EXPECT_FLOAT_EQ(result[13], 20.0f);
    EXPECT_FLOAT_EQ(result[14], 30.0f);
}

// ============================================================================
// Geometry type tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(SurfaceGeometryTest, Properties)
{
    rhi::NullDriver driver;
    SurfaceGeometry geo(driver, rhi::IndexBufferHandle{}, 100, SurfaceType::Opaque, true, true);
    EXPECT_EQ(geo.getTechniqueId(), TechniqueId::Surface);
    EXPECT_EQ(geo.getPass(), Pass::OpaquePlanar);  // isPlanar=true
    EXPECT_EQ(geo.getRenderOrder(), RenderOrder::LitSurface);
    EXPECT_TRUE(geo.isLit());
    EXPECT_TRUE(geo.isPlanar());
    EXPECT_TRUE(geo.hasTextures());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(EdgeGeometryTest, Properties)
{
    EdgeGeometry geo(50);
    EXPECT_EQ(geo.getTechniqueId(), TechniqueId::Edge);
    EXPECT_EQ(geo.getPass(), Pass::OpaqueLinear);
    EXPECT_EQ(geo.getRenderOrder(), RenderOrder::Edge);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(PolylineGeometryTest, Properties)
{
    rhi::NullDriver driver;
    // Faithful thick-line PolylineGeometry ctor (ported from itwinjs-core Polyline.ts).
    // NullDriver's createXxx return {} so all GL handles are nullid — safe for
    // property checks (destroy({}) no-ops in the dtor).
    VertexLutTexture lut;
    PolylineGeometry geo(driver, std::move(lut),
                         rhi::RenderPrimitiveHandle{}, rhi::VertexBufferHandle{},
                         rhi::VertexBufferInfoHandle{}, rhi::BufferObjectHandle{},
                         rhi::BufferObjectHandle{}, rhi::BufferObjectHandle{},
                         30, 2.0f, dqCommon::ColorDef::white);
    // TechniqueId flipped from Surface (PRAGMATIC) to Polyline (faithful) in the
    // SAME commit that swaps the VAO to the corner buffer.
    EXPECT_EQ(geo.getTechniqueId(), TechniqueId::Polyline);
    EXPECT_EQ(geo.getPass(), Pass::OpaqueLinear);
    EXPECT_EQ(geo.getRenderOrder(), RenderOrder::Linear);
    EXPECT_FALSE(geo.usesQuantizedPositions());  // Unquantized LUT path
    // getLineWeight clamps to [1.0, 31.0] (CachedGeometry.ts:140-150).
    EXPECT_FLOAT_EQ(geo.getLineWeight(), 2.0f);
}

// Authored: no reference test exists in itwinjs-core or filament for the clamp boundary;
// the clamp range itself is ported from itwinjs-core CachedGeometry.ts:140-150.
TEST(PolylineGeometryTest, LineWeightClamp)
{
    rhi::NullDriver driver;
    // Below floor: 0 -> 1.0.
    {
        VertexLutTexture lut;
        PolylineGeometry geo(driver, std::move(lut),
                             rhi::RenderPrimitiveHandle{}, rhi::VertexBufferHandle{},
                             rhi::VertexBufferInfoHandle{}, rhi::BufferObjectHandle{},
                             rhi::BufferObjectHandle{}, rhi::BufferObjectHandle{},
                             30, 0.0f, dqCommon::ColorDef::white);
        EXPECT_FLOAT_EQ(geo.getLineWeight(), 1.0f);
    }
    // In range: 15 -> 15.0.
    {
        VertexLutTexture lut;
        PolylineGeometry geo(driver, std::move(lut),
                             rhi::RenderPrimitiveHandle{}, rhi::VertexBufferHandle{},
                             rhi::VertexBufferInfoHandle{}, rhi::BufferObjectHandle{},
                             rhi::BufferObjectHandle{}, rhi::BufferObjectHandle{},
                             30, 15.0f, dqCommon::ColorDef::white);
        EXPECT_FLOAT_EQ(geo.getLineWeight(), 15.0f);
    }
    // Above ceiling: 99 -> 31.0.
    {
        VertexLutTexture lut;
        PolylineGeometry geo(driver, std::move(lut),
                             rhi::RenderPrimitiveHandle{}, rhi::VertexBufferHandle{},
                             rhi::VertexBufferInfoHandle{}, rhi::BufferObjectHandle{},
                             rhi::BufferObjectHandle{}, rhi::BufferObjectHandle{},
                             30, 99.0f, dqCommon::ColorDef::white);
        EXPECT_FLOAT_EQ(geo.getLineWeight(), 31.0f);
    }
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(PointCloudGeometryTest, Properties)
{
    PointCloudGeometry geo(1000, 0.5f);
    EXPECT_EQ(geo.getTechniqueId(), TechniqueId::PointCloud);
    EXPECT_EQ(geo.getPass(), Pass::PointClouds);
    EXPECT_EQ(geo.getVertexCount(), 1000u);
    EXPECT_FLOAT_EQ(geo.getVoxelSize(), 0.5f);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(PointStringGeometryTest, Properties)
{
    rhi::NullDriver driver;
    PointStringGeometry geo(driver, rhi::IndexBufferHandle{}, 50, 3.0f);
    EXPECT_EQ(geo.getTechniqueId(), TechniqueId::PointString);
    EXPECT_EQ(geo.getPass(), Pass::OpaqueLinear);
    EXPECT_FLOAT_EQ(geo.getWeight(), 3.0f);
}

// ============================================================================
// EdgeSettings tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(EdgeSettingsTest, DefaultState)
{
    EdgeSettings settings;
    EXPECT_TRUE(settings.getVisibleEdges());
    EXPECT_FALSE(settings.getHiddenEdges());
    EXPECT_FALSE(settings.getSilhouetteEdges());
    EXPECT_TRUE(settings.hasAnyEdges());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(EdgeSettingsTest, Configure)
{
    EdgeSettings settings;
    settings.setVisibleEdges(false);
    settings.setSilhouetteEdges(true);
    settings.setEdgeWeight(2.0f);

    EXPECT_FALSE(settings.getVisibleEdges());
    EXPECT_TRUE(settings.getSilhouetteEdges());
    EXPECT_FLOAT_EQ(settings.getEdgeWeight(), 2.0f);
}

// ============================================================================
// ColorInfo tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(ColorInfoTest, UniformColor)
{
    auto info = ColorInfo::fromUniform(0xFF0000, 128);
    EXPECT_TRUE(info.isUniform());
    EXPECT_EQ(info.getRgb(), 0xFF0000u);
    EXPECT_EQ(info.getAlpha(), 128);

    auto rgba = info.getFloatRgba();
    EXPECT_FLOAT_EQ(rgba[0], 1.0f);
    EXPECT_FLOAT_EQ(rgba[1], 0.0f);
    EXPECT_FLOAT_EQ(rgba[2], 0.0f);
    EXPECT_FLOAT_EQ(rgba[3], 128.0f / 255.0f);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(ColorInfoTest, PerVertexColor)
{
    auto info = ColorInfo::fromPerVertex();
    EXPECT_TRUE(info.isPerVertex());
    EXPECT_FALSE(info.isUniform());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(ColorInfoTest, PerFeatureColor)
{
    auto info = ColorInfo::fromPerFeature();
    EXPECT_TRUE(info.isPerFeature());
}

// ============================================================================
// FloatRGBA tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FloatRgbTest, FromHex)
{
    auto color = FloatRgb::fromHex(0xFF8040);
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(color.b, 64.0f / 255.0f);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FloatRgbaTest, FromHex)
{
    auto color = FloatRgba::fromHex(0x00FF00, 128);
    EXPECT_FLOAT_EQ(color.r, 0.0f);
    EXPECT_FLOAT_EQ(color.g, 1.0f);
    EXPECT_FLOAT_EQ(color.b, 0.0f);
    EXPECT_FLOAT_EQ(color.a, 128.0f / 255.0f);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FloatRgbaTest, ToHex)
{
    FloatRgba color(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_EQ(color.toHex(), 0xFF0000u);
}

// ============================================================================
// Pixel API tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(PixelDataTest, DefaultState)
{
    Pixel::Data data;
    EXPECT_FALSE(data.feature.has_value());
    EXPECT_FLOAT_EQ(data.distanceFraction, -1.0f);
    EXPECT_EQ(data.type, Pixel::GeometryType::Unknown);
    EXPECT_EQ(data.planarity, Pixel::Planarity::Unknown);
    EXPECT_FALSE(data.hasGeometry());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(PixelDataTest, HitPriority)
{
    Pixel::Data surface;
    surface.type = Pixel::GeometryType::Surface;
    surface.planarity = Pixel::Planarity::Planar;

    Pixel::Data edge;
    edge.type = Pixel::GeometryType::Edge;

    Pixel::Data linear;
    linear.type = Pixel::GeometryType::Linear;

    EXPECT_GT(surface.computeHitPriority(), edge.computeHitPriority());
    EXPECT_GT(edge.computeHitPriority(), linear.computeHitPriority());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(PixelGeometryTypeTest, Values)
{
    EXPECT_EQ(static_cast<uint8_t>(Pixel::GeometryType::Unknown), 0);
    EXPECT_EQ(static_cast<uint8_t>(Pixel::GeometryType::None), 1);
    EXPECT_EQ(static_cast<uint8_t>(Pixel::GeometryType::Surface), 2);
    EXPECT_EQ(static_cast<uint8_t>(Pixel::GeometryType::Linear), 3);
    EXPECT_EQ(static_cast<uint8_t>(Pixel::GeometryType::Edge), 4);
    EXPECT_EQ(static_cast<uint8_t>(Pixel::GeometryType::Silhouette), 5);
}

// ============================================================================
// Vertex shader module tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(VertexShaderModulesTest, UnquantizePosition)
{
    char const* code = getUnquantizePosition();
    EXPECT_NE(std::string(code).find("unquantizePosition"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(VertexShaderModulesTest, OctDecodeNormal)
{
    char const* code = getOctDecodeNormal();
    EXPECT_NE(std::string(code).find("octDecodeNormal"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(VertexShaderModulesTest, LambertLighting)
{
    char const* code = getLambertLighting();
    EXPECT_NE(std::string(code).find("computeLambertLighting"), std::string::npos);
}

// ============================================================================
// FeatureSymbology shader module tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FeatureSymbologyShadersTest, OvrFlagConstants)
{
    char const* code = getOvrFlagConstants();
    std::string src(code);
    EXPECT_NE(src.find("kOvrBit_Rgb"), std::string::npos);
    EXPECT_NE(src.find("kOvrBit_Hilited"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FeatureSymbologyShadersTest, DecodeUint24)
{
    char const* code = getDecodeUint24();
    std::string src(code);
    EXPECT_NE(src.find("decodeUint24"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FeatureSymbologyShadersTest, ExtractNthBit)
{
    char const* code = getExtractNthBit();
    std::string src(code);
    EXPECT_NE(src.find("nthBitSet"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FeatureSymbologyShadersTest, ComputeFeatureOverrides)
{
    char const* code = getComputeFeatureOverrides();
    std::string src(code);
    EXPECT_NE(src.find("computeFeatureOverrides"), std::string::npos);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(FeatureSymbologyShadersTest, RenderOrderConstants)
{
    char const* code = getRenderOrderConstants();
    std::string src(code);
    EXPECT_NE(src.find("kRenderOrder_LitSurface"), std::string::npos);
    EXPECT_NE(src.find("kRenderOrder_Edge"), std::string::npos);
}

// ============================================================================
// RenderCommands tests
// ============================================================================

#include "render/RenderCommands.h"
#include "render/DrawCommand.h"
#include "render/BranchStack.h"
#include "render/Batch.h"
#include "render/TargetImpl.h"

// Helper: construct RenderCommands for testing.
// Allocates raw memory for a TargetImpl without calling constructor —
// safe because tests only use command buffer methods that don't dereference it.
static RenderCommands makeTestRenderCommands(BranchStack& stack, BatchState& batchState)
{
    static char buf[sizeof(TargetImpl)];  // NOLINT — uninitialized is intentional
    auto* fakeTarget = reinterpret_cast<TargetImpl*>(buf);
    return RenderCommands(*fakeTarget, stack, batchState);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(RenderCommandsTest, EmptyCommands)
{
    BranchStack stack;
    BatchState batchState;
    auto cmds = makeTestRenderCommands(stack, batchState);
    auto const& opaqueCmds = cmds.getCommands(RenderPass::OpaqueGeneral);
    EXPECT_TRUE(opaqueCmds.empty());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(RenderCommandsTest, AddPrimitiveNull)
{
    BranchStack stack;
    BatchState batchState;
    auto cmds = makeTestRenderCommands(stack, batchState);
    // Test that null geometry is handled gracefully.
    cmds.addPrimitive(static_cast<CachedGeometry*>(nullptr));
    EXPECT_TRUE(cmds.isEmpty());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(RenderCommandsTest, Clear)
{
    BranchStack stack;
    BatchState batchState;
    auto cmds = makeTestRenderCommands(stack, batchState);
    cmds.clear();

    for (size_t i = 0; i < RenderCommands::getPassCount(); ++i) {
        auto pass = static_cast<RenderPass>(i);
        auto const& passCmds = cmds.getCommands(pass);
        EXPECT_TRUE(passCmds.empty());
    }
}

// ============================================================================
// DrawCommand tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(DrawCommandTest, PrimitiveCommandType)
{
    PrimitiveCommand cmd(nullptr);
    EXPECT_EQ(cmd.getType(), DrawCommandType::Primitive);
    EXPECT_EQ(cmd.getGeometry(), nullptr);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(DrawCommandTest, PushBranchCommandType)
{
    PushBranchCommand cmd;
    EXPECT_EQ(cmd.getType(), DrawCommandType::PushBranch);
    EXPECT_FALSE(cmd.hasViewFlags());
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(DrawCommandTest, PushBranchTransform)
{
    PushBranchCommand cmd;
    float mv[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};
    float mvp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,30,1};
    cmd.setTransform(mv, mvp);

    EXPECT_FLOAT_EQ(cmd.getMv()[12], 10.0f);
    EXPECT_FLOAT_EQ(cmd.getMv()[13], 20.0f);
    EXPECT_FLOAT_EQ(cmd.getMv()[14], 30.0f);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(DrawCommandTest, PushBranchViewFlags)
{
    PushBranchCommand cmd;
    BranchViewFlags flags;
    flags.lighting = false;
    flags.textures = false;
    cmd.setViewFlags(flags);

    EXPECT_TRUE(cmd.hasViewFlags());
    EXPECT_FALSE(cmd.getViewFlags().lighting);
    EXPECT_FALSE(cmd.getViewFlags().textures);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(DrawCommandTest, PushBatchCommandType)
{
    PushBatchCommand cmd(42);
    EXPECT_EQ(cmd.getType(), DrawCommandType::PushBatch);
    EXPECT_EQ(cmd.getBatchId(), 42u);
}

// Authored: no reference test exists in itwinjs-core or filament for render pipeline integration
TEST(DrawCommandTest, PopCommandTypes)
{
    PopBranchCommand popBranch;
    EXPECT_EQ(popBranch.getType(), DrawCommandType::PopBranch);

    PopBatchCommand popBatch;
    EXPECT_EQ(popBatch.getType(), DrawCommandType::PopBatch);

    PushClipCommand pushClip;
    EXPECT_EQ(pushClip.getType(), DrawCommandType::PushClip);

    PopClipCommand popClip;
    EXPECT_EQ(popClip.getType(), DrawCommandType::PopClip);
}
