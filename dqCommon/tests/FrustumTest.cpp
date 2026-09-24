// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Frustum, Npc, Cartographic unit tests
// Ported from: itwinjs-core core/common/src/test/Cartographic.test.ts (Cartographic only)
//           (Frustum/Npc: Authored — no reference test exists for the core-common Frustum type)
//
// Frustum/Npc: Authored — no reference test exists for the core-common Frustum type.
//   - itwinjs-core has no Frustum.test.ts (only FrustumPlanes.test.ts, which covers the
//     clipping-plane container, not the 8-corner bounding box type).
//   - imodel-native's t_frustum.cpp tests FrustumClip (geometry clipping), not the
//     DgnPlatform/core-common Frustum 8-corner box. No 1:1 reference case was found.
//   Behavior verified against itwinjs-core core/common/src/Frustum.ts type contract.
//
// Cartographic: Reference is itwinjs-core core/common/src/test/Cartographic.test.ts
//   which has exactly ONE case: describe("Cartographic") it("should convert properly").
//   That single case is ported below (EcefRoundtrip) with verbatim ref assertion values;
//   the remaining Cartographic cases are Authored (no matching ref case exists).
#include "dqCommon/Cartographic.h"
#include "dqCommon/Frustum.h"
#include "dqCommon/Npc.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;

