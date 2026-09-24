// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — settings that control the behavior of built-in tools
//
// Ported from: itwinjs-core core/frontend/src/tools/ToolSettings.ts
//
// The reference is a class of MUTABLE static properties ("Applications may
// modify these values"). C++ form: static inline data members — call sites
// read/write `dqApp::ToolSettings::preserveWorldUp` etc., 1:1 with
// `ToolSettings.preserveWorldUp`. This replaces the former constexpr k*
// constants in ViewTool.h (kept only the subset Task 9 needed; full port
// lands here — the DiagnosticsPanel ToolSettingsTracker edits these live).
#pragma once

#include "DqApp.h"

#include <dqBase/DqTime.h>
#include <dqGeom/Angle.h>

#include <cstdint>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

// Settings that control the behavior of built-in tools. Applications may modify these values.
// Ported from: itwinjs-core ToolSettings (tools/ToolSettings.ts) — full class, field by field.
class DQ_APP_EXPORT ToolSettings {
public:
    ToolSettings() = delete;  // static-only class (TS: all members static)

    // Two tap must be within this period to be a double tap.
    // Ported from: ToolSettings.ts:18 doubleTapTimeout.
    static inline dqBase::DqDuration doubleTapTimeout = dqBase::DqDuration::FromMilliseconds(250);
    // Two clicks must be within this period to be a double click.
    // Ported from: ToolSettings.ts:20 doubleClickTimeout.
    static inline dqBase::DqDuration doubleClickTimeout = dqBase::DqDuration::FromMilliseconds(500);
    // Number of screen inches of movement allowed between clicks to still qualify as a double-click.
    // Ported from: ToolSettings.ts:22 doubleClickToleranceInches.
    static inline double doubleClickToleranceInches = 0.05;
    // Use virtual cursor to help with locating elements using touch input. By default it's only enabled for snapping.
    // Ported from: ToolSettings.ts:24 enableVirtualCursorForLocate.
    static inline bool enableVirtualCursorForLocate = false;
    // If true, view rotation tool keeps the up vector (worldZ) aligned with screenY.
    // Ported from: ToolSettings.ts:26 preserveWorldUp.
    static inline bool preserveWorldUp = true;
    // Delay with a touch on the surface before a move operation begins.
    // Ported from: ToolSettings.ts:28 touchMoveDelay.
    static inline dqBase::DqDuration touchMoveDelay = dqBase::DqDuration::FromMilliseconds(50);
    // Delay with the mouse down before a drag operation begins.
    // Ported from: ToolSettings.ts:30 startDragDelay.
    static inline dqBase::DqDuration startDragDelay = dqBase::DqDuration::FromMilliseconds(110);
    // Distance in screen inches a touch point must move before being considered motion.
    // Ported from: ToolSettings.ts:32 touchMoveDistanceInches.
    static inline double touchMoveDistanceInches = 0.1;
    // Distance in screen inches the cursor must move before a drag operation begins.
    // Ported from: ToolSettings.ts:34 startDragDistanceInches.
    static inline double startDragDistanceInches = 0.15;
    // Distance in screen inches touch points must move apart to be considered a change in zoom scale.
    // Ported from: ToolSettings.ts:36 touchZoomChangeThresholdInches.
    static inline double touchZoomChangeThresholdInches = 0.20;
    // Radius in screen inches to search for elements that anchor viewing operations.
    // Ported from: ToolSettings.ts:38 viewToolPickRadiusInches.
    static inline double viewToolPickRadiusInches = 0.20;
    // Camera angle enforced for walk tool.
    // Ported from: ToolSettings.ts:40 walkCameraAngle (Angle.createDegrees(75.6)).
    static inline dqGeom::Angle walkCameraAngle = dqGeom::Angle::FromDegrees(75.6);
    // Whether the walk tool enforces worldZ be aligned with screenY.
    // Ported from: ToolSettings.ts:42 walkEnforceZUp.
    static inline bool walkEnforceZUp = false;
    // Speed, in meters per second, for the walk tool.
    // Ported from: ToolSettings.ts:44 walkVelocity.
    static inline double walkVelocity = 3.5;
    // Integer increment used to compute a walkVelocity multiplier for the look and move tool, capped at 10x.
    // Ported from: ToolSettings.ts:46 walkVelocityChange.
    static inline int32_t walkVelocityChange = 0;
    // Whether the walk tool requests pointer lock to hide the cursor for mouse look.
    // Ported from: ToolSettings.ts:48 walkRequestPointerLock.
    static inline bool walkRequestPointerLock = true;
    // Whether the look and move tool detects collisions while moving forward.
    // Ported from: ToolSettings.ts:50 walkCollisions.
    static inline bool walkCollisions = false;
    // Whether the look and move tool adjusts the camera height for stairs/ramps when collisions are enabled.
    // Ported from: ToolSettings.ts:52 walkDetectFloor.
    static inline bool walkDetectFloor = false;
    // Maximum step height in meters above floor/ground to use when floor detection is enabled.
    // Ported from: ToolSettings.ts:54 walkStepHeight.
    static inline double walkStepHeight = 0.3;
    // Camera height in meters above floor/ground to use for set up walk tool.
    // Ported from: ToolSettings.ts:56 walkEyeHeight.
    static inline double walkEyeHeight = 1.6;
    // Scale factor applied for wheel events with "per-line" modifier.
    // Ported from: ToolSettings.ts:58 wheelLineFactor.
    static inline double wheelLineFactor = 40;
    // Scale factor applied for wheel events with "per-page" modifier.
    // Ported from: ToolSettings.ts:60 wheelPageFactor.
    static inline double wheelPageFactor = 120;
    // When the zoom-with-wheel tool (with camera enabled) gets closer than this distance to an obstacle, it "bumps" through.
    // Ported from: ToolSettings.ts:62 wheelZoomBumpDistance (= Constant.oneCentimeter, Constant.ts:18 = 0.01).
    static inline double wheelZoomBumpDistance = 0.01;
    // The speed to scroll for the "scroll view" tool (distance per second).
    // Ported from: ToolSettings.ts:64 scrollSpeed.
    static inline double scrollSpeed = 0.75;
    // The speed to zoom for the "zoom view" tool.
    // Ported from: ToolSettings.ts:66 zoomSpeed.
    static inline double zoomSpeed = 10;
    // Scale factor for zooming with mouse wheel.
    // Ported from: ToolSettings.ts:68 wheelZoomRatio.
    static inline double wheelZoomRatio = 1.5;

