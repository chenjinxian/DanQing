// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — FrustumUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              (no direct reference test exists; tests are authored from FrustumUniforms.ts behavior)
#include "render/FrustumUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;
using namespace dqGeom;

// Helper: build an axis-aligned box frustum.
//   x in [0,100], y in [0,100], near (Front) z = 1, far (Rear) z = 10.
static Frustum makeBoxFrustum()
{
    Frustum f;
    f.getCorner(Npc::LeftBottomRear)   = Point3d::From(0,   0,   10);  // far lower left
    f.getCorner(Npc::RightBottomRear)  = Point3d::From(100, 0,   10);  // far lower right
    f.getCorner(Npc::LeftTopRear)      = Point3d::From(0,   100, 10);  // far upper left
    f.getCorner(Npc::RightTopRear)     = Point3d::From(100, 100, 10);  // far upper right
    f.getCorner(Npc::LeftBottomFront)  = Point3d::From(0,   0,   1);   // near lower left
    f.getCorner(Npc::RightBottomFront) = Point3d::From(100, 0,   1);   // near lower right
    f.getCorner(Npc::LeftTopFront)     = Point3d::From(0,   100, 1);   // near upper left
    f.getCorner(Npc::RightTopFront)    = Point3d::From(100, 100, 1);   // near upper right
    return f;
}

// Default state: identity projection, identity view.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, DefaultState)
TEST(FrustumUniformsTest, DefaultState)
{
    FrustumUniforms u;
    EXPECT_EQ(u.getProjectionMatrix32().data[0], 1.0f);
    EXPECT_EQ(u.getProjectionMatrix32().data[5], 1.0f);
    EXPECT_EQ(u.getProjectionMatrix32().data[10], 1.0f);
    EXPECT_EQ(u.getProjectionMatrix32().data[15], 1.0f);
    // Default frustum type is TwoDee (mFrustumData[2] = 0) -> is2d.
    EXPECT_TRUE(u.getIs2d());
}

// changeFrustum with a 2D box computes the ortho projection, planes, and frustum.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, ChangeFrustumOrtho2d)
TEST(FrustumUniformsTest, ChangeFrustumOrtho2d)
{
    FrustumUniforms u;
    Frustum f = makeBoxFrustum();
    u.changeFrustum(f, 0.0, false);  // is3d=false -> TwoDee ortho path

    // Type = TwoDee, near=0, far=depth=9.
    EXPECT_EQ(u.getType(), FrustumUniformType::TwoDee);
    EXPECT_FLOAT_EQ(u.getNearPlane(), 0.0f);
    EXPECT_FLOAT_EQ(u.getFarPlane(), 9.0f);
    EXPECT_TRUE(u.getIs2d());

    // Planes: { top=halfHeight, bottom=-halfHeight, left=-halfWidth, right=halfWidth } = {50,-50,-50,50}.
    float const* planes = u.getPlanes();
    EXPECT_FLOAT_EQ(planes[0], 50.0f);   // top
    EXPECT_FLOAT_EQ(planes[1], -50.0f);  // bottom
    EXPECT_FLOAT_EQ(planes[2], -50.0f);  // left
    EXPECT_FLOAT_EQ(planes[3], 50.0f);   // right

    // Ortho projection: m00 = 2/100 = 0.02, m11 = 0.02, m22 = -2/9, m23 = -1.
    auto const& p = u.getProjectionMatrix32().data;
    EXPECT_FLOAT_EQ(p[0], 0.02f);
    EXPECT_FLOAT_EQ(p[5], 0.02f);
    EXPECT_NEAR(p[10], -2.0 / 9.0, 1e-5);
    EXPECT_NEAR(p[14], -1.0, 1e-5);  // m23 = -(far+near)/(far-near) = -9/9
    EXPECT_FLOAT_EQ(p[15], 1.0f);

    // View-up vector: world up (0,0,1) rotated by identity view -> (0,0,1).
    float const* up = u.getViewUpVector();
    EXPECT_NEAR(up[0], 0.0, 1e-6);
    EXPECT_NEAR(up[1], 0.0, 1e-6);
    EXPECT_NEAR(up[2], 1.0, 1e-6);

    // LogZ: nearPlane is 0 -> logZ = {0, farPlane}.
    EXPECT_FLOAT_EQ(u.getLogZ()[0], 0.0f);
    EXPECT_FLOAT_EQ(u.getLogZ()[1], 9.0f);
}