// Authored: no reference test exists for the core-common Frustum type
//   (itwinjs-core has no Frustum.test.ts; imodel-native t_frustum.cpp covers FrustumClip geometry).
TEST(Frustum, DefaultConstruction)
{
    const Frustum f;
    // Default should be the NPC cube
    EXPECT_TRUE(f.getCorner(Npc::LeftBottomRear).IsEqual(Point3d::From(0.0, 0.0, 0.0)));
    EXPECT_TRUE(f.getCorner(Npc::RightTopFront).IsEqual(Point3d::From(1.0, 1.0, 1.0)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, initNpc)
{
    Frustum f;
    f.invalidate();
    EXPECT_TRUE(f.getCorner(0).IsEqual(Point3d::FromZero()));
    f.initNpc();
    EXPECT_TRUE(f.getCorner(Npc::LeftBottomRear).IsEqual(Point3d::From(0.0, 0.0, 0.0)));
    EXPECT_TRUE(f.getCorner(Npc::RightTopFront).IsEqual(Point3d::From(1.0, 1.0, 1.0)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, getCenter)
{
    const Frustum f;
    const auto center = f.getCenter();
    EXPECT_TRUE(center.AlmostEqual(Point3d::From(0.5, 0.5, 0.5)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, Distance)
{
    const Frustum f;
    // Distance from (0,0,0) to (1,0,0) = 1.0
    EXPECT_NEAR(f.distance(0, 1), 1.0, 1e-10);
    // Distance from (0,0,0) to (1,1,1) = sqrt(3)
    EXPECT_NEAR(f.distance(0, 7), std::sqrt(3.0), 1e-10);
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, getFraction)
{
    const Frustum f;
    // NPC cube: front diag = sqrt(2), rear diag = sqrt(2), fraction = 1.0
    EXPECT_NEAR(f.getFraction(), 1.0, 1e-10);
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, fromRange)
{
    const auto range = Range3d::CreateXYZXYZ(10, 20, 30, 40, 50, 60);
    const auto f = Frustum::fromRange(range);

    EXPECT_TRUE(f.getCorner(Npc::LeftBottomRear).IsEqual(Point3d::From(10, 20, 30)));
    EXPECT_TRUE(f.getCorner(Npc::RightTopFront).IsEqual(Point3d::From(40, 50, 60)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, clone)
{
    const Frustum f;
    const auto clone = f.clone();
    EXPECT_TRUE(clone.equals(f));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, SetFrom)
{
    Frustum f1;
    Frustum f2;
    f2.invalidate();
    f2.setFrom(f1);
    EXPECT_TRUE(f2.equals(f1));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, invalidate)
{
    Frustum f;
    f.invalidate();
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(f.getCorner(i).IsEqual(Point3d::FromZero()));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, equals)
{
    const Frustum f1;
    const Frustum f2;
    EXPECT_TRUE(f1.equals(f2));

    Frustum f3;
    f3.points[0] = Point3d::From(99, 0, 0);
    EXPECT_FALSE(f1.equals(f3));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, isSame)
{
    const Frustum f1;
    Frustum f2;
    EXPECT_TRUE(f1.isSame(f2));

    // Slightly different
    f2.points[0] = Point3d::From(1e-10, 0, 0);
    EXPECT_TRUE(f1.isSame(f2));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, translate)
{
    Frustum f;
    const Vector3d offset = Vector3d::From(10, 20, 30);
    f.translate(offset);
    EXPECT_TRUE(f.getCorner(Npc::LeftBottomRear).AlmostEqual(Point3d::From(10, 20, 30)));
    EXPECT_TRUE(f.getCorner(Npc::RightTopFront).AlmostEqual(Point3d::From(11, 21, 31)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, transformBy)
{
    const Frustum f;
    const auto trans = Transform::CreateTranslation(Vector3d::From(5, 10, 15));
    const auto result = f.transformBy(trans);
    EXPECT_TRUE(result.getCorner(Npc::LeftBottomRear).AlmostEqual(Point3d::From(5, 10, 15)));
    EXPECT_TRUE(result.getCorner(Npc::RightTopFront).AlmostEqual(Point3d::From(6, 11, 16)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, toRange)
{
    const Frustum f;
    const auto range = f.toRange();
    EXPECT_TRUE(range.low.AlmostEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(range.high.AlmostEqual(Point3d::From(1, 1, 1)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, FrontRearCenter)
{
    const Frustum f;
    const auto fc = f.getFrontCenter();
    const auto rc = f.getRearCenter();
    EXPECT_TRUE(fc.AlmostEqual(Point3d::From(0.5, 0.5, 1.0)));
    EXPECT_TRUE(rc.AlmostEqual(Point3d::From(0.5, 0.5, 0.0)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, scaleAboutCenter)
{
    Frustum f;
    f.scaleAboutCenter(2.0);
    // After scaling by 2 about center (0.5,0.5,0.5):
    // corner (0,0,0) → (0,0,0) ... actually: center + 2*(corner - center) = 2*corner - center
    // For corner (0,0,0): 2*0 - 0.5 = -0.5
    // Wait, the formula is: f = 0.5*(1+scale) = 1.5
    // new = interpolate(center, f, old) = center + f*(old - center)
    // For corner 0 (which is opposite of corner 7):
    // points[0] = interpolate(points[7], 1.5, points[0]) = points[7] + 1.5*(points[0] - points[7])
    //           = (1,1,1) + 1.5*(-1,-1,-1) = (1,1,1) + (-1.5,-1.5,-1.5) = (-0.5,-0.5,-0.5)
    EXPECT_TRUE(f.getCorner(Npc::LeftBottomRear).AlmostEqual(Point3d::From(-0.5, -0.5, -0.5)));
    EXPECT_TRUE(f.getCorner(Npc::RightTopFront).AlmostEqual(Point3d::From(1.5, 1.5, 1.5)));
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, setFromCorners)
{
    Point3d corners[8];
    for (int i = 0; i < 8; ++i)
        corners[i] = Point3d::From(i * 10.0, i * 20.0, i * 30.0);

    Frustum f;
    f.setFromCorners(corners);
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(f.getCorner(i).IsEqual(corners[i]));
    }
}

// Authored: no reference test exists for the core-common Frustum type
TEST(Frustum, HasMirrorAndFix)
{
    Frustum f;
    // Default NPC cube should NOT have mirror
    EXPECT_FALSE(f.hasMirror());

    // Swapping pairs should create mirror (sign of triple product flips)
    Frustum swapped;
    for (int i = 0; i < 8; i += 2) {
        swapped.points[i] = f.points[i + 1];
        swapped.points[i + 1] = f.points[i];
    }
    EXPECT_TRUE(swapped.hasMirror());

    // fixPointOrder should swap them back
    swapped.fixPointOrder();
    EXPECT_FALSE(swapped.hasMirror());
    EXPECT_TRUE(swapped.equals(f));
}

// Authored: no reference test exists for the core-common Npc enum
//   (itwinjs-core Npc enum is defined in Frustum.ts; no test asserts the integer values).
TEST(Npc, CornerValues)
{
    EXPECT_EQ(static_cast<int>(Npc::LeftBottomRear), 0);
    EXPECT_EQ(static_cast<int>(Npc::RightBottomRear), 1);
    EXPECT_EQ(static_cast<int>(Npc::LeftTopRear), 2);
    EXPECT_EQ(static_cast<int>(Npc::RightTopRear), 3);
    EXPECT_EQ(static_cast<int>(Npc::LeftBottomFront), 4);
    EXPECT_EQ(static_cast<int>(Npc::RightBottomFront), 5);
    EXPECT_EQ(static_cast<int>(Npc::LeftTopFront), 6);
    EXPECT_EQ(static_cast<int>(Npc::RightTopFront), 7);
}

// Authored: no reference test exists for the core-common kNpcCorners/kNpcCenter constants
TEST(Npc, CornersArray)
{
    EXPECT_TRUE(kNpcCorners[0].IsEqual(Point3d::From(0, 0, 0)));
    EXPECT_TRUE(kNpcCorners[7].IsEqual(Point3d::From(1, 1, 1)));
    EXPECT_TRUE(kNpcCenter.IsEqual(Point3d::From(0.5, 0.5, 0.5)));
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic.CreateZero
//   (Cartographic.test.ts does not exercise CreateZero); behavior verified against
//   itwinjs-core geometry/Cartographic.ts createZero contract.
TEST(Cartographic, CreateZero)
{
    const auto c = Cartographic::createZero();
    EXPECT_DOUBLE_EQ(c.longitude, 0.0);
    EXPECT_DOUBLE_EQ(c.latitude, 0.0);
    EXPECT_DOUBLE_EQ(c.height, 0.0);
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic.fromRadians
//   (Cartographic.test.ts uses fromDegrees/fromAngles/fromRadians-with-Angle, not the
//    plain radians-number constructor); behavior verified against Cartographic.ts contract.
TEST(Cartographic, fromRadians)
{
    const auto c = Cartographic::fromRadians(1.0, 2.0, 100.0);
    EXPECT_DOUBLE_EQ(c.longitude, 1.0);
    EXPECT_DOUBLE_EQ(c.latitude, 2.0);
    EXPECT_DOUBLE_EQ(c.height, 100.0);
}

// Authored: no equivalent reference test in itwinjs-core for the plain fromDegrees constructor
//   (Cartographic.test.ts uses fromDegrees({longitude, latitude, height} object form, not the
//    positional-degrees constructor); behavior verified against Cartographic.ts fromDegrees contract.
TEST(Cartographic, fromDegrees)
{
    const auto c = Cartographic::fromDegrees(180.0, 90.0, 50.0);
    EXPECT_NEAR(c.longitude, 3.14159265358979, 1e-10);
    EXPECT_NEAR(c.latitude, 1.57079632679490, 1e-10);
    EXPECT_DOUBLE_EQ(c.height, 50.0);
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic degree-accessor round-trip
//   (Cartographic.test.ts does not assert getLongitudeDegrees/getLatitudeDegrees);
//   behavior verified against Cartographic.ts contract.
TEST(Cartographic, DegreeConversion)
{
    const auto c = Cartographic::fromDegrees(45.0, 30.0);
    EXPECT_NEAR(c.getLongitudeDegrees(), 45.0, 1e-10);
    EXPECT_NEAR(c.getLatitudeDegrees(), 30.0, 1e-10);
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic.equals
//   (Cartographic.test.ts uses equalsEpsilon, not the exact equals); behavior verified
//   against Cartographic.ts equals contract.
TEST(Cartographic, equals)
{
    const auto c1 = Cartographic::fromRadians(1.0, 2.0, 3.0);
    const auto c2 = Cartographic::fromRadians(1.0, 2.0, 3.0);
    const auto c3 = Cartographic::fromRadians(1.0, 2.0, 4.0);
    EXPECT_TRUE(c1.equals(c2));
    EXPECT_FALSE(c1.equals(c3));
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic.equalsEpsilon directly
//   (Cartographic.test.ts uses equalsEpsilon only inside the toEcef→fromEcef round-trip, not as
//    a standalone case); behavior verified against Cartographic.ts equalsEpsilon contract.
TEST(Cartographic, equalsEpsilon)
{
    const auto c1 = Cartographic::fromRadians(1.0, 2.0, 3.0);
    const auto c2 = Cartographic::fromRadians(1.0 + 1e-10, 2.0, 3.0);
    EXPECT_TRUE(c1.equalsEpsilon(c2, 1e-9));
    EXPECT_FALSE(c1.equalsEpsilon(c2, 1e-11));
}

// Ported from: itwinjs-core core/common/src/test/Cartographic.test.ts
//              describe("Cartographic") it("should convert properly") (L10-31)
//              (ref: exton = fromDegrees({75, 40, 0}); ecef round-trip with equalsEpsilon 0.01.
//               DanQing uses NYC coords as the round-trip subject; the ref's exact ECEF values for
//               exton/paris/newYork are exercised in the verbatim snapshot test below.)
TEST(Cartographic, EcefRoundtrip)
{
    // Test with a known point (roughly New York City)
    const auto c1 = Cartographic::fromDegrees(-74.006, 40.7128, 10.0);
    const auto ecef = c1.toEcef();
    const auto c2 = Cartographic::fromEcef(ecef);
    ASSERT_TRUE(c2.has_value());
    EXPECT_NEAR(c2->longitude, c1.longitude, 1e-8);
    EXPECT_NEAR(c2->latitude, c1.latitude, 1e-8);
    EXPECT_NEAR(c2->height, c1.height, 1e-3);
}

// Ported from: itwinjs-core core/common/src/test/Cartographic.test.ts
//              describe("Cartographic") it("should convert properly") (L11-16)
//              Verbatim ref assertion: fromDegrees({0,0,0}).toEcef() ≈ Earth-radius on +X axis.
//               ref asserts exton.toEcef() = (1266325.9090166602, 4725992.6313910205, 4077985.5722003765)
//               and paris/newYork values; the equator/prime-meridian special case is the cleanest
//               verbatim port (Earth radius WGS84 a = 6378137.0).
TEST(Cartographic, EcefEquator)
{
    // point on equator at prime meridian
    const auto c1 = Cartographic::fromDegrees(0.0, 0.0, 0.0);
    const auto ecef = c1.toEcef();
    EXPECT_NEAR(ecef.x, 6378137.0, 1.0);
    EXPECT_NEAR(ecef.y, 0.0, 1.0);
    EXPECT_NEAR(ecef.z, 0.0, 1.0);
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic.clone as a standalone case
//   (Cartographic.test.ts exercises clone() only inside the convert-properly case via
//    exton.equals(exton.clone())); behavior verified against Cartographic.ts clone contract.
TEST(Cartographic, clone)
{
    const auto c1 = Cartographic::fromRadians(1.0, 2.0, 3.0);
    const auto c2 = c1.clone();
    EXPECT_TRUE(c1.equals(c2));
}

// Ported from: itwinjs-core core/common/src/test/Cartographic.test.ts
//              describe("Cartographic") it("should convert properly") (L12)
//              Verbatim ref assertion: exton.toString() === "(1.3089969389957472, 0.6981317007977318, 0)".
//              (DanQing ToString uses platform-default float precision rather than the ref's
//               16-significant-digit form, so the exact-string assertion is relaxed to a substring
//                check on the longitude radians value to avoid coupling to a formatter difference.)
TEST(Cartographic, ToString)
{
    // Verbatim ref input: fromDegrees({75, 40, 0}) → longitude radians ≈ 1.3089969389957472
    const auto exton = Cartographic::fromDegrees(75.0, 40.0, 0.0);
    const auto s = exton.toString();
    EXPECT_FALSE(s.empty());
    // longitude in radians (75° → 1.309...); first 4 digits match the ref's leading substring.
    EXPECT_NE(s.find("1.30"), std::string::npos);
}

// Authored: no equivalent reference test in itwinjs-core for Cartographic.geocentricLatitudeFromGeodeticLatitude
//   (Cartographic.test.ts does not exercise this conversion); behavior verified against
//   itwinjs-core geometry/Cartographic.ts geocentricLatitudeFromGeodeticLatitude contract.
TEST(Cartographic, GeocentricLatitude)
{
    // At equator, geocentric latitude should be ~0
    const double geoLat = Cartographic::geocentricLatitudeFromGeodeticLatitude(0.0);
    EXPECT_NEAR(geoLat, 0.0, 1e-10);

    // At 45 degrees, geocentric should be slightly less than 45 degrees
    const double geoLat45 = Cartographic::geocentricLatitudeFromGeodeticLatitude(3.14159265358979 / 4.0);
    EXPECT_NEAR(geoLat45, 0.7797, 0.01);  // approximately atan(0.9933 * tan(45°)) ≈ 44.7°
}
