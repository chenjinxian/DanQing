// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/test/clipping/ClipPlane.test.ts
// DanQing dqGeom - ClipPlane unit tests
#include "dqGeom/ClipPlane.h"
#include "dqGeom/Plane3dByOriginAndUnitNormal.h"

#include <gtest/gtest.h>

using namespace dqGeom;

// Ported from: itwinjs-core ClipPlane.createPlane / altitude (ClipPlane.ts:109-121, 354-358).
TEST(ClipPlaneTest, CreatePlaneFromOriginNormal)
{
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 5), Vector3d::UnitZ());
    ASSERT_TRUE(plane.has_value());
    ClipPlane const cp = ClipPlane::createPlane(*plane);
    // distance = normal · origin = (0,0,1)·(0,0,5) = 5.
    EXPECT_NEAR(cp.getDistanceFromOrigin(), 5.0, 1e-12);
    // altitude: on-plane = 0; +Z side (inside) positive; -Z side (outside) negative.
    EXPECT_NEAR(cp.altitude(Point3d::From(0, 0, 5)), 0.0, 1e-12);
    EXPECT_NEAR(cp.altitude(Point3d::From(0, 0, 10)), 5.0, 1e-12);
    EXPECT_NEAR(cp.altitude(Point3d::From(0, 0, 0)), -5.0, 1e-12);
    EXPECT_TRUE(cp.isPointOnOrInside(Point3d::From(0, 0, 10)));
    EXPECT_FALSE(cp.isPointOnOrInside(Point3d::From(0, 0, 0)));
}

// Ported from: itwinjs-core ClipPlane.createNormalAndDistance (ClipPlane.ts:126-140).
TEST(ClipPlaneTest, CreateNormalAndDistanceNormalizes)
{
    auto cp = ClipPlane::createNormalAndDistance(Vector3d::From(0, 0, 3), 2.0);
    ASSERT_TRUE(cp.has_value());
    EXPECT_NEAR(cp->getNormalRef().z, 1.0, 1e-12);
    EXPECT_NEAR(cp->getDistanceFromOrigin(), 2.0, 1e-12);
    // Zero normal -> nullopt.
    EXPECT_FALSE(ClipPlane::createNormalAndDistance(Vector3d::FromZero(), 0.0).has_value());
}

// Ported from: itwinjs-core ClipPlane.projectXYZToPlane (ClipPlane.ts:651-654).
TEST(ClipPlaneTest, ProjectPointToPlane)
{
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 5), Vector3d::UnitZ());
    ClipPlane const cp = ClipPlane::createPlane(*plane);
    Point3d const q = cp.projectPointToPlane(Point3d::From(1, 2, 10));
    EXPECT_NEAR(q.z, 5.0, 1e-12);
    EXPECT_NEAR(q.x, 1.0, 1e-12);
    EXPECT_NEAR(q.y, 2.0, 1e-12);
}

// Ported from: itwinjs-core ClipPlane.cloneNegated / negateInPlace.
TEST(ClipPlaneTest, CloneNegated)
{
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 5), Vector3d::UnitZ());
    ClipPlane const cp = ClipPlane::createPlane(*plane);
    ClipPlane const neg = cp.cloneNegated();
    EXPECT_NEAR(neg.getNormalRef().z, -1.0, 1e-12);
    EXPECT_NEAR(neg.getDistanceFromOrigin(), -5.0, 1e-12);
    // Negated plane: the +Z side is now outside.
    EXPECT_FALSE(neg.isPointOnOrInside(Point3d::From(0, 0, 10)));
    EXPECT_TRUE(neg.isPointOnOrInside(Point3d::From(0, 0, 0)));
}

// Ported from: itwinjs-core ClipPlane.intersectRange (ClipPlane.ts:598-619).
// Plane z=5 slicing a box [-1,1]x[-1,1]x[0,10] -> the z=5 cross-section rectangle.
TEST(ClipPlaneTest, IntersectRangeProducesCrossSection)
{
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 5), Vector3d::UnitZ());
    ClipPlane const cp = ClipPlane::createPlane(*plane);
    Range3d const range(-1, -1, 0, 1, 1, 10);
    auto poly = cp.intersectRange(range);
    ASSERT_TRUE(poly.has_value());
    EXPECT_EQ(poly->size(), 4u);
    for (auto const& p : *poly) {
        EXPECT_NEAR(p.z, 5.0, 1e-9);
        EXPECT_GE(p.x, -1.0 - 1e-9);
        EXPECT_LE(p.x, 1.0 + 1e-9);
        EXPECT_GE(p.y, -1.0 - 1e-9);
        EXPECT_LE(p.y, 1.0 + 1e-9);
    }
}

// Ported from: ClipPlane.intersectRange - plane above the range -> nullopt.
TEST(ClipPlaneTest, IntersectRangeMissesWhenDisjoint)
{
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 100), Vector3d::UnitZ());
    ClipPlane const cp = ClipPlane::createPlane(*plane);
    Range3d const range(-1, -1, 0, 1, 1, 10);
    auto poly = cp.intersectRange(range);
    EXPECT_FALSE(poly.has_value());
}

// Ported from: ClipPlane.intersectRange - plane coincident with a range face.
TEST(ClipPlaneTest, IntersectRangeOnBoundaryFace)
{
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 0), Vector3d::UnitZ());
    ClipPlane const cp = ClipPlane::createPlane(*plane);
    Range3d const range(-1, -1, 0, 1, 1, 10);
    auto poly = cp.intersectRange(range);
    ASSERT_TRUE(poly.has_value());
    EXPECT_EQ(poly->size(), 4u);
    for (auto const& p : *poly)
        EXPECT_NEAR(p.z, 0.0, 1e-9);
}
