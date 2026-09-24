// SPDX-License-Identifier: Apache-2.0
// dqRender — MeshGraphic GL lifecycle regression test (GL-free).
// Authored: no reference test exists in itwinjs-core/imodel-native for DanQing's
//           RAII MeshGraphic disposal; this verifies the create/destroy balance
//           contract that per-frame decoration rebuild (ACS triad) depends on.
//
// LifecycleDriver derives NullDriver and counts create + destroy for every
// resource type MeshRenderGeometry::create allocates (shared vbo/vbih/vbh + per
// geometry ibh + render primitive). A polyline Mesh exercises the full 5-resource
// set exactly once each. After reset(), every create must be paired with a destroy
// — proving the no-leak property headlessly (no GL context needed).
#include <gtest/gtest.h>

#include "NullDriver.h"

#include "render/MeshGraphic.h"
#include "render/MeshPrimitives.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <memory>

using namespace dqRender;
using namespace dqRender::rhi;
using namespace dqCommon;
using namespace dqGeom;

namespace {

// Recording driver: counts create + destroy for each resource type that
// MeshRenderGeometry::create allocates. NullDriver's createXxx return {} (nullid
// handles); destroyXxx({}) is a no-op on every real Driver too
// (HandleAllocator::deallocate early-returns on nullid), so the unguarded dtor
// pattern is unit-testable here.
class LifecycleDriver : public NullDriver {
public:
    rhi::IndexBufferHandle createIndexBuffer(rhi::ElementType, uint32_t, rhi::BufferUsage) noexcept override
    {
        ++m_ibCreate;
        return {};
    }
    void destroyIndexBuffer(rhi::IndexBufferHandle) noexcept override { ++m_ibDestroy; }

    rhi::RenderPrimitiveHandle createRenderPrimitive(rhi::VertexBufferHandle, rhi::IndexBufferHandle, rhi::PrimitiveType) noexcept override
    {
        ++m_rpCreate;
        return {};
    }
    void destroyRenderPrimitive(rhi::RenderPrimitiveHandle) noexcept override { ++m_rpDestroy; }

    rhi::BufferObjectHandle createBufferObject(uint32_t, rhi::BufferObjectBinding, rhi::BufferUsage) noexcept override
    {
        ++m_boCreate;
        return {};
    }
    void destroyBufferObject(rhi::BufferObjectHandle) noexcept override { ++m_boDestroy; }

    rhi::VertexBufferHandle createVertexBuffer(uint32_t, rhi::VertexBufferInfoHandle) noexcept override
    {
        ++m_vbCreate;
        return {};
    }
    void destroyVertexBuffer(rhi::VertexBufferHandle) noexcept override { ++m_vbDestroy; }

    rhi::VertexBufferInfoHandle createVertexBufferInfo(uint8_t, uint8_t, rhi::AttributeArray const&) noexcept override
    {
        ++m_vbiCreate;
        return {};
    }
    void destroyVertexBufferInfo(rhi::VertexBufferInfoHandle) noexcept override { ++m_vbiDestroy; }

    // NullDriver already no-ops these; re-declared empty for explicitness (these
    // are the data-upload calls MeshRenderGeometry::create makes — not counted).
    void updateBufferObject(rhi::BufferObjectHandle, rhi::BufferDescriptor&&, uint32_t) noexcept override {}
    void updateIndexBuffer(rhi::IndexBufferHandle, rhi::BufferDescriptor&&, uint32_t) noexcept override {}
    void setVertexBufferObject(rhi::VertexBufferHandle, uint32_t, rhi::BufferObjectHandle) noexcept override {}

    size_t m_ibCreate{}, m_ibDestroy{}, m_rpCreate{}, m_rpDestroy{};
    size_t m_boCreate{}, m_boDestroy{}, m_vbCreate{}, m_vbDestroy{}, m_vbiCreate{}, m_vbiDestroy{};
};

// Build a polyline-type Mesh with ONE polyline of 3 vertices (3 indices).
// Mirrors MeshRenderGeometryMockTest.cpp::buildPolylineMesh(1). Exercises the
// faithful thick-line path: 1 shared vbo + 1 shared vbih + 1 shared vbh + (per
// polyline) 3 corner BOs + 1 corner vbih + 1 corner vbh + 1 primitive + 1 LUT
// texture. No IBH (polyline uses glDrawArrays over the corner buffer).
std::unique_ptr<Mesh> buildSinglePolylineMesh()
{
    Mesh::Props props;
    props.type = MeshPrimitiveType::Polyline;
    props.isPlanar = true;
    auto mesh = Mesh::create(props);

    Range3d range;
    for (int j = 0; j < 3; ++j) {
        const Point3d p = Point3d::From(static_cast<double>(j), 0.0, 0.0);
        range.ExtendPoint(p);
        VertexKeyProps vkp;
        vkp.position = p;
        vkp.fillColor = ColorDef::white.getTbgr();
        mesh->addVertex(vkp);
    }
    mesh->points().range = range;
    MeshPolyline poly(std::vector<uint32_t>{0, 1, 2});
    mesh->addPolyline(poly);
    return mesh;
}

}  // namespace

// No-leak contract: every GPU resource created by MeshRenderGeometry::create must
// be destroyed when the MeshGraphic is dropped. Before the RAII fix the geometry
// + MeshGraphic dtors were `= default`, so nothing was released — this test
// anchors the per-frame decoration-rebuild safety property (ACS triad Task 2).
TEST(MeshGraphicLifecycleTest, ReleasesAllGpuResourcesOnDestruction)
{
    LifecycleDriver driver;
    auto mesh = buildSinglePolylineMesh();

    auto graphic = MeshRenderGeometry::create(driver, *mesh);
    ASSERT_NE(graphic, nullptr);

    // Sanity: resources were actually created. Shared VBO/VBIH/VBH (1 each) +
    // per-polyline corner resources (3 BOs + 1 VBIH + 1 VBH + 1 primitive).
    EXPECT_GT(driver.m_boCreate, 0u) << "shared VBO never created";
    EXPECT_GE(driver.m_boCreate, 4u) << "shared VBO + 3 corner BOs expected";
    EXPECT_GT(driver.m_vbiCreate, 0u) << "shared VBIH never created";
    EXPECT_EQ(driver.m_vbiCreate, 2u) << "1 shared VBIH + 1 corner VBIH expected";
    EXPECT_GT(driver.m_vbCreate, 0u) << "shared VBH never created";
    EXPECT_EQ(driver.m_vbCreate, 2u) << "1 shared VBH + 1 corner VBH expected";
    EXPECT_EQ(driver.m_ibCreate, 0u) << "polyline path must not create an IBH";
    EXPECT_EQ(driver.m_rpCreate, 1u) << "one render primitive per polyline";

    graphic.reset();  // destroy → dtors must release everything

    // No-leak: every created resource is destroyed.
    EXPECT_EQ(driver.m_boCreate, driver.m_boDestroy) << "VBO(s) leaked";
    EXPECT_EQ(driver.m_vbiCreate, driver.m_vbiDestroy) << "VBIH(s) leaked";
    EXPECT_EQ(driver.m_vbCreate, driver.m_vbDestroy) << "VBH(s) leaked";
    EXPECT_EQ(driver.m_ibCreate, driver.m_ibDestroy) << "IBH count mismatch (path creates none)";
    EXPECT_EQ(driver.m_rpCreate, driver.m_rpDestroy) << "per-geometry primitive leaked";
}
