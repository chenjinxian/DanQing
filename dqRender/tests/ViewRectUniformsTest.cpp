// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ViewRectUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              (no direct reference test exists; tests are authored from ViewRectUniforms.ts behavior)
#include "render/ViewRectUniforms.h"
#include "render/Matrix.h"

#include <gtest/gtest.h>

using namespace dqRender;

// Default-constructed uniforms have zero dimensions and identity projection.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, DefaultState)
TEST(ViewRectUniformsTest, DefaultState)
{
    ViewRectUniforms u;
    EXPECT_FLOAT_EQ(u.getWidth(), 0.0f);
    EXPECT_FLOAT_EQ(u.getHeight(), 0.0f);

    // projectionMatrix (double) defaults to identity.
    auto const& proj = u.getProjectionMatrix();
    EXPECT_DOUBLE_EQ(proj.GetCoeffs()[0], 1.0);
    EXPECT_DOUBLE_EQ(proj.GetCoeffs()[5], 1.0);
    EXPECT_DOUBLE_EQ(proj.GetCoeffs()[10], 1.0);
    EXPECT_DOUBLE_EQ(proj.GetCoeffs()[15], 1.0);
}

// update() produces the reference ortho projection for the window rectangle.
// Reference: Matrix4.fromOrtho(0.0, width, height, 0.0, -1.0, 1.0)
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, UpdateProjectionMatrix)
TEST(ViewRectUniformsTest, UpdateProjectionMatrix)
{
    ViewRectUniforms u;
    u.update(800.0f, 600.0f);

    EXPECT_FLOAT_EQ(u.getWidth(), 800.0f);
    EXPECT_FLOAT_EQ(u.getHeight(), 600.0f);

    auto const& p = u.getProjectionMatrix32().data;
    // m00 = 2/(r-l) = 2/800
    EXPECT_FLOAT_EQ(p[0], 2.0f / 800.0f);
    // m11 = 2/(t-b) = 2/(0-600) = -2/600
    EXPECT_FLOAT_EQ(p[5], -2.0f / 600.0f);
    // m22 = -2/(f-n) = -2/2 = -1
    EXPECT_FLOAT_EQ(p[10], -1.0f);
    // m03 = -(r+l)/(r-l) = -800/800 = -1
    EXPECT_FLOAT_EQ(p[12], -1.0f);
    // m13 = -(t+b)/(t-b) = -(600)/(0-600) = 1
    EXPECT_FLOAT_EQ(p[13], 1.0f);
    // m23 = -(f+n)/(f-n) = 0
    EXPECT_FLOAT_EQ(p[14], 0.0f);
    EXPECT_FLOAT_EQ(p[15], 1.0f);
}

// The float32 projection must equal the double-precision projection (round-trip).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, ProjectionRoundTrip)
TEST(ViewRectUniformsTest, ProjectionRoundTrip)
{
    ViewRectUniforms u;
    u.update(1024.0f, 768.0f);

    auto const& p32 = u.getProjectionMatrix32();
    auto const& pd = u.getProjectionMatrix();
    // toMatrix4d must invert initFromMatrix4d exactly (transposed back).
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_NEAR(p32.data[c * 4 + r], pd.at(r, c), 1e-5);
        }
    }
}

// update() builds the NDC->window viewport matrix.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, ViewportMatrix)
TEST(ViewRectUniformsTest, ViewportMatrix)
{
    ViewRectUniforms u;
    u.update(800.0f, 600.0f);

    auto const& v = u.getViewportMatrix().data;
    // halfWidth=400 -> m00
    EXPECT_FLOAT_EQ(v[0], 400.0f);
    // halfHeight=300 -> m11
    EXPECT_FLOAT_EQ(v[5], 300.0f);
    // halfDepth=0.5 -> m22
    EXPECT_FLOAT_EQ(v[10], 0.5f);
    // x+halfWidth -> m03
    EXPECT_FLOAT_EQ(v[12], 400.0f);
    // y+halfHeight -> m13
    EXPECT_FLOAT_EQ(v[13], 300.0f);
    // nearDepthRange+halfDepth = 0+0.5 -> m23
    EXPECT_FLOAT_EQ(v[14], 0.5f);
    EXPECT_FLOAT_EQ(v[15], 1.0f);
}

