// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — BranchUniforms.update tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              update() (line 181-257, non-instanced/non-vio path)
#include "render/Uniforms.h"
#include "render/InstancedGeometry.h"
#include "render/InstanceBuffers.h"
#include "render/IndexedGeometry.h"

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>
#include <memory>

using namespace dqRender;
using namespace dqGeom;

// identity model/view/projection -> identity mv and mvp.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, IdentityProducesIdentity)
TEST(BranchUniformsUpdateTest, IdentityProducesIdentity)
{
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), Transform::CreateIdentity(),
              Matrix4d::CreateIdentity(), false);
    EXPECT_FLOAT_EQ(bu.getModelViewMatrix()[0], 1.0f);
    EXPECT_FLOAT_EQ(bu.getModelViewMatrix()[15], 1.0f);
    EXPECT_FLOAT_EQ(bu.getModelViewProjectionMatrix()[0], 1.0f);
    EXPECT_FLOAT_EQ(bu.getModelViewProjectionMatrix()[15], 1.0f);
}

// mv = view * model: a translated model (identity view) yields mv with that translation.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, ModelTranslationPropagates)
TEST(BranchUniformsUpdateTest, ModelTranslationPropagates)
{
    BranchUniforms bu;
    Transform const model(Point3d::From(10.0, 20.0, 30.0), Matrix3d::CreateIdentity());
    bu.update(model, Transform::CreateIdentity(), Matrix4d::CreateIdentity(), false);

    // Column-major: translation lives in [12],[13],[14].
    EXPECT_FLOAT_EQ(bu.getModelViewMatrix()[12], 10.0f);
    EXPECT_FLOAT_EQ(bu.getModelViewMatrix()[13], 20.0f);
    EXPECT_FLOAT_EQ(bu.getModelViewMatrix()[14], 30.0f);
}

// wantThematic populates m32 (model) and v32 (view matrix).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, ThematicPopulatesModelAndView)
TEST(BranchUniformsUpdateTest, ThematicPopulatesModelAndView)
{
    BranchUniforms bu;
    Transform const model(Point3d::From(5.0, 0.0, 0.0), Matrix3d::CreateIdentity());
    bu.update(model, Transform::CreateIdentity(), Matrix4d::CreateIdentity(), true);

    // m32 = model -> translation at [12].
    EXPECT_FLOAT_EQ(bu.getModelMatrix()[12], 5.0f);
    // v32 = identity view -> Matrix3 identity ([0],[4],[8] = 1).
    EXPECT_FLOAT_EQ(bu.getViewMatrix3()[0], 1.0f);
    EXPECT_FLOAT_EQ(bu.getViewMatrix3()[4], 1.0f);
    EXPECT_FLOAT_EQ(bu.getViewMatrix3()[8], 1.0f);

    UniformHandle h;
    bu.bindWorldToViewNTransform(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::Mat3);
}

// Non-thematic leaves m32 at identity (the update path only writes it when thematic).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, NonThematicLeavesModelIdentity)
TEST(BranchUniformsUpdateTest, NonThematicLeavesModelIdentity)
{
    BranchUniforms bu;
    Transform const model(Point3d::From(5.0, 0.0, 0.0), Matrix3d::CreateIdentity());
    bu.update(model, Transform::CreateIdentity(), Matrix4d::CreateIdentity(), false);
    // mModel unchanged from default identity.
    EXPECT_FLOAT_EQ(bu.getModelMatrix()[0], 1.0f);
    EXPECT_FLOAT_EQ(bu.getModelMatrix()[12], 0.0f);
}

// ============================================================================
// Full update() paths (Ported from: itwinjs-core BranchUniforms.update line 181-257)
// Authored: no reference unit test exists in itwinjs-core for BranchUniforms.update
// (it is exercised only end-to-end via the renderer). These verify each branch.
// ============================================================================

