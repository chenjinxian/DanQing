// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipPlane 截面弧区间裁剪实现
// Ported from: itwinjs-core core/geometry/src/clipping/ClipPlane.ts
//              core/geometry/src/clipping/ConvexClipPlaneSet.ts
//
// appendIntersectionRadians / announceClippedArcIntervals 需要 Arc3d 完整类型与
// ClipUtils/Polynomials，从头文件移出以避免 ClipPlane.h ↔ Arc3d.h 互相包含。
#include "dqGeom/ClipPlane.h"

#include "dqGeom/Arc3d.h"
#include "dqGeom/ClipUtils.h"
#include "dqGeom/ConvexClipPlaneSet.h"
#include "dqGeom/Polynomials.h"

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: ClipPlane.appendIntersectionRadians (ClipPlane.ts:441-449)
void ClipPlane::appendIntersectionRadians(Arc3d const& arc, std::vector<double>& intersectionRadians) const
{
    // arc.toVectors() — center + vector0 + vector90
    double const alpha = altitude(arc.CenterRef());
    double const beta = velocity(arc.Vector0());
    double const gamma = velocity(arc.Vector90());
    AnalyticRoots::appendImplicitLineUnitCircleIntersections(
        alpha, beta, gamma, nullptr, nullptr, &intersectionRadians);
}

// Ported from: ClipPlane.announceClippedArcIntervals (ClipPlane.ts:452-458)
bool ClipPlane::announceClippedArcIntervals(Arc3d const& arc,
                                            AnnounceNumberNumberCurvePrimitive const& announce) const
{
    std::vector<double> breaks;
    appendIntersectionRadians(arc, breaks);
    arc.Sweep().radiansArrayToPositivePeriodicFractions(breaks);
    return ClipUtilities::selectIntervals01(arc, breaks, *this, announce);
}

// Ported from: ConvexClipPlaneSet.announceClippedArcIntervals (ConvexClipPlaneSet.ts:412-420)
bool ConvexClipPlaneSet::announceClippedArcIntervals(Arc3d const& arc,
                                                     AnnounceNumberNumberCurvePrimitive const& announce) const
{
    std::vector<double> breaks;
    for (ClipPlane const& clipPlane : planes)
        clipPlane.appendIntersectionRadians(arc, breaks);
    arc.Sweep().radiansArrayToPositivePeriodicFractions(breaks);
    return ClipUtilities::selectIntervals01(arc, breaks, *this, announce);
}

END_DQ_GEOM_NAMESPACE
