// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BezierCurve3d implementation
// Ported from: itwinjs-core core/geometry/src/bspline/BezierCurve3d.ts
#include "dqGeom/BezierCurve3d.h"

#include "dqGeom/GeometryHandler.h"

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: BezierCurve3d.isAlmostEqual（:149-154——同类经 _polygon 逐坐标）。
bool BezierCurve3d::IsAlmostEqual(GeometryQuery const& other, double /*tol*/) const
{
    if (!IsSameGeometryClass(other))
        return false;
    auto const& otherCurve = static_cast<BezierCurve3d const&>(other);
    return m_polygon.isAlmostEqual(otherCurve.m_polygon);
}

// Ported from: BezierCurve3d.dispatchToGeometryHandler（:156-158）。
void BezierCurve3d::DispatchToHandler(GeometryHandler& handler)
{
    handler.HandleBezierCurve3d(*this);
}

END_DQ_GEOM_NAMESPACE
