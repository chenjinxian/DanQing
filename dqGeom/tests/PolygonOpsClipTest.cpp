// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/test/clipping/ClipPlanes.test.ts
//              (IndexedXYZCollectionPolygonOps.clipConvexPolygonInPlace /
//               intersectRangeConvexPolygonInPlace coverage)
// DanQing dqGeom - convex polygon clipping unit tests
#include "dqGeom/PolygonOps.h"

#include <gtest/gtest.h>

using namespace dqGeom;

// Ported from: PolygonOps.clipConvexPolygonInPlace (PolygonOps.ts:1639-1722).
// Clip a unit square by the halfspace x >= 1 (plane abc=(1,0,0), d=-1, altitude=x-1).
TEST(PolygonOpsClipTest, ClipSquareByHalfPlane)
{
    std::vector<Point3d> poly = {
        Point3d::From(0, 0, 0), Point3d::From(2, 0, 0),
        Point3d::From(2, 2, 0), Point3d::From(0, 2, 0),
    };
    int const crossings = PolygonOps::clipConvexPolygonInPlace(Vector3d::From(1, 0, 0), -1.0, poly);
    EXPECT_EQ(poly.size(), 4u);  // x>=1 slice of the square is a 4-gon
    // Every surviving vertex has x >= 1.
    for (auto const& p : poly)
        EXPECT_GE(p.x, 1.0 - 1e-9);
    // The two x==1 edge crossings were inserted.
    bool hasXeq1 = false;
    for (auto const& p : poly)
        if (std::abs(p.x - 1.0) < 1e-9) hasXeq1 = true;
    EXPECT_TRUE(hasXeq1);
    EXPECT_GT(crossings, 0);
}

// Ported from: PolygonOps.intersectRangeConvexPolygonInPlace (PolygonOps.ts:1805-1841).
// An oversized square clipped to the range [-1,1]^2 stays a 4-gon inside the range.
TEST(PolygonOpsClipTest, IntersectRangeClipsOversizedPolygon)
{
    std::vector<Point3d> poly = {
        Point3d::From(-5, -5, 0), Point3d::From(5, -5, 0),
        Point3d::From(5, 5, 0), Point3d::From(-5, 5, 0),
    };
    Range3d const range(-1, -1, -1, 1, 1, 1);
    bool const ok = PolygonOps::intersectRangeConvexPolygonInPlace(range, poly);
    EXPECT_TRUE(ok);
    EXPECT_EQ(poly.size(), 4u);
    for (auto const& p : poly) {
        EXPECT_GE(p.x, -1.0 - 1e-9);
        EXPECT_LE(p.x, 1.0 + 1e-9);
        EXPECT_GE(p.y, -1.0 - 1e-9);
        EXPECT_LE(p.y, 1.0 + 1e-9);
    }
}

// Ported from: PolygonOps.intersectRangeConvexPolygonInPlace - polygon entirely outside
// the range clips to empty.
TEST(PolygonOpsClipTest, IntersectRangeEmptyWhenDisjoint)
{
    std::vector<Point3d> poly = {
        Point3d::From(10, 10, 0), Point3d::From(12, 10, 0),
        Point3d::From(12, 12, 0), Point3d::From(10, 12, 0),
    };
    Range3d const range(-1, -1, -1, 1, 1, 1);
    bool const ok = PolygonOps::intersectRangeConvexPolygonInPlace(range, poly);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(poly.empty());
}
