// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Ellipsoid / EllipsoidPatch
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Ellipsoid.ts
//
// A complete unit sphere mapped by an arbitrary Transform:
//   u = cos(theta)*cos(phi), v = sin(theta)*cos(phi), w = sin(phi)
// The sphere (u,v,w) multiplies the x,y,z columns of the Ellipsoid transform.
//
// Subset (ported for the BackgroundMapGeometry ellipsoid depth-fitting chain,
// BackgroundMapGeometry.ts:314-396):
//   create / createCenterMatrixRadii / transformRef / worldToLocal / localToWorld /
//   clone / isAlmostEqual / tryTransformInPlace / cloneTransformed /
//   projectPointToSurface / silhouetteArc / intersectRay / radiansToPoint /
//   radiansToPointAndDerivatives / radiansToPointAnd2Derivatives /
//   radiansToUnitNormalRay (instance + static) / surfaceNormalToAngles /
//   otherEllipsoidAnglesToThisEllipsoidAngles / isPointOnOrInside /
//   EllipsoidPatch (uvFractionToPoint / uvFractionToAngles / anglesToUnitNormalRay /
//   projectPointToSurface / containsAngles).
// TODO (not consumed by the depth chain): patchRangeStartEndRadians +
// EllipsoidComponentExtrema, radiansPairToGreatArc / anglePairToGreatArc /
// radiansPairToEquatorialEllipsoid, constant{Longitude,Latitude}Arc,
// createSectionArcPointPointVectorInPlane / sectionArcInPlaneOfInterpolatedNormal,
// radiansToFrenetFrame, Clipper interface (announceClippedSegmentIntervals /
// announceClippedArcIntervals), GeodesicPathPoint / GeodesicPathSolver,
// EllipsoidPatch.range / intersectRay / uvFractionToPointAndTangents.
#pragma once

#include "AngleSweep.h"
#include "Arc3d.h"
#include "Export.h"
#include "LongitudeLatitudeAltitude.h"
#include "Matrix3d.h"
#include "Plane3dByOriginAndVectors.h"
#include "Point3d.h"
#include "Point4d.h"
#include "Ray3d.h"
#include "Transform.h"
#include "Vector3d.h"

#include <optional>
#include <vector>

namespace dqGeom {

/// Ported from: itwinjs-core Ellipsoid (Ellipsoid.ts:150-782, subset — see header note)
class DQ_GEOM_EXPORT Ellipsoid {
public:
    /// Create a unit sphere (identity transform) or a mapped sphere from a
    /// Transform / Matrix3d (cloned, not captured).
    /// Ported from: Ellipsoid.create (Ellipsoid.ts:167-174)
    static Ellipsoid create();
    static Ellipsoid create(Transform const& transform);
    static Ellipsoid create(Matrix3d const& matrix);

    /// Create with given center and directions, applying the radii as multipliers
    /// for the respective columns of `axes` (nullptr axes → scale matrix).
    /// Ported from: Ellipsoid.createCenterMatrixRadii (Ellipsoid.ts:183-190)
    static Ellipsoid createCenterMatrixRadii(Point3d const& center, Matrix3d const* axes,
                                             double radiusX, double radiusY, double radiusZ) noexcept;

    /// Return a (REFERENCE TO) the transform from world space to the mapped sphere
    /// space (maps coordinates "relative to the sphere" to world; its inverse maps
    /// world into sphere space).
    /// Ported from: Ellipsoid.transformRef (Ellipsoid.ts:198-200)
    Transform const& transformRef() const noexcept { return m_transform; }

    /// Convert a world point to the underlying mapped sphere space (magnitude 1 =
    /// ON, < 1 = INSIDE, > 1 = OUTSIDE). Returns nullopt if the frame is singular.
    /// Ported from: Ellipsoid.worldToLocal (Ellipsoid.ts:208-210)
    std::optional<Point3d> worldToLocal(Point3d const& worldPoint) const noexcept;

    /// Convert a sphere-space point to world coordinates.
    /// Ported from: Ellipsoid.localToWorld (Ellipsoid.ts:217-219)
    Point3d localToWorld(Point3d const& localPoint) const noexcept;

    /// Return a clone with same coordinates (:222-223).
    Ellipsoid clone() const noexcept { return *this; }

    /// Test equality of the transforms (:226-228).
    bool isAlmostEqual(Ellipsoid const& other) const noexcept {
        return m_transform.IsAlmostEqual(other.m_transform);
    }

    /// Apply the transform: this.transform = transform * this.transform.
    /// Ported from: Ellipsoid.tryTransformInPlace (:230-233)
    bool tryTransformInPlace(Transform const& transform) noexcept {
        // transform.multiplyTransformTransform(this._transform, this._transform)
        m_transform = transform.MultiplyTransform(m_transform);
        return true;
    }

