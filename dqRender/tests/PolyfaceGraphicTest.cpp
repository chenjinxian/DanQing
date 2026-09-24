// SPDX-License-Identifier: Apache-2.0
// dqRender — PolyfaceGraphic index-buffer upload regression test (GL-free).
// Authored: repro for the glDrawElements SIGSEGV (KERN_INVALID_ADDRESS at 0x0).
// PolyfaceGraphic::uploadToGpu must upload index data via Driver::updateIndexBuffer
// (which wires the bytes into the IndexBufferHandle the draw binds as
// GL_ELEMENT_ARRAY_BUFFER), NOT via a disconnected updateBufferObject on a
// throwaway BufferObjectHandle. The grid hid the bug by drawing with glDrawArrays
// (never reading the element buffer); the cube's glDrawElements read an unbacked
// element buffer and faulted.
#include "NullDriver.h"

#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"
#include "render/InstancedGeometry.h"
#include "render/PolyfaceGraphic.h"

#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

using namespace dqRender;

namespace {
// Records which Driver upload API PolyfaceGraphic::uploadToGpu uses.
class RecordingIndexDriver : public rhi::NullDriver {
public:
    void updateIndexBuffer(rhi::IndexBufferHandle, rhi::BufferDescriptor&&, uint32_t) noexcept override {
        ++m_indexUpdates;
    }
    void updateBufferObject(rhi::BufferObjectHandle, rhi::BufferDescriptor&&, uint32_t) noexcept override {
        ++m_bufferObjectUpdates;
    }
    int m_indexUpdates = 0;
    int m_bufferObjectUpdates = 0;
};

// Minimal 3-point triangle polyface with a normal (1-based indices, per
// IndexedPolyface convention — see dqGeom/tests/PolyfaceTest.cpp AddTriangle).
// IndexedPolyface::create returns a RefPtr.
dqBase::RefPtr<dqGeom::IndexedPolyface> makeTrianglePolyface() {
    auto pf = dqGeom::IndexedPolyface::create(true, false);  // needNormals=true
    pf->AddPoint(dqGeom::Point3d::From(0, 0, 0));
    pf->AddPoint(dqGeom::Point3d::From(1, 0, 0));
    pf->AddPoint(dqGeom::Point3d::From(0, 1, 0));
    pf->AddNormal(dqGeom::Vector3d::From(0, 0, 1));
    pf->AddPointIndex(1, true);
    pf->AddPointIndex(2, true);
    pf->AddPointIndex(3, true);
    pf->TerminateFacet();
    return pf;
}
}  // namespace

// Before the fix, uploadToGpu uploaded index bytes to a throwaway BufferObject
// (updateBufferObject) that was never connected to the IndexBufferHandle the draw
// binds — glDrawElements then read an unbacked element buffer and faulted (NULL).
// After the fix, uploadToGpu uses updateIndexBuffer exactly once; updateBufferObject
// is used only for the vertex VBO.
TEST(PolyfaceGraphicTest, UploadWiresIndexBufferViaUpdateIndexBuffer)
{
    RecordingIndexDriver driver;
    auto pf = makeTrianglePolyface();
    PolyfaceGraphic g(driver, *pf, 0xFFFFFFFF);

    ASSERT_GT(g.getIndexCount(), 0u) << "polyface produced no indices";
    ASSERT_GT(g.getVertexCount(), 0u) << "polyface produced no vertices";

    EXPECT_EQ(driver.m_indexUpdates, 1)
        << "index data must be uploaded via updateIndexBuffer (wires it into the "
           "IndexBufferHandle the draw binds); before the fix this was 0";
    EXPECT_EQ(driver.m_bufferObjectUpdates, 1)
        << "updateBufferObject should be used only for the vertex VBO (1); before "
           "the fix it was 2 (vertex + a disconnected throwaway index object)";
}

