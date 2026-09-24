// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom - ClipPlane (single halfspace clip plane)
// Ported from: itwinjs-core core/geometry/src/clipping/ClipPlane.ts:63-655
//
// A ClipPlane is a single plane represented as an inward unit normal (u,v,w) and a
// signedDistance. Halfspace evaluation: altitude(p) = p·normal - distance; positive =
// inside, zero = on, negative = outside. A representative point on the plane is
// distance*normal; given a point on the plane + inward normal, distance = point·normal.
//
// Foundation for the clipping subsystem (ConvexClipPlaneSet / polygonClip) and the
// procedural PlanarGrid (Frustum.getIntersectionWithPlane -> ClipPlane.createPlane +
// intersectRange). The Point4d / Matrix4d transforms (getPlane4d / setPlane4d /
// multiplyPlaneByMatrix4d / weightedAltitude) are deferred - not needed for the grid
// path. intersectRange (plane∩AABB) is ported separately once its deps
// (createRigidHeadsUp / Transform inverse / GrowableXYZArray) land.
#pragma once

#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolygonOps.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>
#include <dqGeom/CurvePrimitive.h>  // AnnounceNumberNumberCurvePrimitive（+ Arc3d 前向声明）

#include <cmath>
#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs-core ClipPlane (ClipPlane.ts:63-655)
struct DQ_GEOM_EXPORT ClipPlane {
    Vector3d inwardNormal;        // unit length
    double distanceFromOrigin = 0.0;
    bool invisible = false;
    bool interior = false;

    ClipPlane() = default;
    ClipPlane(Vector3d const& normal, double distance, bool invis, bool intr) noexcept
        : inwardNormal(normal), distanceFromOrigin(distance), invisible(invis), interior(intr) {}

    /// Create a ClipPlane from a Plane3dByOriginAndUnitNormal.
    /// distance = normal · origin.
    /// Ported from: ClipPlane.createPlane (ClipPlane.ts:109-121)
    static ClipPlane createPlane(Plane3dByOriginAndUnitNormal const& plane,
                                 bool invis = false, bool intr = false) noexcept {
        double const distance = plane.getNormalRef().DotProduct(plane.getOriginRef());
        return ClipPlane(plane.getNormalRef(), distance, invis, intr);
    }

    /// Create a ClipPlane with direct normal + signedDistance; the normal is normalized.
    /// Returns nullopt if the normal is (near) zero.
    /// Ported from: ClipPlane.createNormalAndDistance (ClipPlane.ts:126-140)
    static std::optional<ClipPlane> createNormalAndDistance(
        Vector3d normal, double distance, bool invis = false, bool intr = false) noexcept {
        double const mag = normal.Normalize();  // by-value: normalizes the local copy
        if (mag < 1.0e-15)
            return std::nullopt;
        return ClipPlane(normal, distance, invis, intr);
    }

    /// Create a ClipPlane from a (not necessarily unit) normal and a point on the
    /// plane; distance = normalizedNormal · point. Returns nullopt if the normal
    /// fails to normalize (magnitude <= Geometry.smallFraction).
    /// Ported from: ClipPlane.createNormalAndPoint (ClipPlane.ts:149-168)
    static std::optional<ClipPlane> createNormalAndPoint(
        Vector3d normal, Point3d const& point, bool invis = false, bool intr = false) noexcept {
        double const mag = normal.Normalize();  // by-value copy
        if (mag <= 1.0e-10)   // Geometry.smallFraction — normalize failure cutoff
            return std::nullopt;
        double const distance = normal.DotProduct(point);
        return ClipPlane(normal, distance, invis, intr);
    }

    /// Create a ClipPlane from the xy edge between two points; the plane normal is
    /// perpendicular to the edge in the xy plane ("inside" to the left of the
    /// directed edge). Returns nullopt if the edge is degenerate.
    /// Ported from: ClipPlane.createEdgeXY (ClipPlane.ts:290-295)
    static std::optional<ClipPlane> createEdgeXY(Point3d const& point0, Point3d const& point1) noexcept {
        Vector3d normal = Vector3d::From(point0.y - point1.y, point1.x - point0.x, 0.0);
        double const mag = normal.Normalize();
        if (mag <= 1.0e-10)   // normalizeInPlace failure cutoff (createNormalized)
            return std::nullopt;
        return createNormalAndPoint(normal, point0, false, false);
    }

