// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Ellipsoid implementation
// Ported from: itwinjs-core core/geometry/src/geometry3d/Ellipsoid.ts
#include "dqGeom/Ellipsoid.h"

#include "dqGeom/Geometry.h"
#include "dqGeom/Newton.h"
#include "dqGeom/Polynomials.h"

#include <cmath>

namespace dqGeom {

// ---------------------------------------------------------------------------
// EllipsoidClosestPoint — internal Newton evaluator for projectPointToSurface.
// Ported from: itwinjs-core EllipsoidClosestPoint (Ellipsoid.ts:912-967)
// ---------------------------------------------------------------------------
namespace {

class EllipsoidClosestPoint : public NewtonEvaluatorRRtoRRD {
public:
    explicit EllipsoidClosestPoint(Ellipsoid const& ellipsoid) noexcept
        : m_ellipsoid(ellipsoid) {}

    /// Ported from: EllipsoidClosestPoint.searchClosestPoint (:934-948)
    std::optional<LongitudeLatitudeNumber> searchClosestPoint(Point3d const& spacePoint) {
        m_spacePoint = spacePoint;
        Point3d localPoint;
        if (!m_ellipsoid.transformRef().MultiplyInversePoint3d(spacePoint, localPoint))
            return std::nullopt;
        SphereImplicit const sphere(1.0);
        auto uv = sphere.xyzToThetaPhiR(localPoint);
        Newton2dUnboundedWithDerivative newtonSearcher(*this);
        newtonSearcher.setUV(uv.thetaRadians, uv.phiRadians);
        if (newtonSearcher.runIterations()) {
            uv.thetaRadians = newtonSearcher.getU();
            uv.phiRadians = newtonSearcher.getV();
        }
        return LongitudeLatitudeNumber::createRadians(uv.thetaRadians, uv.phiRadians, 0.0);
    }

