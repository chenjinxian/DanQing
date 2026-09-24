// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Ellipsoid tests
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/EllipsoidPatch.test.ts
//
// 适配说明（CLAUDE.md §5(c)：类型不同仅调整断言，场景/边界/断言值不变）：
//   - GeometryCoreTestIO.capture*/saveGeometry 为参考测试的可视化 IO，不移植
//     （PolyfaceBuilder 网格仅用于截图，无断言参与）。
//   - ck.testCoordinate(a,b)  → EXPECT_NEAR(a, b, 1e-6)（Geometry.isSameCoordinate
//     默认 smallMetricDistance 绝对容差，Geometry.ts:256/353-358）。
//   - ck.testPoint3d          → 逐分量 EXPECT_NEAR(..., 1e-6)（isSamePoint3d 同容差）。
//   - ck.testExactNumber      → EXPECT_DOUBLE_EQ。
//   - ck.testPerpendicular    → isPerpendicularTo 语义（Point3dVector3d.ts:1613-1623：
//     ab² <= smallAngleRadiansSquared(1e-24)·aa·bb，零长守卫 aa|bb < 1e-12 → false）。
//   - EllipsoidPatch.intersectRay(restrictToPatch) 未移植 → IntersectRay/
//     NoIntersections 场景改用 Ellipsoid.intersectRay 直接断言同一光线分数
//     （patch 的两交点均在 patch 内，分数一致；参考断言值不变）。
#include <gtest/gtest.h>

#include <dqGeom/Ellipsoid.h>
#include <dqGeom/Geometry.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point4d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <cmath>

using namespace dqGeom;

namespace {

// Ported from: EllipsoidPatch.test.ts tippedEarthEllipsoidMatrix (:999-1003)
Matrix3d tippedEarthEllipsoidMatrix()
{
    return Matrix3d::CreateRowValues(
        2230956.046389774, 4218075.517914913, 4217984.8981983,
        -5853439.760676313, 635312.655431714, 2444174.054179583,
        1200294.0430858273, -4741818.686714196, 4079572.590348847);
}

void expectPoint3dNear(Point3d const& a, Point3d const& b, double tol, char const* msg)
{
    EXPECT_NEAR(a.x, b.x, tol) << msg;
    EXPECT_NEAR(a.y, b.y, tol) << msg;
    EXPECT_NEAR(a.z, b.z, tol) << msg;
}

// Ported from: Vector3d.isPerpendicularTo (Point3dVector3d.ts:1613-1623, 默认容差)
bool isPerpendicular(Vector3d const& a, Vector3d const& b)
{
    double const aa = a.MagnitudeSquared();
    double const bb = b.MagnitudeSquared();
    if (aa < kSmallMetricDistanceSquared || bb < kSmallMetricDistanceSquared)
        return false;
    double const ab = a.DotProduct(b);
    return ab * ab <= kSmallAngleRadiansSquared * aa * bb;
}

}  // namespace

// Ported from: EllipsoidPatch.test.ts describe("Ellipsoid") it("LocalToWorld") (:547-557)
TEST(EllipsoidTest, LocalToWorld)
{
    Ellipsoid const ellipsoid = Ellipsoid::create(
        Transform::CreateOriginAndMatrix(Point3d::FromZero(), tippedEarthEllipsoidMatrix()));
    for (auto const& angles : {LongitudeLatitudeNumber::createDegrees(0, 0),
                               LongitudeLatitudeNumber::createDegrees(20, 10)}) {
        Point3d const xyz0 = ellipsoid.radiansToPoint(angles.longitudeRadians(), angles.latitudeRadians());
        auto const uvw0 = ellipsoid.worldToLocal(xyz0);
        ASSERT_TRUE(uvw0.has_value());
        Point3d const xyz1 = ellipsoid.localToWorld(*uvw0);
        expectPoint3dNear(xyz0, xyz1, 1.0e-6, "world to local round trip");  // ck.testPoint3d
    }
}

