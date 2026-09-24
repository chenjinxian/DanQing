// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom tests — Map4d + Matrix4d gap methods
// Ported from: itwinjs-core core/geometry/src/test/geometry4d/Geometry4d.test.ts
//              (describe("Geometry4d.BoxMap") + describe("Map4d") + Matrix4d "MultiplyAndRenormalize")
#include <gtest/gtest.h>

#include <dqGeom/Map4d.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <vector>

using namespace dqGeom;

// Ported from: itwinjs-core Geometry4d.test.ts:231-275 (Geometry4d.BoxMap)
TEST(Map4d, BoxMapRoundTrip)
{
    auto lowA = Point3d::From(1, 2, 4);
    auto highA = Point3d::From(2, 3, 5);
    auto lowB = Point3d::From(100, 100, 100);
    auto highB = Point3d::From(101, 101, 101);

    auto map = Map4d::CreateBoxMap(lowA, highA, lowB, highB);
    ASSERT_TRUE(map.has_value());

    // Singular inputs → nullopt (Geometry4d.test.ts:253-255).
    EXPECT_FALSE(Map4d::CreateBoxMap(lowA, highA, lowB, lowB).has_value());   // B zero-size
    EXPECT_FALSE(Map4d::CreateBoxMap(highA, highA, lowB, highB).has_value()); // A zero-size

    // Round-trip the four fraction points (Geometry4d.test.ts:259-268).
    Point3d fracs[4] = {
        Point3d::From(0.4, 0, 0),
        Point3d::From(0, 0.2, 0),
        Point3d::From(0, 0, 1.1),
        Point3d::From(0.3, 0.5, 0.2)};
    Range3d rangeA{lowA, highA};
    Range3d rangeB{lowB, highB};
    for (auto const& f : fracs) {
        Point3d pointA0 = rangeA.FractionToPoint(f.x, f.y, f.z);
        Point3d pointB0 = rangeB.FractionToPoint(f.x, f.y, f.z);
        Point3d pointB1 = map->Transform0().MultiplyPoint3dQuietNormalize(pointA0);
        Point3d pointA1 = map->Transform1().MultiplyPoint3dQuietNormalize(pointB0);
        EXPECT_NEAR(pointB1.x, pointB0.x, 1e-10);
        EXPECT_NEAR(pointB1.y, pointB0.y, 1e-10);
        EXPECT_NEAR(pointB1.z, pointB0.z, 1e-10);
        EXPECT_NEAR(pointA1.x, pointA0.x, 1e-10);
        EXPECT_NEAR(pointA1.y, pointA0.y, 1e-10);
        EXPECT_NEAR(pointA1.z, pointA0.z, 1e-10);
    }
}

// Ported from: itwinjs-core Geometry4d.test.ts:755-764 (Map4d.Create "Identity frustum")
TEST(Map4d, CreateVectorFrustumIdentity)
{
    // origin=0, u=X, v=Y, w=Z, fraction=1.0 → identity map.
    auto frustum = Map4d::CreateVectorFrustum(
        Point3d::From(0, 0, 0),
        Vector3d::From(1, 0, 0),
        Vector3d::From(0, 1, 0),
        Vector3d::From(0, 0, 1),
        1.0);
    ASSERT_TRUE(frustum.has_value());
    EXPECT_TRUE(frustum->IsAlmostEqual(Map4d::CreateIdentity()));
}

// Ported from: itwinjs-core Geometry4d.test.ts:256-258 (singular axes) + :839-849 (VectorFrustum inverts)
TEST(Map4d, CreateVectorFrustumSingularAndInverts)
{
    // u/v/w not independent (uY repeated as w) → slabToWorld not invertible → nullopt.
    EXPECT_FALSE(Map4d::CreateVectorFrustum(
        Point3d::From(0, 0, 1),
        Vector3d::From(1, 0, 0),
        Vector3d::From(0, 1, 0),
        Vector3d::From(0, 1, 0),
        0.8).has_value());

    // A real perspective frustum: transform0 * transform1 == identity.
    auto origin = Point3d::From(111.55210256687462, -8.3610081610513802, -10.253043196228713);
    auto u = Vector3d::From(62.386308713014060, 0, 0);
    auto v = Vector3d::From(0, 62.386308713014060, 0);
    auto w = Vector3d::From(31.041154356507047, 31.041154356507032, 31.041154356507036);
    double fraction = 0.0048728640349349457;
    auto f = Map4d::CreateVectorFrustum(origin, u, v, w, fraction);
    ASSERT_TRUE(f.has_value());
    auto product = f->Transform0().MultiplyMatrixMatrix(f->Transform1());
    EXPECT_TRUE(product.IsIdentity(1.0e-9));
}

// Ported from: itwinjs-core Geometry4d.test.ts:733-753 (Map4d.Create — reverseInPlace/setFrom/setIdentity)
TEST(Map4d, CreateTransformReverseAndSetFrom)
{
    auto const fixedPoint = Point3d::From(1, 2, 3);
    auto const scaleTransform = Transform::CreateScaleAboutPoint(fixedPoint, 2.5);
    Transform scaleInverse;
    ASSERT_TRUE(scaleTransform.Inverse(scaleInverse));

    // Mismatch pair → nullopt.
    EXPECT_FALSE(Map4d::CreateTransform(scaleTransform, &scaleTransform).has_value());
    auto scaleMap = Map4d::CreateTransform(scaleTransform, &scaleInverse);
    ASSERT_TRUE(scaleMap.has_value());
    EXPECT_FALSE(Map4d::CreateIdentity().IsAlmostEqual(*scaleMap));

    // reverseInPlace swaps the pair; equals CreateRefs(t1, t0).
    auto reversed = scaleMap->Clone();
    reversed.ReverseInPlace();
    auto swapped = Map4d::CreateRefs(scaleMap->Transform1().clone(), scaleMap->Transform0().clone());
    EXPECT_TRUE(swapped.IsAlmostEqual(reversed));

    // setFrom / setIdentity.
    Map4d b = Map4d::CreateIdentity();
    b.SetFrom(*scaleMap);
    EXPECT_TRUE(b.IsAlmostEqual(*scaleMap));
    b.SetIdentity();
    EXPECT_TRUE(b.IsAlmostEqual(Map4d::CreateIdentity()));
}

// Ported from: itwinjs-core Geometry4d.test.ts:378-400 (Matrix4d "MultiplyAndRenormalize")
TEST(Matrix4d, MultiplyPoint3dArrayQuietNormalize)
{
    auto m = Matrix4d::CreateRowValues(
        10, 2, 3, 4,
        5, 20, 2, 1,
        4, 6, 30, 2,
        3, 2, 1, 30);
    std::vector<Point3d> pts = {Point3d::From(3, 2, 5), Point3d::From(2, 9, -4)};
    m.MultiplyPoint3dArrayQuietNormalize(pts);
    // (3,2,5): xyz pre-div = (53, 66, 176), w = 48.
    EXPECT_NEAR(pts[0].x, 53.0 / 48.0, 1e-12);
    EXPECT_NEAR(pts[0].y, 66.0 / 48.0, 1e-12);
    EXPECT_NEAR(pts[0].z, 176.0 / 48.0, 1e-12);
    // (2,9,-4): xyz pre-div = (30, 183, -56), w = 50.
    EXPECT_NEAR(pts[1].x, 30.0 / 50.0, 1e-12);
    EXPECT_NEAR(pts[1].y, 183.0 / 50.0, 1e-12);
    EXPECT_NEAR(pts[1].z, -56.0 / 50.0, 1e-12);
}