    /// Return a cloned and transformed ellipsoid (:238-242).
    Ellipsoid cloneTransformed(Transform const& transform) const noexcept {
        Ellipsoid result = clone();
        result.tryTransformInPlace(transform);
        return result;
    }

    /// Find the closest point on the ellipsoid (reliable for points close to the
    /// surface). Returns nullopt when the local point cannot be computed.
    /// Ported from: Ellipsoid.projectPointToSurface (:247-250)
    std::optional<LongitudeLatitudeNumber> projectPointToSurface(Point3d const& spacePoint) const;

    /// Find the silhouette of the ellipsoid as viewed from a homogeneous eyepoint.
    /// Returns nullptr if the eyepoint is inside the ellipsoid.
    /// Ported from: Ellipsoid.silhouetteArc (:255-273)
    // §3.4 type adaptation: Arc3d is a RefCounted GeometryQuery in DanQing, so the
    // reference's `Arc3d | undefined` maps to RefPtr<Arc3d> (nullptr = undefined).
    dqBase::RefPtr<Arc3d> silhouetteArc(Point4d const& eyePoint) const;

    /// Compute intersections with a ray. Returns the number of intersections and
    /// fills any combination of rayFractions / xyz / thetaPhiRadians (each optional,
    /// cleared on entry). Returns 0 if the ray length is too small.
    /// Ported from: Ellipsoid.intersectRay (:284-313)
    size_t intersectRay(Ray3d const& ray,
                        std::vector<double>* rayFractions,
                        std::vector<Point3d>* xyz,
                        std::vector<LongitudeLatitudeNumber>* thetaPhiRadians) const;

    /// Evaluate a point on the ellipsoid at angles given in radians.
    /// Ported from: Ellipsoid.radiansToPoint (:359-365)
    Point3d radiansToPoint(double thetaRadians, double phiRadians) const noexcept;

    /// Evaluate a point and derivatives with respect to angle (u direction =
    /// derivative wrt longitude, v direction = wrt latitude). `applyCosPhiFactor`
    /// selects the properly-scaled theta derivative (zero at the poles) vs the
    /// always-nonzero unscaled form (safe for cross products).
    /// Ported from: Ellipsoid.radiansToPointAndDerivatives (:569-586, non-`result` branch)
    Plane3dByOriginAndVectors radiansToPointAndDerivatives(
        double thetaRadians, double phiRadians, bool applyCosPhiFactor = true) const noexcept;

    /// Evaluate a point and derivatives wrt theta, phi, thetaTheta, phiPhi, thetaPhi.
    /// Ported from: Ellipsoid.radiansToPointAnd2Derivatives (:600-623)
    void radiansToPointAnd2Derivatives(double thetaRadians, double phiRadians,
                                       Point3d& point,
                                       Vector3d& d1Theta, Vector3d& d1Phi,
                                       Vector3d& d2ThetaTheta, Vector3d& d2PhiPhi,
                                       Vector3d& d2ThetaPhi) const noexcept;

    /// Evaluate a point and unit normal at given angles. Returns nullopt only when
    /// the placement transform is singular at the critical angles.
    /// Ported from: Ellipsoid.radiansToUnitNormalRay (:644-647)
    std::optional<Ray3d> radiansToUnitNormalRay(double thetaRadians, double phiRadians) const noexcept;

    /// Find the (unique) extreme point angles for a given true surface perpendicular
    /// vector (outward). Ported from: Ellipsoid.surfaceNormalToAngles (:652-661)
    LongitudeLatitudeNumber surfaceNormalToAngles(Vector3d const& normal) const noexcept;

    /// Evaluate the surface normal on `other` ellipsoid at given angles (nullptr
    /// `other` = unit sphere), then find the angles for the same normal on `this`.
    /// Ported from: Ellipsoid.otherEllipsoidAnglesToThisEllipsoidAngles (:668-673)
    std::optional<LongitudeLatitudeNumber> otherEllipsoidAnglesToThisEllipsoidAngles(
        Ellipsoid const* otherEllipsoid, LongitudeLatitudeNumber const& otherAngles) const noexcept;

    /// If ellipsoid is given (non-null), return its surface point and unit normal
    /// as a Ray3d; if null, the same for the unit sphere.
    /// Ported from: Ellipsoid.radiansToUnitNormalRay (static, :678-688)
    static std::optional<Ray3d> radiansToUnitNormalRay(
        Ellipsoid const* ellipsoid, double thetaRadians, double phiRadians) noexcept;

