// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BSplineCurve3d implementation
// Ported from: itwinjs-core core/geometry/src/bspline/BSplineCurve.ts
#include "dqGeom/BSplineCurve.h"

#include "dqGeom/GeometryHandler.h"

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// 工厂
// ---------------------------------------------------------------------------

// Ported from: BSplineCurve3d.createUniformKnots（:487-509——Point3d 数组形态；
// GrowableXYZArray/Float64Array 两形态未移植[消费面缺席]）。
dqBase::RefPtr<BSplineCurve3d> BSplineCurve3d::createUniformKnots(
    std::vector<Point3d> const& poles, int order)
{
    size_t const numPoles = poles.size();
    if (order < 2 || numPoles < static_cast<size_t>(order))
        return nullptr;
    auto knots = KnotVector::createUniformClamped(static_cast<int>(numPoles), order - 1, 0.0, 1.0);
    auto curve = dqBase::RefPtr<BSplineCurve3d>(new BSplineCurve3d(numPoles, order, knots));
    size_t i = 0;
    for (auto const& p : poles) {
        curve->m_bcurve.packedData[i++] = p.x;
        curve->m_bcurve.packedData[i++] = p.y;
        curve->m_bcurve.packedData[i++] = p.z;
    }
    return curve;
}

// Ported from: BSplineCurve3d.create（:601-635——两计数形态识别核；
// Point3d[]/打包 doubles 双工厂共享）。
dqBase::RefPtr<BSplineCurve3d> BSplineCurve3d::createImpl(size_t numPoles,
                                                          std::vector<double> const& knotArray,
                                                          int order)
{
    if (order < 2)
        return nullptr;
    if (numPoles < static_cast<size_t>(order))
        return nullptr;
    size_t const numKnots = knotArray.size();
    bool const skipFirstAndLast = (numPoles + static_cast<size_t>(order) == numKnots);  // classic over-clamped input knots
    if (!skipFirstAndLast && numPoles + static_cast<size_t>(order) != numKnots + 2)  // modern knots
        return nullptr;
    auto knots = KnotVector::create(knotArray, order - 1, skipFirstAndLast);
    return dqBase::RefPtr<BSplineCurve3d>(new BSplineCurve3d(numPoles, order, knots));
}

dqBase::RefPtr<BSplineCurve3d> BSplineCurve3d::create(std::vector<Point3d> const& poleArray,
                                                      std::vector<double> const& knotArray, int order)
{
    auto curve = createImpl(poleArray.size(), knotArray, order);
    if (!curve.IsValid())
        return nullptr;
    size_t i = 0;
    for (auto const& p : poleArray) {
        curve->m_bcurve.packedData[i++] = p.x;
        curve->m_bcurve.packedData[i++] = p.y;
        curve->m_bcurve.packedData[i++] = p.z;
    }
    return curve;
}

dqBase::RefPtr<BSplineCurve3d> BSplineCurve3d::create(std::vector<double> const& poleArray,
                                                      std::vector<double> const& knotArray, int order)
{
    size_t const numPoles = poleArray.size() / 3;  // blocked as xyz
    auto curve = createImpl(numPoles, knotArray, order);
    if (!curve.IsValid())
        return nullptr;
    size_t i = 0;
    for (double const coordinate : poleArray)
        curve->m_bcurve.packedData[i++] = coordinate;
    return curve;
}

// ---------------------------------------------------------------------------
// 描线（span → 饱和 Bezier 链）
// ---------------------------------------------------------------------------

// Ported from: BSplineCurve3d.getSaturatedBezierSpan3d（:820-831）。
dqBase::RefPtr<BezierCurve3d> BSplineCurve3d::getSaturatedBezierSpan3d(int spanIndex) const
{
    if (spanIndex < 0 || spanIndex >= numSpan())
        return nullptr;
    auto bezier = BezierCurve3d::createOrder(static_cast<size_t>(order()));
    bezier->loadSpanPoles(m_bcurve.packedData, static_cast<size_t>(spanIndex));
    if (bezier->saturateInPlace(m_bcurve.knots, spanIndex))
        return bezier;
    return nullptr;
}

// Ported from: BSplineCurve3d.emitStrokes（:786-794）。
void BSplineCurve3d::EmitStrokes(LineString3d& dest, StrokeOptions const& options) const
{
    int const numSpan = this->numSpan();
    for (int spanIndex = 0; spanIndex < numSpan; ++spanIndex) {
        auto bezier = getSaturatedBezierSpan3d(spanIndex);
        if (bezier.IsValid())
            bezier->EmitStrokes(dest, options);
    }
}

// Ported from: BSplineCurve3d.computeStrokeCountForOptions（:754-764）。
int BSplineCurve3d::computeStrokeCountForOptions(StrokeOptions const* options) const
{
    int numStroke = 0;
    int const numSpan = this->numSpan();
    for (int spanIndex = 0; spanIndex < numSpan; ++spanIndex) {
        auto bezier = getSaturatedBezierSpan3d(spanIndex);
        if (bezier.IsValid())
            numStroke += bezier->computeStrokeCountForOptions(options);
    }
    return numStroke;
}

// ---------------------------------------------------------------------------
// 等同性 / 派发
// ---------------------------------------------------------------------------

// Ported from: BSplineCurve3d.isAlmostEqual（:702-708）。
bool BSplineCurve3d::IsAlmostEqual(GeometryQuery const& other, double /*tol*/) const
{
    if (!IsSameGeometryClass(other))
        return false;
    auto const& otherCurve = static_cast<BSplineCurve3d const&>(other);
    return m_bcurve.knots.isAlmostEqual(otherCurve.m_bcurve.knots)
        && Point3dArray::isAlmostEqual(m_bcurve.packedData, otherCurve.m_bcurve.packedData);
}

// Ported from: BSplineCurve3d.dispatchToGeometryHandler（:850-852）。
void BSplineCurve3d::DispatchToHandler(GeometryHandler& handler)
{
    handler.HandleBSplineCurve3d(*this);
}

END_DQ_GEOM_NAMESPACE
