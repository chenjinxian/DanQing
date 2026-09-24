// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/Box.ts
// DanQing dqGeom — Box implementation
#include "dqGeom/Box.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core Box protected ctor (via clone/factory)
dqBase::RefPtr<Box> Box::Create(Transform const& map, double baseX, double baseY,
                                double topX, double topY, bool capped)
{
    return dqBase::RefPtr<Box>(new Box(map, baseX, baseY, topX, topY, capped));
}

// Ported from: itwinjs-core Box.createDgnBox
dqBase::RefPtr<Box> Box::CreateDgnBox(Point3d const& origin, Vector3d const& vectorX, Vector3d const& vectorY,
                                      Point3d const& topOrigin,
                                      double baseX, double baseY, double topX, double topY, bool capped)
{
    Vector3d vectorZ = Vector3d::From(topOrigin.x - origin.x, topOrigin.y - origin.y, topOrigin.z - origin.z);
    Transform localToWorld = Transform::CreateOriginAndMatrixColumns(origin, vectorX, vectorY, vectorZ);
    return Create(localToWorld, baseX, baseY, topX, topY, capped);
}

// Ported from: itwinjs-core Box.createRange
dqBase::RefPtr<Box> Box::CreateRange(Range3d const& range, bool capped)
{
    if (range.isNull())
        return nullptr;
    Point3d low = range.low;
    double xSize = range.XLength();
    double ySize = range.YLength();
    Point3d zPoint = Point3d::From(low.x, low.y, low.z + range.ZLength());
    return CreateDgnBox(low, Vector3d::UnitX(), Vector3d::UnitY(), zPoint,
                        xSize, ySize, xSize, ySize, capped);
}

// Ported from: itwinjs-core Box.getCorners (x-fastest then y then z lexical order)
std::array<Point3d, 8> Box::GetCorners() const
{
    Transform const& t = m_localToWorld;
    double ax = m_baseX, ay = m_baseY, bx = m_topX, by = m_topY;
    return {{
        t.MultiplyXYZ(0, 0, 0),  t.MultiplyXYZ(ax, 0, 0),  t.MultiplyXYZ(0, ay, 0),  t.MultiplyXYZ(ax, ay, 0),
        t.MultiplyXYZ(0, 0, 1),  t.MultiplyXYZ(bx, 0, 1),  t.MultiplyXYZ(0, by, 1),  t.MultiplyXYZ(bx, by, 1),
    }};
}

// TODO commit-N: port strokeConstantVSection (interpolated rectangle) + Loop::Create for full faithful.
dqBase::RefPtr<CurveCollection> Box::ConstantVSection(double /*zFraction*/) const
{
    return nullptr;
}

// TODO commit-N: return CloneRigid() once Transform::CloneRigid ported (rigid axis normalization).
std::optional<Transform> Box::GetConstructiveFrame() const
{
    return m_localToWorld.clone();
}

Range3d Box::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Ported from: itwinjs-core Box.extendRange (non-transform path; transform variant Phase-N).
void Box::ExtendRange(Range3d& range) const
{
    auto corners = GetCorners();
    for (auto const& c : corners)
        range.ExtendPoint(c);
}

// Ported from: itwinjs-core Box.tryTransformInPlace.
// TODO commit-N: Matrix3d::IsSingular guard + mirror branch (det<0 → reverse z, ScaleColumnsInPlace, swap base/top).
bool Box::TryTransformInPlace(Transform const& transform)
{
    m_localToWorld = transform.MultiplyTransform(m_localToWorld);
    return true;
}

// Ported from: itwinjs-core Box.clone
dqBase::RefPtr<GeometryQuery> Box::clone() const
{
    return Create(m_localToWorld, m_baseX, m_baseY, m_topX, m_topY, m_capped);
}

// Ported from: itwinjs-core Box.cloneTransformed
dqBase::RefPtr<GeometryQuery> Box::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<Box>();
    r->TryTransformInPlace(transform);
    return r;
}

// No-RTTI discriminator (mirrors CurvePrimitive::IsSameGeometryClass pattern).
bool Box::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::Box;
}

// Ported from: itwinjs-core Box.isAlmostEqual
bool Box::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<Box const&>(other);
    if (m_capped != o.m_capped) return false;
    if (!m_localToWorld.IsAlmostEqual(o.m_localToWorld, tol)) return false;
    return std::fabs(m_baseX - o.m_baseX) <= tol && std::fabs(m_baseY - o.m_baseY) <= tol
        && std::fabs(m_topX - o.m_topX) <= tol && std::fabs(m_topY - o.m_topY) <= tol;
}

END_DQ_GEOM_NAMESPACE
