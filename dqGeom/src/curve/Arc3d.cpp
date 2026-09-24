// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Arc3d implementation
// Ported from: itwinjs-core core/geometry/src/curve/Arc3d.ts
//              imodel-native iModelCore/GeomLibs/geom/src/structs/cpp/refmethods/refdellipse3d.cpp
#include "dqGeom/Arc3d.h"

#include "dqGeom/ConvexClipPlaneSet.h"
#include "dqGeom/Polynomials.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
Arc3d::Arc3d(Point3d const& center, Matrix3d const& axes, AngleSweep const& sweep)
    : m_center(center), m_matrix(axes), m_sweep(sweep)
{
}

dqBase::RefPtr<Arc3d> Arc3d::create(Point3d const& center, Matrix3d const& axes,
                                     AngleSweep const& sweep)
{
    return dqBase::RefPtr<Arc3d>(new Arc3d(center, axes, sweep));
}

dqBase::RefPtr<Arc3d> Arc3d::CreateXY(Point3d const& center, double radius,
                                       AngleSweep const& sweep)
{
    // vector0 = (radius, 0, 0), vector90 = (0, radius, 0), normal = (0, 0, 1)
    Vector3d v0 = Vector3d::From(radius, 0.0, 0.0);
    Vector3d v90 = Vector3d::From(0.0, radius, 0.0);
    Vector3d normal = Vector3d::From(0.0, 0.0, 1.0);
    Matrix3d axes = Matrix3d::CreateColumns(v0, v90, normal);
    return create(center, axes, sweep);
}

dqBase::RefPtr<Arc3d> Arc3d::CreateCenterNormalRadius(Point3d const& center,
                                                       Vector3d const& normal, double radius,
                                                       AngleSweep const& sweep)
{
    // Build a coordinate frame from the normal
    // Find a vector not parallel to normal, then cross product
    Vector3d absN = Vector3d::From(std::abs(normal.x), std::abs(normal.y), std::abs(normal.z));
    Vector3d up;
    if (absN.x <= absN.y && absN.x <= absN.z) {
        up = Vector3d::UnitX();
    } else if (absN.y <= absN.z) {
        up = Vector3d::UnitY();
    } else {
        up = Vector3d::UnitZ();
    }

    Vector3d v0 = Vector3d::FromCrossProduct(normal, up);
    v0.Normalize();
    Vector3d v90 = Vector3d::FromCrossProduct(normal, v0);
    v90.Normalize();
    v0.Scale(radius);
    v90.Scale(radius);
    Matrix3d axes = Matrix3d::CreateColumns(v0, v90, normal);
    return create(center, axes, sweep);
}