    /// Construct an arc for the section cut of a plane with the ellipsoid.
    /// Returns nullptr if the plane does not intersect the ellipsoid.
    /// Ported from: Ellipsoid.createPlaneSection (:405-426)
    dqBase::RefPtr<Arc3d> createPlaneSection(Plane3dByOriginAndUnitNormal const& plane) const;

    /// Implementation of Clipper.isPointOnOrInside.
    /// Ported from: Ellipsoid.isPointOnOrInside (:690-695)
    bool isPointOnOrInside(Point3d const& point) const noexcept;

private:
    explicit Ellipsoid(Transform const& transform) noexcept : m_transform(transform) {}

    Transform m_transform;
    // (reference _workUnitVectorA/B + _workPointA/B are mutation scratch for the
    // great-arc/clipper methods — not in the ported subset; add with them.)
};

/// An EllipsoidPatch is an underlying full Ellipsoid plus angular ranges
/// (longitude around the equator, latitude with 0 at equator).
/// Ported from: itwinjs-core EllipsoidPatch (Ellipsoid.ts:791-908, subset — see header note)
class DQ_GEOM_EXPORT EllipsoidPatch {
public:
    Ellipsoid ellipsoid;
    AngleSweep longitudeSweep;
    AngleSweep latitudeSweep;

    /// CAPTURE ellipsoid and sweeps as an EllipsoidPatch.
    /// Ported from: EllipsoidPatch.createCapture (:812-814). (DanQing Ellipsoid is a
    /// value type, so "capture" copies — matches the reference's sharing semantics
    /// for the ported read-only consumers.)
    static EllipsoidPatch createCapture(Ellipsoid const& ellipsoid,
                                        AngleSweep longitudeSweep, AngleSweep latitudeSweep) noexcept {
        return EllipsoidPatch(ellipsoid, longitudeSweep, latitudeSweep);
    }

    /// Return the point on the ellipsoid at fractional positions in the angular ranges.
    /// Ported from: EllipsoidPatch.uvFractionToPoint (:816-818)
    Point3d uvFractionToPoint(double longitudeFraction, double latitudeFraction) const noexcept {
        return ellipsoid.radiansToPoint(longitudeSweep.FractionToRadians(longitudeFraction),
                                        latitudeSweep.FractionToRadians(latitudeFraction));
    }

    /// Compute point (with altitude) at given angles: ray.origin = point at
    /// requested altitude, ray.direction = outward unit normal. Returns nullopt on
    /// singular normal. Ported from: EllipsoidPatch.anglesToUnitNormalRay (:887-893)
    std::optional<Ray3d> anglesToUnitNormalRay(LongitudeLatitudeNumber const& position) const noexcept {
        auto ray = ellipsoid.radiansToUnitNormalRay(position.longitudeRadians(), position.latitudeRadians());
        if (!ray.has_value())
            return std::nullopt;
        // ray.origin = ray.fractionToPoint(position.altitude)
        ray->origin = ray->FractionToPoint(position.altitude());
        return ray;
    }

    /// Return simple angles of a fractional position in the patch.
    /// Ported from: EllipsoidPatch.uvFractionToAngles (:901-903, non-`result` branch)
    LongitudeLatitudeNumber uvFractionToAngles(double longitudeFraction, double phiFraction, double h = 0.0) const noexcept {
        return LongitudeLatitudeNumber::createRadians(
            longitudeSweep.FractionToRadians(longitudeFraction),
            latitudeSweep.FractionToRadians(phiFraction), h);
    }

    /// Find the closest point of the (patch of the) ellipsoid.
    /// Ported from: EllipsoidPatch.projectPointToSurface (:905-907)
    std::optional<LongitudeLatitudeNumber> projectPointToSurface(Point3d const& spacePoint) const {
        return ellipsoid.projectPointToSurface(spacePoint);
    }

    /// Test if the angles are within the sweep ranges.
    /// Ported from: EllipsoidPatch.containsAngles (:873-876)
    bool containsAngles(LongitudeLatitudeNumber const& position, bool allowPeriodicLongitude = true) const noexcept {
        return latitudeSweep.isRadiansInSweep(position.latitudeRadians(), false)
            && longitudeSweep.isRadiansInSweep(position.longitudeRadians(), allowPeriodicLongitude);
    }

private:
    EllipsoidPatch(Ellipsoid const& ellipsoid_, AngleSweep longitudeSweep_, AngleSweep latitudeSweep_) noexcept
        : ellipsoid(ellipsoid_), longitudeSweep(longitudeSweep_), latitudeSweep(latitudeSweep_) {}
};

} // namespace dqGeom
