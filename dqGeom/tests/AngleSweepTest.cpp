// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — AngleSweep tests
// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
#include <gtest/gtest.h>

#include <dqGeom/AngleSweep.h>

using namespace dqGeom;

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, DefaultIsEmpty)
{
    AngleSweep s;
    EXPECT_TRUE(s.isEmpty());
    EXPECT_DOUBLE_EQ(s.SweepRadians(), 0.0);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, FromStartEndRadians)
{
    AngleSweep s = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    EXPECT_DOUBLE_EQ(s.StartRadians(), 0.0);
    EXPECT_DOUBLE_EQ(s.EndRadians(), Angle::kPi);
    EXPECT_NEAR(s.SweepRadians(), Angle::kPi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, FromStartSweepRadians)
{
    AngleSweep s = AngleSweep::FromStartSweepRadians(Angle::kPiOver2, Angle::kPi);
    EXPECT_DOUBLE_EQ(s.StartRadians(), Angle::kPiOver2);
    EXPECT_NEAR(s.EndRadians(), 1.5 * Angle::kPi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, FullCircle)
{
    AngleSweep s = AngleSweep::FullCircle();
    EXPECT_TRUE(s.IsFullCircle());
    EXPECT_NEAR(s.SweepRadians(), Angle::k2Pi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, FractionToRadians)
{
    AngleSweep s = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    EXPECT_DOUBLE_EQ(s.FractionToRadians(0.0), 0.0);
    EXPECT_NEAR(s.FractionToRadians(0.5), Angle::kPiOver2, 1.0e-10);
    EXPECT_NEAR(s.FractionToRadians(1.0), Angle::kPi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, RadiansToFraction)
{
    AngleSweep s = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    EXPECT_NEAR(s.RadiansToFraction(Angle::kPiOver2), 0.5, 1.0e-10);
    EXPECT_NEAR(s.RadiansToFraction(0.0), 0.0, 1.0e-10);
    EXPECT_NEAR(s.RadiansToFraction(Angle::kPi), 1.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, Degrees)
{
    AngleSweep s = AngleSweep::FromStartEndDegrees(0.0, 180.0);
    EXPECT_DOUBLE_EQ(s.StartDegrees(), 0.0);
    EXPECT_DOUBLE_EQ(s.EndDegrees(), 180.0);
    EXPECT_NEAR(s.SweepDegrees(), 180.0, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, ReverseInPlace)
{
    AngleSweep s = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    s.ReverseInPlace();
    EXPECT_DOUBLE_EQ(s.StartRadians(), Angle::kPi);
    EXPECT_DOUBLE_EQ(s.EndRadians(), 0.0);
    EXPECT_NEAR(s.SweepRadians(), -Angle::kPi, 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/geom/src/AngleSweep.cpp
TEST(AngleSweepTest, IsAlmostEqual)
{
    AngleSweep a = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    AngleSweep b = AngleSweep::FromStartEndRadians(0.0, Angle::kPi);
    EXPECT_TRUE(a.IsAlmostEqual(b));
}
