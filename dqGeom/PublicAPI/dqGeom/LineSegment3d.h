// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — LineSegment3d
//
// Ported from: itwinjs-core core/geometry/src/curve/LineSegment3d.ts
//              imodel-native iModelCore/GeomLibs/geom/src/CurvePrimitive/cp_line.cpp
//
// Two-point linear curve primitive.  Parameterization: X(f) = P0 + f*(P1-P0).
#pragma once

#include "CurvePrimitive.h"

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// LineSegment3d — two-point line segment
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT LineSegment3d : public CurvePrimitive {
public:
    // --- Factory ---
    static dqBase::RefPtr<LineSegment3d> create(Point3d const& point0, Point3d const& point1);

    // --- CurvePrimitive interface ---
    CurveType GetCurveType() const noexcept final { return CurveType::LineSegment; }
    void DispatchToHandler(GeometryHandler& handler) final { handler.HandleLineSegment3d(*this); }

    Point3d FractionToPoint(double fraction) const final;
    Point3d FractionToPointAndDerivative(double fraction, Vector3d& derivative) const final;

    double QuickLength() const final;
    double GetFractionToDistanceScale() const final;

    dqBase::RefPtr<GeometryQuery> clone() const final;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const final;
    bool TryTransformInPlace(Transform const& transform) final;

    void ExtendRange(Range3d& range) const final;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const final;

    // --- Accessors ---
    Point3d const& Point0Ref() const noexcept { return m_point0; }
    Point3d const& Point1Ref() const noexcept { return m_point1; }
    void Set(Point3d const& point0, Point3d const& point1);

private:
    LineSegment3d() = default;
    LineSegment3d(Point3d const& p0, Point3d const& p1) : m_point0(p0), m_point1(p1) {}

    Point3d m_point0;
    Point3d m_point1;
};

using LineSegment3dPtr = dqBase::RefPtr<LineSegment3d>;

END_DQ_GEOM_NAMESPACE
