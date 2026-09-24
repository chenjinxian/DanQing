// SPDX-License-Identifier: Apache-2.0
// dqGeom tests — Range3d, Range2d, Range1d
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
#include <gtest/gtest.h>

#include <dqGeom/Range3d.h>

using namespace dqGeom;

// ============================================================================
// Range3d tests
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
// ============================================================================

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, DefaultIsNull) {
    Range3d range;
    EXPECT_TRUE(range.isNull());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, CreateNull) {
    auto range = Range3d::CreateNull();
    EXPECT_TRUE(range.isNull());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, CreateXYZ) {
    auto range = Range3d::CreateXYZ(1.0, 2.0, 3.0);
    EXPECT_FALSE(range.isNull());
    EXPECT_TRUE(range.IsSinglePoint());
    EXPECT_EQ(range.low.x, 1.0);
    EXPECT_EQ(range.low.y, 2.0);
    EXPECT_EQ(range.low.z, 3.0);
    EXPECT_EQ(range.high.x, 1.0);
    EXPECT_EQ(range.high.y, 2.0);
    EXPECT_EQ(range.high.z, 3.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, CreateXYZXYZ) {
    auto range = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    EXPECT_FALSE(range.isNull());
    EXPECT_EQ(range.low.x, 1.0);
    EXPECT_EQ(range.low.y, 2.0);
    EXPECT_EQ(range.low.z, 3.0);
    EXPECT_EQ(range.high.x, 4.0);
    EXPECT_EQ(range.high.y, 5.0);
    EXPECT_EQ(range.high.z, 6.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, CreateXYZXYZAutoCorrect) {
    // Swapped coordinates should be auto-corrected
    auto range = Range3d::CreateXYZXYZ(4.0, 5.0, 6.0, 1.0, 2.0, 3.0);
    EXPECT_FALSE(range.isNull());
    EXPECT_EQ(range.low.x, 1.0);
    EXPECT_EQ(range.high.x, 4.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, SetNull) {
    Range3d range(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    EXPECT_FALSE(range.isNull());

    range.SetNull();
    EXPECT_TRUE(range.isNull());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, SetFrom) {
    auto range1 = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    Range3d range2;
    range2.SetFrom(range1);

    EXPECT_EQ(range2.low.x, 1.0);
    EXPECT_EQ(range2.high.x, 4.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, clone) {
    auto range1 = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    auto range2 = range1.clone();

    EXPECT_EQ(range2.low.x, 1.0);
    EXPECT_EQ(range2.high.x, 4.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, IsAlmostEqual) {
    auto range1 = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    auto range2 = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    auto range3 = Range3d::CreateXYZXYZ(1.1, 2.0, 3.0, 4.0, 5.0, 6.0);

    EXPECT_TRUE(range1.IsAlmostEqual(range2));
    EXPECT_FALSE(range1.IsAlmostEqual(range3));
    EXPECT_TRUE(range1.IsAlmostEqual(range3, 0.2));  // With tolerance
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, Center) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    auto center = range.Center();

    EXPECT_NEAR(center.x, 5.0, 1e-10);
    EXPECT_NEAR(center.y, 10.0, 1e-10);
    EXPECT_NEAR(center.z, 15.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, Diagonal) {
    auto range = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 6.0, 9.0);
    auto diag = range.Diagonal();

    EXPECT_NEAR(diag.x, 3.0, 1e-10);
    EXPECT_NEAR(diag.y, 4.0, 1e-10);
    EXPECT_NEAR(diag.z, 6.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, Lengths) {
    auto range = Range3d::CreateXYZXYZ(1.0, 2.0, 3.0, 4.0, 6.0, 9.0);

    EXPECT_NEAR(range.XLength(), 3.0, 1e-10);
    EXPECT_NEAR(range.YLength(), 4.0, 1e-10);
    EXPECT_NEAR(range.ZLength(), 6.0, 1e-10);
    EXPECT_NEAR(range.MaxLength(), 6.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ContainsPoint) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);

    EXPECT_TRUE(range.ContainsPoint(Point3d::From(5.0, 5.0, 5.0)));
    EXPECT_TRUE(range.ContainsPoint(Point3d::From(0.0, 0.0, 0.0)));
    EXPECT_TRUE(range.ContainsPoint(Point3d::From(10.0, 10.0, 10.0)));
    EXPECT_FALSE(range.ContainsPoint(Point3d::From(11.0, 5.0, 5.0)));
    EXPECT_FALSE(range.ContainsPoint(Point3d::From(-1.0, 5.0, 5.0)));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ContainsXYZ) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);

    EXPECT_TRUE(range.ContainsXYZ(5.0, 5.0, 5.0));
    EXPECT_FALSE(range.ContainsXYZ(11.0, 5.0, 5.0));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ContainsRange) {
    auto range1 = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    auto range2 = Range3d::CreateXYZXYZ(2.0, 2.0, 2.0, 8.0, 8.0, 8.0);
    auto range3 = Range3d::CreateXYZXYZ(2.0, 2.0, 2.0, 12.0, 8.0, 8.0);

    EXPECT_TRUE(range1.ContainsRange(range2));
    EXPECT_FALSE(range1.ContainsRange(range3));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, IntersectsRange) {
    auto range1 = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    auto range2 = Range3d::CreateXYZXYZ(5.0, 5.0, 5.0, 15.0, 15.0, 15.0);
    auto range3 = Range3d::CreateXYZXYZ(11.0, 11.0, 11.0, 20.0, 20.0, 20.0);

    EXPECT_TRUE(range1.IntersectsRange(range2));
    EXPECT_FALSE(range1.IntersectsRange(range3));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, DistanceToPoint) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);

    // Inside
    EXPECT_NEAR(range.DistanceToPoint(Point3d::From(5.0, 5.0, 5.0)), 0.0, 1e-10);

    // Outside
    double dist = range.DistanceToPoint(Point3d::From(15.0, 5.0, 5.0));
    EXPECT_NEAR(dist, 5.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ExtendPoint) {
    Range3d range;
    EXPECT_TRUE(range.isNull());

    range.ExtendPoint(Point3d::From(5.0, 5.0, 5.0));
    EXPECT_FALSE(range.isNull());
    EXPECT_TRUE(range.IsSinglePoint());

    range.ExtendPoint(Point3d::From(10.0, 10.0, 10.0));
    EXPECT_NEAR(range.low.x, 5.0, 1e-10);
    EXPECT_NEAR(range.high.x, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ExtendXYZ) {
    Range3d range;
    range.ExtendXYZ(1.0, 2.0, 3.0);
    range.ExtendXYZ(4.0, 5.0, 6.0);

    EXPECT_NEAR(range.low.x, 1.0, 1e-10);
    EXPECT_NEAR(range.high.x, 4.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ExtendRange) {
    auto range1 = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 5.0, 5.0, 5.0);
    auto range2 = Range3d::CreateXYZXYZ(3.0, 3.0, 3.0, 10.0, 10.0, 10.0);

    range1.ExtendRange(range2);
    EXPECT_NEAR(range1.low.x, 0.0, 1e-10);
    EXPECT_NEAR(range1.high.x, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, Intersect) {
    auto range1 = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    auto range2 = Range3d::CreateXYZXYZ(5.0, 5.0, 5.0, 15.0, 15.0, 15.0);

    auto intersection = range1.Intersect(range2);
    EXPECT_FALSE(intersection.isNull());
    EXPECT_NEAR(intersection.low.x, 5.0, 1e-10);
    EXPECT_NEAR(intersection.high.x, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, IntersectNoOverlap) {
    auto range1 = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 5.0, 5.0, 5.0);
    auto range2 = Range3d::CreateXYZXYZ(6.0, 6.0, 6.0, 10.0, 10.0, 10.0);

    auto intersection = range1.Intersect(range2);
    EXPECT_TRUE(intersection.isNull());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, Union) {
    auto range1 = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 5.0, 5.0, 5.0);
    auto range2 = Range3d::CreateXYZXYZ(3.0, 3.0, 3.0, 10.0, 10.0, 10.0);

    auto unionRange = range1.Union(range2);
    EXPECT_NEAR(unionRange.low.x, 0.0, 1e-10);
    EXPECT_NEAR(unionRange.high.x, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, FractionToPoint) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);

    auto p = range.FractionToPoint(0.5, 0.5, 0.5);
    EXPECT_NEAR(p.x, 5.0, 1e-10);
    EXPECT_NEAR(p.y, 10.0, 1e-10);
    EXPECT_NEAR(p.z, 15.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, Volume) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 2.0, 3.0, 4.0);
    EXPECT_NEAR(range.Volume(), 24.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, CloneTranslated) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    auto translated = range.CloneTranslated(Vector3d::From(5.0, 5.0, 5.0));

    EXPECT_NEAR(translated.low.x, 5.0, 1e-10);
    EXPECT_NEAR(translated.high.x, 15.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range3dTest, ExtendByDistance) {
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    range.ExtendByDistance(2.0);

    EXPECT_NEAR(range.low.x, -2.0, 1e-10);
    EXPECT_NEAR(range.high.x, 12.0, 1e-10);
}

// ============================================================================
// Range2d tests
// ============================================================================

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range2dTest, DefaultIsNull) {
    Range2d range;
    EXPECT_TRUE(range.isNull());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range2dTest, CreateXYXY) {
    auto range = Range2d::CreateXYXY(1.0, 2.0, 4.0, 5.0);
    EXPECT_FALSE(range.isNull());
    EXPECT_EQ(range.low.x, 1.0);
    EXPECT_EQ(range.high.x, 4.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range2dTest, ContainsPoint) {
    auto range = Range2d::CreateXYXY(0.0, 0.0, 10.0, 10.0);

    EXPECT_TRUE(range.ContainsPoint(Point3d::From(5.0, 5.0, 0.0)));
    EXPECT_FALSE(range.ContainsPoint(Point3d::From(11.0, 5.0, 0.0)));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range2dTest, IntersectsRange) {
    auto range1 = Range2d::CreateXYXY(0.0, 0.0, 10.0, 10.0);
    auto range2 = Range2d::CreateXYXY(5.0, 5.0, 15.0, 15.0);
    auto range3 = Range2d::CreateXYXY(11.0, 11.0, 20.0, 20.0);

    EXPECT_TRUE(range1.IntersectsRange(range2));
    EXPECT_FALSE(range1.IntersectsRange(range3));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range2dTest, ExtendAndUnion) {
    Range2d range1;
    range1.ExtendXY(0.0, 0.0);
    range1.ExtendXY(5.0, 5.0);

    Range2d range2;
    range2.ExtendXY(3.0, 3.0);
    range2.ExtendXY(10.0, 10.0);

    auto unionRange = range1.Union(range2);
    EXPECT_NEAR(unionRange.low.x, 0.0, 1e-10);
    EXPECT_NEAR(unionRange.high.x, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range2dTest, Area) {
    auto range = Range2d::CreateXYXY(0.0, 0.0, 3.0, 4.0);
    EXPECT_NEAR(range.Area(), 12.0, 1e-10);
}

// ============================================================================
// Range1d tests
// ============================================================================

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, DefaultIsNull) {
    Range1d range;
    EXPECT_TRUE(range.isNull());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, CreateXX) {
    auto range = Range1d::CreateXX(1.0, 5.0);
    EXPECT_FALSE(range.isNull());
    EXPECT_EQ(range.low, 1.0);
    EXPECT_EQ(range.high, 5.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Contains) {
    auto range = Range1d::CreateXX(0.0, 10.0);

    EXPECT_TRUE(range.Contains(5.0));
    EXPECT_TRUE(range.Contains(0.0));
    EXPECT_TRUE(range.Contains(10.0));
    EXPECT_FALSE(range.Contains(11.0));
    EXPECT_FALSE(range.Contains(-1.0));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, IntersectsRange) {
    auto range1 = Range1d::CreateXX(0.0, 10.0);
    auto range2 = Range1d::CreateXX(5.0, 15.0);
    auto range3 = Range1d::CreateXX(11.0, 20.0);

    EXPECT_TRUE(range1.IntersectsRange(range2));
    EXPECT_FALSE(range1.IntersectsRange(range3));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Extend) {
    Range1d range;
    range.Extend(5.0);
    range.Extend(10.0);
    range.Extend(3.0);

    EXPECT_NEAR(range.low, 3.0, 1e-10);
    EXPECT_NEAR(range.high, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Intersect) {
    auto range1 = Range1d::CreateXX(0.0, 10.0);
    auto range2 = Range1d::CreateXX(5.0, 15.0);

    auto intersection = range1.Intersect(range2);
    EXPECT_FALSE(intersection.isNull());
    EXPECT_NEAR(intersection.low, 5.0, 1e-10);
    EXPECT_NEAR(intersection.high, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Union) {
    auto range1 = Range1d::CreateXX(0.0, 5.0);
    auto range2 = Range1d::CreateXX(3.0, 10.0);

    auto unionRange = range1.Union(range2);
    EXPECT_NEAR(unionRange.low, 0.0, 1e-10);
    EXPECT_NEAR(unionRange.high, 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Fraction) {
    auto range = Range1d::CreateXX(0.0, 10.0);

    EXPECT_NEAR(range.Fraction(5.0), 0.5, 1e-10);
    EXPECT_NEAR(range.Fraction(0.0), 0.0, 1e-10);
    EXPECT_NEAR(range.Fraction(10.0), 1.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, FractionToPoint) {
    auto range = Range1d::CreateXX(0.0, 10.0);

    EXPECT_NEAR(range.FractionToPoint(0.5), 5.0, 1e-10);
    EXPECT_NEAR(range.FractionToPoint(0.0), 0.0, 1e-10);
    EXPECT_NEAR(range.FractionToPoint(1.0), 10.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Length) {
    auto range = Range1d::CreateXX(2.0, 8.0);
    EXPECT_NEAR(range.Length(), 6.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Range.h
TEST(Range1dTest, Center) {
    auto range = Range1d::CreateXX(2.0, 8.0);
    EXPECT_NEAR(range.Center(), 5.0, 1e-10);
}

// Authored: no reference unit test exists in itwinjs core-geometry for Range3d.extendArray /
// isAlmostZeroZ (faithful ports of Range.ts methods consumed by GeometryAccumulator.addLineString/
// addPointString + toMeshBuilderMap). Behaviors defined by core/geometry/src/geometry3d/Range.ts.
TEST(Range3dTest, ExtendArrayAndIsAlmostZeroZ) {
    Range3d r = Range3d::CreateNull();
    r.extendArray({ Point3d::From(1, 2, 3), Point3d::From(4, 5, 6) });
    EXPECT_NEAR(r.XLength(), 3.0, 1e-10);
    EXPECT_NEAR(r.YLength(), 3.0, 1e-10);
    EXPECT_NEAR(r.ZLength(), 3.0, 1e-10);
    EXPECT_FALSE(r.isAlmostZeroZ());

    Range3d flat = Range3d::CreateNull();
    flat.extendArray({ Point3d::From(0, 0, 5), Point3d::From(2, 3, 5) });
    EXPECT_TRUE(flat.isAlmostZeroZ());
}
