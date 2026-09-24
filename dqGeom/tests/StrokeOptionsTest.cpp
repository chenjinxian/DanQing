// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — StrokeOptions tests
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
#include <gtest/gtest.h>

#include <dqGeom/StrokeOptions.h>

using namespace dqGeom;

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, DefaultValues)
{
    StrokeOptions opts;
    EXPECT_DOUBLE_EQ(opts.chordTol, 0.0);
    EXPECT_DOUBLE_EQ(opts.angleTolRadians, 0.0);
    EXPECT_DOUBLE_EQ(opts.maxEdgeLength, 0.0);
    EXPECT_EQ(opts.defaultCircleStrokes, 16);
    EXPECT_FALSE(opts.shouldTriangulate);
    EXPECT_TRUE(opts.needTwoSided);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, CreateForCurves)
{
    StrokeOptions opts = StrokeOptions::CreateForCurves();
    EXPECT_NEAR(opts.angleTolRadians, Angle::DegreesToRadians(15.0), 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, CreateForFacets)
{
    StrokeOptions opts = StrokeOptions::CreateForFacets();
    EXPECT_NEAR(opts.angleTolRadians, Angle::DegreesToRadians(22.5), 1.0e-10);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyTolerancesToArc_AngleTol)
{
    StrokeOptions opts;
    opts.angleTolRadians = Angle::DegreesToRadians(90.0);

    // Full circle with 90° tolerance → 4 strokes
    int count = opts.ApplyTolerancesToArc(1.0, Angle::k2Pi);
    EXPECT_GE(count, 4);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyTolerancesToArc_FullCircle)
{
    StrokeOptions opts;
    opts.angleTolRadians = Angle::DegreesToRadians(15.0);

    // Full circle with 15° tolerance → 24 strokes (360/15)
    int count = opts.ApplyTolerancesToArc(1.0, Angle::k2Pi);
    EXPECT_GE(count, 24);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyTolerancesToArc_HalfCircle)
{
    StrokeOptions opts;
    opts.angleTolRadians = Angle::DegreesToRadians(90.0);

    // Half circle with 90° tolerance → 2 strokes
    int count = opts.ApplyTolerancesToArc(1.0, Angle::kPi);
    EXPECT_GE(count, 2);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyTolerancesToArc_MaxEdgeLength)
{
    StrokeOptions opts;
    opts.maxEdgeLength = 1.0;

    // Circle with radius 5, circumference ≈ 31.4, max edge 1 → 32 strokes
    int count = opts.ApplyTolerancesToArc(5.0, Angle::k2Pi);
    EXPECT_GE(count, 31);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyTolerancesToArc_MinStrokes)
{
    StrokeOptions opts;
    opts.minStrokesPerPrimitive = 100;

    int count = opts.ApplyTolerancesToArc(1.0, Angle::kPiOver2);
    EXPECT_GE(count, 100);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyTolerancesToArc_ZeroSweep)
{
    StrokeOptions opts;
    opts.angleTolRadians = Angle::DegreesToRadians(15.0);

    int count = opts.ApplyTolerancesToArc(1.0, 0.0);
    EXPECT_EQ(count, 1);
}

// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/StrokeOptions.h
TEST(StrokeOptionsTest, ApplyToLine)
{
    StrokeOptions opts;
    EXPECT_EQ(opts.ApplyToLine(), 1);

    opts.minStrokesPerPrimitive = 5;
    EXPECT_EQ(opts.ApplyToLine(), 5);
}
