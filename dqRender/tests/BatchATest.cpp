// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for batch rendering (Phase A)
// DanQing dqRender — Batch A tests (P0 core rendering)
//
// Tests for DrawParams, MeshData, MeshGraphic, TargetGraphics,

#include "render/DrawParams.h"
#include "render/MeshData.h"
#include "render/MeshGraphic.h"
#include "render/TargetGraphics.h"

#include "NullDriver.h"

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;

// ============================================================================
// DrawParams tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(DrawParamsTest, DefaultState)
{
    DrawParams params;
    EXPECT_EQ(params.getProgramParams(), nullptr);
    EXPECT_EQ(params.getGeometry(), nullptr);
    EXPECT_EQ(params.getTarget(), nullptr);
    EXPECT_EQ(params.getRenderPass(), Pass::None);
    EXPECT_EQ(params.getTechniqueId(), TechniqueId::Surface);
    EXPECT_FALSE(params.isOverlayPass());
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(DrawParamsTest, InitReset)
{
    rhi::NullDriver driver;
    DrawParams params;
    SurfaceGeometry geo(driver, rhi::IndexBufferHandle{}, 100, SurfaceType::Opaque, false, false);

    params.init(nullptr, &geo);
    EXPECT_EQ(params.getGeometry(), &geo);
    EXPECT_EQ(params.getRenderPass(), Pass::Opaque);
    EXPECT_EQ(params.getTechniqueId(), TechniqueId::Surface);

    params.reset();
    EXPECT_EQ(params.getGeometry(), nullptr);
}

// ============================================================================
// MeshData tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(MeshDataTest, DefaultState)
{
    MeshData data;
    EXPECT_EQ(data.getVertexCount(), 0u);
    EXPECT_EQ(data.getIndexCount(), 0u);
    EXPECT_FALSE(data.hasNormals());
    EXPECT_FALSE(data.hasColors());
    EXPECT_FALSE(data.hasFeatureIndices());
    EXPECT_FALSE(data.hasParams());
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(MeshDataTest, setData)
{
    MeshData data;
    data.setPositions({0, 0, 0, 1, 0, 0, 0, 1, 0});  // 3 vertices
    data.setNormals({0, 0, 0, 0, 0, 0, 0, 0, 0});
    data.setIndices({0, 1, 2});

    EXPECT_EQ(data.getVertexCount(), 3u);
    EXPECT_EQ(data.getIndexCount(), 3u);
    EXPECT_TRUE(data.hasNormals());
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(MeshDataTest, Clear)
{
    MeshData data;
    data.setPositions({0, 0, 0});
    data.setIndices({0, 1, 2});
    data.clear();

    EXPECT_EQ(data.getVertexCount(), 0u);
    EXPECT_EQ(data.getIndexCount(), 0u);
}

// ============================================================================
// MeshGraphic tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(MeshGraphicTest, Empty)
{
    rhi::NullDriver driver;
    MeshGraphic mesh(driver);
    EXPECT_TRUE(mesh.isEmpty());
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(MeshGraphicTest, AddGeometries)
{
    rhi::NullDriver driver;
    MeshGraphic mesh(driver);
    mesh.addSurface(std::make_unique<SurfaceGeometry>(driver, rhi::IndexBufferHandle{}, 100, SurfaceType::Opaque, false, false));
    mesh.addEdge(std::make_unique<EdgeGeometry>(50));
    mesh.addPolyline(std::make_unique<PolylineGeometry>(driver, VertexLutTexture{},
        rhi::RenderPrimitiveHandle{}, rhi::VertexBufferHandle{}, rhi::VertexBufferInfoHandle{},
        rhi::BufferObjectHandle{}, rhi::BufferObjectHandle{}, rhi::BufferObjectHandle{},
        30, 1.0f, dqCommon::ColorDef::white));

    EXPECT_FALSE(mesh.isEmpty());
    EXPECT_EQ(mesh.getSurfaces().size(), 1u);
    EXPECT_EQ(mesh.getEdges().size(), 1u);
    EXPECT_EQ(mesh.getPolylines().size(), 1u);
}

// ============================================================================
// TargetGraphics tests
// ============================================================================

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(TargetGraphicsTest, DefaultState)
{
    TargetGraphics target;
    EXPECT_TRUE(target.isEmpty());
    EXPECT_EQ(target.getScene(), nullptr);
    EXPECT_TRUE(target.getGraphics().empty());
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(TargetGraphicsTest, SetScene)
{
    Scene scene;
    TargetGraphics target;
    target.setScene(&scene);
    EXPECT_EQ(target.getScene(), &scene);
    EXPECT_FALSE(target.isEmpty());
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(TargetGraphicsTest, AddRemove)
{
    TargetGraphics target;
    // Use nullptr as placeholder since RenderGraphic is abstract
    RenderGraphic* dummy = nullptr;
    target.addGraphic(dummy);
    // Adding nullptr should not change count (filtered by addGraphic)
    EXPECT_EQ(target.getGraphics().size(), 0u);
}

// Authored: no reference test exists in itwinjs-core for batch rendering
TEST(TargetGraphicsTest, Clear)
{
    Scene scene;
    TargetGraphics target;
    target.setScene(&scene);
    target.clear();

    EXPECT_TRUE(target.isEmpty());
    EXPECT_EQ(target.getScene(), nullptr);
}
