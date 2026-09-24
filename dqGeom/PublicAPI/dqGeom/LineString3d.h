// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — LineString3d
//
// Ported from: itwinjs-core core/geometry/src/curve/LineString3d.ts
//
// Polyline curve primitive.  Piecewise linear interpolation through a
// sequence of points.  Fraction 0→first point, fraction 1→last point.
// 替代 Qt QVector，底层用 std::vector。
#pragma once

#include "CurvePrimitive.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// LineString3d — polyline curve primitive
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT LineString3d : public CurvePrimitive {
public:
    // --- Factory ---
    static dqBase::RefPtr<LineString3d> create(std::vector<Point3d> points);

    // --- CurvePrimitive interface ---
    CurveType GetCurveType() const noexcept final { return CurveType::LineString; }
    void DispatchToHandler(GeometryHandler& handler) final { handler.HandleLineString3d(*this); }

    Point3d FractionToPoint(double fraction) const final;
    Point3d FractionToPointAndDerivative(double fraction, Vector3d& derivative) const final;
    // Ported from: itwinjs-core LineString3d.ts:1155 emitStrokes. "There is no
    // need for chordTol and angleTol within a segment. Do NOT apply min strokes
    // per primitive." A LineString's strokes ARE its vertices — emit them directly.
    // Overrides the base CurvePrimitive::EmitStrokes, which resamples the polyline
    // at evenly-spaced fractions (ComputeStrokeCount + FractionToPoint) and so
    // destroys the vertices (e.g. the ACS arrow fill, via Loop.CloneStroked →
    // AddPolygon fan, lost its corners and rendered a distorted shape misaligned
    // with its outline). maxEdgeLength subdivision is Phase-N (CreateForFacets
    // does not set it).
    void EmitStrokes(LineString3d& dest, StrokeOptions const& options) const override;

    double QuickLength() const final;

    dqBase::RefPtr<GeometryQuery> clone() const final;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const final;
    bool TryTransformInPlace(Transform const& transform) final;

    void ExtendRange(Range3d& range) const final;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const final;

    // --- Accessors ---
    std::vector<Point3d> const& Points() const noexcept { return m_points; }
    size_t PointCount() const noexcept { return m_points.size(); }

    // --- Building ---
    void AddPoint(Point3d const& point) { m_points.push_back(point); }

    LineString3d() = default;
    explicit LineString3d(std::vector<Point3d> points) : m_points(std::move(points)) {}

private:

    std::vector<Point3d> m_points;
};

using LineString3dPtr = dqBase::RefPtr<LineString3d>;

END_DQ_GEOM_NAMESPACE
