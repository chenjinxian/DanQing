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
#include <dqGeom/Point3d.h>
#include <dqGeom/PolygonOps.h>
#include <dqGeom/Vector3d.h>

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

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
};

END_DQ_GEOM_NAMESPACE
