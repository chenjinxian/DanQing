// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/RuledSweep.ts
// DanQing dqGeom — RuledSweep implementation
#include "dqGeom/RuledSweep.h"

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core RuledSweep.create (stores contours directly; SweepContour phase).
dqBase::RefPtr<RuledSweep> RuledSweep::Create(std::vector<dqBase::RefPtr<CurveCollection>> const& contours, bool capped)
{
    if (contours.empty()) return nullptr;
    return dqBase::RefPtr<RuledSweep>(new RuledSweep(contours, capped));
}

Range3d RuledSweep::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Ported from: itwinjs-core RuledSweep.extendRange — union of all contour ranges.
void RuledSweep::ExtendRange(Range3d& range) const
{
    for (auto const& c : m_curves)
        if (c) c->ExtendRange(range);
}

// Ported from: itwinjs-core RuledSweep.tryTransformInPlace.
bool RuledSweep::TryTransformInPlace(Transform const& transform)
{
    for (auto const& c : m_curves)
        if (c) c->TryTransformInPlace(transform);
    return true;
}

// Ported from: itwinjs-core RuledSweep.clone
dqBase::RefPtr<GeometryQuery> RuledSweep::clone() const
{
    std::vector<dqBase::RefPtr<CurveCollection>> cloned;
    cloned.reserve(m_curves.size());
    for (auto const& c : m_curves)
        cloned.push_back(c ? c->clone().StaticCast<CurveCollection>() : nullptr);
    return Create(cloned, m_capped);
}

// Ported from: itwinjs-core RuledSweep.cloneTransformed
dqBase::RefPtr<GeometryQuery> RuledSweep::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<RuledSweep>();
    r->TryTransformInPlace(transform);
    return r;
}

bool RuledSweep::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::RuledSweep;
}

// Ported from: itwinjs-core RuledSweep.isAlmostEqual
bool RuledSweep::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<RuledSweep const&>(other);
    if (m_capped != o.m_capped) return false;
    if (m_curves.size() != o.m_curves.size()) return false;
    for (size_t i = 0; i < m_curves.size(); ++i) {
        if (!m_curves[i] || !o.m_curves[i]) return false;
        if (!m_curves[i]->IsAlmostEqual(*o.m_curves[i], tol)) return false;
    }
    return true;
}

END_DQ_GEOM_NAMESPACE