// Instanced non-VIO path (reference line 226):
//   mv = view * instancedGeom.getRtcModelTransform(model)
// identity model+view, rtcCenter=(10,0,0) -> mv origin = (10,0,0).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, InstancedRtcPathUsesRtcModelTransform)
TEST(BranchUniformsUpdateTest, InstancedRtcPathUsesRtcModelTransform)
{
    IndexedGeometryParams params(rhi::RenderPrimitiveHandle{}, rhi::BufferObjectHandle{},
                                 rhi::IndexBufferHandle{}, 0);
    auto base = std::make_unique<IndexedGeometry>(std::move(params));
    // InstancedGeometry owns buffers (destructor deletes it) -> raw new, not unique_ptr.
    InstanceBuffers* buffers = new InstanceBuffers(1, rhi::BufferObjectHandle{}, 10.0f, 0.0f, 0.0f);
    InstancedGeometry inst(base.get(), buffers);

    BranchUniforms::UpdateParams p;
    p.instancedGeom = &inst;
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), Transform::CreateIdentity(),
              Matrix4d::CreateIdentity(), p);

    // mv = identity * rtcOnly = translation(10,0,0) -> column-major [12]=10.
    EXPECT_NEAR(bu.getModelViewMatrix()[12], 10.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[13], 0.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[14], 0.0f, 1e-5f);
}

// Instanced geometry skips mvp computation (reference line 251-254).
// A fresh BranchUniforms has mvp = identity; update must leave it identity.
// A non-identity projection is used so that a *computed* mvp (proj * mv) would
// be non-identity, making the skip observable.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, InstancedSkipsMvpComputation)
TEST(BranchUniformsUpdateTest, InstancedSkipsMvpComputation)
{
    IndexedGeometryParams params(rhi::RenderPrimitiveHandle{}, rhi::BufferObjectHandle{},
                                 rhi::IndexBufferHandle{}, 0);
    auto base = std::make_unique<IndexedGeometry>(std::move(params));
    InstanceBuffers* buffers = new InstanceBuffers(1, rhi::BufferObjectHandle{}, 10.0f, 0.0f, 0.0f);
    InstancedGeometry inst(base.get(), buffers);

    // projection = uniform scale 3 -> if mvp were computed, [0] would be 3.
    Matrix4d const projection = Matrix4d::CreateTransform(
        Transform(Point3d::FromZero(), Matrix3d::CreateUniformScale(3.0)));

    BranchUniforms::UpdateParams p;
    p.instancedGeom = &inst;
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), Transform::CreateIdentity(), projection, p);

    // mvp stays identity ([0]=1) — not recomputed for instanced geometry.
    EXPECT_FLOAT_EQ(bu.getModelViewProjectionMatrix()[0], 1.0f);
    EXPECT_FLOAT_EQ(bu.getModelViewProjectionMatrix()[12], 0.0f);
}

// View-independent-origin path (reference line 229-233): geometry oriented
// independently of view rotation. With vio at the origin + identity model:
//   viModel = rotateAboutOrigin(vio, view^-1) * model = view^-1
//   mv = view * viModel = view * view^-1 = identity.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, ViewIndependentOriginCancelsViewRotation)
TEST(BranchUniformsUpdateTest, ViewIndependentOriginCancelsViewRotation)
{
    Transform view = Transform::CreateIdentity();
    view.SetMatrix(Matrix3d::CreateRotationAroundAxis(Vector3d::From(0.0, 0.0, 1.0), 0.7));
    Point3d const vio = Point3d::From(0.0, 0.0, 0.0);

    BranchUniforms::UpdateParams p;
    p.viewIndependentOrigin = &vio;
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), view, Matrix4d::CreateIdentity(), p);

    // mv matrix ~ identity (rotation canceled).
    EXPECT_NEAR(bu.getModelViewMatrix()[0], 1.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[1], 0.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[5], 1.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[10], 1.0f, 1e-5f);
}

// View-coords path (reference line 203-213): model Z column zeroed ("silly
// clipping tools"), then scaled by devicePixelRatio about the origin.
// identity model + dpr=2 -> zeroed matrix diag(1,1,0), scaled -> diag(2,2,0).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, ViewCoordsScalesByDevicePixelRatioAndZerosZ)
TEST(BranchUniformsUpdateTest, ViewCoordsScalesByDevicePixelRatioAndZerosZ)
{
    BranchUniforms::UpdateParams p;
    p.isViewCoords = true;
    p.devicePixelRatio = 2.0;
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), Transform::CreateIdentity(),
              Matrix4d::CreateIdentity(), p);

    // mv matrix = diag(2,2,0): column-major [0]=2, [5]=2, [10]=0.
    EXPECT_NEAR(bu.getModelViewMatrix()[0], 2.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[5], 2.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[10], 0.0f, 1e-5f);
}

