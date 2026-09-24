// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — frustum planes for containment testing
// Ported from: itwinjs-core core/common/src/geometry/FrustumPlanes.ts
#pragma once

#include "Frustum.h"
#include "DqCommon.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <array>
#include <cstdint>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// ---------------------------------------------------------------------------
// FrustumPlanes — the six planes of a Frustum for containment/intersection
// tests. A valid frustum produces six planes; a degenerate one produces zero
// (isValid == false, everything counts as contained).
// Ported from: itwinjs-core FrustumPlanes (FrustumPlanes.ts)
// ---------------------------------------------------------------------------
struct FrustumPlanes {
    // Degree to which an object is contained within the frustum planes.
    // Ported from: FrustumPlanes.Containment (FrustumPlanes.ts:247-254).
    enum class Containment : uint8_t {
        Outside = 0,  // entirely outside, intersecting none of the planes
        Partial = 1,  // intersects at least one plane
        Inside = 2,   // entirely inside
    };

    // Plane: normal·p + d = 0, interior satisfies normal·p + d > 0.
    // (The reference stores ClipPlanes; altitude(p) = normal·p - distance.)
    struct Plane {
        dqGeom::Vector3d normal;
        double d = 0.0;  // = -(normal·pointOnPlane) + expandPlaneDistance form below
    };

    /// Whether the planes are valid (six of them).
    bool isValid() const noexcept { return m_planes.size() == 6; }

    /// Compute the six planes of the specified frustum.
    /// Ported from: FrustumPlanes.fromFrustum (FrustumPlanes.ts:100-102 →
    /// computeFrustumPlanes :46-78 — five planes from corner cross products,
    /// the front plane derived from the back plane because the front rect can
    /// be tiny and numerically degenerate).
    static FrustumPlanes fromFrustum(Frustum const& frustum)
    {
        // Ordering of sub-arrays is [origin, a, b] — FrustumPlanes.ts:36-44.
        static int const planePointIndices[5][3] = {
            {1, 5, 3},  // right
            {0, 2, 4},  // left
            {2, 3, 6},  // top
            {0, 4, 1},  // bottom
            {0, 1, 2},  // back
        };
        double constexpr expandPlaneDistance = 1e-6;

        FrustumPlanes out;
        dqGeom::Vector3d normal;
        for (auto const& indices : planePointIndices) {
            dqGeom::Point3d const& p0 = frustum.points[indices[0]];
            dqGeom::Point3d const& p1 = frustum.points[indices[1]];
            dqGeom::Point3d const& p2 = frustum.points[indices[2]];
            dqGeom::Vector3d a, b;
            a.DifferenceOf(p1, p0);
            b.DifferenceOf(p2, p0);
            normal.CrossProduct(a, b);
            if (normal.Normalize() == 0.0)
                return {};  // degenerate → zero planes

            Plane plane;
            plane.normal = normal;
            plane.d = -(normal.DotProduct(p0)) + expandPlaneDistance;
            out.m_planes.push_back(plane);
        }

        // Front plane from the negated back normal; distance measured at a
        // point on the FRONT rect (FrustumPlanes.ts:64-75).
        Plane front;
        front.normal.Negate(normal);
        front.d = -(front.normal.DotProduct(frustum.points[4])) + expandPlaneDistance;
        out.m_planes.push_back(front);

        return out;
    }

    /// Compute the degree to which a range (expanded to its 8 corners) is
    /// contained within these planes. `sphere` is an optional bounding sphere
    /// of the range for the cheap early-out test.
    /// Ported from: FrustumPlanes.computeContainment (FrustumPlanes.ts:168-205)
    /// via computeFrustumContainment (:140-142 — the range-corner form is the
    /// Frustum.fromRange(frustum) equivalent).
    Containment computeContainment(dqGeom::Range3d const& range,
                                   dqGeom::Point3d const* sphereCenter = nullptr,
                                   double sphereRadius = 0.0,
                                   double tolerance = 1.0e-8) const
    {
        if (!isValid())
            return Containment::Outside;

        // Cheap bounding-sphere test first (FrustumPlanes.ts:173-183).
        bool planesContainingSphere[6] = {};
        if (sphereCenter) {
            for (size_t i = 0; i < m_planes.size(); ++i) {
                double const centerDistance =
                    m_planes[i].normal.DotProduct(*sphereCenter) + m_planes[i].d;
                double const tolerancePlusRadius = tolerance + sphereRadius;
                if (centerDistance < -tolerancePlusRadius)
                    return Containment::Outside;
                planesContainingSphere[i] = centerDistance > tolerancePlusRadius;
            }
        }

        // Point test over the range's 8 corners (FrustumPlanes.ts:185-202).
        auto const corners = range.Corners();

        bool allInside = true;
        for (size_t i = 0; i < m_planes.size(); ++i) {
            if (sphereCenter && planesContainingSphere[i])
                continue;

            int nOutside = 0;
            for (auto const& point : corners) {
                if (m_planes[i].normal.DotProduct(point) + m_planes[i].d + tolerance < 0.0) {
                    ++nOutside;
                    allInside = false;
                }
            }
            if (nOutside == 8)
                return Containment::Outside;
        }

        return allInside ? Containment::Inside : Containment::Partial;
    }

private:
    std::vector<Plane> m_planes;  // ordered right,left,top,bottom,back,front
};

END_DQ_COMMON_NAMESPACE