    /// Return the Plane3d form of the plane; the plane origin is the point
    /// distance * inwardNormal. (inwardNormal is unit by construction.)
    /// Ported from: ClipPlane.getPlane3d (ClipPlane.ts:313-320)
    Plane3dByOriginAndUnitNormal getPlane3d() const noexcept {
        double const d = distanceFromOrigin;
        Point3d const origin = Point3d::From(inwardNormal.x * d, inwardNormal.y * d, inwardNormal.z * d);
        return Plane3dByOriginAndUnitNormal{origin, inwardNormal};
    }

    /// Transform the plane in place (origin as a point, normal by inverse-transpose
    /// then renormalized; distance recomputed as normal · newOrigin).
    /// Returns false on singular transform or degenerate normal.
    /// Ported from: ClipPlane.transformInPlace (ClipPlane.ts:486-503)
    bool transformInPlace(Transform const& transform) noexcept {
        Plane3dByOriginAndUnitNormal const plane = getPlane3d();
        Matrix3d const& matrix = transform.matrix;
        Point3d const newPoint = transform.MultiplyPoint3d(plane.getOriginRef());
        // Normal transforms as the inverse transpose of the matrix part
        auto newNormal = matrix.multiplyInverseTranspose(plane.getNormalRef());
        if (!newNormal.has_value())
            return false;
        double const mag = newNormal->Normalize();
        if (mag <= 1.0e-10)   // normalize failure cutoff
            return false;
        inwardNormal = *newNormal;
        distanceFromOrigin = inwardNormal.DotProduct(newPoint);
        return true;
    }

    /// Compute intersections of an (UNBOUNDED) arc with this plane and append them
    /// (as radians) to `intersectionRadians`. The arc's angle limits are NOT
    /// considered. Ported from: ClipPlane.appendIntersectionRadians (ClipPlane.ts:441-449)
    void appendIntersectionRadians(Arc3d const& arc, std::vector<double>& intersectionRadians) const;

    /// Announce the fractional intervals of `arc` that are inside this plane's
    /// halfspace. Ported from: ClipPlane.announceClippedArcIntervals (ClipPlane.ts:452-458)
    bool announceClippedArcIntervals(Arc3d const& arc,
                                     AnnounceNumberNumberCurvePrimitive const& announce) const;

    /// Halfspace evaluation: p·normal - distance (positive = inside).
    /// Ported from: ClipPlane.altitude (ClipPlane.ts:354-358)
    double altitude(Point3d const& p) const noexcept {
        return inwardNormal.DotProduct(p) - distanceFromOrigin;
    }
    /// Ported from: ClipPlane.altitudeXYZ (ClipPlane.ts:365-370)
    double altitudeXYZ(double x, double y, double z) const noexcept {
        return inwardNormal.x * x + inwardNormal.y * y + inwardNormal.z * z - distanceFromOrigin;
    }

    /// Dot product of the plane normal with a vector (NOT using distanceFromOrigin).
    /// Ported from: ClipPlane.velocity (ClipPlane.ts:391-393)
    double velocity(Vector3d const& vector) const noexcept {
        return vector.x * inwardNormal.x + vector.y * inwardNormal.y + vector.z * inwardNormal.z;
    }

    /// True if spacePoint is inside or on the plane (tolerance applied to "on").
    /// Ported from: ClipPlane.isPointOnOrInside (ClipPlane.ts:406-410)
    bool isPointOnOrInside(Point3d const& p, double tolerance = 1.0e-6) const noexcept {
        return altitude(p) + tolerance >= 0.0;
    }
    /// True if strictly inside (tolerance applied to "on").
    /// Ported from: ClipPlane.isPointInside (ClipPlane.ts:416-420)
    bool isPointInside(Point3d const& p, double tolerance = 1.0e-6) const noexcept {
        return altitude(p) - tolerance > 0.0;
    }
    /// True if strictly on the plane, within tolerance.
    /// Ported from: ClipPlane.isPointOn (ClipPlane.ts:426-428)
    bool isPointOn(Point3d const& p, double tolerance = 1.0e-6) const noexcept {
        return std::abs(altitude(p)) <= tolerance;
    }

