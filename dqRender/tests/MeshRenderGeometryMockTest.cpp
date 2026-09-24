// SPDX-License-Identifier: Apache-2.0
// Authored: MockDriver runtime-verification of MeshRenderGeometry::create(driver, Mesh) — the GPU
// upload path added in PR-D′/F/follow-ups. No reference test (the itwinjs path is WebGL-internal);
// this asserts the rhi call sequence (vertex count, index count, PrimitiveType per mesh type) via a
// recording driver, WITHOUT a real GL context.
//
// PR-D′/F closed the "build-verified but not runtime-verified" gap on the type/dispatch contract;
// this test closes it for the actual rhi upload calls. See plan: ~/.claude/plans/moonlit-yawning-parasol.md.
#include <gtest/gtest.h>

#include "MockDriver.h"

#include "render/MeshGraphic.h"
#include "render/MeshPrimitives.h"
#include "render/Primitives.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <array>
#include <memory>

using namespace dqRender;
using namespace dqRender::rhi;
using namespace dqCommon;
using namespace dqGeom;

namespace {

// A 4-vertex unit quad Mesh (type=Mesh) with 2 triangles → 6 indices.
std::unique_ptr<Mesh> buildTriangleMesh()
{
    const std::array<Point3d, 4> pts{Point3d::From(0, 0, 0), Point3d::From(1, 0, 0),
                                     Point3d::From(1, 1, 0), Point3d::From(0, 1, 0)};
    Range3d range;
    for (auto const& p : pts) range.ExtendPoint(p);

    Mesh::Props props;
    props.type = MeshPrimitiveType::Mesh;
    props.range = range;
    props.isPlanar = true;
    auto mesh = Mesh::create(props);
    for (auto const& p : pts) {
        VertexKeyProps vkp;
        vkp.position = p;
        vkp.fillColor = ColorDef::white.getTbgr();
        mesh->addVertex(vkp);
    }
    Triangle t0, t1;
    t0.setIndices(0, 1, 2);
    t1.setIndices(0, 2, 3);
    mesh->addTriangle(t0);
    mesh->addTriangle(t1);
    return mesh;
}

// A polyline-type Mesh with `polylineCount` polylines, each of 3 vertices (3 indices).
std::unique_ptr<Mesh> buildPolylineMesh(int polylineCount)
{
    const int vertsPerPoly = 3;
    Mesh::Props props;
    props.type = MeshPrimitiveType::Polyline;
    props.isPlanar = true;
    auto mesh = Mesh::create(props);
    // Vertices: polylineCount * vertsPerPoly distinct points.
    Range3d range;
    int v = 0;
    for (int i = 0; i < polylineCount; ++i) {
        for (int j = 0; j < vertsPerPoly; ++j) {
            const Point3d p = Point3d::From(static_cast<double>(v), 0.0, 0.0);
            range.ExtendPoint(p);
            VertexKeyProps vkp;
            vkp.position = p;
            vkp.fillColor = ColorDef::white.getTbgr();
            mesh->addVertex(vkp);
            ++v;
        }
    }
    mesh->points().range = range;  // ensure range covers the points (Mesh ctor used null range)
    // Add polylines referencing their vertex runs.
    for (int i = 0; i < polylineCount; ++i) {
        const uint32_t base = static_cast<uint32_t>(i * vertsPerPoly);
        MeshPolyline poly(std::vector<uint32_t>{base, base + 1, base + 2});
        mesh->addPolyline(poly);
    }
    return mesh;
}

// A point-type Mesh with 4 vertices (no polylines; create() synthesizes 0..vc-1 indices).
std::unique_ptr<Mesh> buildPointMesh()
{
    const std::array<Point3d, 4> pts{Point3d::From(0, 0, 0), Point3d::From(1, 0, 0),
                                     Point3d::From(2, 0, 0), Point3d::From(3, 0, 0)};
    Range3d range;
    for (auto const& p : pts) range.ExtendPoint(p);

    Mesh::Props props;
    props.type = MeshPrimitiveType::Point;
    props.range = range;
    auto mesh = Mesh::create(props);
    for (auto const& p : pts) {
        VertexKeyProps vkp;
        vkp.position = p;
        vkp.fillColor = ColorDef::white.getTbgr();
        mesh->addVertex(vkp);
    }
    return mesh;
}

}  // namespace

// Triangle mesh → one TRIANGLES primitive with the full index count.
TEST(MeshRenderGeometryMockTest, TriangleMeshUploadsOneTrianglesPrimitive)
{
    MockDriver drv;
    auto mesh = buildTriangleMesh();
    auto mg = MeshRenderGeometry::create(drv, *mesh);
    ASSERT_NE(mg, nullptr);

    EXPECT_EQ(drv.vertexBufferCount(), 1u);       // one shared VBO
    EXPECT_EQ(drv.lastVertexCount(), 4u);          // 4 vertices
    EXPECT_EQ(drv.primitiveCount(), 1u);           // one render primitive
    EXPECT_EQ(drv.primitiveTypeAt(0), PrimitiveType::TRIANGLES);
    EXPECT_EQ(drv.primitiveIndexCount(0), 6u);     // 2 triangles × 3
    EXPECT_EQ(drv.vertexBufferInfoCount(), 1u);    // the shared attribute layout
    // Index data is uploaded via updateIndexBuffer (wires bytes into the
    // IndexBufferHandle the draw binds as GL_ELEMENT_ARRAY_BUFFER) — NOT via a
    // disconnected BufferObjectHandle. Before the fix the index bytes went to a
    // throwaway BufferObject (bufferObjectCount/updateCount == 2), leaving the
    // IndexBufferHandle unbacked → glDrawElements read it and faulted (SIGSEGV at
    // 0). Same bug PolyfaceGraphic already fixed (PolyfaceGraphicTest).
    EXPECT_EQ(drv.bufferObjectCount(), 1u);        // vertex VBO only
    EXPECT_EQ(drv.bufferObjectUpdateCount(), 1u);  // vertex VBO data only
    EXPECT_EQ(drv.indexBufferUpdateCount(), 1u);   // index bytes wired into the ibh
}