dqBase::RefPtr<Arc3d> Arc3d::FromVectors(Point3d const& center, Vector3d const& vector0,
                                          Vector3d const& vector90, AngleSweep const& sweep)
{
    Vector3d normal = Vector3d::FromCrossProduct(vector0, vector90);
    normal.Normalize();
    Matrix3d axes = Matrix3d::CreateColumns(vector0, vector90, normal);
    return create(center, axes, sweep);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
Vector3d Arc3d::Vector0() const { return m_matrix.ColumnX(); }
Vector3d Arc3d::Vector90() const { return m_matrix.ColumnY(); }
Vector3d Arc3d::PerpendicularVector() const { return m_matrix.ColumnZ(); }

bool Arc3d::IsCircular() const
{
    Vector3d v0 = Vector0();
    Vector3d v90 = Vector90();
    double dot = v0.DotProduct(v90);
    double mag0sq = v0.MagnitudeSquared();
    double mag90sq = v90.MagnitudeSquared();
    // Perpendicular and equal length
    return std::abs(dot) < kSmallMetricDistanceSquared &&
           std::abs(mag0sq - mag90sq) < kSmallMetricDistanceSquared;
}

double Arc3d::CircularRadius() const
{
    return Vector0().Magnitude();
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------
// Ported from: imodel-native DEllipse3d::FractionParameterToPoint
// P(theta) = center + vector0*cos(theta) + vector90*sin(theta)
// theta = start + fraction * sweep
Point3d Arc3d::FractionToPoint(double fraction) const
{
    double theta = m_sweep.FractionToRadians(fraction);
    double cosTheta = std::cos(theta);
    double sinTheta = std::sin(theta);

    Vector3d v0 = Vector0();
    Vector3d v90 = Vector90();

    return Point3d::From(m_center.x + v0.x * cosTheta + v90.x * sinTheta,
                         m_center.y + v0.y * cosTheta + v90.y * sinTheta,
                         m_center.z + v0.z * cosTheta + v90.z * sinTheta);
}

// Ported from: imodel-native DEllipse3d::FractionParameterToDerivatives
// dP/df = sweep * (-vector0*sin(theta) + vector90*cos(theta))
Point3d Arc3d::FractionToPointAndDerivative(double fraction, Vector3d& derivative) const
{
    double theta = m_sweep.FractionToRadians(fraction);
    double cosTheta = std::cos(theta);
    double sinTheta = std::sin(theta);
    double sweep = m_sweep.SweepRadians();

    Vector3d v0 = Vector0();
    Vector3d v90 = Vector90();

    Point3d point = Point3d::From(m_center.x + v0.x * cosTheta + v90.x * sinTheta,
                                  m_center.y + v0.y * cosTheta + v90.y * sinTheta,
                                  m_center.z + v0.z * cosTheta + v90.z * sinTheta);

    // First derivative (chain rule: dP/dtheta * dtheta/df)
    derivative = Vector3d::From(sweep * (-v0.x * sinTheta + v90.x * cosTheta),
                                sweep * (-v0.y * sinTheta + v90.y * cosTheta),
                                sweep * (-v0.z * sinTheta + v90.z * cosTheta));
    return point;
}

double Arc3d::QuickLength() const
{
    if (IsCircular()) {
        return std::abs(CircularRadius() * m_sweep.SweepRadians());
    }
    // Elliptical: stroke-based
    return CurveLength();
}

double Arc3d::GetFractionToDistanceScale() const
{
    if (IsCircular()) {
        return std::abs(CircularRadius() * m_sweep.SweepRadians());
    }
    return 0.0;  // Not proportional for elliptical arcs
}

// ---------------------------------------------------------------------------
// GeometryQuery interface
// ---------------------------------------------------------------------------
dqBase::RefPtr<GeometryQuery> Arc3d::clone() const
{
    return dqBase::RefPtr<Arc3d>(new Arc3d(m_center, m_matrix, m_sweep));
}

dqBase::RefPtr<GeometryQuery> Arc3d::CloneTransformed(Transform const& transform) const
{
    auto clone = new Arc3d(m_center, m_matrix, m_sweep);
    clone->TryTransformInPlace(transform);
    return dqBase::RefPtr<Arc3d>(clone);
}

bool Arc3d::TryTransformInPlace(Transform const& transform)
{
    m_center = transform.MultiplyPoint3d(m_center);
    // Transform the axes vectors
    Vector3d v0 = transform.MultiplyVector(Vector0());
    Vector3d v90 = transform.MultiplyVector(Vector90());
    Vector3d normal = Vector3d::FromCrossProduct(v0, v90);
    normal.Normalize();
    m_matrix = Matrix3d::CreateColumns(v0, v90, normal);
    return true;
}

void Arc3d::ExtendRange(Range3d& range) const
{
    // Sample the arc at several points
    int const numSamples = 32;
    for (int i = 0; i <= numSamples; ++i) {
        double f = static_cast<double>(i) / static_cast<double>(numSamples);
        range.ExtendPoint(FractionToPoint(f));
    }
}

bool Arc3d::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (other.Category() != GeometryCategory::CurvePrimitive) return false;
    auto const& otherCurve = static_cast<CurvePrimitive const&>(other);
    if (otherCurve.GetCurveType() != CurveType::Arc) return false;
    auto const& otherArc = static_cast<Arc3d const&>(other);

    // Compare center, axes, and sweep
    if (!m_center.AlmostEqual(otherArc.m_center, tol)) return false;
    if (!m_sweep.IsAlmostEqual(otherArc.m_sweep, Angle::kSmallAngleRadians)) return false;

    // Compare axis vectors
    Vector3d v0 = Vector0();
    Vector3d v90 = Vector90();
    Vector3d ov0 = otherArc.Vector0();
    Vector3d ov90 = otherArc.Vector90();
    return v0.IsEqual(ov0, tol) && v90.IsEqual(ov90, tol);
}

// Ported from: itwinjs Arc3d.computeStrokeCountForOptions
int Arc3d::ComputeStrokeCount(StrokeOptions const& options) const
{
    double radius = CircularRadius();
    double sweepRadians = std::abs(m_sweep.SweepRadians());
    return options.ApplyTolerancesToArc(radius, sweepRadians);
}

// ---------------------------------------------------------------------------
// 截面弧区间裁剪链（BackgroundMapGeometry 深度分支）—— itwinjs Arc3d.ts 1:1
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Arc3d.announceClipIntervals (Arc3d.ts:1418-1420)
bool Arc3d::announceClipIntervals(ConvexClipPlaneSet const& clipper,
                                  AnnounceNumberNumberCurvePrimitive const& announce) const
{
    return clipper.announceClippedArcIntervals(*this, announce);
}

// Ported from: itwinjs-core Arc3d.clonePartialCurve (Arc3d.ts:1346-1357)
dqBase::RefPtr<Arc3d> Arc3d::clonePartialCurve(double fractionA, double fractionB) const
{
    if (fractionB < fractionA) {
        auto arcA = clonePartialCurve(fractionB, fractionA);
        arcA->reverseInPlace();
        return arcA;
    }
    auto arcB = Arc3d::create(m_center, m_matrix, m_sweep);   // this.clone()
    arcB->m_sweep.SetStartEndRadians(
        m_sweep.FractionToRadians(fractionA),
        m_sweep.FractionToRadians(fractionB));
    return arcB;
}

// Ported from: itwinjs-core Arc3d.extendRangeInSweep (Arc3d.ts:1108-1131)
void Arc3d::extendRangeInSweep(Range3d& range, AngleSweep const& sweep,
                               Transform const* transform) const
{
    SineCosinePolynomial trigForm(0.0, 0.0, 0.0);
    Point3d center = m_center;
    Vector3d vectorU = m_matrix.ColumnX();
    Vector3d vectorV = m_matrix.ColumnY();
    if (transform != nullptr) {
        center = transform->MultiplyPoint3d(center);
        vectorU = transform->MultiplyVector(vectorU);
        vectorV = transform->MultiplyVector(vectorV);
    }
    Point3d lowPoint;
    Point3d highPoint;
    for (int i = 0; i < 3; ++i) {
        trigForm.set((&center.x)[i], (&vectorU.x)[i], (&vectorV.x)[i]);  // center.at(i) 等
        Range1d const range1 = trigForm.rangeInSweep(sweep);
        (&lowPoint.x)[i] = range1.low;
        (&highPoint.x)[i] = range1.high;
    }
    range.ExtendPoint(lowPoint);
    range.ExtendPoint(highPoint);
}

END_DQ_GEOM_NAMESPACE
