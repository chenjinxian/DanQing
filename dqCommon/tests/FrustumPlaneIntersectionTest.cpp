// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/common/src/Frustum.ts:317-329 (getIntersectionWithPlane)
// DanQing dqCommon - Frustum.getIntersectionWithPlane unit tests
#include "dqCommon/Frustum.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;

// Ported from: Frustum.getIntersectionWithPlane (Frustum.ts:317-329). An AABB frustum
// (fromRange) cut by a z=5 plane yields the z=5 cross-section rectangle.
TEST(FrustumPlaneIntersection, AabbFrustumIntersectsPlane)
{
    Frustum const f = Frustum::fromRange(Range3d(-1, -1, 0, 1, 1, 10));
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 5), Vector3d::UnitZ());
    auto poly = f.GetIntersectionWithPlane(*plane);
    ASSERT_TRUE(poly.has_value());
    EXPECT_GE(poly->size(), 4u);
    for (auto const& p : *poly) {
        EXPECT_NEAR(p.z, 5.0, 1e-9);
        EXPECT_GE(p.x, -1.0 - 1e-9);
        EXPECT_LE(p.x, 1.0 + 1e-9);
        EXPECT_GE(p.y, -1.0 - 1e-9);
        EXPECT_LE(p.y, 1.0 + 1e-9);
    }
}

// Ported from: Frustum.getIntersectionWithPlane - plane above the frustum -> nullopt.
TEST(FrustumPlaneIntersection, PlaneMissesFrustum)
{
    Frustum const f = Frustum::fromRange(Range3d(-1, -1, 0, 1, 1, 10));
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 100), Vector3d::UnitZ());
    auto poly = f.GetIntersectionWithPlane(*plane);
    EXPECT_FALSE(poly.has_value());
}

// Ported from: Frustum.getIntersectionWithPlane - plane coincident with the frustum's
// bottom face yields that face's rectangle.
TEST(FrustumPlaneIntersection, PlaneOnBottomFace)
{
    Frustum const f = Frustum::fromRange(Range3d(-2, -2, 0, 2, 2, 8));
    auto plane = Plane3dByOriginAndUnitNormal::create(Point3d::From(0, 0, 0), Vector3d::UnitZ());
    auto poly = f.GetIntersectionWithPlane(*plane);
    ASSERT_TRUE(poly.has_value());
    EXPECT_GE(poly->size(), 4u);
    for (auto const& p : *poly)
        EXPECT_NEAR(p.z, 0.0, 1e-9);
}