// Non-thematic contour/constant-lod path (reference line 243-244):
//   m32 = modelMatrix when wantContourLines || hasConstantLodVParams.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, ContourLinesPopulateModelMatrix32WhenNonThematic)
TEST(BranchUniformsUpdateTest, ContourLinesPopulateModelMatrix32WhenNonThematic)
{
    Transform const model(Point3d::From(7.0, 8.0, 9.0), Matrix3d::CreateIdentity());
    BranchUniforms::UpdateParams p;
    p.wantContourLines = true;  // non-thematic
    BranchUniforms bu;
    bu.update(model, Transform::CreateIdentity(), Matrix4d::CreateIdentity(), p);

    // m32 = model -> translation at [12],[13],[14].
    EXPECT_NEAR(bu.getModelMatrix()[12], 7.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelMatrix()[13], 8.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelMatrix()[14], 9.0f, 1e-5f);
}

// Regression: instanced + view-independent-origin combined path (reference line
// 220-224). The view rotation is canceled AND the RTC translation applies:
// identity model, rtcCenter=(10,0,0), vio at origin, rotated view ->
//   mv = view * view^-1 * rtcOnly = translation(10,0,0), matrix = identity.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, InstancedAndViewIndependentOriginCombine)
TEST(BranchUniformsUpdateTest, InstancedAndViewIndependentOriginCombine)
{
    IndexedGeometryParams params(rhi::RenderPrimitiveHandle{}, rhi::BufferObjectHandle{},
                                 rhi::IndexBufferHandle{}, 0);
    auto base = std::make_unique<IndexedGeometry>(std::move(params));
    InstanceBuffers* buffers = new InstanceBuffers(1, rhi::BufferObjectHandle{}, 10.0f, 0.0f, 0.0f);
    InstancedGeometry inst(base.get(), buffers);

    Transform view = Transform::CreateIdentity();
    view.SetMatrix(Matrix3d::CreateRotationAroundAxis(Vector3d::From(0.0, 0.0, 1.0), 0.7));
    Point3d const vio = Point3d::From(0.0, 0.0, 0.0);

    BranchUniforms::UpdateParams p;
    p.instancedGeom = &inst;
    p.viewIndependentOrigin = &vio;
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), view, Matrix4d::CreateIdentity(), p);

    // Rotation canceled -> matrix identity ([0]~1, [1]~0); RTC origin (10,0,0).
    EXPECT_NEAR(bu.getModelViewMatrix()[0], 1.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[1], 0.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[12], 10.0f, 1e-5f);
}

// Regression: view-coords + instanced combined path (reference line 207-208).
// The zeroed-Z model is passed through getRtcModelTransform, then scaled by dpr:
// identity model (Z zeroed), rtcCenter=(10,0,0), dpr=2 ->
//   rtcModel origin = (10,0,0); scaled -> origin (20,0,0), matrix diag(2,2,0).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BranchUniforms.ts
//              TEST(BranchUniformsUpdateTest, ViewCoordsAndInstancedCombine)
TEST(BranchUniformsUpdateTest, ViewCoordsAndInstancedCombine)
{
    IndexedGeometryParams params(rhi::RenderPrimitiveHandle{}, rhi::BufferObjectHandle{},
                                 rhi::IndexBufferHandle{}, 0);
    auto base = std::make_unique<IndexedGeometry>(std::move(params));
    InstanceBuffers* buffers = new InstanceBuffers(1, rhi::BufferObjectHandle{}, 10.0f, 0.0f, 0.0f);
    InstancedGeometry inst(base.get(), buffers);

    BranchUniforms::UpdateParams p;
    p.isViewCoords = true;
    p.devicePixelRatio = 2.0;
    p.instancedGeom = &inst;
    BranchUniforms bu;
    bu.update(Transform::CreateIdentity(), Transform::CreateIdentity(),
              Matrix4d::CreateIdentity(), p);

    // matrix diag(2,2,0): [0]=2, [10]=0; origin scaled by dpr: [12]=20.
    EXPECT_NEAR(bu.getModelViewMatrix()[0], 2.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[10], 0.0f, 1e-5f);
    EXPECT_NEAR(bu.getModelViewMatrix()[12], 20.0f, 1e-5f);
}