// Ported from: EllipsoidPatch.test.ts it("NormalInversion") (:398-435)
TEST(EllipsoidTest, NormalInversion)
{
    Point3d const center = Point3d::From(1, 2, 3);
    double const degrees[] = {0, 10, 45, 80, -85, -60};  // one set of angle samples -- double it for theta
    Ellipsoid const unitSphere = Ellipsoid::create();
    Matrix3d const matrices[] = {
        Matrix3d::CreateIdentity(),
        Matrix3d::CreateRotationAroundX(Angle::DegreesToRadians(23.7))  // createRotationAroundAxisIndex(0, 23.7°)
    };
    for (auto const& matrix : matrices) {
        for (double e : {1.0, 2.0, 0.5}) {
            Ellipsoid const ellipsoid = Ellipsoid::createCenterMatrixRadii(center, &matrix, e, e, 1.0);
            for (double phiDegrees : degrees) {
                for (double thetaDegreesA : degrees) {
                    double const thetaDegrees = 2.0 * thetaDegreesA;
                    double const thetaRadians = Angle::DegreesToRadians(thetaDegrees);
                    double const phiRadians = Angle::DegreesToRadians(phiDegrees);
                    auto const tangentPlane = ellipsoid.radiansToPointAndDerivatives(thetaRadians, phiRadians);
                    Vector3d normal = Vector3d::FromCrossProduct(tangentPlane.vectorU, tangentPlane.vectorV);
                    double const normalMag = normal.Normalize();
                    ASSERT_GT(normalMag, 1.0e-10)  // unitCrossProduct defined
                        << "theta=" << thetaDegrees << " phi=" << phiDegrees;
                    auto const inverseAngles = ellipsoid.surfaceNormalToAngles(normal);
                    EXPECT_NEAR(thetaRadians, inverseAngles.longitudeRadians(), 1.0e-6);  // ck.testCoordinate
                    EXPECT_NEAR(phiRadians, inverseAngles.latitudeRadians(), 1.0e-6);     // ck.testCoordinate

                    auto const anglesOnUnitSphere = LongitudeLatitudeNumber::createRadians(thetaRadians, phiRadians);
                    auto const unitNormal = unitSphere.radiansToUnitNormalRay(thetaRadians, phiRadians);
                    ASSERT_TRUE(unitNormal.has_value());
                    auto const myAngles = ellipsoid.otherEllipsoidAnglesToThisEllipsoidAngles(
                        &unitSphere, anglesOnUnitSphere);
                    ASSERT_TRUE(myAngles.has_value());
                    auto const myUnitNormal = ellipsoid.radiansToUnitNormalRay(
                        myAngles->longitudeRadians(), myAngles->latitudeRadians());
                    ASSERT_TRUE(myUnitNormal.has_value());
                    EXPECT_NEAR(unitNormal->direction.x, myUnitNormal->direction.x, 1.0e-6);  // ck.testVector3d
                    EXPECT_NEAR(unitNormal->direction.y, myUnitNormal->direction.y, 1.0e-6);
                    EXPECT_NEAR(unitNormal->direction.z, myUnitNormal->direction.z, 1.0e-6);

                    // verify default handling in inversion (undefined → unit sphere) ...
                    auto const myAngles1 = ellipsoid.otherEllipsoidAnglesToThisEllipsoidAngles(
                        nullptr, anglesOnUnitSphere);
                    ASSERT_TRUE(myAngles1.has_value());
                    EXPECT_TRUE(myAngles->isAlmostEqual(*myAngles1))  // ck.testLongitudeLatitudeNumber
                        << "exercise default unit sphere branch in otherEllipsoidAnglesToThisEllipsoidAngles";
                }
            }
        }
    }
}

