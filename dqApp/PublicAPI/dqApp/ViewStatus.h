// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewStatus
// Ported from: itwinjs-core core/frontend/src/ViewStatus.ts
#pragma once

namespace dqApp {

// Describes the result of a viewing operation such as those exposed by ViewState
// and Viewport.
// Ported from: itwinjs-core ViewStatus (ViewStatus.ts:13-38) — 全量 24 值。
enum class ViewStatus : int {
    Success = 0,
    ViewNotInitialized,
    AlreadyAttached,
    NotAttached,
    DrawFailure,
    NotResized,
    ModelNotFound,
    InvalidWindow,
    MinWindow,
    MaxWindow,
    MaxZoom,
    MaxDisplayDepth,
    InvalidUpVector,
    InvalidTargetPoint,
    InvalidLens,
    InvalidViewport,
    InvalidDirection,
    NotGeolocated,
    NotCameraView,
    NotEllipsoidGlobeMode,
    NotOrthographicView,
    DegenerateGeometry,
    HeightBelowTransition,
    NoTransitionRequired,
};

}  // namespace dqApp
