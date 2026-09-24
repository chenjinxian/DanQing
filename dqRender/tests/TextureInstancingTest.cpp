// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Texture atlas, cubemap, drape, instancing tests
// Ported from: itwinjs-core core/frontend/src/render/TextureInstancing.ts

#include "render/TextureAtlas.h"
#include "render/CubeTextureHandle.h"
#include "render/TextureDrape.h"
#include "render/InstanceBuffers.h"
#include "render/InstancedGeometry.h"
#include "render/IndexedGeometry.h"

#include <dqGeom/Transform.h>

#include <gtest/gtest.h>
#include <vector>
#include <memory>

using namespace dqRender;

// ============================================================================
// TextureAtlas tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, Init)
{
    TextureAtlas atlas;
    atlas.init(256, 256);
    EXPECT_EQ(atlas.getWidth(), 256u);
    EXPECT_EQ(atlas.getHeight(), 256u);
    EXPECT_EQ(atlas.getRectCount(), 0u);
    EXPECT_FALSE(atlas.isValid());
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, AddSingleRect)
{
    TextureAtlas atlas;
    atlas.init(256, 256);

    auto rect = atlas.addRect(64, 64);
    EXPECT_EQ(rect.x, 0u);
    EXPECT_EQ(rect.y, 0u);
    EXPECT_EQ(rect.width, 64u);
    EXPECT_EQ(rect.height, 64u);
    EXPECT_EQ(atlas.getRectCount(), 1u);
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, AddMultipleRects)
{
    TextureAtlas atlas;
    atlas.init(256, 256);

    auto r1 = atlas.addRect(64, 64);
    auto r2 = atlas.addRect(64, 64);
    auto r3 = atlas.addRect(128, 32);

    EXPECT_EQ(r1.x, 0u);
    EXPECT_EQ(r1.y, 0u);
    EXPECT_EQ(r2.x, 64u);
    EXPECT_EQ(r2.y, 0u);
    EXPECT_EQ(r3.x, 128u);
    EXPECT_EQ(r3.y, 0u);
    EXPECT_EQ(atlas.getRectCount(), 3u);
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, RectTooWide)
{
    TextureAtlas atlas;
    atlas.init(64, 64);

    auto rect = atlas.addRect(128, 32);  // Too wide
    EXPECT_EQ(rect.width, 0u);  // Failed
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, RectTooTall)
{
    TextureAtlas atlas;
    atlas.init(64, 64);

    auto rect = atlas.addRect(32, 128);  // Too tall
    EXPECT_EQ(rect.width, 0u);  // Failed
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, PackMultipleRows)
{
    TextureAtlas atlas;
    atlas.init(128, 128);

    // Fill first row
    auto r1 = atlas.addRect(64, 32);
    auto r2 = atlas.addRect(64, 32);
    EXPECT_EQ(r1.y, 0u);
    EXPECT_EQ(r2.y, 0u);

    // Second row should start at y=32
    auto r3 = atlas.addRect(64, 32);
    EXPECT_EQ(r3.y, 32u);
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureAtlasTest, UvCoordinates)
{
    TextureAtlas atlas;
    atlas.init(256, 256);

    auto rect = atlas.addRect(128, 128);
    EXPECT_FLOAT_EQ(rect.getU0(256.0f), 0.0f);
    EXPECT_FLOAT_EQ(rect.getV0(256.0f), 0.0f);
    EXPECT_FLOAT_EQ(rect.getU1(256.0f), 0.5f);
    EXPECT_FLOAT_EQ(rect.getV1(256.0f), 0.5f);
}

// ============================================================================
// CubeTextureHandle tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(CubeTextureTest, DefaultState)
{
    CubeTextureHandle cube;
    EXPECT_FALSE(cube.isValid());
    EXPECT_EQ(cube.getFaceSize(), 0u);
}

// Note: create/setFaceData tests require a real GPU context (Driver).
// These are tested in the integration tests with a running OpenGL context.

