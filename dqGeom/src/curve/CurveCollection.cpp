// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/curve/CurveCollection.ts (CurveChain method bodies)
// DanQing dqGeom — CurveCollection / CurveChain implementation
#include "dqGeom/CurveCollection.h"

#include "dqGeom/Point3d.h"
#include "dqGeom/Range3d.h"

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core CurveChain.getChild
CurvePrimitivePtr CurveChain::GetChild(int index) const noexcept {
    if (index < 0 || static_cast<size_t>(index) >= m_curves.size())
        return nullptr;
    return m_curves[static_cast<size_t>(index)];
}

// Ported from: itwinjs-core CurveChain.tryAddChild (captures a CurvePrimitive)
bool CurveChain::TryAddChild(CurvePrimitivePtr const& child) noexcept {
    if (!child)
        return false;
    m_curves.push_back(child);
    return true;
}

// Ported from: itwinjs-core CurveChain.startPoint (fraction 0 of first child)
std::optional<Point3d> CurveChain::StartPoint() const {
    if (m_curves.empty())
        return std::nullopt;
    return m_curves.front()->FractionToPoint(0.0);
}

// Ported from: itwinjs-core CurveChain.endPoint (fraction 1 of last child)
std::optional<Point3d> CurveChain::EndPoint() const {
    if (m_curves.empty())
        return std::nullopt;
    return m_curves.back()->FractionToPoint(1.0);
}

// Ported from: itwinjs-core CurveChain.isPhysicallyClosedCurve
bool CurveChain::IsPhysicallyClosedCurve(double tolerance, bool xyOnly) const {
    auto p0 = StartPoint();
    auto p1 = EndPoint();
    if (!p0 || !p1)
        return false;
    if (xyOnly) {
        return std::fabs(p0->x - p1->x) <= tolerance && std::fabs(p0->y - p1->y) <= tolerance;
    }
    return p0->IsEqual(*p1, tolerance);
}

// Ported from: itwinjs-core CurveChain.reverseChildrenInPlace.
// Reverses child order; per-child reverseInPlace pending CurvePrimitive::ReverseInPlace port (Phase-N).
void CurveChain::ReverseChildrenInPlace() {
    // TODO Phase-N: call child->ReverseInPlace() for each child once CurvePrimitive::ReverseInPlace is ported.
    std::reverse(m_curves.begin(), m_curves.end());
}

// Ported from: itwinjs-core CurveChain.extendRange (extend by each child's range)
void CurveChain::ExtendRange(Range3d& range) const {
    for (auto const& curve : m_curves)
        curve->ExtendRange(range);
}

// Ported from: itwinjs-core CurveCollection.range (bbox over children)
Range3d CurveChain::Range() const {
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Ported from: itwinjs-core CurveCollection.tryTransformInPlace (transform each child)
bool CurveChain::TryTransformInPlace(Transform const& transform) {
    for (auto const& curve : m_curves)
        curve->TryTransformInPlace(transform);
    return true;
}

// Ported from: itwinjs-core GeometryQuery.clone (empty peer + cloned children)
dqBase::RefPtr<GeometryQuery> CurveChain::clone() const {
    auto result = CloneEmptyPeer();
    for (auto const& curve : m_curves)
        result->m_curves.push_back(curve->clone().StaticCast<CurvePrimitive>());
    return result;
}

// Ported from: itwinjs-core GeometryQuery.cloneTransformed (clone then transform)
dqBase::RefPtr<GeometryQuery> CurveChain::CloneTransformed(Transform const& transform) const {
    auto result = clone();
    result->TryTransformInPlace(transform);
    return result;
}

// No-RTTI discriminator match (mirrors CurvePrimitive::IsSameGeometryClass pattern).
bool CurveChain::IsSameGeometryClass(GeometryQuery const& other) const noexcept {
    if (other.Category() != GeometryCategory::CurveCollection) return false;
    return static_cast<CurveCollection const&>(other).GetCurveCollectionType() == GetCurveCollectionType();
}

// Ported from: itwinjs-core CurveCollection.isAlmostEqual (same class + per-child isAlmostEqual)
bool CurveChain::IsAlmostEqual(GeometryQuery const& other, double tol) const {
    if (!IsSameGeometryClass(other)) return false;
    auto const& otherChain = static_cast<CurveChain const&>(other);
    if (m_curves.size() != otherChain.m_curves.size()) return false;
    for (size_t i = 0; i < m_curves.size(); ++i) {
        if (!m_curves[i]->IsAlmostEqual(*otherChain.m_curves[i], tol)) return false;
    }
    return true;
}

END_DQ_GEOM_NAMESPACE
