// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom - ConvexClipPlaneSet (intersection of halfspaces)
// Ported from: itwinjs-core core/geometry/src/clipping/ConvexClipPlaneSet.ts
//
// A convex set of ClipPlanes; the inside region is the intersection of the halfspaces.
// polygonClip clips a convex polygon to the inside region (Sutherland-Hodgman against
// each plane in turn). Consumed by Frustum.getIntersectionWithPlane (the frustum's 6
// face planes bound the plane∩AABB polygon to the actual frustum) on the path to the
// procedural PlanarGrid.
#pragma once

#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipUtils.h>
#include <dqGeom/Geometry.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolygonOps.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

/// Wire format describing a ConvexClipPlaneSet: array of clip plane props.
/// Ported from: itwinjs-core ConvexClipPlaneSetProps (ConvexClipPlaneSet.ts:32-35)
using ConvexClipPlaneSetProps = std::vector<ClipPlaneProps>;

// Ported from: itwinjs-core ConvexClipPlaneSet (ConvexClipPlaneSet.ts)
class DQ_GEOM_EXPORT ConvexClipPlaneSet {
public:
    std::vector<ClipPlane> planes;

    ConvexClipPlaneSet() = default;

    /// Ported from: ConvexClipPlaneSet.createEmpty
    static ConvexClipPlaneSet createEmpty() { return ConvexClipPlaneSet{}; }

    /// Create a convex set for a xy-aligned box (4 planes, interior=true).
    /// Ported from: ConvexClipPlaneSet.createXYBox (ConvexClipPlaneSet.ts:172-185)
    static ConvexClipPlaneSet createXYBox(double x0, double y0, double x1, double y1) noexcept {
        ConvexClipPlaneSet result;
        auto const clip0 = ClipPlane::createNormalAndDistance(Vector3d::From(-1, 0, 0), -x1, false, true);
        auto const clip1 = ClipPlane::createNormalAndDistance(Vector3d::From(1, 0, 0), x0, false, true);
        auto const clip2 = ClipPlane::createNormalAndDistance(Vector3d::From(0, -1, 0), -y1, false, true);
        auto const clip3 = ClipPlane::createNormalAndDistance(Vector3d::From(0, 1, 0), y0, false, true);
        if (clip0.has_value() && clip1.has_value() && clip2.has_value() && clip3.has_value()) {
            result.planes.push_back(*clip0);
            result.planes.push_back(*clip1);
            result.planes.push_back(*clip2);
            result.planes.push_back(*clip3);
        }
        return result;
    }

    /// Deep clone.
    /// Ported from: ConvexClipPlaneSet.clone
    ConvexClipPlaneSet clone() const { return *this; }

    /// Add a plane to the set.
    /// Ported from: ConvexClipPlaneSet.addPlaneToConvexSet
    void addPlaneToConvexSet(ClipPlane const& plane) { planes.push_back(plane); }

    /// Transform each plane in place.
    /// Ported from: ConvexClipPlaneSet.transformInPlace (ConvexClipPlaneSet.ts:443-447)
    void transformInPlace(Transform const& transform) {
        for (auto& plane : planes)
            plane.transformInPlace(transform);
    }

    /// Return true if `point` satisfies isPointOnOrInside for all planes.
    /// Ported from: ConvexClipPlaneSet.isPointOnOrInside (ConvexClipPlaneSet.ts:350-357)
    bool isPointOnOrInside(Point3d const& point, double tolerance = 1.0e-6) const noexcept {
        double const interiorTolerance = std::abs(tolerance);  // always positive (TFS# 246598)
        for (ClipPlane const& plane : planes) {
            if (!plane.isPointOnOrInside(point, (plane.interior ? interiorTolerance : tolerance)))
                return false;
        }
        return true;
    }

    /// Returns StronglyInside / Ambiguous / StronglyOutside for the point array
    /// (fast pre-filter: only detects the single-plane-all-outside case).
    /// Ported from: ConvexClipPlaneSet.classifyPointContainment (ConvexClipPlaneSet.ts:525-542)
    ClipPlaneContainment classifyPointContainment(std::vector<Point3d> const& points, bool onIsOutside) const noexcept {
        bool allInside = true;
        double const onTolerance = onIsOutside ? 1.0e-8 : -1.0e-8;
        double const interiorTolerance = 1.0e-8;   // Interior tolerance should always be positive

        for (ClipPlane const& plane : planes) {
            size_t nOutside = 0;
            for (Point3d const& point : points) {
                if (plane.altitude(point) < (plane.interior ? interiorTolerance : onTolerance)) {
                    nOutside++;
                    allInside = false;
                }
            }
            if (nOutside == points.size())
                return ClipPlaneContainment::StronglyOutside;
        }
        return allInside ? ClipPlaneContainment::StronglyInside : ClipPlaneContainment::Ambiguous;
    }

