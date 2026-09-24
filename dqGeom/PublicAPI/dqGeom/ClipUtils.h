// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipUtils (clipper containment enum + interval selection)
//
// Ported from: itwinjs-core core/geometry/src/clipping/ClipUtils.ts
//
// Subset: ClipPlaneContainment enum + ClipUtilities.selectIntervals01 — consumed
// by the section-arc clip chain (ClipPlane / ConvexClipPlaneSet
// announceClippedArcIntervals, BackgroundMapGeometry depth fitting). The rest of
// ClipUtils.ts (Clipper interface, polygon clippers, unions) is TODO.
#pragma once

#include "Arc3d.h"
#include "CurvePrimitive.h"
#include "Export.h"
#include "Geometry.h"

#include <algorithm>
#include <vector>

namespace dqGeom {

/// Enumeration for polygon containment classification.
/// Ported from: itwinjs-core ClipPlaneContainment (ClipUtils.ts:48-55)
enum class ClipPlaneContainment : int {
    /// All points inside.
    StronglyInside = 1,
    /// Inside/outside state unknown.
    Ambiguous = 2,
    /// All points outside.
    StronglyOutside = 3,
};

/// Class whose various static methods are functions for clipping geometry.
/// Ported from: itwinjs-core ClipUtilities (ClipUtils.ts:158+, subset)
struct DQ_GEOM_EXPORT ClipUtilities {
    /// Augment the unsortedFractions with 0 and 1, sort, test the midpoint of each
    /// interval with clipper.isPointOnOrInside, and pass accepted intervals to
    /// announce(f0, f1, curve).
    /// Ported from: ClipUtilities.selectIntervals01 (ClipUtils.ts:171-202)
    // §3.4 模板适配：参考的 clipper 形参是 Clipper 接口（isPointOnOrInside）；
    // 已移植的调用方是 ClipPlane 与 ConvexClipPlaneSet 两个具体类，模板化避免
    // 引入 Clipper 虚接口（随接口移植再收敛）。curve 同样收窄为 Arc3d（唯一
    // 已移植生产者）。
    // NOTE 参考怪癖 1:1 保留：announce 非空时恒返回 false（ClipUtils.ts:192-201 —
    // announce 分支不置 true）；无 announce 时遇首个 inside 区间即 return true。
    template <typename ClipperT>
    static bool selectIntervals01(
        Arc3d const& curve,
        std::vector<double>& unsortedFractions,
        ClipperT const& clipper,
        AnnounceNumberNumberCurvePrimitive const& announce) {
        unsortedFractions.push_back(0.0);
        unsortedFractions.push_back(1.0);
        std::sort(unsortedFractions.begin(), unsortedFractions.end());
        double f0 = unsortedFractions[0];
        for (size_t i = 1; i < unsortedFractions.size(); ++i) {
            double const f1 = unsortedFractions[i];
            if (f1 > f0 + kSmallFraction) {   // Geometry.smallFraction
                double const fMid = 0.5 * (f0 + f1);
                if (fMid >= 0.0 && fMid <= 1.0) {
                    Point3d const testPoint = curve.FractionToPoint(fMid);
                    if (clipper.isPointOnOrInside(testPoint)) {
                        if (announce)
                            announce(f0, f1, curve);
                        else
                            return true;
                    }
                }
                f0 = f1;
            }
        }
        return false;
    }
};

} // namespace dqGeom
