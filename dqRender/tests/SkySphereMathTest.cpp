// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — SkySphere worldPos / u_worldEye math unit tests
// Authored: no reference test exists in itwinjs for SkySphereViewportQuadGeometry's
//           per-frame worldPos/u_worldEye derivation (itwinjs tests the live WebGL
//           sky render, not the CPU-side corner/eye math). Values verified by hand
//           against the itwinjs formulas (CachedGeometry.ts:583-596 non-globe
//           worldPos + SkySphere.ts:251-264 ortho u_worldEye).
#include "render/ViewportQuadGeometry.h"

#include <dqCommon/Frustum.h>
#include <dqGeom/Point3d.h>

#include <gtest/gtest.h>

using namespace dqRender;

// Build a box frustum with explicit Rear (far) and Front (near) faces.
static dqCommon::Frustum makeBoxFrustum(double minX, double maxX,
                                        double minY, double maxY,
                                        double rearZ, double frontZ)
{
    dqCommon::Frustum f;
    f.points[0] = dqGeom::Point3d::From(minX, minY, rearZ);   // LeftBottomRear
    f.points[1] = dqGeom::Point3d::From(maxX, minY, rearZ);   // RightBottomRear
    f.points[2] = dqGeom::Point3d::From(minX, maxY, rearZ);   // LeftTopRear
    f.points[3] = dqGeom::Point3d::From(maxX, maxY, rearZ);   // RightTopRear
    f.points[4] = dqGeom::Point3d::From(minX, minY, frontZ);  // LeftBottomFront
    f.points[5] = dqGeom::Point3d::From(maxX, minY, frontZ);  // RightBottomFront
    f.points[6] = dqGeom::Point3d::From(minX, maxY, frontZ);  // LeftTopFront
    f.points[7] = dqGeom::Point3d::From(maxX, maxY, frontZ);  // RightTopFront
    return f;
}

// Authored: known box → 4 mid-depth world corners + ortho pseudo-camera worldEye.
// Box [-5,5]², rear z=10, front z=0:
//   worldPos = mid-depth corners at z=5; worldEye.z = 10 + (-10)·zScale.
TEST(SkySphereMathTest, WorldPosMidDepthAndOrthoEye)
{
    auto const f = makeBoxFrustum(/*x*/ -5.0, 5.0, /*y*/ -5.0, 5.0, /*rearZ*/ 10.0, /*frontZ*/ 0.0);
    float worldPos[12] = {};
    float worldEye[3] = {};
    ComputeSkySphereWorldPosAndEye(f, worldPos, worldEye);

    // 4 mid-depth corners (LeftBottom, RightBottom, RightTop, LeftTop) at z = (10+0)/2 = 5.
    EXPECT_NEAR(worldPos[0], -5.0, 1e-4);   // LeftBottom.x
    EXPECT_NEAR(worldPos[1], -5.0, 1e-4);   // LeftBottom.y
    EXPECT_NEAR(worldPos[2], 5.0, 1e-4);    // LeftBottom.z (mid-depth)
    EXPECT_NEAR(worldPos[3], 5.0, 1e-4);    // RightBottom.x
    EXPECT_NEAR(worldPos[5], 5.0, 1e-4);    // RightBottom.z
    EXPECT_NEAR(worldPos[6], 5.0, 1e-4);    // RightTop.x
    EXPECT_NEAR(worldPos[7], 5.0, 1e-4);    // RightTop.y
    EXPECT_NEAR(worldPos[8], 5.0, 1e-4);    // RightTop.z
    EXPECT_NEAR(worldPos[9], -5.0, 1e-4);   // LeftTop.x
    EXPECT_NEAR(worldPos[11], 5.0, 1e-4);   // LeftTop.z

    // worldEye = rearCenter + delta·zScale; rearCenter=(0,0,10), delta=(0,0,-10).
    // Reference formula quirk (SkySphere.ts:255): focalLength = diagonal /
    // (2·Math.atan(22.5°)) — Math.atan, NOT Math.tan (ported verbatim; the reference
    // is the spec even where the trig looks odd).
    //   diagonal=√200≈14.1421; atan(0.3926991)≈0.3741945;
    //   focalLength=14.1421/(2·0.3741945)≈18.8967; zScale≈1.88967;
    //   worldEye.z = 10 + (-10)·1.88967 ≈ -8.8967.
    EXPECT_NEAR(worldEye[0], 0.0, 1e-3);
    EXPECT_NEAR(worldEye[1], 0.0, 1e-3);
    EXPECT_NEAR(worldEye[2], -8.8967, 2e-2);
}