    /// Clip a convex polygon `xyz` in place to the intersection of all halfspaces.
    /// Returns false if the polygon becomes empty. (itwinjs's polygonClip writes to a
    /// separate out array; DanQing clips in place - the algorithm is 1:1.)
    /// Ported from: ConvexClipPlaneSet.polygonClip (ConvexClipPlaneSet.ts:626)
    bool polygonClip(std::vector<Point3d>& xyz) const noexcept
    {
        for (ClipPlane const& plane : planes) {
            // ClipPlane altitude = inwardNormal·p - distance (>= 0 inside).
            // clipConvexPolygonInPlace uses planeAbc·p + planeD >= 0 with keepPositive;
            // map planeAbc = inwardNormal, planeD = -distance.
            PolygonOps::clipConvexPolygonInPlace(plane.getNormalRef(),
                                                 -plane.getDistanceFromOrigin(), xyz, true);
            if (xyz.empty())
                return false;
        }
        return !xyz.empty();
    }

    /// Clip a polygon to the inside of the convex set, in place; clipping stops
    /// early when fewer than 3 points remain (the sliver is left as-is).
    /// Ported from: ConvexClipPlaneSet.clipConvexPolygonInPlace (ConvexClipPlaneSet.ts:456-464)
    // §3.4 适配：参考逐面调用 ClipPlane.clipConvexPolygonInPlace(xyz, work, true,
    // tolerance)（→ IndexedXYZCollectionPolygonOps.clipConvexPolygonInPlace），以
    // work 缓冲避免分配；DanQing 的 PolygonOps::clipConvexPolygonInPlace 是同一参考
    // 算法的原地移植（内部分配 survivors），work 形参仅保留签名对齐。
    void clipConvexPolygonInPlace(std::vector<Point3d>& xyz, std::vector<Point3d>& /*work*/,
                                  double tolerance = 1.0e-6) const noexcept {
        for (ClipPlane const& plane : planes) {
            PolygonOps::clipConvexPolygonInPlace(plane.getNormalRef(),
                                                 -plane.getDistanceFromOrigin(), xyz, true, tolerance);
            if (xyz.size() < 3)
                return;
        }
    }

    /// Announce the fractional intervals of `arc` that are inside this convex set.
    /// Ported from: ConvexClipPlaneSet.announceClippedArcIntervals
    /// (ConvexClipPlaneSet.ts:412-420)
    bool announceClippedArcIntervals(Arc3d const& arc,
                                     AnnounceNumberNumberCurvePrimitive const& announce) const;

    // ------------------------------------------------------------------
    // M-P P-A additions (sectioning chain). All Ported from
    // ConvexClipPlaneSet.ts unless noted; §3.4 adaptations as annotated.

    /// Create a set from an array of planes (references taken into the result).
    /// Ported from: ConvexClipPlaneSet.createPlanes (ConvexClipPlaneSet.ts:101-114)
    // §3.4 适配：Plane3dByOriginAndUnitNormal 形态缺（DanQing 调用方均传 ClipPlane）。
    static ConvexClipPlaneSet createPlanes(std::vector<ClipPlane> const& planes) {
        ConvexClipPlaneSet result;
        for (ClipPlane const& plane : planes)
            result.planes.push_back(plane);
        return result;
    }

