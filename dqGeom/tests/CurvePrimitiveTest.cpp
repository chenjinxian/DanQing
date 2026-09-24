// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — CurvePrimitive tests
// Ported from: itwinjs-core core/geometry/src/test/curve/CurvePrimitive.test.ts
//              imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
#include <gtest/gtest.h>
#include <vector>

#include <dqGeom/LineSegment3d.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Arc3d.h>

using namespace dqGeom;

// ---------------------------------------------------------------------------
// LineSegment3d tests
// ---------------------------------------------------------------------------
// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, CreateAndAccess)
TEST(LineSegment3dTest, CreateAndAccess)
{
    auto seg = LineSegment3d::create(Point3d::From(0, 0, 0), Point3d::From(1, 0, 0));
    EXPECT_EQ(seg->GetCurveType(), CurveType::LineSegment);
    EXPECT_TRUE(seg->Point0Ref().AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(seg->Point1Ref().AlmostEqual(Point3d::From(1, 0, 0)));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, FractionToPoint)
TEST(LineSegment3dTest, FractionToPoint)
{
    auto seg = LineSegment3d::create(Point3d::From(0, 0, 0), Point3d::From(10, 0, 0));

    Point3d p0 = seg->FractionToPoint(0.0);
    EXPECT_TRUE(p0.AlmostEqual(Point3d::From(0, 0, 0)));

    Point3d p1 = seg->FractionToPoint(1.0);
    EXPECT_TRUE(p1.AlmostEqual(Point3d::From(10, 0, 0)));

    Point3d pm = seg->FractionToPoint(0.5);
    EXPECT_TRUE(pm.AlmostEqual(Point3d::From(5, 0, 0)));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, FractionToPointAndDerivative)
TEST(LineSegment3dTest, FractionToPointAndDerivative)
{
    auto seg = LineSegment3d::create(Point3d::From(0, 0, 0), Point3d::From(10, 0, 0));
    Vector3d derivative;
    Point3d p = seg->FractionToPointAndDerivative(0.5, derivative);
    EXPECT_TRUE(p.AlmostEqual(Point3d::From(5, 0, 0)));
    EXPECT_TRUE(derivative.AlmostEqual(Vector3d::From(10, 0, 0)));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, Length)
TEST(LineSegment3dTest, Length)
{
    auto seg = LineSegment3d::create(Point3d::From(0, 0, 0), Point3d::From(3, 4, 0));
    EXPECT_NEAR(seg->QuickLength(), 5.0, 1.0e-10);
    EXPECT_NEAR(seg->CurveLength(), 5.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, clone)
TEST(LineSegment3dTest, clone)
{
    auto seg = LineSegment3d::create(Point3d::From(1, 2, 3), Point3d::From(4, 5, 6));
    auto clone = seg->clone();
    EXPECT_TRUE(clone.IsValid());
    EXPECT_TRUE(seg->IsAlmostEqual(*clone, 1.0e-10));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, TransformInPlace)
TEST(LineSegment3dTest, TransformInPlace)
{
    auto seg = LineSegment3d::create(Point3d::From(0, 0, 0), Point3d::From(1, 0, 0));
    Transform t = Transform::CreateTranslation(Vector3d::From(10, 20, 30));
    seg->TryTransformInPlace(t);
    EXPECT_TRUE(seg->Point0Ref().AlmostEqual(Point3d::From(10, 20, 30)));
    EXPECT_TRUE(seg->Point1Ref().AlmostEqual(Point3d::From(11, 20, 30)));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineSegment3dTest, Range)
TEST(LineSegment3dTest, Range)
{
    auto seg = LineSegment3d::create(Point3d::From(-1, -2, -3), Point3d::From(4, 5, 6));
    Range3d range = seg->Range();
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(-1, -2, -3)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(4, 5, 6)));
}

// ---------------------------------------------------------------------------
// LineString3d tests
// ---------------------------------------------------------------------------
// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineString3dTest, CreateAndAccess)
TEST(LineString3dTest, CreateAndAccess)
{
    std::vector<Point3d> pts = {Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(2, 0, 0)};
    auto ls = LineString3d::create(pts);
    EXPECT_EQ(ls->GetCurveType(), CurveType::LineString);
    EXPECT_EQ(ls->PointCount(), 3u);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineString3dTest, FractionToPoint)