// changeFrustum with is3d=true, fraction > 0.999 takes the ortho path -> Orthographic type.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, ChangeFrustumOrtho3d)
TEST(FrustumUniformsTest, ChangeFrustumOrtho3d)
{
    FrustumUniforms u;
    Frustum f = makeBoxFrustum();
    u.changeFrustum(f, 1.0, true);  // fraction=1.0 > 0.999, is3d=true -> Orthographic

    EXPECT_EQ(u.getType(), FrustumUniformType::Orthographic);
    EXPECT_FALSE(u.getIs2d());
    EXPECT_FLOAT_EQ(u.getFarPlane(), 9.0f);
}

// changeFrustum with is3d=true, mid fraction takes the perspective path.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, ChangeFrustumPerspective)
TEST(FrustumUniformsTest, ChangeFrustumPerspective)
{
    FrustumUniforms u;
    Frustum f = makeBoxFrustum();
    u.changeFrustum(f, 0.5, true);  // perspective path

    EXPECT_EQ(u.getType(), FrustumUniformType::Perspective);
    EXPECT_FALSE(u.getIs2d());

    // Perspective projection always carries m32 = -1 (column-major data[11]),
    // matching the reference frustum() formula's fourth-row sentinel.
    EXPECT_NEAR(u.getProjectionMatrix32().data[11], -1.0, 1e-5);
}

// bindProjectionMatrix uploads a mat4.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, BindProjectionMatrix)
TEST(FrustumUniformsTest, BindProjectionMatrix)
{
    FrustumUniforms u;
    Frustum f = makeBoxFrustum();
    u.changeFrustum(f, 0.0, false);

    UniformHandle h;
    u.bindProjectionMatrix(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::Mat4);
}

// bindUpVector uploads a vec3.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, BindUpVector)
TEST(FrustumUniformsTest, BindUpVector)
{
    FrustumUniforms u;
    UniformHandle h;
    u.bindUpVector(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::Vec3);
}

// changeProjectionMatrix replaces the projection and refreshes the float32 copy.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, ChangeProjectionMatrix)
TEST(FrustumUniformsTest, ChangeProjectionMatrix)
{
    FrustumUniforms u;
    Matrix4d m = Matrix4d::CreateRowValues(
        2, 0, 0, 0,
        0, 3, 0, 0,
        0, 0, 4, 0,
        0, 0, 0, 5);
    u.changeProjectionMatrix(m);

    // Column-major: data[0]=m.at(0,0)=2, data[5]=m.at(1,1)=3, data[10]=4, data[15]=5.
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32().data[0], 2.0f);
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32().data[5], 3.0f);
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32().data[10], 4.0f);
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32().data[15], 5.0f);
}

// Camera-view lifecycle callback fires on the perspective path.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/FrustumUniforms.test.ts
//              TEST(FrustumUniformsTest, CameraViewCallbackFires)
TEST(FrustumUniformsTest, CameraViewCallbackFires)
{
    static int fireCount = 0;
    FrameLifecycle::setOnChangeCameraView([](float const*, float const*, float const*, float const*) {
        ++fireCount;
    });

    int before = fireCount;
    FrustumUniforms u;
    Frustum f = makeBoxFrustum();
    u.changeFrustum(f, 0.5, true);  // perspective -> fires camera view event
    EXPECT_GT(fireCount, before);

    FrameLifecycle::setOnChangeCameraView(nullptr);
}