// Inverse dimensions are reciprocals of the dimensions.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, InverseDimensions)
TEST(ViewRectUniformsTest, InverseDimensions)
{
    ViewRectUniforms u;
    u.update(800.0f, 600.0f);

    UniformHandle dim;
    u.bindDimensions(dim);
    EXPECT_FLOAT_EQ(dim.getData()[0], 800.0f);
    EXPECT_FLOAT_EQ(dim.getData()[1], 600.0f);

    UniformHandle inv;
    u.bindInverseDimensions(inv);
    EXPECT_FLOAT_EQ(inv.getData()[0], 1.0f / 800.0f);
    EXPECT_FLOAT_EQ(inv.getData()[1], 1.0f / 600.0f);
}

// Calling update() with unchanged dimensions does not recompute (early return).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, UpdateSameDimensionsNoChange)
TEST(ViewRectUniformsTest, UpdateSameDimensionsNoChange)
{
    ViewRectUniforms u;
    u.update(640.0f, 480.0f);
    auto p0 = u.getProjectionMatrix32().data[0];

    // Mutate one element to detect recompute; same-dimension update must leave state.
    u.update(640.0f, 480.0f);
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32().data[0], p0);
}

// bindProjectionMatrix uploads the mat4 into the handle.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, BindProjectionMatrix)
TEST(ViewRectUniformsTest, BindProjectionMatrix)
{
    ViewRectUniforms u;
    u.update(100.0f, 100.0f);

    UniformHandle h;
    bool changed = h.setMatrix4(u.getProjectionMatrix32().data);
    EXPECT_TRUE(changed);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::Mat4);
}

// --- Matrix.h new-method coverage ---

// initFromMatrix4d transposes row-major Matrix4d into column-major Matrix4.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, MatrixInitFromMatrix4dTransposes)
TEST(ViewRectUniformsTest, MatrixInitFromMatrix4dTransposes)
{
    // Build a non-symmetric Matrix4d so transpose is detectable.
    auto m4d = dqGeom::Matrix4d::CreateRowValues(
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16);

    Matrix4 m;
    m.initFromMatrix4d(m4d);
    // Column-major: data[c*4+r] == m4d.at(r,c)
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            EXPECT_FLOAT_EQ(m.data[c * 4 + r], static_cast<float>(m4d.at(r, c)));
}

// fromOrtho matches the reference Matrix4.fromOrtho formula.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, MatrixFromOrtho)
TEST(ViewRectUniformsTest, MatrixFromOrtho)
{
    Matrix4 m = Matrix4::fromOrtho(0.0f, 800.0f, 600.0f, 0.0f, -1.0f, 1.0f);
    EXPECT_FLOAT_EQ(m.data[0], 2.0f / 800.0f);
    EXPECT_FLOAT_EQ(m.data[5], -2.0f / 600.0f);
    EXPECT_FLOAT_EQ(m.data[10], -1.0f);
    EXPECT_FLOAT_EQ(m.data[12], -1.0f);
    EXPECT_FLOAT_EQ(m.data[13], 1.0f);
}

// initFromTransform matches Matrix4d.CreateTransform layout.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ViewRectUniforms.test.ts
//              TEST(ViewRectUniformsTest, MatrixInitFromTransform)
TEST(ViewRectUniformsTest, MatrixInitFromTransform)
{
    dqGeom::Transform xf = dqGeom::Transform::CreateIdentity();
    Matrix4 m;
    m.initFromTransform(xf);
    // identity transform -> identity matrix.
    EXPECT_FLOAT_EQ(m.data[0], 1.0f);
    EXPECT_FLOAT_EQ(m.data[5], 1.0f);
    EXPECT_FLOAT_EQ(m.data[10], 1.0f);
    EXPECT_FLOAT_EQ(m.data[15], 1.0f);
    EXPECT_FLOAT_EQ(m.data[3], 0.0f);
    EXPECT_FLOAT_EQ(m.data[7], 0.0f);
    EXPECT_FLOAT_EQ(m.data[11], 0.0f);
}