    /// Create a convex set using selected planes of a Range3d.
    /// Ported from: ConvexClipPlaneSet.createRange3dPlanes (ConvexClipPlaneSet.ts:125-152)
    // §3.4 适配：createNormalAndPointXYZXYZ → createNormalAndPoint（同一数学：
    /// 过点、给法向的单位平面）。
    static ConvexClipPlaneSet createRange3dPlanes(
        Range3d const& range, bool lowX = true, bool highX = true,
        bool lowY = true, bool highY = true, bool lowZ = true, bool highZ = true) noexcept {
        ConvexClipPlaneSet result = createEmpty();
        // all normals are nonzero, so ClipPlane creation can only fail on out-of-memory
        if (lowX)
            if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(1, 0, 0), Point3d::From(range.low.x, 0, 0))) result.planes.push_back(*p);
        if (highX)
            if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(-1, 0, 0), Point3d::From(range.high.x, 0, 0))) result.planes.push_back(*p);
        if (lowY)
            if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(0, 1, 0), Point3d::From(0, range.low.y, 0))) result.planes.push_back(*p);
        if (highY)
            if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(0, -1, 0), Point3d::From(0, range.high.y, 0))) result.planes.push_back(*p);
        if (lowZ)
            if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(0, 0, 1), Point3d::From(0, 0, range.low.z))) result.planes.push_back(*p);
        if (highZ)
            if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(0, 0, -1), Point3d::From(0, 0, range.high.z))) result.planes.push_back(*p);
        return result;
    }

    /// Negate all planes of the set.
    /// Ported from: ConvexClipPlaneSet.negateAllPlanes (ConvexClipPlaneSet.ts:161-165)
    void negateAllPlanes() noexcept {
        for (ClipPlane& plane : planes)
            plane.negateInPlace();
    }

    /// Return true if all members are almostEqual to corresponding members of
    /// other (same order).
    /// Ported from: ConvexClipPlaneSet.isAlmostEqual (ConvexClipPlaneSet.ts:88-95)
    bool isAlmostEqual(ConvexClipPlaneSet const& other) const noexcept {
        if (planes.size() != other.planes.size())
            return false;
        for (size_t i = 0; i < planes.size(); ++i)
            if (!planes[i].isAlmostEqual(other.planes[i]))
                return false;
        return true;
    }

    /// Return true if `point` satisfies isPointInside for all planes.
    /// Ported from: ConvexClipPlaneSet.isPointInside (ConvexClipPlaneSet.ts:339-347)
    bool isPointInside(Point3d const& point) const noexcept {
        for (ClipPlane const& plane : planes) {
            if (!plane.isPointInside(point))
                return false;
        }
        return true;
    }

    /// Test if a sphere is completely inside the convex set.
    /// Ported from: ConvexClipPlaneSet.isSphereInside (ConvexClipPlaneSet.ts:363-371)
    bool isSphereInside(Point3d const& centerPoint, double radius) const noexcept {
        double const r1 = std::abs(radius) + 1.0e-6;  // + Geometry.smallMetricDistance
        for (ClipPlane const& plane : planes) {
            if (!plane.isPointOnOrInside(centerPoint, r1))
                return false;
        }
        return true;
    }

    /// Emit json form.
    /// Ported from: ConvexClipPlaneSet.toJSON (ConvexClipPlaneSet.ts:62-67)
    ConvexClipPlaneSetProps toJSON() const noexcept {
        ConvexClipPlaneSetProps val;
        for (ClipPlane const& plane : planes)
            val.push_back(plane.toJSON());
        return val;
    }
    /// Extract clip planes from a props array; non-plane members are ignored
    /// (null plane entries skipped — 1:1 reference).
    /// Ported from: ConvexClipPlaneSet.fromJSON (ConvexClipPlaneSet.ts:72-83)
    static ConvexClipPlaneSet fromJSON(ConvexClipPlaneSetProps const* json) noexcept {
        ConvexClipPlaneSet result;
        if (json == nullptr)
            return result;
        for (ClipPlaneProps const& jsonPlane : *json) {
            std::optional<ClipPlane> plane = ClipPlane::fromJSON(&jsonPlane);
            if (plane.has_value())
                result.planes.push_back(*plane);
        }
        return result;
    }

    /// Announce the fractional interval [f0,f1] of segment pointA..pointB that
    /// is inside all planes. Returns true if an interval survived.
    /// Ported from: ConvexClipPlaneSet.announceClippedSegmentIntervals
    /// (ConvexClipPlaneSet.ts:372-409)
    bool announceClippedSegmentIntervals(
        double f0, double f1, Point3d const& pointA, Point3d const& pointB,
        AnnounceNumberNumber const& announce = nullptr) const noexcept {
        if (f1 < f0)
            return false;
        for (ClipPlane const& plane : planes) {
            double const hA = -plane.altitude(pointA);
            double const hB = -plane.altitude(pointB);
            std::optional<double> fraction = conditionalDivideFraction(-hA, (hB - hA));
            if (!fraction.has_value()) {
                // Line parallel to the plane. If positive, it is all OUT
                if (hA > 0.0)
                    return false;
            } else if (hB > hA) {  // STRICTLY moving outward
                if (*fraction < f0)
                    return false;
                if (*fraction < f1)
                    f1 = *fraction;
            } else if (hA > hB) {  // STRICTLY moving inward
                if (*fraction > f1)
                    return false;
                if (*fraction > f0)
                    f0 = *fraction;
            } else {
                // Strictly equal evaluations
                if (hA > 0.0)
                    return false;
            }
        }
        if (f1 >= f0) {
            if (announce)
                announce(f0, f1);
            return true;
        }
        return false;
    }

    /// Clip a polygon to the inside of the convex set (output-array form with
    /// optional plane to skip).
    /// Ported from: ConvexClipPlaneSet.polygonClip (ConvexClipPlaneSet.ts:623-643)
    // §3.4 适配：GrowableXYZArray → std::vector<Point3d>（clip 族先例）；
    /// planeToSkip 的对象同一性 → ClipPlane const*（指向集合内成员的指针）。
    void polygonClip(std::vector<Point3d> const& input, std::vector<Point3d>& output,
                     std::vector<Point3d>& work, ClipPlane const* planeToSkip = nullptr,
                     double tolerance = 1.0e-6) const noexcept {
        output = input;
        for (ClipPlane const& plane : planes) {
            if (planeToSkip == &plane)
                continue;
            if (output.empty())
                break;
            PolygonOps::clipConvexPolygonInPlace(plane.getNormalRef(),
                                                 -plane.getDistanceFromOrigin(), output, true, tolerance);
        }
        (void)work;
    }

    /// Set the invisible property on each plane.
    /// Ported from: ConvexClipPlaneSet.setInvisible (ConvexClipPlaneSet.ts:748-752)
    void setInvisible(bool invisible) noexcept {
        for (ClipPlane& plane : planes)
            plane.setInvisible(invisible);
    }

    /// Add planes for z-direction clip between low and high z levels.
    /// Ported from: ConvexClipPlaneSet.addZClipPlanes (ConvexClipPlaneSet.ts:759-764)
    void addZClipPlanes(bool invisible, std::optional<double> zLow = std::nullopt,
                        std::optional<double> zHigh = std::nullopt) noexcept {
        if (zLow.has_value())
            if (auto p = ClipPlane::createNormalAndDistance(Vector3d::From(0, 0, 1), *zLow, invisible))
                planes.push_back(*p);
        if (zHigh.has_value())
            if (auto p = ClipPlane::createNormalAndDistance(Vector3d::From(0, 0, -1), -*zHigh, invisible))
                planes.push_back(*p);
    }

    /// Compute intersections among all combinations of 3 planes in the convex
    /// set; optionally collect points / extend a range; testContainment drops
    /// points outside the set. Returns number of accepted points.
    /// Ported from: ConvexClipPlaneSet.computePlanePlanePlaneIntersections
    /// (ConvexClipPlaneSet.ts:709-743)
    size_t computePlanePlanePlaneIntersections(
        std::vector<Point3d>* points, Range3d* rangeToExtend,
        Transform const* transform = nullptr, bool testContainment = true) const noexcept {
        size_t numPoints = 0;
        size_t const n = planes.size();
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j)
                for (size_t k = j + 1; k < n; ++k) {
                    Matrix3d const normalRows = Matrix3d::CreateRowValues(
                        planes[i].inwardNormal.x, planes[i].inwardNormal.y, planes[i].inwardNormal.z,
                        planes[j].inwardNormal.x, planes[j].inwardNormal.y, planes[j].inwardNormal.z,
                        planes[k].inwardNormal.x, planes[k].inwardNormal.y, planes[k].inwardNormal.z);
                    Matrix3d inverse;
                    if (normalRows.Inverse(inverse)) {
                        // §3.4 适配：computeCachedInverse + multiplyInverseXYZAsPoint3d
                        // → Inverse + MultiplyVector（同一数学）。
                        Point3d xyz = Point3d::From(0, 0, 0);
                        Vector3d const sol = inverse.MultiplyVector(Vector3d::From(
                            planes[i].getDistanceFromOrigin(),
                            planes[j].getDistanceFromOrigin(),
                            planes[k].getDistanceFromOrigin()));
                        xyz = Point3d::From(sol.x, sol.y, sol.z);
                        if (!testContainment || isPointOnOrInside(xyz, 1.0e-6)) {
                            numPoints++;
                            if (transform)
                                xyz = transform->MultiplyPoint3d(xyz);
                            if (points)
                                points->push_back(xyz);
                            if (rangeToExtend)
                                rangeToExtend->ExtendPoint(xyz);
                        }
                    }
                }
        }
        return numPoints;
    }
};

END_DQ_GEOM_NAMESPACE
