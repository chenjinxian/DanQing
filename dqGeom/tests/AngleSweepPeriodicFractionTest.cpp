// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — AngleSweep periodic-fraction tests
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/Angle.test.ts
//              describe("Angle.radiansToPositivePeriodicFractionStartEnd") (:833-900)
//              it("SmallSweep") (:428-435)
#include <gtest/gtest.h>

#include <dqGeom/AngleSweep.h>
#include <dqGeom/Geometry.h>

#include <cmath>

using namespace dqGeom;

namespace {
void expectSameCoordinate(double a, double b, char const* msg)
{
    EXPECT_TRUE(isSameCoordinate(a, b)) << msg << " (" << a << " vs " << b << ")";
}
}  // namespace

// Ported from: Angle.test.ts it("Angle.10and(20,30)") (:835-842)
TEST(AngleSweepPeriodicFractionTest, Radians10And20To30)
{
    double const radians = Angle::kPi / 18.0;   // 10 degrees
    double const radians0 = Angle::kPi / 9.0;   // 20 degrees
    double const radians1 = Angle::kPi / 6.0;   // 30 degrees
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(35.0, outputFraction, "(10-20)/(30-20) = -10/10 ==> 350/10 = 35");
}

// Ported from: Angle.test.ts it("Angle.25and(20,30)") (:843-850)
TEST(AngleSweepPeriodicFractionTest, Radians25And20To30)
{
    double const radians = 5.0 * Angle::kPi / 36.0;   // 25 degrees
    double const radians0 = Angle::kPi / 9.0;
    double const radians1 = Angle::kPi / 6.0;
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(0.5, outputFraction, "(25-20)/(30-20) = 5/10 = 0.5");
}

// Ported from: Angle.test.ts it("Angle.40and(20,30)") (:851-858)
TEST(AngleSweepPeriodicFractionTest, Radians40And20To30)
{
    double const radians = 2.0 * Angle::kPi / 9.0;   // 40 degrees
    double const radians0 = Angle::kPi / 9.0;
    double const radians1 = Angle::kPi / 6.0;
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(2.0, outputFraction, "(40-20)/(30-20) = 20/10 = 2");
}

// Ported from: Angle.test.ts it("Angle.385and(20,30)") (:859-866)
TEST(AngleSweepPeriodicFractionTest, Radians385And20To30)
{
    double const radians = 2.0 * Angle::kPi + 5.0 * Angle::kPi / 36.0;   // 360+25 = 385 degrees
    double const radians0 = Angle::kPi / 9.0;
    double const radians1 = Angle::kPi / 6.0;
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(0.5, outputFraction, "(385-20)/(30-20) = 365/10 ==> 5/10 = 0.5");
}

// Ported from: Angle.test.ts it("Angle.10and(30,20)") (:867-874)
TEST(AngleSweepPeriodicFractionTest, Radians10And30To20)
{
    double const radians = Angle::kPi / 18.0;   // 10 degrees
    double const radians0 = Angle::kPi / 6.0;   // 30 degrees
    double const radians1 = Angle::kPi / 9.0;   // 20 degrees
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(2.0, outputFraction, "(10-30)/(20-30) = -20/-10 ==> 20/10 = 2");
}

// Ported from: Angle.test.ts it("Angle.25and(30,20)") (:875-882)
TEST(AngleSweepPeriodicFractionTest, Radians25And30To20)
{
    double const radians = 5.0 * Angle::kPi / 36.0;   // 25 degrees
    double const radians0 = Angle::kPi / 6.0;
    double const radians1 = Angle::kPi / 9.0;
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(0.5, outputFraction, "(25-30)/(20-30) = -5/-10 ==> 5/10 = 0.5");
}

// Ported from: Angle.test.ts it("Angle.40and(30,20)") (:883-890)
TEST(AngleSweepPeriodicFractionTest, Radians40And30To20)
{
    double const radians = 2.0 * Angle::kPi / 9.0;   // 40 degrees
    double const radians0 = Angle::kPi / 6.0;
    double const radians1 = Angle::kPi / 9.0;
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(35.0, outputFraction, "(40-30)/(20-30) = 10/-10 ==> -10/10 ==> 350/10 = 35");
}

// Ported from: Angle.test.ts it("Angle.385and(30,20)") (:891-898)
TEST(AngleSweepPeriodicFractionTest, Radians385And30To20)
{
    double const radians = 2.0 * Angle::kPi + 5.0 * Angle::kPi / 36.0;   // 360+25 = 385 degrees
    double const radians0 = Angle::kPi / 6.0;
    double const radians1 = Angle::kPi / 9.0;
    double const outputFraction = AngleSweep::radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1);
    expectSameCoordinate(0.5, outputFraction, "(385-30)/(20-30) = 355/-10 ==> -355/10 ==> 5/10 = 0.5");
}

// Ported from: Angle.test.ts it("SmallSweep") (:428-435)
TEST(AngleSweepPeriodicFractionTest, SmallSweep)
{
    double const defaultFraction = 3.0;
    AngleSweep const sweep = AngleSweep::FromStartEndRadians(0.14859042783429374, 0.14859042783429377);
    double const f = sweep.radiansToPositivePeriodicFraction(3.2901830814240864, defaultFraction);
    EXPECT_NEAR(defaultFraction, f, 1.0e-6);  // ck.testCoordinate(f, defaultFraction)
}
