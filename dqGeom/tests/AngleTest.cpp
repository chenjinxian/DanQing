// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Angle tests
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
#include <gtest/gtest.h>

#include <dqGeom/Angle.h>

using namespace dqGeom;

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, DefaultIsZero)
{
    Angle a;
    EXPECT_DOUBLE_EQ(a.Radians(), 0.0);
    EXPECT_DOUBLE_EQ(a.Degrees(), 0.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, FromRadians)
{
    Angle a = Angle::FromRadians(Angle::kPi);
    EXPECT_DOUBLE_EQ(a.Radians(), Angle::kPi);
    EXPECT_NEAR(a.Degrees(), 180.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, FromDegrees)
{
    Angle a = Angle::FromDegrees(90.0);
    EXPECT_NEAR(a.Radians(), Angle::kPiOver2, 1.0e-10);
    EXPECT_DOUBLE_EQ(a.Degrees(), 90.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, FromAtan2)
{
    Angle a = Angle::FromAtan2(1.0, 0.0);
    EXPECT_NEAR(a.Radians(), Angle::kPiOver2, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, Zero)
{
    Angle a = Angle::Zero();
    EXPECT_DOUBLE_EQ(a.Radians(), 0.0);
    EXPECT_TRUE(a.IsAlmostZero());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, FullCircle)
{
    Angle a = Angle::FullCircle();
    EXPECT_TRUE(a.IsFullCircle());
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, TrigFunctions)
{
    Angle a = Angle::FromDegrees(45.0);
    EXPECT_NEAR(a.Cos(), std::cos(Angle::kPiOver4), 1.0e-10);
    EXPECT_NEAR(a.Sin(), std::sin(Angle::kPiOver4), 1.0e-10);
    EXPECT_NEAR(a.Tan(), 1.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, NormalizeTo02Pi)
{
    Angle a = Angle::FromRadians(-Angle::kPiOver2);
    Angle n = a.NormalizeTo02Pi();
    EXPECT_NEAR(n.Radians(), 1.5 * Angle::kPi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, NormalizeToPlusMinusPi)
{
    Angle a = Angle::FromRadians(1.5 * Angle::kPi);
    Angle n = a.NormalizeToPlusMinusPi();
    EXPECT_NEAR(n.Radians(), -0.5 * Angle::kPi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, IsAlmostEqual)
{
    Angle a = Angle::FromRadians(1.0);
    Angle b = Angle::FromRadians(1.0 + 1.0e-14);
    EXPECT_TRUE(a.IsAlmostEqual(b));
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
TEST(AngleTest, StaticUtilities)
{
    EXPECT_NEAR(Angle::DegreesToRadians(180.0), Angle::kPi, 1.0e-10);
    EXPECT_NEAR(Angle::RadiansToDegrees(Angle::kPi), 180.0, 1.0e-10);
    EXPECT_NEAR(Angle::AdjustRadians0To2Pi(-0.1), Angle::k2Pi - 0.1, 1.0e-10);
    EXPECT_TRUE(Angle::IsFullCircleRadians(Angle::k2Pi));
    EXPECT_TRUE(Angle::IsHalfCircleRadians(Angle::kPi));
}