// Ported from: EllipsoidPatch.test.ts it("SectionPlanes") (:559-613)
TEST(EllipsoidTest, SectionPlanes)
{
    Matrix3d const matrixArray[] = {
        Matrix3d::CreateIdentity(),
        Matrix3d::CreateScale(1, 1, 2),
        tippedEarthEllipsoidMatrix()
    };
    Vector3d const normalArray[] = {
        Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ(),
        Vector3d::From(1, 1, 1), Vector3d::From(4, 2, 8)
    };
    Point3d const originArray[] = {
        Point3d::From(0, 0, 0), Point3d::From(0.2, 0.3, 0.5)
    };
    for (auto const& matrix : matrixArray) {
        Ellipsoid const ellipsoid = Ellipsoid::create(
            Transform::CreateOriginAndMatrix(Point3d::FromZero(), matrix));
        for (auto const& localPoint : originArray) {
            for (auto const& normal : normalArray) {
                Point3d const worldPoint = ellipsoid.transformRef().MultiplyPoint3d(localPoint);
                auto const plane = Plane3dByOriginAndUnitNormal::create(worldPoint, normal);
                ASSERT_TRUE(plane.has_value());
                auto const arc = ellipsoid.createPlaneSection(*plane);
                ASSERT_TRUE(arc.IsValid()) << "Expect good section arc";  // ck.testDefined
                for (double fraction : {0.0, 0.25, 0.6, 0.8, 0.95}) {
                    Point3d const pointOnArc = arc->FractionToPoint(fraction);
                    // ck.testTrue(plane.isPointInPlane(pointOnArc)) — Plane3d base
                    // isPointInPlane: |altitude| <= smallMetricDistance (Plane3d.ts:64-66)
                    EXPECT_LE(std::abs(plane->altitude(pointOnArc)), 1.0e-6)
                        << "section point is in the plane (fraction=" << fraction << ")";
                    auto const angles = ellipsoid.projectPointToSurface(pointOnArc);
                    ASSERT_TRUE(angles.has_value());  // ck.testDefined
                    Point3d const pointOnEllipsoid = ellipsoid.radiansToPoint(
                        angles->longitudeRadians(), angles->latitudeRadians());
                    expectPoint3dNear(pointOnArc, pointOnEllipsoid, 1.0e-6,
                                      "section point is on the ellipsoid");  // ck.testPoint3d
                }
            }
        }
    }
}

// Ported from: EllipsoidPatch.test.ts it("Silhouette") (:765-800)
TEST(EllipsoidTest, Silhouette)
{
    Point4d const eyePoints[] = {
        Point4d::create(4, 0, 0, 1),
        Point4d::create(1, 2, 3, 0),
        Point4d::create(0, 0, 1, 0)
    };
    Matrix3d const rotated = Matrix3d::CreateRotationAroundAxis(
        Vector3d::From(1, 2, 3), Angle::DegreesToRadians(32));
    Matrix3d const skewMatrix = Matrix3d::CreateRowValues(
        1, 0, 0.2,
        0.1, 3, 0.2,
        -0.3, 0.1, 2);
    Ellipsoid const ellipsoids[] = {
        Ellipsoid::createCenterMatrixRadii(Point3d::From(0, 0, 0), nullptr, 1, 1, 1),
        Ellipsoid::createCenterMatrixRadii(Point3d::From(3, 5, 1), &rotated, 1, 1.2, 2.0),
        Ellipsoid::createCenterMatrixRadii(Point3d::From(0, 5, 0), &skewMatrix, 1, 1, 1),
    };
    for (auto const& eyePoint : eyePoints) {
        for (auto const& ellipsoid : ellipsoids) {
            auto const arc = ellipsoid.silhouetteArc(eyePoint);
            if (arc.IsValid()) {
                for (double arcFraction : {0.0, 0.25, 0.4, 0.8}) {
                    Point3d const q = arc->FractionToPoint(arcFraction);
                    auto const angles = ellipsoid.projectPointToSurface(q);
                    ASSERT_TRUE(angles.has_value());  // ck.testDefined
                    EXPECT_NEAR(0.0, angles->altitude(), 1.0e-6)  // ck.testCoordinate
                        << "silhouette arc points are on ellipsoid";
                    auto const surfaceNormal = ellipsoid.radiansToUnitNormalRay(
                        angles->longitudeRadians(), angles->latitudeRadians());
                    ASSERT_TRUE(surfaceNormal.has_value());
                    Vector3d const vectorToEye = eyePoint.crossWeightedMinusPoint3d(q);
                    EXPECT_TRUE(isPerpendicular(surfaceNormal->direction, vectorToEye))  // ck.testPerpendicular
                        << "eye vector perpendicular to surface normal (fraction=" << arcFraction << ")";
                }
            }
        }
    }
}

