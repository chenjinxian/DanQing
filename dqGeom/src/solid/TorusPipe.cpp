// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/TorusPipe.ts
// DanQing dqGeom — TorusPipe implementation
#include "dqGeom/TorusPipe.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core TorusPipe protected ctor (via clone/factory)
dqBase::RefPtr<TorusPipe> TorusPipe::Create(Transform const& map, double radiusA, double radiusB,
                                            Angle const& sweep, bool capped)
{
    return dqBase::RefPtr<TorusPipe>(new TorusPipe(map, radiusA, radiusB, sweep, capped));
}

// Ported from: itwinjs-core TorusPipe.createInFrame — validate radii/sweep; mirror+negative-sweep
// branch (scaleColumnsInPlace) is Phase-N.
dqBase::RefPtr<TorusPipe> TorusPipe::CreateInFrame(Transform const& frame, double majorRadius, double minorRadius,
                                                   Angle const& sweep, bool capped)
{
    majorRadius = std::fabs(majorRadius);
    minorRadius = std::fabs(minorRadius);
    if (majorRadius < minorRadius) return nullptr;
    if (majorRadius == 0.0 || minorRadius == 0.0) return nullptr;
    if (sweep.IsAlmostZero()) return nullptr;
    // TODO commit-N: mirror (det<0) + negative-sweep handling (needs Matrix3d::scaleColumnsInPlace).
    return Create(frame, majorRadius, minorRadius, sweep, capped);
}

Range3d TorusPipe::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Bbox of the torus pipe: outer extent = (majorRadius + minorRadius) in xy, minorRadius in z (local),
// then mapped through the frame. Sample the local bbox corners through localToWorld for a containing box.
void TorusPipe::ExtendRange(Range3d& range) const
{
    Transform const& t = m_localToWorld;
    double outer = m_radiusA + m_radiusB;
    double inner = m_radiusA - m_radiusB;
    // Local-space extremes: x,y in [-outer, +outer]; z in [-m_radiusB, +m_radiusB].
    // Map the 8 local bbox corners through the frame for a containing world box.
    double xs[2] = {-outer, outer};
    double ys[2] = {-outer, outer};
    double zs[2] = {-m_radiusB, m_radiusB};
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 2; ++k)
                range.ExtendPoint(t.MultiplyXYZ(xs[i], ys[j], zs[k]));
    (void)inner;
}

// Ported from: itwinjs-core TorusPipe.tryTransformInPlace.
// TODO commit-N: Matrix3d::IsSingular guard + mirror branch (scaleColumns z).
bool TorusPipe::TryTransformInPlace(Transform const& transform)
{
    m_localToWorld = transform.MultiplyTransform(m_localToWorld);
    return true;
}

// Ported from: itwinjs-core TorusPipe.clone
dqBase::RefPtr<GeometryQuery> TorusPipe::clone() const
{
    return Create(m_localToWorld, m_radiusA, m_radiusB, m_sweep, m_capped);
}

// Ported from: itwinjs-core TorusPipe.cloneTransformed
dqBase::RefPtr<GeometryQuery> TorusPipe::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<TorusPipe>();
    r->TryTransformInPlace(transform);
    return r;
}

// No-RTTI discriminator (mirrors Box/Cone/Sphere pattern).
bool TorusPipe::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::TorusPipe;
}

// Ported from: itwinjs-core TorusPipe.isAlmostEqual.
bool TorusPipe::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<TorusPipe const&>(other);
    if (m_capped != o.m_capped) return false;
    if (!m_localToWorld.IsAlmostEqual(o.m_localToWorld, tol)) return false;
    return std::fabs(m_radiusA - o.m_radiusA) <= tol
        && std::fabs(m_radiusB - o.m_radiusB) <= tol
        && std::fabs(m_sweep.Radians() - o.m_sweep.Radians()) <= tol;
}

END_DQ_GEOM_NAMESPACE
