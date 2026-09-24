// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/Plane3dByOriginAndUnitNormal.test.ts
// DanQing dqGeom — Plane3dByOriginAndUnitNormal unit tests
#include "dqGeom/Plane3dByOriginAndUnitNormal.h"

#include <gtest/gtest.h>

using namespace dqGeom;

// Ported from: itwinjs-core Plane3dByOriginAndUnitNormal.create / altitude / altitudeXYZ
//              (create normalizes; altitude is signed distance along the unit normal).
TEST(Plane3dByOriginAndUnitNormalTest, CreateNormalizesAndComputesAltitude)
{
    auto p = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 0), Vector3d::From(0, 0, 5));
    ASSERT_TRUE(p.has_value());
    // The non-unit normal (0,0,5) is normalized to (0,0,1).
    EXPECT_NEAR(p->normalZ(), 1.0, 1e-12);
    EXPECT_NEAR(p->normalX(), 0.0, 1e-12);
    EXPECT_NEAR(p->normalY(), 0.0, 1e-12);
    // altitude of (1,1,3) above the XY plane through the origin = 3.
    EXPECT_NEAR(p->altitude(Point3d::From(1, 1, 3)), 3.0, 1e-12);
    EXPECT_NEAR(p->altitudeXYZ(1, 1, 3), 3.0, 1e-12);
    // Below the plane is negative.
    EXPECT_NEAR(p->altitude(Point3d::From(0, 0, -2)), -2.0, 1e-12);
}

// Ported from: itwinjs-core Plane3dByOriginAndUnitNormal.create — zero normal -> nullopt.
TEST(Plane3dByOriginAndUnitNormalTest, CreateRejectsZeroNormal)
{
    auto p = Plane3dByOriginAndUnitNormal::create(Point3d::FromZero(), Vector3d::FromZero());
    EXPECT_FALSE(p.has_value());
}

// Ported from: itwinjs-core Plane3dByOriginAndUnitNormal.createXYPlane +
//              projectPointToPlane.
TEST(Plane3dByOriginAndUnitNormalTest, XYPlaneProjectsPoints)
{
    auto p = Plane3dByOriginAndUnitNormal::createXYPlane(Point3d::From(1, 2, 0));
    // project (1,2,5) onto the z=0 plane through (1,2,0) -> (1,2,0).
    Point3d const q = p.projectPointToPlane(Point3d::From(1, 2, 5));
    EXPECT_NEAR(q.x, 1.0, 1e-12);
    EXPECT_NEAR(q.y, 2.0, 1e-12);
    EXPECT_NEAR(q.z, 0.0, 1e-12);
}

// Ported from: itwinjs-core Plane3dByOriginAndUnitNormal.altitude through a translated
// origin: a plane through (1,1,1) with normal +Z has altitude z-1.
TEST(Plane3dByOriginAndUnitNormalTest, AltitudeThroughTranslatedOrigin)
{
    auto p = Plane3dByOriginAndUnitNormal::createXYPlane(Point3d::From(1, 1, 1));
    EXPECT_NEAR(p.altitude(Point3d::From(0, 0, 4)), 3.0, 1e-12);
    EXPECT_NEAR(p.altitude(Point3d::From(0, 0, 0)), -1.0, 1e-12);
}
