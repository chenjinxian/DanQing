// SPDX-License-Identifier: Apache-2.0
// dqGeom Vector3d 测试
// Ported from: imodel-native iModelCore/GeomLibs/geom/test/structsTest/t_dvec3d.cpp
//
// API 以 GeomLibs DVec3d 为主权威（原地修改、From* 工厂、Normalize 零向量→(1,0,0) 返回 0、
// TryNormalize bool+out、IsParallelTo(v)/IsParallelTo(v,radians)/IsPositiveParallelTo、无运算符重载）。
#include <gtest/gtest.h>

#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <cmath>

using namespace dqGeom;

namespace {
constexpr double kTol = 1.0e-9;

void ExpectNear(const Vector3d& a, const Vector3d& b, double tol = kTol) {
    EXPECT_NEAR(a.x, b.x, tol);
    EXPECT_NEAR(a.y, b.y, tol);
    EXPECT_NEAR(a.z, b.z, tol);
}
} // namespace

// Ported from: imodel-native iModelCore/GeomLibs/geom/test/structsTest/t_dvec3d.cpp
//              TEST(DVec3d, DirectInitialization) + FromArray/FromXYZ/FromPoint/FromXY
TEST(Vector3dTest, InitAndFactories) {
    Vector3d v;
    v.Init(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(v.x, 1.0);
    EXPECT_DOUBLE_EQ(v.y, 2.0);
    EXPECT_DOUBLE_EQ(v.z, 3.0);

    const double arr[3] = {2.3, 3.4, 4.5};
    ExpectNear(Vector3d::FromArray(arr), Vector3d::From(2.3, 3.4, 4.5));

    Vector3d a;
    a.Init(2.3, 3.4, 4.5);
    ExpectNear(a, Vector3d::From(2.3, 3.4, 4.5));

    // From(Point3d)
    ExpectNear(Vector3d::From(Point3d::From(2.3, 3.4, 4.5)), Vector3d::From(2.3, 3.4, 4.5));

    // From(x,y) → z=0
    ExpectNear(Vector3d::From(2.3, 3.4), Vector3d::From(2.3, 3.4, 0.0));
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, FromXYAngleAndMagnitude)
TEST(Vector3dTest, FromXYAngleAndMagnitude) {
    constexpr double PI = 3.14159265358979323846;
    ExpectNear(Vector3d::FromXYAngleAndMagnitude(PI, 12.0), Vector3d::From(-12.0, 0.0, 0.0));
    Vector3d v;
    v.InitFromXYAngleAndMagnitude(PI, 12.0);
    ExpectNear(v, Vector3d::From(-12.0, 0.0, 0.0));
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, DotProduct)
TEST(Vector3dTest, DotProduct) {
    const Vector3d v0 = Vector3d::From(1.0, 2.0, 3.0);
    const Vector3d v1 = Vector3d::From(5.0, 7.0, 11.0);
    EXPECT_DOUBLE_EQ(v0.DotProduct(v1), 52.0); // 5 + 14 + 33
    EXPECT_DOUBLE_EQ(v0.DotProduct(v1.x, v1.y, v1.z), v0.DotProduct(v1));
    EXPECT_DOUBLE_EQ(v1.DotProduct(v0), v0.DotProduct(v1)); // 对称
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, FromSumOfVectors)
TEST(Vector3dTest, FromSumOfTrailingScaleConvention) {
    const Vector3d v0 = Vector3d::From(2.0, 3.0, 3.0);
    const Vector3d v1 = Vector3d::From(3.0, 7.0, 0.0);
    const Vector3d v2 = Vector3d::From(3.0, 2.0, 7.0);
    const Vector3d v3 = Vector3d::From(1.0, 5.0, 1.0);
    constexpr double s1 = 3.1;
    constexpr double s2 = 1.6;
    constexpr double s3 = 4.9;

    // FromSumOf(v0, v1, s1) = v0 + s1*v1（首向量不缩放）
    ExpectNear(Vector3d::FromSumOf(v0, v1, s1), Vector3d::From(11.3, 24.7, 3.0));
    // FromSumOf(v0, v1, s1, v2, s2)
    ExpectNear(Vector3d::FromSumOf(v0, v1, s1, v2, s2), Vector3d::From(16.1, 27.9, 14.2));
    // FromSumOf(v0, v1, s1, v2, s2, v3, s3)
    ExpectNear(Vector3d::FromSumOf(v0, v1, s1, v2, s2, v3, s3), Vector3d::From(21.0, 52.4, 19.1));
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, All) → TestOps 子集（原地修改 + 返回模长语义）
TEST(Vector3dTest, OpsNormalizeScaleSafeDivideNegate) {
    const Vector3d v1 = Vector3d::From(4.0, 5.0, -6.0);
    const double mag1 = std::sqrt(77.0); // |v1|

    // Normalize(src) 返回原模长，结果为单位向量
    Vector3d n;
    const double pre = n.Normalize(v1);
    EXPECT_NEAR(pre, mag1, kTol);
    EXPECT_NEAR(n.Magnitude(), 1.0, kTol);

    // ScaleToLength(src, b) 返回原模长，结果 ∥ src、长度 b
    Vector3d s;
    constexpr double b = 2.901;
    const double preS = s.ScaleToLength(v1, b);
    EXPECT_NEAR(preS, mag1, kTol);
    EXPECT_NEAR(s.Magnitude(), b, kTol);
    EXPECT_TRUE(s.IsParallelTo(v1));

    // Scale(c) 原地
    Vector3d sc = v1;
    constexpr double c = 4.201;
    sc.Scale(c);
    EXPECT_NEAR(sc.Magnitude(), c * mag1, kTol);
    EXPECT_TRUE(sc.IsParallelTo(v1));

    // SafeDivide(scaledByC, c) → 原 v1
    Vector3d back;
    ASSERT_TRUE(back.SafeDivide(sc, c));
    ExpectNear(back, v1);
    EXPECT_FALSE(back.SafeDivide(sc, 0.0)); // 除零返回 false

    // Negate() 原地
    Vector3d neg = v1;
    neg.Negate();
    ExpectNear(neg, Vector3d::From(-4.0, -5.0, 6.0));

    // Zero/One
    Vector3d z;
    z.Zero();
    EXPECT_DOUBLE_EQ(z.MaxAbs(), 0.0);
    Vector3d one;
    one.One();
    EXPECT_DOUBLE_EQ(one.MaxAbs(), 1.0);
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, All) → TestOps（IsParallelTo / IsPositiveParallelTo 语义）
TEST(Vector3dTest, ParallelAndPositiveParallel) {
    const Vector3d v0 = Vector3d::From(1.0, 2.0, 3.0);
    const Vector3d v1 = Vector3d::From(4.0, 5.0, -6.0);
    const Vector3d rev1 = Vector3d::FromScale(v1, -1.0);

    EXPECT_TRUE(v1.IsParallelTo(v1));
    EXPECT_TRUE(v1.IsParallelTo(rev1));        // 反向也算平行
    EXPECT_FALSE(v1.IsParallelTo(v0));
    EXPECT_TRUE(v1.IsPositiveParallelTo(v1));
    EXPECT_FALSE(v1.IsPositiveParallelTo(rev1)); // 反向不算正平行
    EXPECT_FALSE(v1.IsPositiveParallelTo(v0));
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, All) → TestOps（Normalize 零向量 → (1,0,0) 返回 0）
TEST(Vector3dTest, NormalizeZeroVectorBecomesUnitX) {
    Vector3d z = Vector3d::FromZero();
    const double pre = z.Normalize();
    EXPECT_DOUBLE_EQ(pre, 0.0);
    ExpectNear(z, Vector3d::UnitX()); // GeomLibs DVec3d 零向量归一化 → (1,0,0)
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, All) → TestOps（TryNormalize bool+out）
TEST(Vector3dTest, TryNormalize) {
    Vector3d v = Vector3d::From(3.0, 4.0, 0.0);
    Vector3d u;
    double mag = 0.0;
    ASSERT_TRUE(u.TryNormalize(v, mag)); // this=u 写入，source=v
    EXPECT_NEAR(mag, 5.0, kTol);
    EXPECT_NEAR(u.Magnitude(), 1.0, kTol);

    EXPECT_FALSE(u.TryNormalize(Vector3d::FromZero(), mag)); // 零源返回 false
}

// Ported from: t_dvec3d.cpp TEST(DVec3d, All) → TestProducts（叉积⊥两参数、量级关系）
TEST(Vector3dTest, CrossProductPerpendicularAndMagnitude) {
    const Vector3d v0 = Vector3d::From(1.0, 2.0, 3.0);
    const Vector3d v1 = Vector3d::From(4.0, 5.0, -6.0);
    const Vector3d cross = Vector3d::FromCrossProduct(v0, v1);
    EXPECT_NEAR(cross.DotProduct(v0), 0.0, kTol); // ⊥ v0
    EXPECT_NEAR(cross.DotProduct(v1), 0.0, kTol); // ⊥ v1
    EXPECT_NEAR(cross.Magnitude(), v0.CrossProductMagnitude(v1), kTol);
    // |cross| = |v0||v1|sin(theta)
    EXPECT_NEAR(cross.MagnitudeSquared(),
                v0.MagnitudeSquared() * v1.MagnitudeSquared() - std::pow(v0.DotProduct(v1), 2.0), kTol);
}