TEST(LineString3dTest, FractionToPoint)
{
    std::vector<Point3d> pts = {Point3d::From(0, 0, 0), Point3d::From(10, 0, 0)};
    auto ls = LineString3d::create(pts);

    EXPECT_TRUE(ls->FractionToPoint(0.0).AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(ls->FractionToPoint(1.0).AlmostEqual(Point3d::From(10, 0, 0)));
    EXPECT_TRUE(ls->FractionToPoint(0.5).AlmostEqual(Point3d::From(5, 0, 0)));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(LineString3dTest, Length)
TEST(LineString3dTest, Length)
{
    std::vector<Point3d> pts = {Point3d::From(0, 0, 0), Point3d::From(3, 0, 0), Point3d::From(3, 4, 0)};
    auto ls = LineString3d::create(pts);
    EXPECT_NEAR(ls->QuickLength(), 7.0, 1.0e-10);  // 3 + 4 = 7
}

// ---------------------------------------------------------------------------
// Arc3d tests
// ---------------------------------------------------------------------------
// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, CreateXY)
TEST(Arc3dTest, CreateXY)
{
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0);
    EXPECT_EQ(arc->GetCurveType(), CurveType::Arc);
    EXPECT_TRUE(arc->IsCircular());
    EXPECT_NEAR(arc->CircularRadius(), 5.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, FractionToPointFullCircle)
TEST(Arc3dTest, FractionToPointFullCircle)
{
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0);

    // fraction 0 → (5, 0, 0)
    Point3d p0 = arc->FractionToPoint(0.0);
    EXPECT_NEAR(p0.x, 5.0, 1.0e-10);
    EXPECT_NEAR(p0.y, 0.0, 1.0e-10);

    // fraction 0.25 → (0, 5, 0)
    Point3d p1 = arc->FractionToPoint(0.25);
    EXPECT_NEAR(p1.x, 0.0, 1.0e-10);
    EXPECT_NEAR(p1.y, 5.0, 1.0e-10);

    // fraction 0.5 → (-5, 0, 0)
    Point3d p2 = arc->FractionToPoint(0.5);
    EXPECT_NEAR(p2.x, -5.0, 1.0e-10);
    EXPECT_NEAR(p2.y, 0.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, FractionToPointAndDerivative)
TEST(Arc3dTest, FractionToPointAndDerivative)
{
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0);
    Vector3d derivative;
    Point3d p = arc->FractionToPointAndDerivative(0.0, derivative);

    // At fraction 0, point = (5, 0, 0)
    EXPECT_NEAR(p.x, 5.0, 1.0e-10);
    EXPECT_NEAR(p.y, 0.0, 1.0e-10);

    // Derivative = sweep * (-vector0*sin(0) + vector90*cos(0))
    // = 2*pi * (-(5,0,0)*0 + (0,5,0)*1) = 2*pi * (0, 5, 0) = (0, 10*pi, 0)
    EXPECT_NEAR(derivative.x, 0.0, 1.0e-8);
    EXPECT_NEAR(derivative.y, 10.0 * Angle::kPi, 1.0e-8);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, CircularLength)
TEST(Arc3dTest, CircularLength)
{
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0);
    double expectedLength = 2.0 * Angle::kPi * 5.0;
    EXPECT_NEAR(arc->QuickLength(), expectedLength, 1.0e-8);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, HalfCircleLength)
TEST(Arc3dTest, HalfCircleLength)
{
    AngleSweep sweep = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0, sweep);
    double expectedLength = Angle::kPi * 5.0;
    EXPECT_NEAR(arc->QuickLength(), expectedLength, 1.0e-8);
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, clone)
TEST(Arc3dTest, clone)
{
    auto arc = Arc3d::CreateXY(Point3d::From(1, 2, 3), 5.0);
    auto clone = arc->clone();
    EXPECT_TRUE(clone.IsValid());
    EXPECT_TRUE(arc->IsAlmostEqual(*clone, 1.0e-10));
}

// Ported from: imodel-native iModelCore/GeomLibs/Tests/NonPublished/t_CurvePrimitive.cpp
//              TEST(Arc3dTest, Range)
TEST(Arc3dTest, Range)
{
    auto arc = Arc3d::CreateXY(Point3d::FromZero(), 5.0);
    Range3d range = arc->Range();
    EXPECT_NEAR(range.low.x, -5.0, 0.1);
    EXPECT_NEAR(range.low.y, -5.0, 0.1);
    EXPECT_NEAR(range.high.x, 5.0, 0.1);
    EXPECT_NEAR(range.high.y, 5.0, 0.1);
}
