// SPDX-License-Identifier: Apache-2.0
// dqGeom Point2d 测试
// Ported from: imodel-native iModelCore/GeomLibs/geom/test/structsTest/t_DPoint2d.cpp
//
// API 以 GeomLibs DPoint2d 为主权威（原地修改、From* 工厂、Normalize 返回原模长、
// index&0x01 循环下标、无运算符重载）。
// 仅移植自包含场景（不依赖 DVec2d/DPoint2dOps/bsiXxx/DoubleOps 的部分），其余随依赖类型就绪补。
#include <gtest/gtest.h>

#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>

#include <cmath>
#include <vector>

using namespace dqGeom;

namespace {
constexpr double kTol = 1.0e-9;
constexpr double PI = 3.14159265358979323846;

void ExpectNear(const Point2d& expected, const Point2d& actual, double tol = kTol) {
    EXPECT_NEAR(expected.x, actual.x, tol);
    EXPECT_NEAR(expected.y, actual.y, tol);
}
} // namespace

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, MagnitudeSquared)
TEST(Point2dTest, MagnitudeSquared) {
    Point2d point0 = Point2d::From(0.0, 1.0);
    Point2d point1 = Point2d::From(1.0, 1.0);
    Point2d point2 = Point2d::From(0.0, 0.0);
    Point2d point3;
    Point2d point4 = Point2d::From(2.0, 2.0);
    Point2d point5;
    Point2d point6 = Point2d::From(0.0, 0.5);
    Point2d point7;
    Point2d point8 = Point2d::From(1.0, 2.0);
    Point2d point9 = Point2d::From(0.0, 1.0);
    Point2d point10 = Point2d::From(1.0, 1.0);
    Point2d point11 = Point2d::From(1.0, 0.0);
    Point2d point12;
    Point2d point13 = Point2d::From(-1.0, 0.0);
    Point2d point14;
    double scale = 2.0;
    double s = 0.5;
    double magnitudeSqr0 = point0.MagnitudeSquared();
    double cross0 = point0.CrossProduct(point1);
    double cross1 = point2.CrossProductToPoints(point0, point1);
    double dot0 = point0.DotProduct(point1);
    double dot1 = point2.DotProductToPoints(point0, point1);
    double distanceSqr0 = point0.DistanceSquared(point1);
    point3.SumOf(point2, point1, scale);
    point5.Interpolate(point2, s, point0);
    point7.SumOf(point0, point1);
    point9.Add(point1);
    point10.Subtract(point0);
    point12.DifferenceOf(point0, point1);
    point14.Scale(point1, scale);
    EXPECT_NEAR(1.0, magnitudeSqr0, kTol);
    EXPECT_NEAR(-1.0, cross0, kTol);
    EXPECT_NEAR(-1.0, cross1, kTol);
    EXPECT_NEAR(1.0, dot0, kTol);
    EXPECT_NEAR(1.0, dot1, kTol);
    EXPECT_NEAR(1.0, distanceSqr0, kTol);
    ExpectNear(point4, point3);
    ExpectNear(point6, point5);
    ExpectNear(point8, point7);
    ExpectNear(point8, point9);
    ExpectNear(point11, point10);
    ExpectNear(point13, point12);
    ExpectNear(point4, point14);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, Normalize)
TEST(Point2dTest, Normalize) {
    Point2d point0 = Point2d::From(0.0, 0.0);
    Point2d point1 = Point2d::From(-1.0, 2.0);
    Point2d point5 = Point2d::From(-1.0, 2.0);
    Point2d point2 = Point2d::From(-1.0 / (std::sqrt(5.0)), 2.0 / (std::sqrt(5.0)));
    Point2d point3;
    Point2d point4;
    double magnitude0 = point0.Normalize();
    double magnitude1 = point1.Normalize();
    double magnitude2 = point3.Normalize(point0);
    double magnitude3 = point4.Normalize(point5);
    EXPECT_NEAR(0.0, magnitude0, kTol);
    EXPECT_NEAR(std::sqrt(5.0), magnitude1, kTol);
    ExpectNear(point2, point1);
    EXPECT_NEAR(0.0, magnitude2, kTol);
    EXPECT_NEAR(std::sqrt(5.0), magnitude3, kTol);
    ExpectNear(point2, point4);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, Rotate90)
TEST(Point2dTest, Rotate90) {
    Point2d point0, point4, point5;
    Point2d point1 = Point2d::From(1.0, 0.0);
    Point2d point2 = Point2d::From(0.0, 1.0);
    Point2d point6 = Point2d::From(0.0, -1.0);
    double radian0 = PI / 2;
    double radian1 = -PI / 2;
    point0.Rotate90(point1);
    point4.RotateCCW(point1, radian0);
    point5.RotateCCW(point1, radian1);
    ExpectNear(point2, point0);
    ExpectNear(point2, point4);
    ExpectNear(point6, point5);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, Zero)
