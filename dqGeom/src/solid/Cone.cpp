// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/Cone.ts
// DanQing dqGeom — Cone implementation
#include "dqGeom/Cone.h"

#include "dqGeom/Angle.h"     // Angle::k2Pi
#include "dqGeom/Geometry.h"  // interpolate / hypotenuseXY

#include <algorithm>
#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core Cone protected ctor (via clone/factory)
dqBase::RefPtr<Cone> Cone::Create(Transform const& map, double radiusA, double radiusB, bool capped)
{
    return dqBase::RefPtr<Cone>(new Cone(map, radiusA, radiusB, capped));
}

// Ported from: itwinjs-core Cone.createBaseAndTarget — frame from centerA + vectorX/Y/Z, |radii|.
dqBase::RefPtr<Cone> Cone::CreateBaseAndTarget(Point3d const& centerA, Point3d const& centerB,
                                               Vector3d const& vectorX, Vector3d const& vectorY,
                                               double radiusA, double radiusB, bool capped)
{
    Vector3d vectorZ = Vector3d::From(centerB.x - centerA.x, centerB.y - centerA.y, centerB.z - centerA.z);
    Transform localToWorld = Transform::CreateOriginAndMatrixColumns(centerA, vectorX, vectorY, vectorZ);
    return Create(localToWorld, std::fabs(radiusA), std::fabs(radiusB), capped);
}

Range3d Cone::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Bbox = union of the two end circles' axis-aligned boxes (tight per-axis half-extent).
void Cone::ExtendRange(Range3d& range) const
{
    Vector3d vX = GetVectorX();
    Vector3d vY = GetVectorY();
    auto extendCircle = [&](Point3d const& c, double r) {
        double hx = r * std::sqrt(vX.x * vX.x + vY.x * vY.x);
        double hy = r * std::sqrt(vX.y * vX.y + vY.y * vY.y);
        double hz = r * std::sqrt(vX.z * vX.z + vY.z * vY.z);
        range.ExtendPoint(Point3d::From(c.x - hx, c.y - hy, c.z - hz));
        range.ExtendPoint(Point3d::From(c.x + hx, c.y + hy, c.z + hz));
    };
    extendCircle(GetCenterA(), m_radiusA);
    extendCircle(GetCenterB(), m_radiusB);
}

// Ported from: itwinjs-core Cone.tryTransformInPlace.
// TODO commit-N: Matrix3d::IsSingular guard + mirror branch (det<0 → swap radiusA/B, reverse z).
bool Cone::TryTransformInPlace(Transform const& transform)
{
    m_localToWorld = transform.MultiplyTransform(m_localToWorld);
    return true;
}

// Ported from: itwinjs-core Cone.clone
dqBase::RefPtr<GeometryQuery> Cone::clone() const
{
    return Create(m_localToWorld, m_radiusA, m_radiusB, m_capped);
}

// Ported from: itwinjs-core Cone.cloneTransformed
dqBase::RefPtr<GeometryQuery> Cone::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<Cone>();
    r->TryTransformInPlace(transform);
    return r;
}

// No-RTTI discriminator (mirrors Box::IsSameGeometryClass pattern).
bool Cone::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::Cone;
}

// Ported from: itwinjs-core Cone.isAlmostEqual (reference uses isAlmostEqualAllowZRotation; phase to IsAlmostEqual).
bool Cone::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<Cone const&>(other);
    if (m_capped != o.m_capped) return false;
    if (!m_localToWorld.IsAlmostEqual(o.m_localToWorld, tol)) return false;
    return std::fabs(m_radiusA - o.m_radiusA) <= tol && std::fabs(m_radiusB - o.m_radiusB) <= tol;
}

// Ported from: itwinjs-core Cone.uvFractionToPoint (Cone.ts:275-280) —
// v=0 底面 / v=1 顶面；u∈[0,1] 环绕一周。
Point3d Cone::UVFractionToPoint(double uFraction, double vFraction) const
{
    double const theta = uFraction * Angle::k2Pi;
    double const r = interpolate(m_radiusA, vFraction, m_radiusB);
    double const cosTheta = std::cos(theta);
    double const sinTheta = std::sin(theta);
    return m_localToWorld.MultiplyXYZ(r * cosTheta, r * sinTheta, vFraction);
}

// Ported from: itwinjs-core Cone.uvFractionToPointAndTangents (Cone.ts:286-299)
Plane3dByOriginAndVectors Cone::UVFractionToPointAndTangents(double uFraction, double vFraction) const
{
    double const theta = uFraction * Angle::k2Pi;
    double const r = interpolate(m_radiusA, vFraction, m_radiusB);
    double const drdv = m_radiusB - m_radiusA;
    double const cosTheta = std::cos(theta);
    double const sinTheta = std::sin(theta);
    double const fTheta = Angle::k2Pi;
    return Plane3dByOriginAndVectors::createCapture(
        m_localToWorld.MultiplyXYZ(r * cosTheta, r * sinTheta, vFraction),
        m_localToWorld.MultiplyVector(Vector3d::From(-r * sinTheta * fTheta, r * cosTheta * fTheta, 0.0)),
        m_localToWorld.MultiplyVector(Vector3d::From(drdv * cosTheta, drdv * sinTheta, 1.0)));
}

// Ported from: itwinjs-core Cone.maxIsoParametricDistance (Cone.ts:334-346) —
// u = 最大端圆周长；v = 母线长（径向差 + z 偏斜，勾股合成）。实例退化时返回零向量。
Point2d Cone::MaxIsoParametricDistance() const noexcept
{
    Vector3d const vectorX = m_localToWorld.matrix.ColumnX();
    Vector3d const vectorY = m_localToWorld.matrix.ColumnY();
    // unitCrossProduct(vectorX, vectorY)：叉积过小时参考返回 undefined。
    Vector3d xyNormal = Vector3d::FromCrossProduct(vectorX, vectorY);
    if (xyNormal.Normalize() == 0.0)
        return Point2d::From(0.0, 0.0);
    Vector3d const columnZ = m_localToWorld.matrix.ColumnZ();
    double const hZ = xyNormal.DotProduct(columnZ);
    // zSkewVector = columnZ.plusScaled(xyNormal, hZ)（逐字参考——注意是 +hZ）
    Vector3d const zSkewVector = Vector3d::From(
        columnZ.x + xyNormal.x * hZ, columnZ.y + xyNormal.y * hZ, columnZ.z + xyNormal.z * hZ);
    double const zSkewDistance = zSkewVector.MagnitudeXY();
    double const rMax = m_radiusA > m_radiusB ? m_radiusA : m_radiusB;
    return Point2d::From(Angle::k2Pi * rMax,
                         hypotenuseXY(std::fabs(m_radiusB - m_radiusA) + zSkewDistance, hZ));
}

END_DQ_GEOM_NAMESPACE