    Vector3d const& getNormalRef() const noexcept { return inwardNormal; }
    double getDistanceFromOrigin() const noexcept { return distanceFromOrigin; }

    /// Return the intersection of the plane with a range cube (a convex polygon), or
    /// nullopt if the plane does not cut the range.
    /// Ported from: ClipPlane.intersectRange (ClipPlane.ts:598-619). Algorithm: transform
    /// the 8 range corners into the plane's local frame (z = inward normal), bail if all
    /// are on the same side, build an oversized polygon on local z=0, transform back to
    /// world, then clip it to the range (intersectRangeConvexPolygonInPlace).
    std::optional<std::vector<Point3d>> intersectRange(Range3d const& range,
                                                       bool addClosurePoint = false) const noexcept
    {
        if (range.isNull())
            return std::nullopt;

        // frame: origin = distance * inwardNormal (closest plane point to world origin),
        // z-axis = inwardNormal (CreateRigidHeadsUp, ZXY).
        Point3d const frameOrigin = Point3d::From(
            inwardNormal.x * distanceFromOrigin,
            inwardNormal.y * distanceFromOrigin,
            inwardNormal.z * distanceFromOrigin);
        Transform const frame(frameOrigin, Matrix3d::CreateRigidHeadsUp(inwardNormal, AxisOrder::ZXY));

        // Transform the 8 range corners into the local frame (inverse) and bound them.
        auto const corners = range.Corners();
        Range3d localRange = Range3d::CreateNull();
        for (auto const& c : corners) {
            Point3d lc;
            frame.MultiplyInversePoint3d(c, lc);
            localRange.ExtendPoint(lc);
        }
        // All corners on the same side of the plane -> no intersection.
        if (localRange.low.z * localRange.high.z > 0.0)
            return std::nullopt;

        // Oversized polygon on local z=0, then transform back to world.
        std::vector<Point3d> polygon = {
            Point3d::From(localRange.low.x, localRange.low.y, 0.0),
            Point3d::From(localRange.high.x, localRange.low.y, 0.0),
            Point3d::From(localRange.high.x, localRange.high.y, 0.0),
            Point3d::From(localRange.low.x, localRange.high.y, 0.0),
        };
        frame.MultiplyPoint3dArrayInPlace(polygon);

        // Clip the polygon to the range (keeps only the in-box portion).
        if (!PolygonOps::intersectRangeConvexPolygonInPlace(range, polygon))
            return std::nullopt;
        if (addClosurePoint && !polygon.empty())
            polygon.push_back(polygon.front());
        return polygon;
    }

    /// Project (x,y,z) onto the plane.
    /// Ported from: ClipPlane.projectXYZToPlane (ClipPlane.ts:651-654)
    Point3d projectXYZToPlane(double x, double y, double z) const noexcept {
        double const scale = -altitudeXYZ(x, y, z);
        return Point3d::From(x + scale * inwardNormal.x,
                             y + scale * inwardNormal.y,
                             z + scale * inwardNormal.z);
    }
    Point3d projectPointToPlane(Point3d const& p) const noexcept {
        return projectXYZToPlane(p.x, p.y, p.z);
    }

    /// Negate the inward normal and distance in place.
    /// Ported from: ClipPlane.negateInPlace
    void negateInPlace() noexcept {
        inwardNormal.x = -inwardNormal.x;
        inwardNormal.y = -inwardNormal.y;
        inwardNormal.z = -inwardNormal.z;
        distanceFromOrigin = -distanceFromOrigin;
    }
    /// Ported from: ClipPlane.cloneNegated (ClipPlane.ts:103-107)
    ClipPlane cloneNegated() const noexcept {
        ClipPlane c = *this;
        c.negateInPlace();
        return c;
    }
    /// Ported from: ClipPlane.clone (ClipPlane.ts:98-101)
    ClipPlane clone() const noexcept { return *this; }

    /// Ported from: ClipPlane.isAlmostEqual (ClipPlane.ts:91-96)
    bool isAlmostEqual(ClipPlane const& other) const noexcept {
        return std::abs(distanceFromOrigin - other.distanceFromOrigin) <= 1.0e-6
            && inwardNormal.AlmostEqual(other.inwardNormal)
            && invisible == other.invisible
            && interior == other.interior;
    }
};

END_DQ_GEOM_NAMESPACE