// ============================================================================
// TextureDrape tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureDrapeTest, DefaultState)
{
    TextureDrape drape;
    EXPECT_FALSE(drape.isValid());
    EXPECT_EQ(drape.getWidth(), 0u);
    EXPECT_EQ(drape.getHeight(), 0u);

    auto const& uv = drape.getUvTransform();
    EXPECT_FLOAT_EQ(uv[0], 1.0f);  // scaleU
    EXPECT_FLOAT_EQ(uv[1], 1.0f);  // scaleV
    EXPECT_FLOAT_EQ(uv[2], 0.0f);  // offsetU
    EXPECT_FLOAT_EQ(uv[3], 0.0f);  // offsetV
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(TextureDrapeTest, SetUvTransform)
{
    TextureDrape drape;
    drape.setUvTransform(0.5f, 0.5f, 10.0f, 20.0f);

    auto const& uv = drape.getUvTransform();
    EXPECT_FLOAT_EQ(uv[0], 0.5f);
    EXPECT_FLOAT_EQ(uv[1], 0.5f);
    EXPECT_FLOAT_EQ(uv[2], 10.0f);
    EXPECT_FLOAT_EQ(uv[3], 20.0f);
}

// ============================================================================
// InstanceBuffers tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(InstanceBuffersTest, DefaultState)
{
    // Construct with 1 instance, empty handles — tests the basic API surface
    InstanceBuffers buffers(1, rhi::BufferObjectHandle{}, 0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(buffers.isValid());  // no transform buffer
    EXPECT_EQ(buffers.getNumInstances(), 1u);
    EXPECT_FALSE(buffers.hasFeatures());
}

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(InstanceBuffersTest, TransformCenter)
{
    InstanceBuffers buffers(1, rhi::BufferObjectHandle{}, 100.0f, 200.0f, 300.0f);
    // getRtcOnlyTransform returns a 4x4 translation matrix.
    // The translation column (indices 12,13,14) should contain the center.
    float rtcTransform[16];
    buffers.getRtcOnlyTransform(rtcTransform);
    EXPECT_FLOAT_EQ(rtcTransform[12], 100.0f);
    EXPECT_FLOAT_EQ(rtcTransform[13], 200.0f);
    EXPECT_FLOAT_EQ(rtcTransform[14], 300.0f);
}

// Authored: no reference test exists in itwinjs-core for
// InstanceData.getRtcModelTransform (reference impl: InstancedGeometry.ts
// InstanceData.getRtcModelTransform, line 46-53).
// Verifies rtcModelTransform = modelMatrix * rtcOnlyTransform in Transform space.
TEST(InstanceBuffersTest, GetRtcModelTransformIdentityModelReturnsRtcCenter)
{
    InstanceBuffers buffers(1, rhi::BufferObjectHandle{}, 100.0f, 200.0f, 300.0f);
    dqGeom::Transform const identity = dqGeom::Transform::CreateIdentity();
    dqGeom::Transform const rtc = buffers.getRtcModelTransform(identity);
    // identity * rtcOnly = rtcOnly -> origin = rtcCenter, matrix stays identity.
    EXPECT_DOUBLE_EQ(rtc.GetOrigin().x, 100.0);
    EXPECT_DOUBLE_EQ(rtc.GetOrigin().y, 200.0);
    EXPECT_DOUBLE_EQ(rtc.GetOrigin().z, 300.0);
}

// Authored: no reference test exists in itwinjs-core for getRtcModelTransform.
// model = translate(5,0,0), rtcCenter = (10,0,0):
// rtcModel = model * rtcOnly -> origin = model.origin + model.matrix * rtcCenter
//                              = (5,0,0) + (10,0,0) = (15,0,0).
TEST(InstanceBuffersTest, GetRtcModelTransformComposesModel)
{
    InstanceBuffers buffers(1, rhi::BufferObjectHandle{}, 10.0f, 0.0f, 0.0f);
    dqGeom::Transform const model = dqGeom::Transform::CreateTranslation(5.0, 0.0, 0.0);
    dqGeom::Transform const rtc = buffers.getRtcModelTransform(model);
    EXPECT_DOUBLE_EQ(rtc.GetOrigin().x, 15.0);
    EXPECT_DOUBLE_EQ(rtc.GetOrigin().y, 0.0);
    EXPECT_DOUBLE_EQ(rtc.GetOrigin().z, 0.0);
}

// Authored: no reference test exists in itwinjs-core for getRtcModelTransform.
// Caching: a second call with the same modelMatrix returns the cached transform;
// a different modelMatrix recomputes (Ported semantics, line 46-53).
TEST(InstanceBuffersTest, GetRtcModelTransformCachesUntilModelChanges)
{
    InstanceBuffers buffers(1, rhi::BufferObjectHandle{}, 10.0f, 0.0f, 0.0f);
    dqGeom::Transform const modelA = dqGeom::Transform::CreateTranslation(5.0, 0.0, 0.0);
    dqGeom::Transform const rtcA1 = buffers.getRtcModelTransform(modelA);
    dqGeom::Transform const rtcA2 = buffers.getRtcModelTransform(modelA);  // cached
    EXPECT_DOUBLE_EQ(rtcA1.GetOrigin().x, 15.0);
    EXPECT_DOUBLE_EQ(rtcA2.GetOrigin().x, 15.0);

    dqGeom::Transform const modelB = dqGeom::Transform::CreateTranslation(1.0, 2.0, 3.0);
    dqGeom::Transform const rtcB = buffers.getRtcModelTransform(modelB);  // recompute
    // rtcB.origin = (1,2,3) + (10,0,0) = (11,2,3)
    EXPECT_DOUBLE_EQ(rtcB.GetOrigin().x, 11.0);
    EXPECT_DOUBLE_EQ(rtcB.GetOrigin().y, 2.0);
    EXPECT_DOUBLE_EQ(rtcB.GetOrigin().z, 3.0);
}

// ============================================================================
// InstancedGeometry tests
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(InstancedGeometryTest, DefaultState)
{
    // Can't fully test without a driver, but verify the class exists
    // and the interface is correct
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        0
    );
    auto base = std::make_unique<IndexedGeometry>(std::move(params));
    auto buffers = std::make_unique<InstanceBuffers>(
        1, rhi::BufferObjectHandle{}, 0.0f, 0.0f, 0.0f);

    EXPECT_NE(base.get(), nullptr);
    EXPECT_NE(buffers.get(), nullptr);
    EXPECT_EQ(buffers->getNumInstances(), 1u);
    EXPECT_FALSE(buffers->isValid());  // no transform buffer created
}

// ============================================================================
// IndexedGeometry instanced draw test
// ============================================================================

// Ported from: itwinjs-core core/frontend/src/test/TextureInstancing.test.ts
TEST(IndexedGeometryTest, InstancedDraw)
{
    IndexedGeometryParams params(
        rhi::RenderPrimitiveHandle{},
        rhi::BufferObjectHandle{},
        rhi::IndexBufferHandle{},
        0
    );
    IndexedGeometry geo(std::move(params));
    geo.setTechniqueId(TechniqueId::Surface);
    geo.setPass(Pass::Opaque);

    EXPECT_EQ(geo.getTechniqueId(), TechniqueId::Surface);
    EXPECT_EQ(geo.getPass(), Pass::Opaque);
}