    // Parameters for viewing operations with *inertia* (i.e. they continue briefly if used with a throwing action).
    // Ported from: ToolSettings.ts:70-77 viewingInertia (nested object → nested struct).
    struct ViewingInertia {
        // Flag to enable inertia.
        // Ported from: ToolSettings.ts:72 enabled.
        bool enabled = true;
        // How quickly the inertia decays. The smaller the damping value the faster the inertia decays. Must be less than 1.0.
        // Ported from: ToolSettings.ts:74 damping.
        double damping = 0.96;
        // Maximum duration of the inertia operation. Important when frame rates are low.
        // Ported from: ToolSettings.ts:76 duration.
        dqBase::DqDuration duration = dqBase::DqDuration::FromMilliseconds(500);
    };
    static inline ViewingInertia viewingInertia;

    // Maximum number of times in a second the accuSnap tool's onMotion function is called.
    // Ported from: ToolSettings.ts:79 maxOnMotionSnapCallPerSecond.
    static inline int32_t maxOnMotionSnapCallPerSecond = 15;
    // If true, drag box selection will accept spatial elements that are inside or overlap a clip volume instead of only what is visible in the view.
    // Ported from: ToolSettings.ts:81 enableVolumeSelection.
    static inline bool enableVolumeSelection = false;
    // If true, pressing Escape key sets focus to Home to allow shortcuts to be used.
    // Ported from: ToolSettings.ts:84 escapeMovesFocusToHome.
    static inline bool escapeMovesFocusToHome = true;
};

END_DQ_APP_NAMESPACE
