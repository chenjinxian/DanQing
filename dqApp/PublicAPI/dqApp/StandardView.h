// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Standard view rotations
// Ported from: itwinjs-core core/frontend/src/StandardView.ts
#pragma once

#include "Export.h"

#include <dqGeom/Matrix3d.h>

#include <cstdint>

namespace dqApp {

// ---------------------------------------------------------------------------
// StandardViewId — commonly-used view rotations
// Ported from: itwinjs-core StandardViewId enum (StandardView.ts:14-27)
// ---------------------------------------------------------------------------
enum class StandardViewId : int8_t {
    NotStandard = -1,
    Top = 0,
    Bottom = 1,
    Left = 2,
    Right = 3,
    Front = 4,
    Back = 5,
    Iso = 6,
    RightIso = 7,
};

// ---------------------------------------------------------------------------
// StandardView — access to standard rotation matrices
// Ported from: itwinjs-core StandardView class (StandardView.ts:59-75)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT StandardView {
public:
    // Ported from: itwinjs-core StandardView.top (StandardView.ts:60)
    static dqGeom::Matrix3d Top() { return GetStandardRotation(StandardViewId::Top); }

    // Ported from: itwinjs-core StandardView.bottom (StandardView.ts:61)
    static dqGeom::Matrix3d Bottom() { return GetStandardRotation(StandardViewId::Bottom); }

    // Ported from: itwinjs-core StandardView.left (StandardView.ts:62)
    static dqGeom::Matrix3d Left() { return GetStandardRotation(StandardViewId::Left); }

    // Ported from: itwinjs-core StandardView.right (StandardView.ts:63)
    static dqGeom::Matrix3d Right() { return GetStandardRotation(StandardViewId::Right); }

    // Ported from: itwinjs-core StandardView.front (StandardView.ts:64)
    static dqGeom::Matrix3d Front() { return GetStandardRotation(StandardViewId::Front); }

    // Ported from: itwinjs-core StandardView.back (StandardView.ts:65)
    static dqGeom::Matrix3d Back() { return GetStandardRotation(StandardViewId::Back); }

    // Ported from: itwinjs-core StandardView.iso (StandardView.ts:66)
    static dqGeom::Matrix3d Iso() { return GetStandardRotation(StandardViewId::Iso); }

    // Ported from: itwinjs-core StandardView.rightIso (StandardView.ts:67)
    static dqGeom::Matrix3d RightIso() { return GetStandardRotation(StandardViewId::RightIso); }

    // Ported from: itwinjs-core StandardView.getStandardRotation() (StandardView.ts:73-75)
    static dqGeom::Matrix3d GetStandardRotation(StandardViewId id);

    // Ported from: itwinjs-core StandardView.adjustToStandardRotation() (StandardView.ts:85-93).
    // Snaps `matrix` in place to the nearest standard rotation whose max-entry-difference
    // from it is <= 1e-7 (the reference's literal threshold, NOT isAlmostEqual's 1e-6);
    // leaves `matrix` unchanged if no standard rotation is within tolerance.
    static void adjustToStandardRotation(dqGeom::Matrix3d& matrix);

private:
    StandardView() = delete;
};

}  // namespace dqApp
