// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/RotationalSweep.ts
// DanQing dqGeom — RotationalSweep implementation
#include "dqGeom/RotationalSweep.h"

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core RotationalSweep.create (stores contour + axis directly; SweepContour phase).
dqBase::RefPtr<RotationalSweep> RotationalSweep::Create(dqBase::RefPtr<CurveCollection> const& curves,
                                                        Point3d const& axisOrigin, Vector3d const& axisDirection,
                                                        Angle const& sweepAngle, bool capped)
{
    if (!curves) return nullptr;
    return dqBase::RefPtr<RotationalSweep>(new RotationalSweep(curves, axisOrigin, axisDirection, sweepAngle, capped));
}

Range3d RotationalSweep::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// TODO commit-N: true swept bbox (rotation about axis); currently delegates to contour range (subset).
void RotationalSweep::ExtendRange(Range3d& range) const
{
    if (m_curves) m_curves->ExtendRange(range);
}

// Ported from: itwinjs-core RotationalSweep.tryTransformInPlace.
bool RotationalSweep::TryTransformInPlace(Transform const& transform)
{
    if (m_curves) m_curves->TryTransformInPlace(transform);
    m_axisOrigin = transform.MultiplyPoint3d(m_axisOrigin);
    m_axisDirection = transform.MultiplyVector(m_axisDirection);
    return true;
}

// Ported from: itwinjs-core RotationalSweep.clone
dqBase::RefPtr<GeometryQuery> RotationalSweep::clone() const
{
    auto curvesClone = m_curves ? m_curves->clone().StaticCast<CurveCollection>() : nullptr;
    return Create(curvesClone, m_axisOrigin, m_axisDirection, m_sweepAngle, m_capped);
}

// Ported from: itwinjs-core RotationalSweep.cloneTransformed
dqBase::RefPtr<GeometryQuery> RotationalSweep::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<RotationalSweep>();
    r->TryTransformInPlace(transform);
    return r;
}

bool RotationalSweep::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::RotationalSweep;
}

// Ported from: itwinjs-core RotationalSweep.isAlmostEqual
bool RotationalSweep::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<RotationalSweep const&>(other);
    if (m_capped != o.m_capped) return false;
    if (!m_axisOrigin.AlmostEqual(o.m_axisOrigin, tol)) return false;
    if (!m_axisDirection.AlmostEqual(o.m_axisDirection)) return false;
    if (std::fabs(m_sweepAngle.Radians() - o.m_sweepAngle.Radians()) > tol) return false;
    return m_curves && o.m_curves && m_curves->IsAlmostEqual(*o.m_curves, tol);
}

END_DQ_GEOM_NAMESPACE
