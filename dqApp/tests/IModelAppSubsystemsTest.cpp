// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — fidelity tests for the IModelApp frontend subsystem skeletons.
// These assert the ported C++ values match the itwinjs-core reference exactly,
// closing the runtime-coverage gap for the SnapMode bitmask and QuantityFormatter
// default (previously only asserted against the stubs_test copy, not the real headers).

#include <gtest/gtest.h>

#include "dqApp/AccuSnap.h"
#include "dqApp/QuantityFormatter.h"

using namespace dqApp;

// Ported from: itwinjs-core core/frontend/src/HitDetail.ts:22-32
//              (SnapMode enum — reference defines a bit-flag enum, not sequential)
TEST(IModelAppSubsystemsTest, SnapModeBitmaskMatchesReference)
{
    // The reference enum is a bitmask: each mode is a distinct power of two so
    // multiple snap modes can be active at once (itwinjs snapModes arrays).
    EXPECT_EQ(static_cast<int>(SnapMode::Nearest), 1);
    EXPECT_EQ(static_cast<int>(SnapMode::NearestKeypoint), 1 << 1);
    EXPECT_EQ(static_cast<int>(SnapMode::MidPoint), 1 << 2);
    EXPECT_EQ(static_cast<int>(SnapMode::Center), 1 << 3);
    EXPECT_EQ(static_cast<int>(SnapMode::Origin), 1 << 4);
    EXPECT_EQ(static_cast<int>(SnapMode::Bisector), 1 << 5);
    EXPECT_EQ(static_cast<int>(SnapMode::Intersection), 1 << 6);
    EXPECT_EQ(static_cast<int>(SnapMode::PerpendicularPoint), 1 << 7);
    EXPECT_EQ(static_cast<int>(SnapMode::TangentPoint), 1 << 8);
}

// Ported from: itwinjs-core core/frontend/src/HitDetail.ts:22-32
//              (SnapMode used as combinable bit flags)
TEST(IModelAppSubsystemsTest, SnapModeFlagsAreCombinable)
{
    // Reference combines snap modes as bit flags; OR-ing two must yield their
    // union without collision (e.g. Nearest | Center == 1 | 8 == 9).
    const auto combined =
        static_cast<int>(SnapMode::Nearest) | static_cast<int>(SnapMode::Center);
    EXPECT_EQ(combined, 1 | 8);
    EXPECT_NE(static_cast<int>(SnapMode::TangentPoint), static_cast<int>(SnapMode::Origin));
}

// Ported from: itwinjs-core core/frontend/src/quantity-formatting/QuantityFormatter.ts:407
//              (_activeUnitSystem: UnitSystemKey = "imperial")
TEST(IModelAppSubsystemsTest, QuantityFormatterDefaultsToImperial)
{
    QuantityFormatter qf;
    EXPECT_EQ(qf.getActiveUnitSystem(), "imperial");
}

// Authored: no reference test exists in itwinjs-core for the C++ setter/getter round-trip
TEST(IModelAppSubsystemsTest, QuantityFormatterRoundTripsUnitSystem)
{
    QuantityFormatter qf;
    qf.setActiveUnitSystem("metric");
    EXPECT_EQ(qf.getActiveUnitSystem(), "metric");
    qf.setActiveUnitSystem("usCustomary");
    EXPECT_EQ(qf.getActiveUnitSystem(), "usCustomary");
}