    /// Ported from: EllipsoidClosestPoint.evaluate (:949-966)
    bool evaluate(double thetaRadians, double phiRadians) override {
        m_ellipsoid.radiansToPointAnd2Derivatives(thetaRadians, phiRadians,
                                                  m_surfacePoint,
                                                  m_d1Theta, m_d1Phi,
                                                  m_d2Theta, m_d2Phi,
                                                  m_d2ThetaPhi);
        m_delta = Vector3d::FromStartEnd(m_spacePoint, m_surfacePoint);
        double const q = m_d1Theta.DotProduct(m_d1Phi) + m_delta.DotProduct(m_d2ThetaPhi);
        currentF.setOriginAndVectorsXYZ(
            // f,g,0
            m_delta.DotProduct(m_d1Theta), m_delta.DotProduct(m_d1Phi), 0.0,
            // df/dTheta, dg/dTheta, 0
            m_d1Theta.DotProduct(m_d1Theta) + m_delta.DotProduct(m_d2Theta), q, 0.0,
            // df/dPhi, dg/dPhi, 0
            q, m_d1Phi.DotProduct(m_d1Phi) + m_delta.DotProduct(m_d2Phi), 0.0);
        return true;
    }

private:
    Ellipsoid const& m_ellipsoid;
    Point3d m_spacePoint;
    Point3d m_surfacePoint;
    Vector3d m_d1Theta;
    Vector3d m_d2Theta;
    Vector3d m_d1Phi;
    Vector3d m_d2Phi;
    Vector3d m_d2ThetaPhi;
    Vector3d m_delta;
};

}  // namespace

// ---------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------

// Ported from: Ellipsoid.create (:167-174)
Ellipsoid Ellipsoid::create() {
    return Ellipsoid(Transform::CreateIdentity());
}

Ellipsoid Ellipsoid::create(Transform const& transform) {
    return Ellipsoid(transform);
}

Ellipsoid Ellipsoid::create(Matrix3d const& matrix) {
    return Ellipsoid(Transform::CreateOriginAndMatrix(Point3d::FromZero(), matrix));
}

// Ported from: Ellipsoid.createCenterMatrixRadii (:183-190)
Ellipsoid Ellipsoid::createCenterMatrixRadii(Point3d const& center, Matrix3d const* axes,
                                             double radiusX, double radiusY, double radiusZ) noexcept {
    Matrix3d const scaledAxes = (axes == nullptr)
        ? Matrix3d::CreateScale(radiusX, radiusY, radiusZ)
        : axes->scaleColumns(radiusX, radiusY, radiusZ);
    return Ellipsoid(Transform::CreateOriginAndMatrix(center, scaledAxes));
}

// Ported from: Ellipsoid.worldToLocal (:208-210)
std::optional<Point3d> Ellipsoid::worldToLocal(Point3d const& worldPoint) const noexcept {
    Point3d result;
    if (!m_transform.MultiplyInversePoint3d(worldPoint, result))
        return std::nullopt;
    return result;
}

// Ported from: Ellipsoid.localToWorld (:217-219)
Point3d Ellipsoid::localToWorld(Point3d const& localPoint) const noexcept {
    return m_transform.MultiplyPoint3d(localPoint);
}

// Ported from: Ellipsoid.projectPointToSurface (:247-250)
std::optional<LongitudeLatitudeNumber> Ellipsoid::projectPointToSurface(Point3d const& spacePoint) const {
    EllipsoidClosestPoint searcher(*this);
    return searcher.searchClosestPoint(spacePoint);
}

// Ported from: Ellipsoid.silhouetteArc (:255-273)
dqBase::RefPtr<Arc3d> Ellipsoid::silhouetteArc(Point4d const& eyePoint) const {
    auto const localEyePoint = m_transform.multiplyInversePoint4d(eyePoint);
    if (localEyePoint.has_value()) {
        // localEyePoint is now looking at a unit sphere centered at the origin.
        // the plane through the silhouette is the eye point with z negated ...
        Point4d const localPlaneA = Point4d::create(
            localEyePoint->x, localEyePoint->y, localEyePoint->z, -localEyePoint->w);
        auto const localPlaneB = localPlaneA.toPlane3dByOriginAndUnitNormal();
        // if the silhouette plane has origin inside the sphere, there is a silhouette
        // with center at the plane origin.
        if (localPlaneB.has_value()) {
            double const rr = 1.0 - localPlaneB->getOriginRef().MagnitudeSquared();  // squared radius of silhouette arc
            if (rr > 0.0 && rr <= 1.0) {
                auto arc = Arc3d::CreateCenterNormalRadius(
                    localPlaneB->getOriginRef(), localPlaneB->getNormalRef(), std::sqrt(rr));
                if (arc->TryTransformInPlace(m_transform))
                    return arc;
            }
        }
    }
    return nullptr;
}

// Ported from: Ellipsoid.intersectRay (:284-313)
size_t Ellipsoid::intersectRay(Ray3d const& ray,
                               std::vector<double>* rayFractions,
                               std::vector<Point3d>* xyz,
                               std::vector<LongitudeLatitudeNumber>* thetaPhiRadians) const {
    if (xyz != nullptr)
        xyz->clear();
    if (thetaPhiRadians != nullptr)
        thetaPhiRadians->clear();
    if (rayFractions != nullptr)
        rayFractions->clear();
    // if ray comes in unit vector in large ellipsoid, localRay direction is minuscule.
    // use a ray scaled up so its direction vector magnitude is comparable to the
    // ellipsoid radiusX
    Ray3d ray1 = ray.clone();
    double const a0 = ray.direction.Magnitude();
    double const aX = m_transform.matrix.ColumnXMagnitude();
    auto const scale = conditionalDivideCoordinate(aX, a0);
    if (!scale.has_value())
        return 0;
    ray1.direction.Scale(*scale);
    auto const localRay = ray1.cloneInverseTransformed(m_transform);
    if (localRay.has_value()) {
        size_t const n = SphereImplicit::intersectSphereRay(
            Point3d::FromZero(), 1.0, *localRay, rayFractions, xyz, thetaPhiRadians);
        if (rayFractions != nullptr)
            for (double& f : *rayFractions)
                f *= *scale;
        if (xyz != nullptr)
            m_transform.MultiplyPoint3dArrayInPlace(*xyz);
        return n;
    }
    return 0;
}

// Ported from: Ellipsoid.radiansToPoint (:359-365)
Point3d Ellipsoid::radiansToPoint(double thetaRadians, double phiRadians) const noexcept {
    double const cosTheta = std::cos(thetaRadians);
    double const sinTheta = std::sin(thetaRadians);
    double const cosPhi = std::cos(phiRadians);
    double const sinPhi = std::sin(phiRadians);
    return m_transform.MultiplyXYZ(cosTheta * cosPhi, sinTheta * cosPhi, sinPhi);
}

// Ported from: Ellipsoid.radiansToPointAndDerivatives (:569-586, non-`result` branch)
Plane3dByOriginAndVectors Ellipsoid::radiansToPointAndDerivatives(
    double thetaRadians, double phiRadians, bool applyCosPhiFactor) const noexcept {
    double const cosTheta = std::cos(thetaRadians);
    double const sinTheta = std::sin(thetaRadians);
    double const cosPhi = std::cos(phiRadians);
    double const cosPhiA = applyCosPhiFactor ? cosPhi : 1.0;
    double const sinPhi = std::sin(phiRadians);
    Matrix3d const& matrix = m_transform.matrix;
    return Plane3dByOriginAndVectors::createCapture(
        m_transform.MultiplyXYZ(cosTheta * cosPhi, sinTheta * cosPhi, sinPhi),
        matrix.MultiplyVector(Vector3d::From(-sinTheta * cosPhiA, cosTheta * cosPhiA, 0.0)),
        matrix.MultiplyVector(Vector3d::From(-sinPhi * cosTheta, -sinPhi * sinTheta, cosPhi)));
}

// Ported from: Ellipsoid.radiansToPointAnd2Derivatives (:600-623)
void Ellipsoid::radiansToPointAnd2Derivatives(double thetaRadians, double phiRadians,
                                              Point3d& point,
                                              Vector3d& d1Theta, Vector3d& d1Phi,
                                              Vector3d& d2ThetaTheta, Vector3d& d2PhiPhi,
                                              Vector3d& d2ThetaPhi) const noexcept {
    double const cosTheta = std::cos(thetaRadians);
    double const sinTheta = std::sin(thetaRadians);
    double const cosPhi = std::cos(phiRadians);
    double const sinPhi = std::sin(phiRadians);
    Matrix3d const& matrix = m_transform.matrix;
    point = m_transform.MultiplyXYZ(cosTheta * cosPhi, sinTheta * cosPhi, sinPhi);
    // theta derivatives
    d1Theta = matrix.MultiplyVector(Vector3d::From(-sinTheta * cosPhi, cosTheta * cosPhi, 0.0));
    d2ThetaTheta = matrix.MultiplyVector(Vector3d::From(-cosTheta * cosPhi, -sinTheta * cosPhi, 0.0));
    // phi derivatives
    d1Phi = matrix.MultiplyVector(Vector3d::From(-cosTheta * sinPhi, -sinTheta * sinPhi, cosPhi));
    d2PhiPhi = matrix.MultiplyVector(Vector3d::From(-cosTheta * cosPhi, -sinTheta * cosPhi, -sinPhi));
    // mixed derivative
    d2ThetaPhi = matrix.MultiplyVector(Vector3d::From(sinTheta * sinPhi, -cosTheta * sinPhi, 0.0));
}

// Ported from: Ellipsoid.radiansToUnitNormalRay (:644-647)
std::optional<Ray3d> Ellipsoid::radiansToUnitNormalRay(double thetaRadians, double phiRadians) const noexcept {
    auto const plane = radiansToPointAndDerivatives(thetaRadians, phiRadians, false);
    return plane.unitNormalRay();
}

// Ported from: Ellipsoid.surfaceNormalToAngles (:652-661)
LongitudeLatitudeNumber Ellipsoid::surfaceNormalToAngles(Vector3d const& normal) const noexcept {
    Matrix3d const& matrix = m_transform.matrix;
    Vector3d const conjugateVector = matrix.MultiplyTransposeVector(normal);
    double const thetaRadians = std::atan2(conjugateVector.y, conjugateVector.x);
    // For that phi arc,
    double const axy = -(conjugateVector.x * std::cos(thetaRadians) + conjugateVector.y * std::sin(thetaRadians));
    double const az = conjugateVector.z;
    double const phiRadians = std::atan2(az, -axy);
    return LongitudeLatitudeNumber::createRadians(thetaRadians, phiRadians, 0.0);
}

// Ported from: Ellipsoid.otherEllipsoidAnglesToThisEllipsoidAngles (:668-673)
std::optional<LongitudeLatitudeNumber> Ellipsoid::otherEllipsoidAnglesToThisEllipsoidAngles(
    Ellipsoid const* otherEllipsoid, LongitudeLatitudeNumber const& otherAngles) const noexcept {
    auto const normal = Ellipsoid::radiansToUnitNormalRay(
        otherEllipsoid, otherAngles.longitudeRadians(), otherAngles.latitudeRadians());
    if (normal.has_value())
        return surfaceNormalToAngles(normal->direction);
    return std::nullopt;
}

// Ported from: Ellipsoid.radiansToUnitNormalRay (static, :678-688)
std::optional<Ray3d> Ellipsoid::radiansToUnitNormalRay(
    Ellipsoid const* ellipsoid, double thetaRadians, double phiRadians) noexcept {
    if (ellipsoid != nullptr)
        return ellipsoid->radiansToUnitNormalRay(thetaRadians, phiRadians);
    // for unit sphere, the vector from center to surface point is identical to the
    // unit normal.
    Ray3d result = Ray3d::createZAxis();
    Vector3d xyz;
    SphereImplicit::radiansToUnitSphereXYZ(thetaRadians, phiRadians, xyz);
    result.origin = Point3d::From(xyz.x, xyz.y, xyz.z);
    result.direction = Vector3d::From(result.origin);
    return result;
}

// Ported from: Ellipsoid.createPlaneSection (:405-426)
dqBase::RefPtr<Arc3d> Ellipsoid::createPlaneSection(Plane3dByOriginAndUnitNormal const& plane) const {
    auto const localPlane = plane.cloneTransformed(m_transform, true);
    if (localPlane.has_value()) {
        // construct center and arc vectors in the local system --- later transform
        // them out to global.
        Point3d center = localPlane->projectPointToPlane(Point3d::FromZero());
        double const d = center.Magnitude();
        if (d < 1.0) {
            Matrix3d const frame = Matrix3d::CreateRigidHeadsUp(localPlane->getNormalRef(), AxisOrder::ZYX);
            Vector3d vector0 = frame.ColumnX();
            Vector3d vector90 = frame.ColumnY();
            double const sectionRadius = std::sqrt(1.0 - d * d);
            vector0.Scale(sectionRadius);
            vector90.Scale(sectionRadius);

            center = m_transform.MultiplyPoint3d(center);
            vector0 = m_transform.MultiplyVector(vector0);
            vector90 = m_transform.MultiplyVector(vector90);
            return Arc3d::FromVectors(center, vector0, vector90, AngleSweep::FullCircle());
        }
    }
    return nullptr;
}

// Ported from: Ellipsoid.isPointOnOrInside (:690-695)
bool Ellipsoid::isPointOnOrInside(Point3d const& point) const noexcept {
    Point3d localPoint;
    if (m_transform.MultiplyInversePoint3d(point, localPoint))
        return localPoint.Magnitude() <= 1.0;
    return false;
}

} // namespace dqGeom