TEST(Point2dTest, Zero) {
    Point2d point0 = Point2d::From(12.9, -4.3);
    Point2d point1 = Point2d::From(0.0, 0.0);
    Point2d point2 = Point2d::From(1.0, 1.0);
    Point2d point3 = Point2d::From(-54.0, 3.6);
    point0.Zero();
    point3.One();
    ExpectNear(point1, point0);
    ExpectNear(point2, point3);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, InitFromArray)（含 Init(DPoint3d)）
TEST(Point2dTest, InitFromArray) {
    Point2d point0, point2;
    Point2d point1 = Point2d::From(2.0, -7.9);
    Point3d point3 = Point3d::From(2.0, -7.9, 8.9);
    double pxy[2];
    pxy[0] = 2.0;
    pxy[1] = -7.9;
    point0.InitFromArray(pxy);
    point2.Init(point3);
    ExpectNear(point1, point0);
    ExpectNear(point1, point2);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, SetComponents)
TEST(Point2dTest, SetComponents) {
    Point2d point0;
    Point2d point1 = Point2d::From(2.0, -7.9);
    Point2d point2 = Point2d::From(4.0, -7.9);
    Point2d point3 = Point2d::From(2.0, 9.3);
    Point2d point4 = Point2d::From(-5.1, -7.9);
    double xx = 2.0;
    double yy = -7.9;
    int index0 = 0;
    int index1 = 1;
    int index2 = 2; // 循环归约 → x（index & 0x01）
    double xCoord = 0.0, yCoord = 0.0;
    point0.Init(xx, yy);
    point2.SetComponent(xx, index0);
    point3.SetComponent(yy, index1);
    point4.SetComponent(xx, index2);
    double component0 = point1.GetComponent(index0);
    double component1 = point1.GetComponent(index1);
    double component2 = point1.GetComponent(index2);
    point1.GetComponents(xCoord, yCoord);
    ExpectNear(point1, point0);
    ExpectNear(point1, point2);
    ExpectNear(point1, point3);
    ExpectNear(point1, point4);
    EXPECT_NEAR(xx, component0, kTol);
    EXPECT_NEAR(yy, component1, kTol);
    EXPECT_NEAR(xx, component2, kTol);
    EXPECT_NEAR(xx, xCoord, kTol);
    EXPECT_NEAR(yy, yCoord, kTol);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, SumOf)
TEST(Point2dTest, SumOf) {
    Point2d point0, point6;
    Point2d point1 = Point2d::From(0.0, 0.0);
    Point2d point2 = Point2d::From(2.0, -7.9);
    Point2d point3 = Point2d::From(4.0, -7.9);
    Point2d point4 = Point2d::From(-8.0, 7.9);
    Point2d point5 = Point2d::From(3.0, 2.0);
    Point2d point7 = Point2d::From(-5.0, 9.9);
    double scale0 = 2.0;
    double scale1 = -3.0;
    double scale2 = 1.0;
    point0.SumOf(point1, point2, scale0, point3, scale1);
    point6.SumOf(point1, point2, scale0, point3, scale1, point5, scale2);
    ExpectNear(point4, point0);
    ExpectNear(point7, point6);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, NormalizedDifferenceOf)
TEST(Point2dTest, NormalizedDifferenceOf) {
    Point2d point0;
    Point2d point1 = Point2d::From(2.0, -7.9);
    Point2d point2 = Point2d::From(4.0, -7.9);
    Point2d point3 = Point2d::From(-1.0, 0.0);
    double magnitude0 = point0.NormalizedDifferenceOf(point1, point2);
    double magnitude1 = point0.Magnitude();
    EXPECT_NEAR(2.0, magnitude0, kTol);
    EXPECT_NEAR(1.0, magnitude1, kTol);
    ExpectNear(point3, point0);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, AngleTo)
TEST(Point2dTest, AngleTo) {
    Point2d point0 = Point2d::From(1.0, 0.0);
    Point2d point1 = Point2d::From(0.0, 1.0);
    Point2d point2 = Point2d::From(1.0, 1.0);
    double angle0 = point0.AngleTo(point1);
    double angle1 = point0.AngleTo(point2);
    double angle2 = point2.AngleTo(point0);
    EXPECT_NEAR(PI / 2, angle0, kTol);
    EXPECT_NEAR(PI / 4, angle1, kTol);
    EXPECT_NEAR(-PI / 4, angle2, kTol);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, Distance)