// Authored: worldEye is at or beyond the front (near) plane — zScale ≥ 1.000001
// guarantees the eye is not inside the frustum (SkySphere.ts:624-625 clamp).
TEST(SkySphereMathTest, WorldEyeNotInsideFrustum)
{
    // Small frustum: diagonal=√8≈2.828, depth=5 → zScale clamps to 1.000001.
    auto const f = makeBoxFrustum(-1.0, 1.0, -1.0, 1.0, /*rearZ*/ 5.0, /*frontZ*/ 0.0);
    float worldPos[12] = {};
    float worldEye[3] = {};
    ComputeSkySphereWorldPosAndEye(f, worldPos, worldEye);

    // mid-depth z = (5+0)/2 = 2.5; eye.z = 5 + (-5)·1.000001 ≈ 0 (at the front plane).
    EXPECT_NEAR(worldPos[2], 2.5, 1e-4);
    EXPECT_LE(worldEye[2], 0.0 + 1e-3);  // eye at/beyond front (near) plane, not inside
}

// Authored: no reference test exists for SkySphereViewportQuadGeometry's gradient
//           color/exponent unpacking (itwinjs tests the live WebGL render, not the
//           CPU-side ctor). Locks the faithful 2-color/4-color branching ported from
//           CachedGeometry.ts:686-716 (audit D9.1): the default SkyGradient is
//           twoColor=false → 4-color, so all four colors + the exponents must flow
//           through (earlier the ctor forced the 2-color path unconditionally).
TEST(SkySphereMathTest, GradientFourColorUnpacksAllFourColors)
{
    RenderSkyGradientParams params;  // default SkyGradient: twoColor=false → 4-color
    auto* geom = SkySphereViewportQuadGeometry::createGeometry(params);
    ASSERT_NE(geom, nullptr);

    // CachedGeometry.ts:707-709 — typeAndExponents = {1.0, skyExponent, groundExponent}.
    auto const& te = geom->getTypeAndExponents();
    EXPECT_FLOAT_EQ(te[0], 1.0f);
    EXPECT_NEAR(te[1], static_cast<float>(params.gradient.skyExponent), 1e-6);
    EXPECT_NEAR(te[2], static_cast<float>(params.gradient.groundExponent), 1e-6);

    // All four colors populated from the SkyGradient defaults (CachedGeometry.ts:689-694, 710-715).
    auto const& colors = geom->getColors();
    auto sum = [](std::array<float, 3> const& c) { return c[0] + c[1] + c[2]; };
    EXPECT_GT(sum(colors.zenith), 0.0f);
    EXPECT_GT(sum(colors.sky), 0.0f);
    EXPECT_GT(sum(colors.ground), 0.0f);
    EXPECT_GT(sum(colors.nadir), 0.0f);

    delete geom;
}

// Authored: companion to GradientFourColorUnpacksAllFourColors — locks the 2-color
//           branch (CachedGeometry.ts:696-705): typeAndExponents[0]=-1.0 and sky/ground
//           zeroed, zenith/nadir still populated.
TEST(SkySphereMathTest, GradientTwoColorZeroesSkyAndGround)
{
    RenderSkyGradientParams params;
    params.gradient.twoColor = true;
    auto* geom = SkySphereViewportQuadGeometry::createGeometry(params);
    ASSERT_NE(geom, nullptr);

    EXPECT_FLOAT_EQ(geom->getTypeAndExponents()[0], -1.0f);  // CachedGeometry.ts:697

    auto const& colors = geom->getColors();
    auto sum = [](std::array<float, 3> const& c) { return c[0] + c[1] + c[2]; };
    EXPECT_EQ(sum(colors.sky), 0.0f);      // CachedGeometry.ts:700-702
    EXPECT_EQ(sum(colors.ground), 0.0f);   // CachedGeometry.ts:703-705
    EXPECT_GT(sum(colors.zenith), 0.0f);   // zenith/nadir still populated

    delete geom;
}
