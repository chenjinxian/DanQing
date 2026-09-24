// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Plane3dByOriginAndUnitNormal (plane by origin + unit normal)
// Ported from: itwinjs-core core/geometry/src/geometry3d/Plane3dByOriginAndUnitNormal.ts
//
// A plane defined by any point on the plane (origin) and a unit normal. Foundation
// type for the clipping subsystem (ClipPlane / ConvexClipPlaneSet / polygonClip) and
// the procedural PlanarGrid (PlanarGrid.ts:53 — plane through grid.origin with normal
// rMatrix.rowZ()). The abstract Plane3d base of the reference is deferred; this is the
// concrete plane representation the grid/clip path needs.
#pragma once

#include <dqGeom/Point3d.h>
#include <dqGeom/Point4d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core Plane3dByOriginAndUnitNormal
//              (Plane3dByOriginAndUnitNormal.ts:24-359)
struct DQ_GEOM_EXPORT Plane3dByOriginAndUnitNormal {
    Point3d origin;
    Vector3d normal;  // unit length

    Plane3dByOriginAndUnitNormal() = default;
    Plane3dByOriginAndUnitNormal(Point3d const& o, Vector3d const& n) noexcept
        : origin(o), normal(n) {}

    /// Create a plane from an origin and a (not necessarily unit) normal.
    /// Returns nullopt if the normal is (near) zero. Inputs are NOT captured.
    /// Ported from: Plane3dByOriginAndUnitNormal.create (L69-82)
    static std::optional<Plane3dByOriginAndUnitNormal> create(
        Point3d const& origin, Vector3d normal) noexcept {
        double const mag = normal.Normalize();  // by-value: normalizes the local copy
        if (mag < 1.0e-15)                       // Geometry.smallFraction
            return std::nullopt;
        return Plane3dByOriginAndUnitNormal{origin, normal};
    }

    /// Create a plane from direct origin + normal coordinates. Returns nullopt if the
    /// normal is (near) zero.
    /// Ported from: Plane3dByOriginAndUnitNormal.createXYZUVW (L110-122)
    static std::optional<Plane3dByOriginAndUnitNormal> createXYZUVW(
        double ax, double ay, double az,
        double ux, double uy, double uz) noexcept {
        return create(Point3d::From(ax, ay, az), Vector3d::From(ux, uy, uz));
    }

    /// Create a plane parallel to the XY plane (normal +Z) through `origin`.
    /// Ported from: Plane3dByOriginAndUnitNormal.createXYPlane (L41-45)
    static Plane3dByOriginAndUnitNormal createXYPlane(
        Point3d const& origin = Point3d::FromZero()) noexcept {
        return Plane3dByOriginAndUnitNormal{origin, Vector3d::UnitZ()};
    }

    Point3d const& getOriginRef() const noexcept { return origin; }
    Vector3d const& getNormalRef() const noexcept { return normal; }
    double normalX() const noexcept { return normal.x; }
    double normalY() const noexcept { return normal.y; }
    double normalZ() const noexcept { return normal.z; }

    /// Signed altitude of spacePoint above the plane (below is negative).
    /// Ported from: Plane3dByOriginAndUnitNormal.altitude (L284-286)
    double altitude(Point3d const& spacePoint) const noexcept {
        return normal.x * (spacePoint.x - origin.x)
             + normal.y * (spacePoint.y - origin.y)
             + normal.z * (spacePoint.z - origin.z);
    }

    /// Signed altitude of (x,y,z) above the plane.
    /// Ported from: Plane3dByOriginAndUnitNormal.altitudeXYZ (L335-337)
    double altitudeXYZ(double x, double y, double z) const noexcept {
        return normal.x * (x - origin.x)
             + normal.y * (y - origin.y)
             + normal.z * (z - origin.z);
    }

    /// Project spacePoint onto the plane.
    /// Ported from: Plane3dByOriginAndUnitNormal.projectPointToPlane (L343-349)
    Point3d projectPointToPlane(Point3d const& spacePoint) const noexcept {
        double const scale = -altitude(spacePoint);
        return Point3d::From(spacePoint.x + scale * normal.x,
                             spacePoint.y + scale * normal.y,
                             spacePoint.z + scale * normal.z);
    }

    /// (Toleranced) equality.
    /// Ported from: Plane3dByOriginAndUnitNormal.isAlmostEqual (L196-198)
    bool isAlmostEqual(Plane3dByOriginAndUnitNormal const& other) const noexcept {
        return origin.AlmostEqual(other.origin) && normal.AlmostEqual(other.normal);
    }

    /// Deep clone.
    /// Ported from: Plane3dByOriginAndUnitNormal.clone (L256-262)
    Plane3dByOriginAndUnitNormal clone() const noexcept { return *this; }

    /// Return a cloned plane transformed by `transform` (or by its inverse when
    /// `inverse` is true — the normal is then multiplied by the transpose and the
    /// origin by the full inverse). Returns nullopt on singular transform or
    /// degenerate normal.
    /// Ported from: Plane3dByOriginAndUnitNormal.cloneTransformed (L264-278)
    std::optional<Plane3dByOriginAndUnitNormal> cloneTransformed(
        Transform const& transform, bool inverse = false) const noexcept {
        Plane3dByOriginAndUnitNormal result = clone();
        if (inverse) {
            Point3d newOrigin;
            if (!transform.MultiplyInversePoint3d(result.origin, newOrigin))
                return std::nullopt;
            result.origin = newOrigin;
            Vector3d newNormal = transform.matrix.MultiplyTransposeVector(result.normal);
            double const mag = newNormal.Normalize();
            if (mag <= 0.0)
                return std::nullopt;
            result.normal = newNormal;
            return result;
        }
        result.origin = transform.MultiplyPoint3d(result.origin);
        auto newNormal = transform.matrix.multiplyInverseTranspose(result.normal);
        if (!newNormal.has_value())
            return std::nullopt;
        double const mag = newNormal->Normalize();
        if (mag <= 0.0)
            return std::nullopt;
        result.normal = *newNormal;
        return result;
    }
};

// ---------------------------------------------------------------------------
// Point4d 跨类型成员定义（声明见 Point4d.h；单向 include 模式）
// ---------------------------------------------------------------------------

/// Ported from: Point4d.toPlane3dByOriginAndUnitNormal (Point4d.ts:560-562) =
/// Plane3dByOriginAndUnitNormal.createFrom(this) — unit normal from the xyz part
/// (Plane3d base getUnitNormal: Vector3d.createNormalized, cutoff smallFraction);
/// origin = world-origin projected to the plane (Plane3d.ts default
/// getAnyPointOnPlane: projectPointToPlane((0,0,0))).
inline std::optional<Plane3dByOriginAndUnitNormal> Point4d::toPlane3dByOriginAndUnitNormal() const noexcept
{
    Vector3d normal = Vector3d::From(x, y, z);
    double const mag = normal.Normalize();
    if (mag <= 1.0e-10)   // Geometry.smallFraction — normalize failure cutoff
        return std::nullopt;
    Point3d const origin = projectPointToPlane(Point3d::FromZero());
    return Plane3dByOriginAndUnitNormal{origin, normal};
}

END_DQ_GEOM_NAMESPACE
