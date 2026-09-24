// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Plane3dByOriginAndVectors (plane by origin + in-plane vectors)
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Plane3dByOriginAndVectors.ts
//
// Subset: storage + the factories/evaluators consumed by the Ellipsoid port
// (radiansToPointAndDerivatives, NewtonEvaluatorRRtoRRD.currentF) and the ported
// tests (unitNormalRay). The full Plane3d interface surface (transforms, grids,
// frenet frames, ...) is TODO — add as consumers are ported.
#pragma once

#include "Geometry.h"
#include "Point3d.h"
#include "Ray3d.h"
#include "Vector3d.h"

#include <optional>

namespace dqGeom {

/// A plane defined by an origin and two vectors in the plane.
/// Ported from: itwinjs-core Plane3dByOriginAndVectors
struct DQ_GEOM_EXPORT Plane3dByOriginAndVectors {
    Point3d origin;
    Vector3d vectorU;
    Vector3d vectorV;

    Plane3dByOriginAndVectors() = default;
    Plane3dByOriginAndVectors(Point3d const& o, Vector3d const& u, Vector3d const& v) noexcept
        : origin(o), vectorU(u), vectorV(v) {}

    /// Ported from: Plane3dByOriginAndVectors.createCapture (:114-123, non-`result` branch)
    static Plane3dByOriginAndVectors createCapture(
        Point3d const& origin, Vector3d const& vectorU, Vector3d const& vectorV) noexcept {
        return Plane3dByOriginAndVectors(origin, vectorU, vectorV);
    }

    /// Ported from: Plane3dByOriginAndVectors.createOriginAndVectorsXYZ (:149-156,
    /// non-`result` branch)
    static Plane3dByOriginAndVectors createOriginAndVectorsXYZ(
        double x0, double y0, double z0, double ux, double uy, double uz,
        double vx, double vy, double vz) noexcept {
        return Plane3dByOriginAndVectors(
            Point3d::From(x0, y0, z0), Vector3d::From(ux, uy, uz), Vector3d::From(vx, vy, vz));
    }

    /// Ported from: Plane3dByOriginAndVectors.createXYPlane (:175-177, non-`result` branch)
    static Plane3dByOriginAndVectors createXYPlane() noexcept {
        return createOriginAndVectorsXYZ(0, 0, 0, 1, 0, 0, 0, 1, 0);
    }

    /// Set all origin and both vectors from direct numeric parameters.
    /// Ported from: Plane3dByOriginAndVectors.setOriginAndVectorsXYZ (:126-133)
    Plane3dByOriginAndVectors& setOriginAndVectorsXYZ(
        double x0, double y0, double z0, double ux, double uy, double uz,
        double vx, double vy, double vz) noexcept {
        origin = Point3d::From(x0, y0, z0);
        vectorU = Vector3d::From(ux, uy, uz);
        vectorV = Vector3d::From(vx, vy, vz);
        return *this;
    }

    /// Ported from: Plane3dByOriginAndVectors.setOriginAndVectors (:138-141)
    Plane3dByOriginAndVectors& setOriginAndVectors(
        Point3d const& origin_, Vector3d const& vectorU_, Vector3d const& vectorV_) noexcept {
        return setOriginAndVectorsXYZ(
            origin_.x, origin_.y, origin_.z,
            vectorU_.x, vectorU_.y, vectorU_.z,
            vectorV_.x, vectorV_.y, vectorV_.z);
    }

    /// Return a ray with origin at the plane origin and direction the (unit) cross
    /// product of vectorU and vectorV. Returns nullopt if the vectors are (near)
    /// parallel so no unit normal exists.
    /// Ported from: Plane3dByOriginAndVectors.unitNormalRay (:306-313)
    std::optional<Ray3d> unitNormalRay() const noexcept {
        // vectorU.unitCrossProduct(vectorV) = crossProduct then normalize; normalize
        // fails (undefined) when the magnitude is <= Geometry.smallFraction (1e-10):
        // normalizeWithLength → correctSmallFraction snaps to 0 → safeDivideOrNull(0)
        // (Point3dVector3d.ts:937-965, Geometry.ts:327-332).
        Vector3d unitNormal = Vector3d::FromCrossProduct(vectorU, vectorV);
        double const mag = unitNormal.Normalize();
        if (mag <= kSmallFraction)
            return std::nullopt;
        return Ray3d::FromOriginAndDirection(origin, unitNormal);
    }
};

} // namespace dqGeom