// Ported from: EllipsoidPatch.test.ts it("SilhouetteA") (:802-822)
TEST(EllipsoidTest, SilhouetteA)
{
    Matrix3d const matrix = Matrix3d::CreateRowValues(
        2230955.696607988, 4218074.856581404, 4217984.23465426,
        -5853438.842941238, 635312.5558238369, 2444173.6696791253,
        1200293.8548970034, -4741817.943265299, 4079571.948578869);
    Point4d const eye = Point4d::create(0.3901908903099731, 0.2662862131349449, 0.8813868173585089, 0);
    Ellipsoid const ellipsoid = Ellipsoid::create(matrix);
    auto const silhouette = ellipsoid.silhouetteArc(eye);
    EXPECT_TRUE(silhouette.IsValid()) << "silhouette arc exists for exterior eye vector";

    // expect failure for interior point ..
    auto const silhouetteInside = ellipsoid.silhouetteArc(Point4d::create(100, 100, 200, 1));
    EXPECT_TRUE(silhouetteInside.IsNull());  // ck.testUndefined
}

// Ported from: EllipsoidPatch.test.ts it("IntersectRay") (:105-211)
TEST(EllipsoidTest, IntersectRay)
{
    double const radiusX = 0.9;
    double const radiusY = 1.1;
    double const radiusZ = 2.0;
    Point3d const center = Point3d::From(0.2, 0.4, -0.5);
    double const pole = Angle::kPi * 0.5;
    double const halfCircle = Angle::kPi;
    double const phiRadiansArray[] = {-pole, 0.2 * pole, pole};
    double const thetaRadiansArray[] = {-halfCircle, 0.45 * halfCircle, halfCircle, 1.3 * halfCircle};

    double const thetaFractionA = 0.3;
    double const thetaFractionB = 0.75;
    double const phiFractionA = 0.1;
    double const phiFractionB = 0.45;
    Ray3d const distantRay = Ray3d::createXYZUVW(12, 0, 1, 0.1, 0.1, 5.0);
    Ray3d const nullRay = Ray3d::createXYZUVW(1, 2, 3, 0, 0, 0);
    Transform const transform2 = Transform::CreateOriginAndMatrix(
        Point3d::From(3, 1, 2),
        Matrix3d::CreateRowValues(
            1.2, 0, 2,
            0.2, 4, 2,
            -0.2, 3, 4));
    Matrix3d const skewMatrix = Matrix3d::CreateRowValues(
        1.0, 0.2, 0.3,
        -0.2, 1.0, 0.4,
        0.1, -0.5, 1.2);  // true skewed ellipsoid
    for (auto const& ellipsoid : {
             Ellipsoid::createCenterMatrixRadii(center, nullptr, radiusX, radiusY, radiusZ),
             Ellipsoid::createCenterMatrixRadii(center, &skewMatrix, radiusX, radiusY, radiusZ)}) {
        // confirm no intersections for distant ray . . .
        EXPECT_EQ(0u, ellipsoid.intersectRay(distantRay, nullptr, nullptr, nullptr))
            << "confirm zero-intersection case";  // ck.testExactNumber(0, ...)
        EXPECT_EQ(0u, ellipsoid.intersectRay(nullRay, nullptr, nullptr, nullptr));
        // confirm transform effects . ..
        Ellipsoid const ellipsoid1 = ellipsoid.clone();
        Ellipsoid const ellipsoid2 = ellipsoid.cloneTransformed(transform2);
        EXPECT_TRUE(ellipsoid.isAlmostEqual(ellipsoid1)) << "clone is almostEqual";
        EXPECT_FALSE(ellipsoid.isAlmostEqual(ellipsoid2)) << "cloneTransformed is different";
        double const theta3Radians = 0.5;
        double const phi3Radians = 0.24;
        Point3d const point3A = ellipsoid.radiansToPoint(theta3Radians, phi3Radians);
        Point3d const point3B = ellipsoid2.radiansToPoint(theta3Radians, phi3Radians);
        Point3d const point3C = transform2.MultiplyPoint3d(point3A);
        expectPoint3dNear(point3B, point3C, 1.0e-6, "transformed ellipse point");  // ck.testPoint3d

        for (double theta0Radians : thetaRadiansArray) {
            for (double theta1Radians : thetaRadiansArray) {
                if (std::abs(theta1Radians - theta0Radians) > Angle::kPi * 2.001)
                    continue;
                if (std::abs(theta1Radians - theta0Radians) < 1.0e-8)
                    continue;
                for (double phi0Radians : phiRadiansArray) {
                    for (double phi1Radians : phiRadiansArray) {
                        if (std::abs(phi0Radians - phi1Radians) < 1.0e-8)
                            continue;
                        double const thetaA = interpolate(theta0Radians, thetaFractionA, theta1Radians);
                        double const thetaB = interpolate(theta0Radians, thetaFractionB, theta1Radians);
                        double const phiA = interpolate(phi0Radians, phiFractionA, phi1Radians);
                        double const phiB = interpolate(phi0Radians, phiFractionB, phi1Radians);
                        Point3d const pointA = ellipsoid.radiansToPoint(thetaA, phiA);
                        Point3d const pointB = ellipsoid.radiansToPoint(thetaB, phiB);
                        // Create the ray with start/end spread outside the sphere points .....
                        Ray3d const rayAB = Ray3d::FromStartEnd(
                            Point3d::FromInterpolate(pointA, -1.0, pointB),
                            Point3d::FromInterpolate(pointA, 3.0, pointB));
                        std::vector<double> fractions;
                        size_t const nHits = ellipsoid.intersectRay(rayAB, &fractions, nullptr, nullptr);
                        EXPECT_EQ(2u, nHits)  // ck.testExactNumber(2, hits.length)
                            << "Expect 2 intersections";
                        ASSERT_EQ(2u, fractions.size());
                        for (double f : fractions) {
                            EXPECT_TRUE(isSameCoordinate(0.25, f) || isSameCoordinate(0.5, f))
                                << "Expect intersections at endpoints (f=" << f << ")";
                        }
                    }
                }
            }
        }
    }
}

