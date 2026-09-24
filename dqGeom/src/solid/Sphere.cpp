// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/Sphere.ts
// DanQing dqGeom — Sphere implementation
#include "dqGeom/Sphere.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core Sphere private ctor (via clone/factory)
dqBase::RefPtr<Sphere> Sphere::Create(Transform const& localToWorld, AngleSweep const& latitudeSweep, bool capped)
{
    return dqBase::RefPtr<Sphere>(new Sphere(localToWorld, latitudeSweep, capped));
}

// Ported from: itwinjs-core Sphere.createCenterRadius — origin + uniform scale by radius.
dqBase::RefPtr<Sphere> Sphere::CreateCenterRadius(Point3d const& center, double radius,
                                                  AngleSweep const& latitudeSweep, bool capped)
{
    Transform localToWorld(center, Matrix3d::CreateUniformScale(radius));
    return Create(localToWorld, latitudeSweep, capped);
}

// Ported from: itwinjs-core Sphere.createEllipsoid — arbitrary local-to-world frame.
dqBase::RefPtr<Sphere> Sphere::CreateEllipsoid(Transform const& localToWorld, AngleSweep const& latitudeSweep, bool capped)
{
    return Create(localToWorld.clone(), latitudeSweep, capped);
}

// Ported from: itwinjs-core Sphere.isClosedVolume (capped || full-latitude sweep).
bool Sphere::IsClosedVolume() const
{
    return m_capped || m_latitudeSweep.SweepRadians() >= Angle::kPi - Angle::kSmallAngleRadians;
}

// Ported from: itwinjs-core Sphere.maxAxisRadius — largest frame column magnitude.
double Sphere::MaxAxisRadius() const noexcept
{
    Matrix3d const& m = m_localToWorld.matrix;
    double x = m.ColumnXMagnitude();
    double y = m.ColumnYMagnitude();
    double z = m.ColumnZMagnitude();
    return x > y ? (x > z ? x : z) : (y > z ? y : z);
}

Range3d Sphere::Range() const
{
    Range3d r = Range3d::CreateNull();
    ExtendRange(r);
    return r;
}

// Tight axis-aligned bbox of the ellipsoid = origin ± |matrix row i| per axis.
void Sphere::ExtendRange(Range3d& range) const
{
    Matrix3d const& m = m_localToWorld.matrix;
    double r0 = std::sqrt(m.coffs[0] * m.coffs[0] + m.coffs[1] * m.coffs[1] + m.coffs[2] * m.coffs[2]);
    double r1 = std::sqrt(m.coffs[3] * m.coffs[3] + m.coffs[4] * m.coffs[4] + m.coffs[5] * m.coffs[5]);
    double r2 = std::sqrt(m.coffs[6] * m.coffs[6] + m.coffs[7] * m.coffs[7] + m.coffs[8] * m.coffs[8]);
    Point3d o = m_localToWorld.origin;
    range.ExtendPoint(Point3d::From(o.x - r0, o.y - r1, o.z - r2));
    range.ExtendPoint(Point3d::From(o.x + r0, o.y + r1, o.z + r2));
}

// Ported from: itwinjs-core Sphere.tryTransformInPlace.
// TODO commit-N: Matrix3d::IsSingular guard + mirror branch (det<0 → scaleColumns z, flip latitude sweep).
bool Sphere::TryTransformInPlace(Transform const& transform)
{
    m_localToWorld = transform.MultiplyTransform(m_localToWorld);
    return true;
}

// Ported from: itwinjs-core Sphere.clone
dqBase::RefPtr<GeometryQuery> Sphere::clone() const
{
    return Create(m_localToWorld, m_latitudeSweep, m_capped);
}

// Ported from: itwinjs-core Sphere.cloneTransformed
dqBase::RefPtr<GeometryQuery> Sphere::CloneTransformed(Transform const& transform) const
{
    auto r = clone().StaticCast<Sphere>();
    r->TryTransformInPlace(transform);
    return r;
}

// No-RTTI discriminator (mirrors Box/Cone pattern).
bool Sphere::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    if (other.Category() != GeometryCategory::Solid) return false;
    return static_cast<SolidPrimitive const&>(other).GetSolidPrimitiveType() == SolidPrimitiveType::Sphere;
}

