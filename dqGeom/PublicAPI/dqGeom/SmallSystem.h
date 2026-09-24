// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — SmallSystem (small dense linear systems)
//
// Ported from: itwinjs-core core/geometry/src/numerics/SmallSystem.ts
//
// Subset: linearSystem2d (consumed by Newton2dUnboundedWithDerivative on the
// Ellipsoid projectPointToSurface path). The rest of SmallSystem is TODO.
#pragma once

#include "Export.h"
#include "Geometry.h"
#include "Point2d.h"

namespace dqGeom {

/// Ported from: itwinjs-core SmallSystem (SmallSystem.ts)
struct DQ_GEOM_EXPORT SmallSystem {
    /// Solve the 2x2 system [ux vx; uy vy] * (s,t) = (cx,cy) by Cramer's rule.
    /// Returns false (and sets result to (0,0)) when the divide fails.
    /// Ported from: SmallSystem.linearSystem2d (SmallSystem.ts:346-363)
    // §3.4 type adaptation: reference result is Vector2d; dqGeom has no Vector2d
    // (Point2d.h:26 forward-declares it as 待移植) — Point2d carries the same x,y
    // pair. Newton2dUnboundedWithDerivative likewise stores its uv/step as Point2d.
    static bool linearSystem2d(
        double ux, double vx,   // first row of matrix
        double uy, double vy,   // second row of matrix
        double cx, double cy,   // right side
        Point2d& result) noexcept {
        double const uv = crossProductXYXY(ux, uy, vx, vy);
        double const cv = crossProductXYXY(cx, cy, vx, vy);
        double const cu = crossProductXYXY(ux, uy, cx, cy);
        auto const s = conditionalDivideFraction(cv, uv);
        auto const t = conditionalDivideFraction(cu, uv);
        if (s.has_value() && t.has_value()) {
            result.x = *s;
            result.y = *t;
            return true;
        }
        result.x = 0.0;
        result.y = 0.0;
        return false;
    }
};

} // namespace dqGeom