// Ported from: itwinjs-core PolyfaceGraphic per-corner UV from PolyfaceData.param
//              (SurfaceGeometry texture path; SurfaceTextureShaders.h v_texCoord).
TEST(PolyfaceGraphicTest, ReadsUvFromPolyfaceParams)
{
    rhi::NullDriver driver;
    auto pf = dqGeom::IndexedPolyface::create(false, false, false, /*needParams=*/true);
    pf->AddPoint(dqGeom::Point3d::From(0, 0, 0));
    pf->AddPoint(dqGeom::Point3d::From(1, 0, 0));
    pf->AddPoint(dqGeom::Point3d::From(0, 1, 0));
    pf->AddParam(dqGeom::Point2d::From(0.0, 0.0));
    pf->AddParam(dqGeom::Point2d::From(1.0, 0.0));
    pf->AddParam(dqGeom::Point2d::From(0.5, 1.0));
    for (int32_t i = 1; i <= 3; ++i) {
        pf->AddPointIndex(i);
        pf->AddParamIndex(i);
    }
    pf->TerminateFacet();

    PolyfaceGraphic g(driver, *pf, 0xFFFFFFFF);
    ASSERT_EQ(g.getVertexCount(), 3u);
    float uv[2] = {-1.f, -1.f};
    g.getVertexTexCoord(1, uv);
    EXPECT_FLOAT_EQ(uv[0], 1.0f);  // vertex 1 -> param (1.0, 0.0)
    EXPECT_FLOAT_EQ(uv[1], 0.0f);
    g.getVertexTexCoord(2, uv);
    EXPECT_FLOAT_EQ(uv[0], 0.5f);
    EXPECT_FLOAT_EQ(uv[1], 1.0f);
}

// Ported from: itwinjs-core CachedGeometry.getSurfaceTexture surface-texture
//              routing (Surface.ts addTexture :596-609 — compositor bind source).
TEST(PolyfaceGraphicTest, TextureHandleRoundTrip)
{
    rhi::NullDriver driver;
    auto pf = makeTrianglePolyface();
    PolyfaceGraphic g(driver, *pf, 0xFFFFFFFF);
    // Default: null handle (HandleBase's nullid sentinel). Handle has no
    // isValid(); truthiness is the explicit operator bool.
    EXPECT_FALSE(static_cast<bool>(g.getSurfaceTexture()));
    // TextureHandle's id constructor is private (HandleAllocator is the sole
    // friend, Handle.h:99), so a synthetic id like {42} cannot be spelled from
    // a test — allocate a real handle instead (VertexLutTextureTest.cpp:42
    // pattern) and assert the round-trip semantic: the set handle comes back
    // unchanged (same id).
    rhi::HandleAllocator allocator;
    rhi::TextureHandle const tex = allocator.allocate<rhi::HwTexture>();
    ASSERT_TRUE(static_cast<bool>(tex));
    g.setTexture(tex);
    EXPECT_EQ(g.getSurfaceTexture(), tex);
    EXPECT_EQ(g.getSurfaceTexture().getId(), tex.getId());
}

// Ported from: itwinjs-core InstancedGeometry forwards all render queries to
//              repr (InstancedGeometry.ts:357-376) — getSurfaceTexture must
//              forward too. The old compositor bind reached repr's texture via
//              asSurface() (itself forwarded, InstancedGeometry.cpp:33); the new
//              CachedGeometry virtual routing regressed instanced surfaces to the
//              base-class default {} (never binds s_texture) without this override.
TEST(PolyfaceGraphicTest, InstancedGeometryForwardsSurfaceTexture)
{
    rhi::NullDriver driver;
    auto pf = makeTrianglePolyface();
    PolyfaceGraphic repr(driver, *pf, 0xFFFFFFFF);
    rhi::HandleAllocator allocator;
    rhi::TextureHandle const tex = allocator.allocate<rhi::HwTexture>();
    ASSERT_TRUE(static_cast<bool>(tex));
    repr.setTexture(tex);

    // InstanceBuffers' public ctor is a pure data holder (nullid handles are
    // fine under NullDriver); ownership passes to InstancedGeometry (its dtor
    // deletes the buffers), so it must be heap-allocated.
    InstanceBuffers* buffers =
        new InstanceBuffers(1, rhi::BufferObjectHandle{}, 0.f, 0.f, 0.f);
    InstancedGeometry instanced(&repr, buffers);

    EXPECT_EQ(instanced.getSurfaceTexture(), tex);
    EXPECT_EQ(instanced.getSurfaceTexture().getId(), repr.getSurfaceTexture().getId());
}
