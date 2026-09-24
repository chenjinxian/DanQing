// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Standard view rotations implementation
// Ported from: itwinjs-core core/frontend/src/StandardView.ts
//              getMatrices() function (lines 31-54)
#include "dqApp/StandardView.h"

namespace dqApp {

// Ported from: itwinjs-core StandardView.ts getMatrices() (lines 31-54)
// Each matrix defines the view's local coordinate frame:
//   Row 0 = X axis (right), Row 1 = Y axis (up), Row 2 = Z axis (look direction, inverted)
dqGeom::Matrix3d StandardView::GetStandardRotation(StandardViewId id)
{
    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Top] = Matrix3d.identity (line 37)
    static const dqGeom::Matrix3d s_top = dqGeom::Matrix3d::CreateIdentity();

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Bottom] (line 38)
    // Matrix3d.createRowValues(1, 0, 0, 0, -1, 0, 0, 0, -1)
    static const dqGeom::Matrix3d s_bottom = dqGeom::Matrix3d::CreateRowValues(
        1, 0, 0,
        0, -1, 0,
        0, 0, -1);

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Left] (line 39)
    // Matrix3d.createRowValues(0, -1, 0, 0, 0, 1, -1, 0, 0)
    static const dqGeom::Matrix3d s_left = dqGeom::Matrix3d::CreateRowValues(
        0, -1, 0,
        0, 0, 1,
        -1, 0, 0);

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Right] (line 40)
    // Matrix3d.createRowValues(0, 1, 0, 0, 0, 1, 1, 0, 0)
    static const dqGeom::Matrix3d s_right = dqGeom::Matrix3d::CreateRowValues(
        0, 1, 0,
        0, 0, 1,
        1, 0, 0);

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Front] (line 41)
    // Matrix3d.createRowValues(1, 0, 0, 0, 0, 1, 0, -1, 0)
    static const dqGeom::Matrix3d s_front = dqGeom::Matrix3d::CreateRowValues(
        1, 0, 0,
        0, 0, 1,
        0, -1, 0);

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Back] (line 42)
    // Matrix3d.createRowValues(-1, 0, 0, 0, 0, 1, 0, 1, 0)
    static const dqGeom::Matrix3d s_back = dqGeom::Matrix3d::CreateRowValues(
        -1, 0, 0,
        0, 0, 1,
        0, 1, 0);

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.Iso] (lines 43-46)
    // Matrix3d.createRowValues(
    //   0.707106781186548, -0.70710678118654757, 0.00000000000000000,
    //   0.408248290463863, 0.40824829046386302, 0.81649658092772603,
    //   -0.577350269189626, -0.57735026918962573, 0.57735026918962573)
    static const dqGeom::Matrix3d s_iso = dqGeom::Matrix3d::CreateRowValues(
        0.707106781186548, -0.70710678118654757, 0.00000000000000000,
        0.408248290463863, 0.40824829046386302, 0.81649658092772603,
        -0.577350269189626, -0.57735026918962573, 0.57735026918962573);

    // Ported from: itwinjs-core standardViewMatrices[StandardViewId.RightIso] (lines 47-50)
    // Matrix3d.createRowValues(
    //   0.707106781186548, 0.70710678118654757, 0.00000000000000000,
    //   -0.408248290463863, 0.40824829046386302, 0.81649658092772603,
    //   0.577350269189626, -0.57735026918962573, 0.57735026918962573)
    static const dqGeom::Matrix3d s_rightIso = dqGeom::Matrix3d::CreateRowValues(
        0.707106781186548, 0.70710678118654757, 0.00000000000000000,
        -0.408248290463863, 0.40824829046386302, 0.81649658092772603,
        0.577350269189626, -0.57735026918962573, 0.57735026918962573);

    switch (id) {
        case StandardViewId::Top:       return s_top;
        case StandardViewId::Bottom:    return s_bottom;
        case StandardViewId::Left:      return s_left;
        case StandardViewId::Right:     return s_right;
        case StandardViewId::Front:     return s_front;
        case StandardViewId::Back:      return s_back;
        case StandardViewId::Iso:       return s_iso;
        case StandardViewId::RightIso:  return s_rightIso;
        default:                        return s_top;
    }
}

// Ported from: itwinjs-core StandardView.adjustToStandardRotation() (StandardView.ts:85-93).
// getMatrices().some(test => { if (test.maxDiff(matrix) > 1.0e-7) return false;
// matrix.setFrom(test); return true; }) — the first standard rotation within the
// reference's literal 1e-7 snaps `matrix` to it (in place); no match leaves it unchanged.
void StandardView::adjustToStandardRotation(dqGeom::Matrix3d& matrix)
{
    // StandardView.ts:87 — the reference rejects when test.maxDiff(matrix) > 1.0e-7, i.e.
    // when the max absolute entry difference exceeds 1e-7. isAlmostEqual(other, tol) returns
    // maxDiff <= tol, so the accept predicate is isAlmostEqual(matrix, 1e-7). Do NOT use
    // isAlmostEqual's default tol (kSmallMetricDistance = 1e-6) — that would widen the
    // acceptance band 10x and accept matrices the reference rejects.
    static constexpr double kStandardRotationTol = 1.0e-7;

    for (int i = static_cast<int>(StandardViewId::Top); i <= static_cast<int>(StandardViewId::RightIso); ++i) {
        auto const test = GetStandardRotation(static_cast<StandardViewId>(i));
        if (!test.IsAlmostEqual(matrix, kStandardRotationTol))
            continue;
        matrix = test;  // matrix.setFrom(test) — copy all 9 coffs (Matrix3d is a value struct)
        return;
    }
}

}  // namespace dqApp
