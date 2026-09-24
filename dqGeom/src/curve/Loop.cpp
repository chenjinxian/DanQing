// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/curve/Loop.ts
// DanQing dqGeom — Loop implementation
#include "dqGeom/Loop.h"

#include "dqGeom/LineString3d.h"
#include "dqGeom/Point3d.h"

#include <utility>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core Loop constructor (`new Loop()`)
dqBase::RefPtr<Loop> Loop::Create() {
    return dqBase::RefPtr<Loop>(new Loop());
}

// Ported from: itwinjs-core Loop.create(...curves) — captures each CurvePrimitive.
dqBase::RefPtr<Loop> Loop::Create(std::vector<CurvePrimitivePtr> const& curves) {
    auto result = Create();
    for (auto const& curve : curves)
        result->m_curves.push_back(curve);
    return result;
}

// Ported from: itwinjs-core Loop.createPolygon — LineString3d over points + closure point.
dqBase::RefPtr<Loop> Loop::CreatePolygon(std::vector<Point3d> const& points) {
    std::vector<Point3d> pts = points;
    // addClosurePoint: append the first point if the last is not already equal to it.
    // (1:1 LineString3d.addClosurePoint semantics.)
    if (!pts.empty() && !pts.front().AlmostEqual(pts.back()))
        pts.push_back(pts.front());
    auto result = Create();
    result->m_curves.push_back(LineString3d::create(std::move(pts)));
    return result;
}

// Ported from: itwinjs-core Loop.cloneStroked — stroke all children, ensure closure, wrap as Loop.
dqBase::RefPtr<CurveChain> Loop::CloneStroked(StrokeOptions const& options) const {
    std::vector<Point3d> pts;
    for (auto const& curve : m_curves) {
        auto strokes = LineString3d::create(std::vector<Point3d>{});
        curve->EmitStrokes(*strokes, options);
        for (auto const& p : strokes->Points())
            pts.push_back(p);
    }
    // TODO Phase-N: full removeDuplicatePoints + isPhysicallyClosed pop/addClosurePoint
    //   (LineString3d methods pending). Closure fallback: append first if last != first.
    if (!pts.empty() && !pts.front().AlmostEqual(pts.back()))
        pts.push_back(pts.front());
    auto result = Create();
    result->m_curves.push_back(LineString3d::create(std::move(pts)));
    return result; // RefPtr<Loop> → RefPtr<CurveChain>
}

END_DQ_GEOM_NAMESPACE