// Regression: the index buffer MUST be uploaded via Driver::updateIndexBuffer (the
// only API that glBufferData's the IndexBufferHandle the draw binds as
// GL_ELEMENT_ARRAY_BUFFER). MeshGraphic previously uploaded to a disconnected
// BufferObjectHandle (createBufferObject + updateBufferObject, target=VERTEX) and
// discarded it — the IndexBufferHandle stayed unbacked, so glDrawElements read an
// empty element buffer and SIGSEGV'd (KERN_INVALID_ADDRESS at 0x0) on the 2nd+
// draw. This is the GraphicBuilder→PrimitiveBuilder path the ACS triad exercises.
// Mirrors PolyfaceGraphicTest.UploadWiresIndexBufferViaUpdateIndexBuffer.
TEST(MeshRenderGeometryMockTest, IndexBufferWiredViaUpdateIndexBuffer)
{
    MockDriver drv;
    auto mesh = buildTriangleMesh();
    auto mg = MeshRenderGeometry::create(drv, *mesh);
    ASSERT_NE(mg, nullptr);

    EXPECT_EQ(drv.indexBufferUpdateCount(), 1u)
        << "index data must be uploaded via updateIndexBuffer (wires it into the "
           "IndexBufferHandle the draw binds); before the fix this was 0";
    EXPECT_EQ(drv.bufferObjectUpdateCount(), 1u)
        << "updateBufferObject is for the vertex VBO only; before the fix it was 2 "
           "(vertex + a disconnected throwaway index object)";
}

// Polyline mesh → one TRIANGLES primitive per MeshPolyline (faithful thick-line
// path: PolylineTesselator expands each line into a corner buffer drawn as
// glDrawArrays(GL_TRIANGLES); no element index buffer is created for polylines).
TEST(MeshRenderGeometryMockTest, PolylineMeshUploadsOneTrianglesPrimitivePerPolyline)
{
    MockDriver drv;
    const int polylineCount = 2;
    auto mesh = buildPolylineMesh(polylineCount);
    auto mg = MeshRenderGeometry::create(drv, *mesh);
    ASSERT_NE(mg, nullptr);

    // vertexBufferCount: 1 shared vbh (built unconditionally) + 1 corner vbh per
    // polyline = 1 + polylineCount.
    EXPECT_EQ(drv.vertexBufferCount(), 1u + static_cast<size_t>(polylineCount));
    // Each corner vbh is created with the tesselator's numCorners (a multiple of
    // 3 — triangle list); don't pin the exact count (tesselator-internal).
    EXPECT_GT(drv.lastVertexCount(), 0u);
    EXPECT_EQ(drv.lastVertexCount() % 3u, 0u);
    EXPECT_EQ(drv.primitiveCount(), static_cast<size_t>(polylineCount));  // one TRIANGLES per polyline
    EXPECT_EQ(drv.primitiveTypeAt(0), PrimitiveType::TRIANGLES);
    EXPECT_EQ(drv.primitiveTypeAt(1), PrimitiveType::TRIANGLES);
    // No element index buffer is created on the faithful thick-line path
    // (PolylineBuffers has no indices buffer; CachedGeometry.ts:1148-1150 only
    // holds the 3 corner VBOs).
    EXPECT_EQ(drv.indexBufferCount(), 0u);
}

// Single-polyline sanity (the common addPath/addLineString case).
TEST(MeshRenderGeometryMockTest, SinglePolylineUploadsOneTrianglesPrimitive)
{
    MockDriver drv;
    auto mesh = buildPolylineMesh(1);
    auto mg = MeshRenderGeometry::create(drv, *mesh);
    ASSERT_NE(mg, nullptr);
    EXPECT_EQ(drv.primitiveCount(), 1u);
    EXPECT_EQ(drv.primitiveTypeAt(0), PrimitiveType::TRIANGLES);
    EXPECT_EQ(drv.indexBufferCount(), 0u);  // no IBO on the corner-buffer path
}

// Point mesh → one POINTS primitive over all vertices (synthesized 0..vc-1 indices).
TEST(MeshRenderGeometryMockTest, PointMeshUploadsOnePointsPrimitive)
{
    MockDriver drv;
    auto mesh = buildPointMesh();
    auto mg = MeshRenderGeometry::create(drv, *mesh);
    ASSERT_NE(mg, nullptr);

    EXPECT_EQ(drv.vertexBufferCount(), 1u);
    EXPECT_EQ(drv.lastVertexCount(), 4u);
    EXPECT_EQ(drv.primitiveCount(), 1u);
    EXPECT_EQ(drv.primitiveTypeAt(0), PrimitiveType::POINTS);
    EXPECT_EQ(drv.primitiveIndexCount(0), 4u);     // synthesized 0..3
}