TEST(Point2dTest, Distance) {
    Point2d point0 = Point2d::From(1.0, 0.0);
    Point2d point1 = Point2d::From(2.0, 0.0);
    Point2d point2 = Point2d::From(-2.0, 0.0);
    Point2d point3;
    double distance0 = point0.Distance(point1);
    double distance1 = point0.Distance(point2);
    double magnitude0 = point0.Magnitude();
    double magnitude1 = point2.Magnitude();
    point3.Negate(point2);
    EXPECT_NEAR(1.0, distance0, kTol);
    EXPECT_NEAR(3.0, distance1, kTol);
    EXPECT_NEAR(1.0, magnitude0, kTol);
    EXPECT_NEAR(2.0, magnitude1, kTol);
    ExpectNear(point1, point3);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, IsParallelTo)
TEST(Point2dTest, IsParallelTo) {
    Point2d point0 = Point2d::From(2.0, 2.0);
    Point2d point1 = Point2d::From(1.0, 1.0);
    Point2d point2 = Point2d::From(1.0, 0.0);
    Point2d point3 = Point2d::From(0.0, 1.0);
    EXPECT_TRUE(point0.IsParallelTo(point1));
    EXPECT_TRUE(!(point0.IsParallelTo(point2)));
    EXPECT_TRUE(point2.IsPerpendicularTo(point3));
    EXPECT_TRUE(!(point2.IsPerpendicularTo(point1)));
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, IsEqual)
TEST(Point2dTest, IsEqual) {
    Point2d point0 = Point2d::From(2.0, 2.0);
    Point2d point1 = Point2d::From(2.0, 2.0);
    Point2d point2 = Point2d::From(1.0, 1.0);
    Point2d point3 = Point2d::From(0.0, 0.0);
    Point2d point4 = Point2d::From(0.00001, 0.00001);
    double tol0 = 0.01;
    double tol1 = -0.0001; // 负容差返回 false（对齐 GeomLibs 语义）
    EXPECT_TRUE(point0.IsEqual(point1));
    EXPECT_TRUE(!(point0.IsEqual(point2)));
    EXPECT_TRUE(point3.IsEqual(point4, tol0));
    EXPECT_TRUE(!(point3.IsEqual(point4, tol1)));
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, MaxAbs)
TEST(Point2dTest, MaxAbs) {
    Point2d point0 = Point2d::From(-2.0, -15.0);
    Point2d point1 = Point2d::From(4.0, -5.0);
    double maxVal0 = point0.MaxAbs();
    double maxVal1 = point1.MaxAbs();
    EXPECT_NEAR(15.0, maxVal0, kTol);
    EXPECT_NEAR(5.0, maxVal1, kTol);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, Swap)
TEST(Point2dTest, Swap) {
    Point2d dpFirst = Point2d::From(3, 5);
    Point2d dpSecond = Point2d::From(6, 8);
    dpFirst.Swap(dpSecond);
    EXPECT_DOUBLE_EQ(6, dpFirst.x);
    EXPECT_DOUBLE_EQ(8, dpFirst.y);
    EXPECT_DOUBLE_EQ(3, dpSecond.x);
    EXPECT_DOUBLE_EQ(5, dpSecond.y);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, SumOfTwoVectorsWithScale)
TEST(Point2dTest, SumOfTwoVectorsWithScale) {
    Point2d dpMain;
    Point2d dpFirst = Point2d::From(3, 5);
    double scale1 = 2;
    Point2d dpSecond = Point2d::From(6, 8);
    double scale2 = 3;
    dpMain.SumOf(dpFirst, scale1, dpSecond, scale2);
    EXPECT_NEAR(24, dpMain.x, kTol);
    EXPECT_NEAR(34, dpMain.y, kTol);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, ScaleToLength)
TEST(Point2dTest, ScaleToLength) {
    Point2d dpMain;
    Point2d dpOriginal1 = Point2d::From(6, 8);
    double length = 5;
    EXPECT_NEAR(10, dpMain.ScaleToLength(dpOriginal1, length), kTol);
    EXPECT_NEAR(3, dpMain.x, kTol);
    EXPECT_NEAR(4, dpMain.y, kTol);
    length = 0;
    EXPECT_NEAR(5, dpMain.ScaleToLength(length), kTol);
    EXPECT_NEAR(0, dpMain.x, kTol);
    EXPECT_NEAR(0, dpMain.y, kTol);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, MaxDiff)
TEST(Point2dTest, MaxDiff) {
    double pXY[] = {3, 2};
    Point2d pnt = Point2d::FromArray(pXY);
    Point2d pnt2 = Point2d::FromOne();
    double diff = pnt.MaxDiff(pnt2);
    EXPECT_NEAR(2, diff, kTol);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, IsConvexPair)（bvector→std::vector，DoubleOps::IsIn01→内联）
