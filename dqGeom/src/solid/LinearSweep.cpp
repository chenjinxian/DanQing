// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/LinearSweep.ts
// DanQing dqGeom — LinearSweep implementation
#include "dqGeom/LinearSweep.h"

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core LinearSweep.create (stores contour directly; SweepContour frame phase).
dqBase::RefPtr<LinearSweep> LinearSweep::Create(dqBase::RefPtr<CurveCollection> const& curves,
                                                Vector3d const& direction, bool capped)
{
    if (!curves) return nullptr;
    return dqBase::RefPtr<LinearSweep>(new LinearSweep(curves, direction, capped));
}

// Ported from: itwinjs-core LinearSweep.isClosedVolume (capped && contour.curves.isAnyRegionType).
// Region ≈ Loop here (ParityRegion/UnionRegion phased in PR 1).
bool LinearSweep::IsClosedVolume() const
{
    return m_capped && m_curves && m_curves->GetCurveCollectionType() == CurveCollectionType::Loop;
}

Range3d LinearSweep::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Contour range extended by the sweep vector (bbox of base + swept range).
void LinearSweep::ExtendRange(Range3d& range) const
{
    if (!m_curves) return;
    Range3d cr = m_curves->Range();
    if (cr.isNull()) return;
    range.ExtendPoint(cr.low);
    range.ExtendPoint(cr.high);
    range.ExtendPoint(Point3d::From(cr.low.x + m_direction.x, cr.low.y + m_direction.y, cr.low.z + m_direction.z));
    range.ExtendPoint(Point3d::From(cr.high.x + m_direction.x, cr.high.y + m_direction.y, cr.high.z + m_direction.z));
}

// Ported from: itwinjs-core LinearSweep.tryTransformInPlace.
bool LinearSweep::TryTransformInPlace(Transform const& transform)
{
    if (m_curves) m_curves->TryTransformInPlace(transform);
    m_direction = transform.MultiplyVector(m_direction);
    return true;
}

// Ported from: itwinjs-core LinearSweep.clone
dqBase::RefPtr<GeometryQuery> LinearSweep::clone() const
{
    auto curvesClone = m_curves ? m_curves->clone().StaticCast<CurveCollection>() : nullptr;
    return Create(curvesClone, m_direction, m_capped);
}

// Ported from: itwinjs-core LinearSweep.cloneTransformed
dqBase::RefPtr<GeometryQuery> LinearSweep::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<LinearSweep>();
    r->TryTransformInPlace(transform);
    return r;
}

bool LinearSweep::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::LinearSweep;
}

// Ported from: itwinjs-core LinearSweep.isAlmostEqual
bool LinearSweep::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<LinearSweep const&>(other);
    if (m_capped != o.m_capped) return false;
    if (!m_direction.AlmostEqual(o.m_direction)) return false;
    return m_curves && o.m_curves && m_curves->IsAlmostEqual(*o.m_curves, tol);
}

END_DQ_GEOM_NAMESPACE
