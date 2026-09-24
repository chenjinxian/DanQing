// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Ray3d tests
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/Ray3d.test.ts
#include <gtest/gtest.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Ray3d.h>

using namespace dqGeom;

// Ported from: itwinjs-core Ray3d.test.ts "Ray3d.IntersectWithPlane" (:255-274)。
// 近平行光线必须按 conditionalDivideFraction（largeFractionResult=1e10）挡回
// undefined——交点参数冲出上限时不得返回（BackgroundMapGeometry 深度污染回归，
// 见 Ray3d.h 注释与 SkyRotateDiag 的 Iso→Left 实例）。
TEST(Ray3dTest, IntersectWithPlane)
{
    auto plane = Plane3dByOriginAndUnitNormal::createXYZUVW(1, 3, 2, 5, 2, 9);
    ASSERT_TRUE(plane.has_value());
    auto rayA = Ray3d::FromOriginAndDirection(Point3d{5, 4, 2}, Vector3d{1, 3, 2});
    auto rayB = Ray3d::FromOriginAndDirection(Point3d{5, 4, 1}, Vector3d{-2, 5, 0});

    auto fractionA = rayA.intersectionWithPlane(*plane);
    if (fractionA.has_value()) {
        EXPECT_TRUE(std::isfinite(*fractionA));
        auto point = rayA.FractionToPoint(*fractionA);
        EXPECT_NEAR(0.0, plane->altitude(point), 1.0e-8)  // ck.testCoordinate(0, plane.altitude(point))
            << "intersection point must lie on the plane";
    } else {
        FAIL() << "rayA must intersect the plane (fractionA defined in reference)";
    }

    auto fractionB = rayB.intersectionWithPlane(*plane);
    EXPECT_FALSE(fractionB.has_value())  // ck.testUndefined(fractionB, "Detect ray parallel to plane")
        << "ray parallel to plane must return undefined";

    // This pair generates aDotN and UDotN both near zero (:266-273)。
    auto planeQ = Plane3dByOriginAndUnitNormal::createXYZUVW(
        101.38054428331306, -7.136947376249823, 14.575798896766162,
        1.4069516995683865e-17, 1.0468826690441132e-32, 1);
    ASSERT_TRUE(planeQ.has_value());
    auto rayQ = Ray3d::FromOriginAndDirection(
        Point3d{95.87780347429201, -7.1369473762498234, 14.575798896766187},
        Vector3d{-1, -4.61132646190051e-31, 4.567684502237405e-15});
    EXPECT_FALSE(rayQ.intersectionWithPlane(*planeQ).has_value())  // ck.testUndefined
        << "near-zero aDotN/uDotN pair must return undefined (largeFractionResult cap)";
}