// Ported from: EllipsoidPatch.test.ts it("NoIntersections") (:212-264)
// 参考为回归/崩溃护栏场景（唯一断言是 ck.getNumErrors()==0，即数值路径不炸）；
// 此移植保留场景与数值，断言收缩为 API 契约：命中数与数组长度一致、分数有限。
TEST(EllipsoidTest, NoIntersections)
{
    Point3d const center = Point3d::From(19476.688224632293, 9394.94304587366, -6369311.983587892);
    Matrix3d const matrix = Matrix3d::CreateRowValues(
        2230956.046389774, 4218075.517914913, 4217984.8981983,
        -5853439.760676313, 635312.655431714, 2444174.054179583,
        1200294.0430858273, -4741818.686714196, 4079572.590348847);
    double const referenceSize = matrix.ColumnXMagnitude();
    Ray3d const ray = Ray3d::createXYZUVW(
        38729632.01074491, -5490050.664369064, 12881295.636822795,
        0.09684505394456912, 0.9848250824825425, 0.1440159450884755);
    Ellipsoid const ellipsoid = Ellipsoid::createCenterMatrixRadii(center, &matrix, 1.0, 1.0, 1.0);
    for (double rayScale : {1.0, std::sqrt(referenceSize), referenceSize}) {
        for (double placementFraction : {0.0, 0.8, 0.9, 1.0, 1.05, 1.10, 2.0}) {
            Ray3d ray1 = Ray3d::create(
                Point3d::FromInterpolate(ray.origin, placementFraction, center),
                ray.direction);
            ray1.direction.Scale(rayScale);
            std::vector<double> fractions;
            std::vector<Point3d> xyz;
            std::vector<LongitudeLatitudeNumber> angles;
            size_t const nHits = ellipsoid.intersectRay(ray1, &fractions, &xyz, &angles);
            ASSERT_EQ(nHits, fractions.size()) << "API contract: count matches fractions";
            ASSERT_EQ(nHits, xyz.size());
            ASSERT_EQ(nHits, angles.size());
            for (double f : fractions)
                EXPECT_TRUE(std::isfinite(f)) << "intersection fraction must be finite";
        }
    }
}

