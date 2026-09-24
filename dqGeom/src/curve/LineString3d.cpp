// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — LineString3d implementation
// Ported from: itwinjs-core core/geometry/src/curve/LineString3d.ts
#include "dqGeom/LineString3d.h"

#include <cmath>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

dqBase::RefPtr<LineString3d> LineString3d::create(std::vector<Point3d> points)
{
    return dqBase::RefPtr<LineString3d>(new LineString3d(std::move(points)));
}

// Ported from: itwinjs-core LineString3d.ts:1155 emitStrokes. "There is no need
// for chordTol and angleTol within a segment. Do NOT apply min strokes per
// primitive." A LineString's strokes ARE its vertices — emit them directly. The
// base CurvePrimitive::EmitStrokes resamples the polyline at evenly-spaced
// fractions (ComputeStrokeCount + FractionToPoint), which destroys the vertices
// (e.g. the ACS arrow fill, via Loop.CloneStroked → AddPolygon fan, lost its
// corners and rendered a distorted shape misaligned with its outline).
// maxEdgeLength subdivision is Phase-N (StrokeOptions.hasMaxEdgeLength pending).
void LineString3d::EmitStrokes(LineString3d& dest, StrokeOptions const& /*options*/) const
{
    for (auto const& p : m_points)
        dest.AddPoint(p);
}

// Ported from: itwinjs-core LineString3d.fractionToPoint
Point3d LineString3d::FractionToPoint(double fraction) const
{
    int n = static_cast<int>(m_points.size());
    if (n == 0) return Point3d::FromZero();
    if (n == 1) return m_points[0];

    double scaledF = fraction * static_cast<double>(n - 1);
    int segIndex = static_cast<int>(scaledF);
    if (segIndex < 0) segIndex = 0;
    if (segIndex >= n - 1) segIndex = n - 2;

    double localF = scaledF - static_cast<double>(segIndex);
    return Point3d::FromInterpolate(m_points[static_cast<size_t>(segIndex)], localF,
                                     m_points[static_cast<size_t>(segIndex + 1)]);
}

// Ported from: itwinjs-core LineString3d.fractionToPointAndDerivative
Point3d LineString3d::FractionToPointAndDerivative(double fraction, Vector3d& derivative) const
{
    int n = static_cast<int>(m_points.size());
    if (n == 0) {
        derivative = Vector3d::FromZero();
        return Point3d::FromZero();
    }
    if (n == 1) {
        derivative = Vector3d::FromZero();
        return m_points[0];
    }

    double scaledF = fraction * static_cast<double>(n - 1);
    int segIndex = static_cast<int>(scaledF);
    if (segIndex < 0) segIndex = 0;
    if (segIndex >= n - 1) segIndex = n - 2;

    derivative = Vector3d::FromStartEnd(m_points[static_cast<size_t>(segIndex)],
                                         m_points[static_cast<size_t>(segIndex + 1)]);
    derivative.Scale(static_cast<double>(n - 1));

    double localF = scaledF - static_cast<double>(segIndex);
    return Point3d::FromInterpolate(m_points[static_cast<size_t>(segIndex)], localF,
                                     m_points[static_cast<size_t>(segIndex + 1)]);
}

double LineString3d::QuickLength() const
{
    double total = 0.0;
    for (size_t i = 1; i < m_points.size(); ++i) {
        total += Vector3d::FromStartEnd(m_points[i - 1], m_points[i]).Magnitude();
    }
    return total;
}

dqBase::RefPtr<GeometryQuery> LineString3d::clone() const
{
    return dqBase::RefPtr<LineString3d>(new LineString3d(m_points));
}

dqBase::RefPtr<GeometryQuery> LineString3d::CloneTransformed(Transform const& xform) const
{
    std::vector<Point3d> xformed;
    xformed.reserve(m_points.size());
    for (auto const& p : m_points) {
        xformed.push_back(xform.MultiplyPoint3d(p));
    }
    return dqBase::RefPtr<LineString3d>(new LineString3d(std::move(xformed)));
}

bool LineString3d::TryTransformInPlace(Transform const& transform)
{
    for (auto& p : m_points) {
        p = transform.MultiplyPoint3d(p);
    }
    return true;
}

void LineString3d::ExtendRange(Range3d& range) const
{
    for (auto const& p : m_points) {
        range.ExtendPoint(p);
    }
}

bool LineString3d::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (other.Category() != GeometryCategory::CurvePrimitive) return false;
    auto const& otherCurve = static_cast<CurvePrimitive const&>(other);
    if (otherCurve.GetCurveType() != CurveType::LineString) return false;
    auto const& otherLine = static_cast<LineString3d const&>(other);
    if (m_points.size() != otherLine.m_points.size()) return false;
    for (size_t i = 0; i < m_points.size(); ++i) {
        if (!m_points[i].AlmostEqual(otherLine.m_points[i], tol)) return false;
    }
    return true;
}

END_DQ_GEOM_NAMESPACE
