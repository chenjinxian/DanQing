// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — LineSegment3d implementation
// Ported from: itwinjs-core core/geometry/src/curve/LineSegment3d.ts
//              imodel-native iModelCore/GeomLibs/geom/src/CurvePrimitive/cp_line.cpp
#include "dqGeom/LineSegment3d.h"

BEGIN_DQ_GEOM_NAMESPACE

dqBase::RefPtr<LineSegment3d> LineSegment3d::create(Point3d const& point0, Point3d const& point1)
{
    return dqBase::RefPtr<LineSegment3d>(new LineSegment3d(point0, point1));
}

// Ported from: imodel-native cp_line.cpp FractionToPoint
// X(f) = P0 + f * (P1 - P0)
Point3d LineSegment3d::FractionToPoint(double fraction) const
{
    return Point3d::FromInterpolate(m_point0, fraction, m_point1);
}

// Ported from: imodel-native cp_line.cpp FractionToPoint(point, tangent)
// derivative = P1 - P0 (constant)
Point3d LineSegment3d::FractionToPointAndDerivative(double fraction, Vector3d& derivative) const
{
    (void)fraction;
    derivative = Vector3d::FromStartEnd(m_point0, m_point1);
    return Point3d::FromInterpolate(m_point0, fraction, m_point1);
}

double LineSegment3d::QuickLength() const
{
    return Vector3d::FromStartEnd(m_point0, m_point1).Magnitude();
}

// For line segments, distance is proportional to fraction
double LineSegment3d::GetFractionToDistanceScale() const
{
    return QuickLength();
}

dqBase::RefPtr<GeometryQuery> LineSegment3d::clone() const
{
    return dqBase::RefPtr<LineSegment3d>(new LineSegment3d(m_point0, m_point1));
}

dqBase::RefPtr<GeometryQuery> LineSegment3d::CloneTransformed(Transform const& transform) const
{
    auto clone = new LineSegment3d(m_point0, m_point1);
    clone->TryTransformInPlace(transform);
    return dqBase::RefPtr<LineSegment3d>(clone);
}

bool LineSegment3d::TryTransformInPlace(Transform const& transform)
{
    m_point0 = transform.MultiplyPoint3d(m_point0);
    m_point1 = transform.MultiplyPoint3d(m_point1);
    return true;
}

void LineSegment3d::ExtendRange(Range3d& range) const
{
    range.ExtendPoint(m_point0);
    range.ExtendPoint(m_point1);
}

bool LineSegment3d::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (other.Category() != GeometryCategory::CurvePrimitive) return false;
    auto const& otherCurve = static_cast<CurvePrimitive const&>(other);
    if (otherCurve.GetCurveType() != CurveType::LineSegment) return false;
    auto const& otherSeg = static_cast<LineSegment3d const&>(other);
    return m_point0.AlmostEqual(otherSeg.m_point0, tol) &&
           m_point1.AlmostEqual(otherSeg.m_point1, tol);
}

void LineSegment3d::Set(Point3d const& point0, Point3d const& point1)
{
    m_point0 = point0;
    m_point1 = point1;
}

END_DQ_GEOM_NAMESPACE
