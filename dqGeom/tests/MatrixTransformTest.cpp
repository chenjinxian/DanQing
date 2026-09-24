// SPDX-License-Identifier: Apache-2.0
// dqGeom tests — Matrix3d, Transform
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
#include <gtest/gtest.h>

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Transform.h>

#include <cmath>

using namespace dqGeom;

// ============================================================================
// Matrix3d tests
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
// ============================================================================

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, DefaultIsZero) {
    Matrix3d m;
    EXPECT_TRUE(m.IsExactEqual(Matrix3d::CreateZero()));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, CreateIdentity) {
    auto m = Matrix3d::CreateIdentity();
    EXPECT_EQ(m.coffs[0], 1.0);
    EXPECT_EQ(m.coffs[4], 1.0);
    EXPECT_EQ(m.coffs[8], 1.0);
    EXPECT_EQ(m.coffs[1], 0.0);
    EXPECT_EQ(m.coffs[2], 0.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, CreateScale) {
    auto m = Matrix3d::CreateScale(2.0, 3.0, 4.0);
    EXPECT_EQ(m.coffs[0], 2.0);
    EXPECT_EQ(m.coffs[4], 3.0);
    EXPECT_EQ(m.coffs[8], 4.0);
    EXPECT_EQ(m.coffs[1], 0.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, CreateUniformScale) {
    auto m = Matrix3d::CreateUniformScale(5.0);
    EXPECT_EQ(m.coffs[0], 5.0);
    EXPECT_EQ(m.coffs[4], 5.0);
    EXPECT_EQ(m.coffs[8], 5.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, SetIdentity) {
    Matrix3d m;
    m.SetIdentity();
    EXPECT_TRUE(m.IsExactEqual(Matrix3d::CreateIdentity()));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, SetFrom) {
    auto m1 = Matrix3d::CreateScale(2.0, 3.0, 4.0);
    Matrix3d m2;
    m2.SetFrom(m1);
    EXPECT_TRUE(m2.IsExactEqual(m1));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, IsAlmostEqual) {
    auto m1 = Matrix3d::CreateIdentity();
    auto m2 = Matrix3d::CreateIdentity();
    m2.coffs[0] += 1e-10;
    EXPECT_TRUE(m1.IsAlmostEqual(m2));
    EXPECT_FALSE(m1.IsAlmostEqual(m2, 1e-15));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, IsExactEqual) {
    auto m1 = Matrix3d::CreateIdentity();
    auto m2 = Matrix3d::CreateIdentity();
    EXPECT_TRUE(m1.IsExactEqual(m2));
    m2.coffs[0] = 1.000001;
    EXPECT_FALSE(m1.IsExactEqual(m2));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, Columns) {
    auto m = Matrix3d::CreateRowValues(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );

    auto colX = m.ColumnX();
    auto colY = m.ColumnY();
    auto colZ = m.ColumnZ();

    EXPECT_EQ(colX.x, 1.0);
    EXPECT_EQ(colX.y, 4.0);
    EXPECT_EQ(colX.z, 7.0);

    EXPECT_EQ(colY.x, 2.0);
    EXPECT_EQ(colY.y, 5.0);
    EXPECT_EQ(colY.z, 8.0);

    EXPECT_EQ(colZ.x, 3.0);
    EXPECT_EQ(colZ.y, 6.0);
    EXPECT_EQ(colZ.z, 9.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, Rows) {
    auto m = Matrix3d::CreateRowValues(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );

    auto rowX = m.RowX();
    auto rowY = m.RowY();
    auto rowZ = m.RowZ();

    EXPECT_EQ(rowX.x, 1.0);
    EXPECT_EQ(rowX.y, 2.0);
    EXPECT_EQ(rowX.z, 3.0);

    EXPECT_EQ(rowY.x, 4.0);
    EXPECT_EQ(rowY.y, 5.0);
    EXPECT_EQ(rowY.z, 6.0);

    EXPECT_EQ(rowZ.x, 7.0);
    EXPECT_EQ(rowZ.y, 8.0);
    EXPECT_EQ(rowZ.z, 9.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, Transpose) {
    auto m = Matrix3d::CreateRowValues(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );

    auto mt = m.Transpose();

    EXPECT_EQ(mt.coffs[0], 1.0);
    EXPECT_EQ(mt.coffs[1], 4.0);
    EXPECT_EQ(mt.coffs[2], 7.0);
    EXPECT_EQ(mt.coffs[3], 2.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, TransposeInPlace) {
    auto m = Matrix3d::CreateRowValues(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );

    m.TransposeInPlace();

    EXPECT_EQ(m.coffs[0], 1.0);
    EXPECT_EQ(m.coffs[1], 4.0);
    EXPECT_EQ(m.coffs[2], 7.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, Determinant) {
    auto m = Matrix3d::CreateIdentity();
    EXPECT_NEAR(m.Determinant(), 1.0, 1e-10);

    auto m2 = Matrix3d::CreateScale(2.0, 3.0, 4.0);
    EXPECT_NEAR(m2.Determinant(), 24.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, Inverse) {
    auto m = Matrix3d::CreateScale(2.0, 4.0, 8.0);
    Matrix3d inv;
    ASSERT_TRUE(m.Inverse(inv));

    // m * inv should be identity
    auto product = m.MultiplyMatrix(inv);
    EXPECT_TRUE(product.IsAlmostEqual(Matrix3d::CreateIdentity(), 1e-10));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, InverseSingular) {
    // Zero matrix is singular
    auto m = Matrix3d::CreateZero();
    Matrix3d inv;
    EXPECT_FALSE(m.Inverse(inv));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, MultiplyVector) {
    auto m = Matrix3d::CreateScale(2.0, 3.0, 4.0);
    auto v = Vector3d::From(1.0, 2.0, 3.0);

    auto result = m.MultiplyVector(v);
    EXPECT_NEAR(result.x, 2.0, 1e-10);
    EXPECT_NEAR(result.y, 6.0, 1e-10);
    EXPECT_NEAR(result.z, 12.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, MultiplyTransposeVector) {
    auto m = Matrix3d::CreateRowValues(
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    );
    auto v = Vector3d::From(1.0, 0.0, 0.0);

    auto result = m.MultiplyTransposeVector(v);
    // M^T * (1,0,0) = column 0 of M^T = row 0 of M = (1, 2, 3)
    EXPECT_NEAR(result.x, 1.0, 1e-10);
    EXPECT_NEAR(result.y, 2.0, 1e-10);
    EXPECT_NEAR(result.z, 3.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, MultiplyMatrix) {
    auto m1 = Matrix3d::CreateScale(2.0, 3.0, 4.0);
    auto m2 = Matrix3d::CreateScale(5.0, 6.0, 7.0);

    auto product = m1.MultiplyMatrix(m2);
    EXPECT_NEAR(product.coffs[0], 10.0, 1e-10);
    EXPECT_NEAR(product.coffs[4], 18.0, 1e-10);
    EXPECT_NEAR(product.coffs[8], 28.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, CreateRotationAroundZ) {
    double angle = M_PI / 2.0;  // 90 degrees
    auto m = Matrix3d::CreateRotationAroundZ(angle);

    auto v = Vector3d::From(1.0, 0.0, 0.0);
    auto result = m.MultiplyVector(v);

    EXPECT_NEAR(result.x, 0.0, 1e-10);
    EXPECT_NEAR(result.y, 1.0, 1e-10);
    EXPECT_NEAR(result.z, 0.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, CreateRotationAroundX) {
    double angle = M_PI / 2.0;
    auto m = Matrix3d::CreateRotationAroundX(angle);

    auto v = Vector3d::From(0.0, 1.0, 0.0);
    auto result = m.MultiplyVector(v);

    EXPECT_NEAR(result.x, 0.0, 1e-10);
    EXPECT_NEAR(result.y, 0.0, 1e-10);
    EXPECT_NEAR(result.z, 1.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, Magnitudes) {
    auto m = Matrix3d::CreateRowValues(
        1, 0, 0,
        0, 2, 0,
        0, 0, 3
    );

    EXPECT_NEAR(m.ColumnXMagnitude(), 1.0, 1e-10);
    EXPECT_NEAR(m.ColumnYMagnitude(), 2.0, 1e-10);
    EXPECT_NEAR(m.ColumnZMagnitude(), 3.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(Matrix3dTest, ColumnDotProducts) {
    auto m = Matrix3d::CreateIdentity();
    EXPECT_NEAR(m.ColumnXDotColumnY(), 0.0, 1e-10);
    EXPECT_NEAR(m.ColumnXDotColumnZ(), 0.0, 1e-10);
    EXPECT_NEAR(m.ColumnYDotColumnZ(), 0.0, 1e-10);
}

// ============================================================================
// Transform tests
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
// ============================================================================

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, DefaultIsIdentity) {
    Transform t;
    EXPECT_TRUE(t.IsIdentity());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, CreateIdentity) {
    auto t = Transform::CreateIdentity();
    EXPECT_TRUE(t.IsIdentity());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, CreateTranslation) {
    auto t = Transform::CreateTranslation(1.0, 2.0, 3.0);

    auto p = t.MultiplyPoint3d(Point3d::From(0.0, 0.0, 0.0));
    EXPECT_NEAR(p.x, 1.0, 1e-10);
    EXPECT_NEAR(p.y, 2.0, 1e-10);
    EXPECT_NEAR(p.z, 3.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, MultiplyPoint3d) {
    auto t = Transform::CreateTranslation(10.0, 20.0, 30.0);

    auto p = t.MultiplyPoint3d(Point3d::From(1.0, 2.0, 3.0));
    EXPECT_NEAR(p.x, 11.0, 1e-10);
    EXPECT_NEAR(p.y, 22.0, 1e-10);
    EXPECT_NEAR(p.z, 33.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, MultiplyVector) {
    auto t = Transform::CreateTranslation(10.0, 20.0, 30.0);

    // Vectors are not affected by translation
    auto v = t.MultiplyVector(Vector3d::From(1.0, 2.0, 3.0));
    EXPECT_NEAR(v.x, 1.0, 1e-10);
    EXPECT_NEAR(v.y, 2.0, 1e-10);
    EXPECT_NEAR(v.z, 3.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, MultiplyInversePoint3d) {
    auto t = Transform::CreateTranslation(10.0, 20.0, 30.0);

    Point3d p = Point3d::From(11.0, 22.0, 33.0);
    Point3d inv;
    ASSERT_TRUE(t.MultiplyInversePoint3d(p, inv));
    EXPECT_NEAR(inv.x, 1.0, 1e-10);
    EXPECT_NEAR(inv.y, 2.0, 1e-10);
    EXPECT_NEAR(inv.z, 3.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, Inverse) {
    auto t = Transform::CreateTranslation(10.0, 20.0, 30.0);

    Transform inv;
    ASSERT_TRUE(t.Inverse(inv));

    // t * inv should be identity
    auto product = t.MultiplyTransform(inv);
    EXPECT_TRUE(product.IsIdentity());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, MultiplyTransform) {
    auto t1 = Transform::CreateTranslation(1.0, 2.0, 3.0);
    auto t2 = Transform::CreateTranslation(4.0, 5.0, 6.0);

    auto combined = t1.MultiplyTransform(t2);

    auto p = combined.MultiplyPoint3d(Point3d::From(0.0, 0.0, 0.0));
    EXPECT_NEAR(p.x, 5.0, 1e-10);
    EXPECT_NEAR(p.y, 7.0, 1e-10);
    EXPECT_NEAR(p.z, 9.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, CreateScaleAboutPoint) {
    Point3d center = Point3d::From(1.0, 1.0, 1.0);
    auto t = Transform::CreateScaleAboutPoint(center, 2.0);

    // Center should be fixed
    auto p = t.MultiplyPoint3d(center);
    EXPECT_NEAR(p.x, center.x, 1e-10);
    EXPECT_NEAR(p.y, center.y, 1e-10);
    EXPECT_NEAR(p.z, center.z, 1e-10);

    // Point at (2,2,2) should scale to (3,3,3)
    auto p2 = t.MultiplyPoint3d(Point3d::From(2.0, 2.0, 2.0));
    EXPECT_NEAR(p2.x, 3.0, 1e-10);
    EXPECT_NEAR(p2.y, 3.0, 1e-10);
    EXPECT_NEAR(p2.z, 3.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, CreateFixedPointAndMatrix) {
    Point3d point = Point3d::From(1.0, 0.0, 0.0);
    auto rot = Matrix3d::CreateRotationAroundZ(M_PI / 2.0);
    auto t = Transform::CreateFixedPointAndMatrix(point, rot);

    // Point should be fixed
    auto p = t.MultiplyPoint3d(point);
    EXPECT_NEAR(p.x, point.x, 1e-10);
    EXPECT_NEAR(p.y, point.y, 1e-10);
    EXPECT_NEAR(p.z, point.z, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, MultiplyRange) {
    auto t = Transform::CreateTranslation(10.0, 20.0, 30.0);
    auto range = Range3d::CreateXYZXYZ(0.0, 0.0, 0.0, 1.0, 1.0, 1.0);

    auto transformed = t.MultiplyRange(range);
    EXPECT_NEAR(transformed.low.x, 10.0, 1e-10);
    EXPECT_NEAR(transformed.low.y, 20.0, 1e-10);
    EXPECT_NEAR(transformed.low.z, 30.0, 1e-10);
    EXPECT_NEAR(transformed.high.x, 11.0, 1e-10);
    EXPECT_NEAR(transformed.high.y, 21.0, 1e-10);
    EXPECT_NEAR(transformed.high.z, 31.0, 1e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, SetIdentity) {
    auto t = Transform::CreateTranslation(1.0, 2.0, 3.0);
    EXPECT_FALSE(t.IsIdentity());

    t.SetIdentity();
    EXPECT_TRUE(t.IsIdentity());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Transform.h
TEST(TransformTest, IsAlmostEqual) {
    auto t1 = Transform::CreateTranslation(1.0, 2.0, 3.0);
    auto t2 = Transform::CreateTranslation(1.0, 2.0, 3.0);
    EXPECT_TRUE(t1.IsAlmostEqual(t2));

    auto t3 = Transform::CreateTranslation(1.1, 2.0, 3.0);
    EXPECT_FALSE(t1.IsAlmostEqual(t3));
    EXPECT_TRUE(t1.IsAlmostEqual(t3, 0.2));
}

// Authored: no reference unit test exists in itwinjs core-geometry for Transform.multiplyRange(range,
// result) / multiplyPoint3dArrayInPlace (faithful ports of Transform.ts methods consumed by
// GeometryAccumulator.calculateTransform + GeometryPrimitives.collectCurveStrokes).
TEST(TransformTest, MultiplyRangeAndPointArrayInPlace) {
    Transform t = Transform::CreateTranslation(10, 20, 30);
    Range3d r(Point3d::From(0, 0, 0), Point3d::From(1, 1, 1));
    Range3d out;
    t.MultiplyRange(r, out);
    EXPECT_NEAR(out.low.x, 10.0, 1e-10);
    EXPECT_NEAR(out.low.z, 30.0, 1e-10);
    EXPECT_NEAR(out.high.x, 11.0, 1e-10);
    EXPECT_NEAR(out.high.z, 31.0, 1e-10);

    std::vector<Point3d> pts = { Point3d::From(1, 2, 3), Point3d::From(4, 5, 6) };
    t.MultiplyPoint3dArrayInPlace(pts);
    EXPECT_NEAR(pts[0].x, 11.0, 1e-10);
    EXPECT_NEAR(pts[0].y, 22.0, 1e-10);
    EXPECT_NEAR(pts[1].z, 36.0, 1e-10);
}
