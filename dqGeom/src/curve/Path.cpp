// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/curve/Path.ts
// DanQing dqGeom — Path implementation
#include "dqGeom/Path.h"

#include "dqGeom/LineString3d.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core Path constructor (`new Path()`)
dqBase::RefPtr<Path> Path::Create() {
    return dqBase::RefPtr<Path>(new Path());
}

// Ported from: itwinjs-core Path.create(...curves) — captures each CurvePrimitive.
dqBase::RefPtr<Path> Path::Create(std::vector<CurvePrimitivePtr> const& curves) {
    auto result = Create();
    for (auto const& curve : curves)
        result->m_curves.push_back(curve);
    return result;
}

// Ported from: itwinjs-core Path.create(Point3d[]) — wraps points as a single LineString3d child.
dqBase::RefPtr<Path> Path::Create(std::vector<Point3d> const& points) {
    auto result = Create();
    result->m_curves.push_back(LineString3d::create(points));
    return result;
}

// Ported from: itwinjs-core Path.cloneStroked — stroke each child into one LineString3d, wrap as Path.
dqBase::RefPtr<CurveChain> Path::CloneStroked(StrokeOptions const& options) const {
    auto strokes = LineString3d::create(std::vector<Point3d>{});
    for (auto const& curve : m_curves)
        curve->EmitStrokes(*strokes, options);
    auto result = Create();
    result->m_curves.push_back(strokes);
    return result; // RefPtr<Path> → RefPtr<CurveChain>
}

END_DQ_GEOM_NAMESPACE
