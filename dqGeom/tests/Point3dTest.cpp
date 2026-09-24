// SPDX-License-Identifier: Apache-2.0
// dqGeom Point3d 测试
// Ported from: imodel-native iModelCore/GeomLibs/geom/test/structsTest/t_DPoint3d.cpp
//
// API 以 GeomLibs DPoint3d 为主权威（原地修改、From* 工厂、Normalize 返回原模长、
// GetComponent/MaxAbsIndex/Zero/AlmostEqual/Add/Subtract 命名、无运算符重载）。
// 仅移植自包含场景（不依赖 Matrix3d/Transform/DPoint4d/DRange3d 的部分），其余随依赖类型就绪补。
#include <gtest/gtest.h>

#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <cmath>

using namespace dqGeom;

namespace {
constexpr double kTol = 1.0e-9;
constexpr double PI = 3.14159265358979323846;

void ExpectNear(const Point3d& a, const Point3d& b, double tol = kTol) {
    EXPECT_NEAR(a.x, b.x, tol);
    EXPECT_NEAR(a.y, b.y, tol);
    EXPECT_NEAR(a.z, b.z, tol);
}
} // namespace

// Ported from: imodel-native iModelCore/GeomLibs/geom/test/structsTest/t_DPoint3d.cpp
//              TEST(DPoint3d, FromArray) + TEST(DPoint3d, InitFromArray)
TEST(Point3dTest, FromArrayAndInit) {
    const double arr[3] = {1.0, 1.0, 1.0};
    ExpectNear(Point3d::From(1.0, 1.0, 1.0), Point3d::FromArray(arr));

    Point3d p;
    p.InitFromArray(arr);
    ExpectNear(p, Point3d::From(1.0, 1.0, 1.0));

    // Init 2-arg → z=0（对齐 GeomLibs Init(x,y)）
    Point3d q;
    q.Init(2.0, -7.9);
    ExpectNear(q, Point3d::From(2.0, -7.9, 0.0));
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, Zero) + TEST(DPoint3d, SetToZero) + TEST(DPoint3d, One)
TEST(Point3dTest, ZeroAndOne) {
    Point3d p = Point3d::From(12.9, -4.3, -3.4);
    p.Zero();
    ExpectNear(p, Point3d::FromZero());

    Point3d q = Point3d::From(-54.0, 3.6, -78.9);
    q.One();
    ExpectNear(q, Point3d::From(1.0, 1.0, 1.0));
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, SetComponent)
TEST(Point3dTest, SetComponentAndGetComponents) {
    Point3d p;
    p.SetComponent(2.0, 0);
    p.SetComponent(-7.9, 1);
    p.SetComponent(5.0, 2);
    EXPECT_DOUBLE_EQ(p.GetComponent(0), 2.0);
    EXPECT_DOUBLE_EQ(p.GetComponent(1), -7.9);
    EXPECT_DOUBLE_EQ(p.GetComponent(2), 5.0);
    // 循环下标：index 3 → x（对齐 GeomLibs Angle::Cyclic3dAxis）
    EXPECT_DOUBLE_EQ(p.GetComponent(3), 2.0);

    double cx = 0.0;
    double cy = 0.0;
    double cz = 0.0;
    p.GetComponents(cx, cy, cz);
    EXPECT_DOUBLE_EQ(cx, 2.0);
    EXPECT_DOUBLE_EQ(cy, -7.9);
    EXPECT_DOUBLE_EQ(cz, 5.0);
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, MaxAbs) + TEST(DPoint3d, MaxAbsIndex)
TEST(Point3dTest, MaxAbsAndMaxAbsIndex) {
    EXPECT_DOUBLE_EQ(Point3d::From(-2.0, -15.0, 1.0).MaxAbs(), 15.0);
    EXPECT_DOUBLE_EQ(Point3d::From(4.0, -5.0, 1.0).MaxAbs(), 5.0);
    EXPECT_EQ(Point3d::From(-2.334, 1.234, 4.43).MaxAbsIndex(), 2);
    EXPECT_EQ(Point3d::From(-2.334, -1.234, -4.43).MaxAbsIndex(), 2);
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, Distance)
TEST(Point3dTest, DistanceAndMagnitude) {
    const Point3d v0 = Point3d::FromZero();
    const Point3d v1 = Point3d::From(4.0, 5.0, 0.0);
    const Point3d v2 = Point3d::From(-1.0, -7.8, 1.0);

    EXPECT_DOUBLE_EQ(v0.Distance(v0), 0.0);
    EXPECT_NEAR(v1.Distance(v2), std::sqrt(189.84), kTol);
    EXPECT_DOUBLE_EQ(v1.DistanceSquared(v2), 189.84);
    EXPECT_DOUBLE_EQ(v1.DistanceSquaredXY(v2), 188.84);
    EXPECT_NEAR(v1.DistanceXY(v2), std::sqrt(188.84), kTol);
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, MagnitudeSquared)
TEST(Point3dTest, MagnitudeSquared) {
    const Point3d p = Point3d::From(6.0, -18.0, -9.0);
    EXPECT_DOUBLE_EQ(p.MagnitudeSquared(), 441.0);
    EXPECT_DOUBLE_EQ(p.MagnitudeSquaredXY(), 360.0);
    EXPECT_NEAR(p.MagnitudeXY(), std::sqrt(360.0), kTol);
    EXPECT_NEAR(p.Magnitude(), 21.0, kTol);

    // Scale(point, s) 写入 this
    Point3d r;
    r.Scale(p, 2.0);
    ExpectNear(r, Point3d::From(12.0, -36.0, -18.0));
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, DotProduct)
TEST(Point3dTest, DotProduct) {
    const Point3d p0 = Point3d::From(1.0, 2.0, 3.0);
    const Point3d p1 = Point3d::From(2.0, 5.0, 6.0);
    EXPECT_DOUBLE_EQ(p0.DotProduct(p1), 30.0);     // 2 + 10 + 18
    EXPECT_DOUBLE_EQ(p0.DotProductXY(p1), 12.0);    // 2 + 10
    EXPECT_DOUBLE_EQ(p0.DotProduct(2.0, 5.0, 6.0), 30.0);
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, CrossProductToPointsXY)
TEST(Point3dTest, CrossAndTripleProductToPoints) {
    const Point3d p0 = Point3d::FromZero();
    const Point3d p1 = Point3d::From(1.0, 1.0, 1.0);
    const Point3d p2 = Point3d::From(4.0, 5.0, 6.0);
    const Point3d p3 = Point3d::From(3.0, 4.0, 8.0);

    EXPECT_DOUBLE_EQ(p0.CrossProductToPointsXY(p1, p2), 1.0);   // (1,1)×(4,5) z = 1*5-1*4
    EXPECT_DOUBLE_EQ(p0.DotProductToPoints(p1, p2), 15.0);      // (1,1,1)·(4,5,6)
    EXPECT_DOUBLE_EQ(p0.DotProductToPointsXY(p1, p2), 9.0);     // (1,1)·(4,5)
    EXPECT_DOUBLE_EQ(p0.TripleProductToPoints(p1, p2, p3), 3.0); // 标量三重积
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, CrossProduct)
TEST(Point3dTest, CrossProductInPlace) {
    const Point3d p1 = Point3d::From(1.0, 2.0, 3.0);
    const Point3d p3 = Point3d::From(2.0, 5.0, 6.0);
    Point3d r;
    r.CrossProduct(p1, p3); // (1,2,3)×(2,5,6) = (2*6-3*5, 3*2-1*6, 1*5-2*2) = (-3, 0, 1)
    ExpectNear(r, Point3d::From(-3.0, 0.0, 1.0));
    EXPECT_DOUBLE_EQ(p1.CrossProductXY(p3), 1.0); // 1*5 - 2*2
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, IsEqual)
TEST(Point3dTest, IsEqual) {
    const Point3d p0 = Point3d::From(2.0, 2.0, 2.0);
    const Point3d p1 = Point3d::From(2.0, 2.0, 2.0);
    const Point3d p2 = Point3d::From(1.0, 1.0, 1.0);
    const Point3d p3 = Point3d::FromZero();
    const Point3d p4 = Point3d::From(1.0e-5, 1.0e-5, 1.0e-5);
    EXPECT_TRUE(p0.IsEqual(p1));
    EXPECT_FALSE(p0.IsEqual(p2));
    EXPECT_TRUE(p3.IsEqual(p4, 0.01));
    EXPECT_FALSE(p3.IsEqual(p4, -0.0001)); // 负容差 → false
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, IsParallelTo)
TEST(Point3dTest, IsParallelAndPerpendicular) {
    const Point3d p0 = Point3d::From(2.0, 2.0, 2.0);
    const Point3d p1 = Point3d::From(1.0, 1.0, 1.0);
    const Point3d p2 = Point3d::From(1.0, 0.0, 0.0);
    const Point3d p3 = Point3d::From(0.0, 1.0, 0.0);
    EXPECT_TRUE(p0.IsParallelTo(p1));
    EXPECT_FALSE(p0.IsParallelTo(p2));
    EXPECT_TRUE(p2.IsPerpendicularTo(p3));
    EXPECT_FALSE(p2.IsPerpendicularTo(p1));
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, FromInterpolate)（instance + static）
TEST(Point3dTest, Interpolate) {
    const Point3d a = Point3d::From(0.0, 1.0, 0.0);
    const Point3d b = Point3d::From(0.0, 0.0, 0.0);
    Point3d r;
    r.Interpolate(b, 0.5, a); // instance: this = b + 0.5*(a-b)
    ExpectNear(r, Point3d::From(0.0, 0.5, 0.0));

    ExpectNear(Point3d::FromInterpolate(Point3d::From(2.0, 3.0, 4.0), 0.5, Point3d::From(5.0, 8.0, 6.0)),
               Point3d::From(3.5, 5.5, 5.0));
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, Negate)
TEST(Point3dTest, NegateAndNormalize) {
    const Point3d p1 = Point3d::From(2.0, -5.0, 1.0);
    Point3d r;
    r.Negate(p1);
    ExpectNear(r, Point3d::From(-2.0, 5.0, -1.0));

    Point3d n;
    const double mag = n.Normalize(p1); // 返回原模长
    EXPECT_NEAR(mag, std::sqrt(30.0), kTol);
    EXPECT_NEAR(n.Magnitude(), 1.0, kTol);
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, ScaleToLength)
TEST(Point3dTest, ScaleToLength) {
    const Point3d unit = Point3d::From(1.0, 0.0, 0.0);
    Point3d r;
    const double pre = r.ScaleToLength(unit, 2.0); // 返回原模长 1.0，结果 (2,0,0)
    EXPECT_DOUBLE_EQ(pre, 1.0);
    ExpectNear(r, Point3d::From(2.0, 0.0, 0.0));

    Point3d s = Point3d::From(2.0, 0.0, 0.0);
    const double pre2 = s.Normalize(); // 返回 2.0，结果 (1,0,0)
    EXPECT_DOUBLE_EQ(pre2, 2.0);
    ExpectNear(s, unit);
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, Subtract)（SumOf origin-first 约定）
TEST(Point3dTest, SubtractAndSumOf) {
    const Point3d origin = Point3d::FromZero();
    const Vector3d v = Vector3d::From(0.5, 0.5, 0.5);
    const Vector3d w = Vector3d::From(2.0, 3.0, 4.0);

    // SumOf(origin, v, 3) = 3*v
    Point3d r1;
    r1.SumOf(origin, v, 3.0);
    ExpectNear(r1, Point3d::From(1.5, 1.5, 1.5));

    // SumOf(origin, v, 3, w, 2) = 3*v + 2*w
    Point3d r2;
    r2.SumOf(origin, v, 3.0, w, 2.0);
    ExpectNear(r2, Point3d::From(5.5, 7.5, 9.5));

    // DifferenceOf(a,b) = a - b
    Point3d r3;
    r3.DifferenceOf(Point3d::From(1.0, 1.0, 1.0), Point3d::From(0.5, 0.5, 0.5));
    ExpectNear(r3, Point3d::From(0.5, 0.5, 0.5));
}