// Ported from: itwinjs-core Sphere.isAlmostEqual.
bool Sphere::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& o = static_cast<Sphere const&>(other);
    if (m_capped != o.m_capped) return false;
    if (!m_localToWorld.IsAlmostEqual(o.m_localToWorld, tol)) return false;
    return std::fabs(m_latitudeSweep.StartRadians() - o.m_latitudeSweep.StartRadians()) <= tol
        && std::fabs(m_latitudeSweep.SweepRadians() - o.m_latitudeSweep.SweepRadians()) <= tol;
}

// Ported from: itwinjs-core Sphere.uvFractionToPoint (Sphere.ts:294-308) —
// theta = longitude (u×2π), phi = latitude sweep 插值。
Point3d Sphere::UVFractionToPoint(double uFraction, double vFraction) const
{
    double const thetaRadians = UFractionToRadians(uFraction);
    double const phiRadians = VFractionToRadians(vFraction);
    double const cosTheta = std::cos(thetaRadians);
    double const sinTheta = std::sin(thetaRadians);
    double const sinPhi = std::sin(phiRadians);
    double const cosPhi = std::cos(phiRadians);
    return m_localToWorld.MultiplyXYZ(cosTheta * cosPhi, sinTheta * cosPhi, sinPhi);
}

// Ported from: itwinjs-core Sphere.uvFractionToPointAndTangents (Sphere.ts:312-327)
// NOTE: the u-derivative omits the cosPhi scale — the reference comment says the
// u-derivative scale is wrong but nonzero at the poles; 逐字保留（含该缺陷），
// addUVGridBody 只取其叉积方向（法线方向不受影响）。
Plane3dByOriginAndVectors Sphere::UVFractionToPointAndTangents(double uFraction, double vFraction) const
{
    double const thetaRadians = UFractionToRadians(uFraction);
    double const phiRadians = VFractionToRadians(vFraction);
    double const fTheta = Angle::k2Pi;
    double const fPhi = m_latitudeSweep.SweepRadians();
    double const cosTheta = std::cos(thetaRadians);
    double const sinTheta = std::sin(thetaRadians);
    double const sinPhi = std::sin(phiRadians);
    double const cosPhi = std::cos(phiRadians);
    return Plane3dByOriginAndVectors::createCapture(
        m_localToWorld.MultiplyXYZ(cosTheta * cosPhi, sinTheta * cosPhi, sinPhi),
        m_localToWorld.matrix.MultiplyVector(Vector3d::From(-fTheta * sinTheta, fTheta * cosTheta, 0.0)),
        m_localToWorld.matrix.MultiplyVector(Vector3d::From(-fPhi * cosTheta * sinPhi, -fPhi * sinTheta * sinPhi, fPhi * cosPhi)));
}

// Ported from: itwinjs-core Sphere.maxIsoParametricDistance (Sphere.ts:345-356) —
// u = 赤道周长（纬度 0 不在 sweep 内时按端点纬度圈缩放），v = 经线弧长。
Point2d Sphere::MaxIsoParametricDistance() const noexcept
{
    Matrix3d const& m = m_localToWorld.matrix;
    double const rX = m.ColumnXMagnitude();
    double const rY = m.ColumnYMagnitude();
    double const rZ = m.ColumnZMagnitude();
    double const rMaxU = rX > rY ? rX : rY;
    double dMaxU = Angle::k2Pi * rMaxU;
    if (!m_latitudeSweep.isRadiansInSweep(0.0)) {
        double const c0 = std::cos(std::fabs(m_latitudeSweep.StartRadians()));
        double const c1 = std::cos(std::fabs(m_latitudeSweep.EndRadians()));
        dMaxU *= c0 > c1 ? c0 : c1;
    }
    double const dMaxV = (rMaxU > rZ ? rMaxU : rZ) * std::fabs(m_latitudeSweep.SweepRadians());
    return Point2d::From(dMaxU, dMaxV);
}

END_DQ_GEOM_NAMESPACE
