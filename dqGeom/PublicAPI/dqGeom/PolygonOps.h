// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/geometry3d/PolygonOps.ts (areaNormalGo / unitNormal)
// DanQing dqGeom — PolygonOps (polygon area / normal helpers; minimal subset for BuildAverageNormals)
//
// 保真依据：移植 PolygonOps.ts 的 areaNormalGo + unitNormal（buildAverageNormals / buildPerFaceNormals
// 消费）。areaNormalGo：n==3 取叉积，n>3 鞋带（sum cross(p0,p[i-1],p[i])），×0.5；零向量→nullopt。
// unitNormal：areaNormalGo + Normalize。PolygonOps.ts 其余方法（area/areaXY/centroidAreaNormal/...）
// 留待消费方需要时补。本文件 header-only（两短方法）。
#pragma once

#include "Point3d.h"
#include "Range3d.h"
#include "Vector3d.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// 1:1 PolygonOps (areaNormalGo / unitNormal subset).
class PolygonOps {
public:
    /// Compute the polygon area normal (magnitude == polygon area). Returns nullopt for degenerate
    /// (fewer than 3 points, or colinear). 1:1 PolygonOps.areaNormalGo.
    static std::optional<Vector3d> areaNormalGo(const std::vector<Point3d>& points) noexcept
    {
        const size_t n = points.size();
        if (n < 3)
            return std::nullopt;

        const Point3d& p0 = points[0];
        Vector3d result = Vector3d::FromZero();
        if (n == 3) {
            // cross(p1-p0, p2-p0)
            result = Vector3d::FromCrossProduct(
                points[1].x - p0.x, points[1].y - p0.y, points[1].z - p0.z,
                points[2].x - p0.x, points[2].y - p0.y, points[2].z - p0.z);
        } else {
            // Shoelace: sum cross(p0, p[i-1], p[i]) for i = 2..n-1 (closure point optional).
            for (size_t i = 2; i < n; ++i) {
                const Vector3d c = Vector3d::FromCrossProduct(
                    points[i - 1].x - p0.x, points[i - 1].y - p0.y, points[i - 1].z - p0.z,
                    points[i].x - p0.x, points[i].y - p0.y, points[i].z - p0.z);
                result.x += c.x;
                result.y += c.y;
                result.z += c.z;
            }
        }
        // ALL BRANCHES SUM FULL CROSS PRODUCTS AND EXPECT SCALE HERE (1:1 reference).
        result.Scale(0.5);
        if (result.x == 0.0 && result.y == 0.0 && result.z == 0.0)
            return std::nullopt;
        return result;
    }

    /// Unit normal of the polygon plane; returns false if degenerate. 1:1 PolygonOps.unitNormal.
    static bool unitNormal(const std::vector<Point3d>& points, Vector3d& out) noexcept
    {
        const auto an = areaNormalGo(points);
        if (!an)
            return false;
        out = *an;
        return out.Normalize() > 0.0;
    }

    /// Clip convex polygon `xyz` in place by the halfspace {p : planeAbc·p + planeD >= 0}
    /// (keepPositive=true) or its complement. planeAbc/planeD are the Point4d-style plane
    /// coefficients (a,b,c,d) with altitude a*x+b*y+c*z+d. Returns numSimpleCrossings +
    /// numTangentRuns (faithful return); the polygon is modified in place.
    /// Ported from: IndexedXYZCollectionPolygonOps.clipConvexPolygonInPlace (PolygonOps.ts:1639-1722)
    static int clipConvexPolygonInPlace(Vector3d const& planeAbc, double planeD,
                                        std::vector<Point3d>& xyz, bool keepPositive = true,
                                        double tolerance = 1.0e-6) noexcept
    {
        // Ignore closure point(s) exact-equal to the last vertex.
        while (xyz.size() > 1 && xyz.front().IsEqual(xyz.back()))
            xyz.pop_back();

        const size_t n = xyz.size();
        if (n < 3) {  // swallow degenerate polygon
            xyz.clear();
            return 0;
        }

        const double s = keepPositive ? 1.0 : -1.0;
        auto altitude = [&](Point3d const& p) -> double {
            const double a = s * (planeAbc.DotProduct(p) + planeD);
            return std::abs(a) < tolerance ? 0.0 : a;
        };

        const double fractionTol = 1.0e-8;
        int numClippedVertices = 0;
        int numSimpleCrossings = 0;  // new surviving vertices at edge∩plane
        int numTangentRuns = 0;      // on-plane vertex runs
        std::vector<Point3d> surviving;
        surviving.reserve(n + 2);

        double a0 = altitude(xyz[n - 1]);
        size_t index0 = n - 1;
        for (size_t index1 = 0; index1 < n; ++index1) {
            const double a1 = altitude(xyz[index1]);
            if (a1 < 0.0)
                numClippedVertices++;
            if (a0 * a1 < 0.0) {  // simple crossing
                const double f = -a0 / (a1 - a0);
                if (!(f > 1.0 - fractionTol && a1 > 0.0)) {
                    // interpolate between index0 and index1 at fraction f.
                    surviving.push_back(Point3d::From(
                        xyz[index0].x + f * (xyz[index1].x - xyz[index0].x),
                        xyz[index0].y + f * (xyz[index1].y - xyz[index0].y),
                        xyz[index0].z + f * (xyz[index1].z - xyz[index0].z)));
                    numSimpleCrossings++;
                }
            }
            if (a1 >= 0.0) {
                surviving.push_back(xyz[index1]);
                if (a1 == 0.0 && a0 != 0.0)
                    numTangentRuns++;
            }
            a0 = a1;
            index0 = index1;
        }

        if (numClippedVertices > 0)
            xyz = surviving;
        return numSimpleCrossings + numTangentRuns;
    }

    /// Clip convex polygon `xyz` in place to the interior of `range` (6 axis-aligned
    /// halfspaces). Returns false (with xyz cleared) if the polygon is empty after any clip.
    /// Ported from: IndexedXYZCollectionPolygonOps.intersectRangeConvexPolygonInPlace
    ///               (PolygonOps.ts:1805-1841)
    static bool intersectRangeConvexPolygonInPlace(Range3d const& range,
                                                   std::vector<Point3d>& xyz) noexcept
    {
        if (range.isNull())
            return false;

        // Each clip plane (abc, d) with altitude a*x+b*y+c*z+d >= 0 selects the in-range side.
        auto clip = [&](Vector3d const& abc, double d) -> bool {
            clipConvexPolygonInPlace(abc, d, xyz, true);
            return !xyz.empty();
        };
        if (!clip(Vector3d::From(0, 0, -1), range.high.z)) return false;  // z <= high.z
        if (!clip(Vector3d::From(0, 0, 1), -range.low.z)) return false;   // z >= low.z
        if (!clip(Vector3d::From(0, -1, 0), range.high.y)) return false;  // y <= high.y
        if (!clip(Vector3d::From(0, 1, 0), -range.low.y)) return false;   // y >= low.y
        if (!clip(Vector3d::From(-1, 0, 0), range.high.x)) return false;  // x <= high.x
        if (!clip(Vector3d::From(1, 0, 0), -range.low.x)) return false;   // x >= low.x
        return true;
    }
};

END_DQ_GEOM_NAMESPACE