// Ported from: t_DPoint3d.cpp TEST(DPoint3d, AngleTo)
TEST(Point3dTest, Angles) {
    const Point3d p0 = Point3d::From(1.0, 0.0, 0.0);
    const Point3d p1 = Point3d::From(0.0, 1.0, 0.0);
    const Point3d p2 = Point3d::From(1.0, 1.0, 0.0);
    const Point3d p3 = Point3d::From(1.0, -1.0, 0.0);
    const Point3d zUp = Point3d::From(0.0, 0.0, 1.0);

    EXPECT_NEAR(p0.AngleTo(p1), PI / 2.0, kTol);
    EXPECT_NEAR(p0.AngleTo(p2), PI / 4.0, kTol);
    EXPECT_NEAR(p2.AngleTo(p0), PI / 4.0, kTol);          // 无方向 [0,PI]
    EXPECT_NEAR(p2.AngleToXY(p0), -PI / 4.0, kTol);      // 有方向 [-PI,PI]
    EXPECT_NEAR(p2.SignedAngleTo(p0, zUp), -PI / 4.0, kTol);
    EXPECT_NEAR(p1.SmallerUnorientedAngleTo(p3), PI / 4.0, kTol); // [0,PI/2]
    EXPECT_NEAR(p0.PlanarAngleTo(p1, zUp), PI / 2.0, kTol);
}