TEST(Point2dTest, IsConvexPair) {
    Point2d point1 = Point2d::From(0, 0.4);
    EXPECT_FALSE(point1.IsConvexPair());
    Point2d point2 = Point2d::From(0.2, 0.4);
    EXPECT_FALSE(point2.IsConvexPair());
    std::vector<double> fractions{-0.3, 0, 0.2, 0.4, 0.99, 1.0, 1.2};
    for (double f : fractions) {
        Point2d xy = Point2d::From(f, 1.0 - f);
        bool isIn01 = (f >= 0.0 && f <= 1.0); // DoubleOps::IsIn01 等价：[0,1] 闭区间
        if (isIn01) {
            EXPECT_TRUE(xy.IsConvexPair(false));
            EXPECT_TRUE(xy.IsConvexPair(true));
        } else {
            EXPECT_FALSE(xy.IsConvexPair(false));
            EXPECT_TRUE(xy.IsConvexPair(true));
        }
    }
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, FromOne)
TEST(Point2dTest, FromOne) {
    Point2d point2 = Point2d::FromOne();
    EXPECT_TRUE(point2.x * point2.y == 1);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, FromArray)
TEST(Point2dTest, FromArray) {
    double pts[] = {3, 4};
    Point2d point2 = Point2d::FromArray(pts);
    point2.ScaleToLength(point2, 2);
    EXPECT_DOUBLE_EQ(0.4 * 3, point2.x);
    EXPECT_DOUBLE_EQ(0.4 * 4, point2.y);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, Resultant)
TEST(Point2dTest, Resultant) {
    Point2d pnt0 = Point2d::FromOne();
    Point2d pnt1 = Point2d::From(22, 24);
    pnt0.ScaleToLength(22);

    Point2d res;
    res.SumOf(pnt0, pnt1);
    Point2d resEq;
    pnt0.Swap(pnt1);
    resEq.SumOf(pnt0, pnt1);
    ExpectNear(res, resEq);
}

// Ported from: t_DPoint2d.cpp TEST(DPoint2d, FromScale)/FromInterpolate/FromForwardLeftInterpolate/
//              FromInterpolateBilinear/FromSumOf/LexicalXYLessThan（自包含工厂与工具，对齐参考语义）
TEST(Point2dTest, StaticFactories) {
    // FromScale
    ExpectNear(Point2d::From(6, 8), Point2d::FromScale(Point2d::From(3, 4), 2.0));
    // From(Point3d) 丢 z
    ExpectNear(Point2d::From(3, 4), Point2d::From(Point3d::From(3, 4, 99)));
    // FromInterpolate（中点）
    ExpectNear(Point2d::From(3, 5), Point2d::FromInterpolate(Point2d::From(1, 2), 0.5, Point2d::From(5, 8)));
    // FromForwardLeftInterpolate：从 (0,0) 沿 (dx,dy)=(4,3) 前移 tangentFraction=1、
    // 左移 leftFraction=1（左法向 (-dy,dx)=(-3,4)）→ (0+4-3, 0+3+4) = (1,7)
    ExpectNear(Point2d::From(1, 7),
              Point2d::FromForwardLeftInterpolate(Point2d::From(0, 0), 1.0, 1.0, Point2d::From(4, 3)));
    // FromInterpolateBilinear：u=v=0 → data00
    ExpectNear(Point2d::From(1, 2),
              Point2d::FromInterpolateBilinear(Point2d::From(1, 2), Point2d::From(3, 4),
                                               Point2d::From(5, 6), Point2d::From(7, 8), 0.0, 0.0));
    // u=1,v=0 → data10
    ExpectNear(Point2d::From(3, 4),
              Point2d::FromInterpolateBilinear(Point2d::From(1, 2), Point2d::From(3, 4),
                                               Point2d::From(5, 6), Point2d::From(7, 8), 1.0, 0.0));
    // FromSumOf 2-pt / 3-pt
    ExpectNear(Point2d::From(24, 34),
              Point2d::FromSumOf(Point2d::From(3, 5), 2.0, Point2d::From(6, 8), 3.0));
    ExpectNear(Point2d::From(6, 12),
              Point2d::FromSumOf(Point2d::From(1, 2), 1.0, Point2d::From(2, 3), 1.0, Point2d::From(3, 7), 1.0));
}

// Ported from: t_DPoint2d.cpp DPoint2d::LexicalXYLessThan（参考 dpoint2d.h + refdpoint2d.cpp 语义）
TEST(Point2dTest, LexicalXYLessThan) {
    EXPECT_TRUE(Point2d::LexicalXYLessThan(Point2d::From(1, 9), Point2d::From(2, 0)));  // x 小 → true
    EXPECT_FALSE(Point2d::LexicalXYLessThan(Point2d::From(2, 0), Point2d::From(1, 9)));
    EXPECT_TRUE(Point2d::LexicalXYLessThan(Point2d::From(1, 2), Point2d::From(1, 3)));  // x 等、y 小 → true
    EXPECT_FALSE(Point2d::LexicalXYLessThan(Point2d::From(1, 3), Point2d::From(1, 2)));
}
