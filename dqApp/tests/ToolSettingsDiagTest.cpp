// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ToolSettings (mutable static class) + ScreenViewport.animation
// contract tests.
//
// Authored: no reference test exists in itwinjs-core for ToolSettings defaults
// (TS statics have no unit test); the values are locked field-by-field against
// ToolSettings.ts:18-85 and Viewport.ts:3125-3160 (the DiagnosticsPanel
// ToolSettingsTracker edits these live — 2026-09-21 Debug Info 全量任务).
#include <dqApp/ToolSettings.h>
#include <dqApp/Viewport.h>

#include <gtest/gtest.h>

using namespace dqApp;

// Authored: 见文件头 — defaults locked to ToolSettings.ts.
TEST(ToolSettingsDiag, DefaultsMatchReference)
{
    EXPECT_EQ(ToolSettings::doubleTapTimeout.ToMilliseconds(), 250);   // :18
    EXPECT_EQ(ToolSettings::doubleClickTimeout.ToMilliseconds(), 500); // :20
    EXPECT_DOUBLE_EQ(ToolSettings::doubleClickToleranceInches, 0.05);  // :22
    EXPECT_FALSE(ToolSettings::enableVirtualCursorForLocate);          // :24
    EXPECT_TRUE(ToolSettings::preserveWorldUp);                        // :26
    EXPECT_EQ(ToolSettings::touchMoveDelay.ToMilliseconds(), 50);      // :28
    EXPECT_EQ(ToolSettings::startDragDelay.ToMilliseconds(), 110);     // :30
    EXPECT_DOUBLE_EQ(ToolSettings::touchMoveDistanceInches, 0.1);      // :32
    EXPECT_DOUBLE_EQ(ToolSettings::startDragDistanceInches, 0.15);     // :34
    EXPECT_DOUBLE_EQ(ToolSettings::touchZoomChangeThresholdInches, 0.20); // :36
    EXPECT_DOUBLE_EQ(ToolSettings::viewToolPickRadiusInches, 0.20);    // :38
    EXPECT_DOUBLE_EQ(ToolSettings::walkCameraAngle.Degrees(), 75.6);   // :40
    EXPECT_FALSE(ToolSettings::walkEnforceZUp);                        // :42
    EXPECT_DOUBLE_EQ(ToolSettings::walkVelocity, 3.5);                 // :44
    EXPECT_EQ(ToolSettings::walkVelocityChange, 0);                    // :46
    EXPECT_TRUE(ToolSettings::walkRequestPointerLock);                 // :48
    EXPECT_FALSE(ToolSettings::walkCollisions);                        // :50
    EXPECT_FALSE(ToolSettings::walkDetectFloor);                       // :52
    EXPECT_DOUBLE_EQ(ToolSettings::walkStepHeight, 0.3);               // :54
    EXPECT_DOUBLE_EQ(ToolSettings::walkEyeHeight, 1.6);                // :56
    EXPECT_DOUBLE_EQ(ToolSettings::wheelLineFactor, 40);               // :58
    EXPECT_DOUBLE_EQ(ToolSettings::wheelPageFactor, 120);              // :60
    EXPECT_DOUBLE_EQ(ToolSettings::wheelZoomBumpDistance, 0.01);       // :62 Constant.oneCentimeter
    EXPECT_DOUBLE_EQ(ToolSettings::scrollSpeed, 0.75);                 // :64
    EXPECT_DOUBLE_EQ(ToolSettings::zoomSpeed, 10);                     // :66
    EXPECT_DOUBLE_EQ(ToolSettings::wheelZoomRatio, 1.5);               // :68
    EXPECT_TRUE(ToolSettings::viewingInertia.enabled);                 // :72
    EXPECT_DOUBLE_EQ(ToolSettings::viewingInertia.damping, 0.96);      // :74
    EXPECT_EQ(ToolSettings::viewingInertia.duration.ToMilliseconds(), 500); // :76
    EXPECT_EQ(ToolSettings::maxOnMotionSnapCallPerSecond, 15);         // :79
    EXPECT_FALSE(ToolSettings::enableVolumeSelection);                 // :81
    EXPECT_TRUE(ToolSettings::escapeMovesFocusToHome);                 // :84
}

// Authored: 见文件头 — runtime mutability (the ToolSettingsTracker edit path).
TEST(ToolSettingsDiag, EditableAtRuntime)
{
    auto const savedRatio = ToolSettings::wheelZoomRatio;
    auto const savedAngle = ToolSettings::walkCameraAngle;
    auto const savedDuration = ToolSettings::viewingInertia.duration;

    ToolSettings::wheelZoomRatio = 1.75;
    ToolSettings::walkCameraAngle.SetDegrees(60.0);
    ToolSettings::viewingInertia.duration = dqBase::DqDuration::FromMilliseconds(1500);

    EXPECT_DOUBLE_EQ(ToolSettings::wheelZoomRatio, 1.75);
    EXPECT_DOUBLE_EQ(ToolSettings::walkCameraAngle.Degrees(), 60.0);
    EXPECT_EQ(ToolSettings::viewingInertia.duration.ToMilliseconds(), 1500);

    // Restore (static state — other tests read the defaults).
    ToolSettings::wheelZoomRatio = savedRatio;
    ToolSettings::walkCameraAngle = savedAngle;
    ToolSettings::viewingInertia.duration = savedDuration;
}

// Authored: 见文件头 — ScreenViewport.animation defaults (Viewport.ts:3126-3158)
// + the ToolSettingsTracker "Animation Duration (ms)" edit path.
TEST(ToolSettingsDiag, ScreenViewportAnimationDefaultsAndEdit)
{
    auto& anim = Viewport::animation();
    EXPECT_DOUBLE_EQ(anim.time.fast, 500.0);     // :3127 fromSeconds(.5)
    EXPECT_DOUBLE_EQ(anim.time.normal, 1000.0);  // :3128 fromSeconds(1.0)
    EXPECT_DOUBLE_EQ(anim.time.slow, 1250.0);    // :3129 fromSeconds(1.25)
    EXPECT_DOUBLE_EQ(anim.time.wheel, 500.0);    // :3131 fromSeconds(.5)

    anim.time.normal = 750.0;  // ToolSettingsTracker.ts:63-68 edit
    EXPECT_DOUBLE_EQ(Viewport::animation().time.normal, 750.0);
    anim.time.normal = 1000.0;  // restore
}