// Ported from: EllipsoidPatch.test.ts it("ProjectSpacePoint") (:479-546)
TEST(EllipsoidTest, ProjectSpacePoint)
{
    Point3d const center = Point3d::From(0, 0, 0);
    double const fractions[] = {-0.1, 0, 0.5, 0.9, 1.0, 1.1};
    for (double distanceFromSurface : {0.1, 0.2, -0.05}) {
        Matrix3d const matrices[] = {
            Matrix3d::CreateIdentity(),
            Matrix3d::CreateRotationAroundX(Angle::DegreesToRadians(23.7))
        };
        for (auto const& matrix : matrices) {
            for (double e : {1.0, 2.0, 0.5}) {
                Ellipsoid const ellipsoid = Ellipsoid::createCenterMatrixRadii(
                    center, &matrix, e, e, 1.0);
                auto const patch = EllipsoidPatch::createCapture(
                    ellipsoid,
                    AngleSweep::FromStartEndDegrees(0, 50),
                    AngleSweep::FromStartEndDegrees(0, 90));
                for (double thetaFraction : fractions) {
                    for (double phiFraction : fractions) {
                        auto const anglesA = patch.uvFractionToAngles(
                            thetaFraction, phiFraction, distanceFromSurface);
                        auto const rayA = patch.anglesToUnitNormalRay(anglesA);
                        ASSERT_TRUE(rayA.has_value());
                        auto const anglesB = patch.projectPointToSurface(rayA->origin);
                        ASSERT_TRUE(anglesB.has_value())  // ck.testDefined
                            << "thetaFraction=" << thetaFraction << " phiFraction=" << phiFraction;
                        auto const planeB = patch.ellipsoid.radiansToPointAndDerivatives(
                            anglesB->longitudeRadians(), anglesB->latitudeRadians(), false);
                        Vector3d const vectorAB = Vector3d::FromStartEnd(planeB.origin, rayA->origin);
                        if (!isSameCoordinate(0.0, std::abs(distanceFromSurface))) {
                            EXPECT_TRUE(isPerpendicular(vectorAB, planeB.vectorU))  // ck.testPerpendicular
                                << "vectorAB perpendicular to dTheta";
                            EXPECT_TRUE(isPerpendicular(vectorAB, planeB.vectorV))
                                << "vectorAB perpendicular to dPhi";
                        }
                        EXPECT_NEAR(std::abs(distanceFromSurface),
                                    planeB.origin.Distance(rayA->origin), 1.0e-6)  // ck.testCoordinate
                            << "projected point is at the offset distance";
                    }
                }
            }
        }
    }
}

// Ported from: EllipsoidPatch.test.ts it("Singular") (:328-361, subset — 只覆盖已移植
// 方法子集：patchRangeStartEndRadians / constant{Longitude,Latitude}Arc 未移植，略)
TEST(EllipsoidTest, Singular)
{
    // flatten to the equator.
    // normal is bad at phi = 0
    Transform const flatten = Transform::CreateOriginAndMatrix(
        Point3d::From(1, 2, 3),
        Matrix3d::CreateRowValues(
            1, 0, 0,
            0, 1, 0,
            0, 0, 0));
    Ellipsoid const ellipsoid = Ellipsoid::create(flatten);
    auto const patch = EllipsoidPatch::createCapture(
        ellipsoid,
        AngleSweep::FromStartEndDegrees(0, 180),
        AngleSweep::FromStartEndDegrees(0, 90));
    EXPECT_FALSE(patch.anglesToUnitNormalRay(LongitudeLatitudeNumber::createDegrees(0, 0, 0)).has_value());
    Point3d const equatorPoint = ellipsoid.radiansToPoint(0.1, 0.0);
    EXPECT_FALSE(ellipsoid.projectPointToSurface(equatorPoint).has_value())
        << " Project to ellipsoid fails on fold line";
    EXPECT_EQ(0u, ellipsoid.intersectRay(Ray3d::createZAxis(), nullptr, nullptr, nullptr));

    // pole predicates (Angle.isAlmostNorthOrSouthPole)
    Angle const northPole = Angle::FromDegrees(90);
    Angle const southPole = Angle::FromDegrees(-90);
    Angle const notPole = Angle::FromDegrees(10);
    EXPECT_TRUE(northPole.isAlmostNorthOrSouthPole()) << "north pole test";
    EXPECT_TRUE(southPole.isAlmostNorthOrSouthPole()) << "south pole test";
    EXPECT_FALSE(notPole.isAlmostNorthOrSouthPole()) << "+=90 degree angle test";
}
