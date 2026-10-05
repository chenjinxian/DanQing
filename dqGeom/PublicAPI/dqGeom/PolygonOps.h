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
#include "Ray3d.h"
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

    /// Signed area of the xy polygon (shoelace via cross products to p0).
    /// Ported from: PolygonOps.areaXY (PolygonOps.ts) — Point3d[] 形态分支
    static double areaXY(const std::vector<Point3d>& points) noexcept
    {
        double area = 0.0;
        for (size_t i = 1; i + 1 < points.size(); ++i)
            area += points[0].CrossProductToPointsXY(points[i], points[i + 1]);
        return 0.5 * area;
    }

    /// Centroid, area normal, and area of a 3d polygon (fan sum; works with or
    /// without closure point). Returns the centroid/normal ray with
    /// area-weight normalized normal; nullopt for degenerate (<3 pts or zero
    /// area).
    /// Ported from: PolygonOps.centroidAreaNormal (PolygonOps.ts:577-634)
    /// M-P P-F（ViewClipDecoration 的 loop 质心面）。tryNormalizeInPlaceWithAreaWeight
    /// 语义 = 面积权重 > smallMetricDistance 时归一。
    static std::optional<dqGeom::Ray3d> centroidAreaNormal(std::vector<Point3d> const& points)
    {
        size_t const n = points.size();
        if (n < 3)
            return std::nullopt;
        if (n == 3) {
            dqGeom::Vector3d const normal = dqGeom::Vector3d::FromCrossProduct(
                points[1].x - points[0].x, points[1].y - points[0].y, points[1].z - points[0].z,
                points[2].x - points[0].x, points[2].y - points[0].y, points[2].z - points[0].z);
            double const a = 0.5 * std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            dqGeom::Point3d centroid = dqGeom::Point3d::From(
                (points[0].x + points[1].x + points[2].x) / 3.0,
                (points[0].y + points[1].y + points[2].y) / 3.0,
                (points[0].z + points[1].z + points[2].z) / 3.0);
            dqGeom::Ray3d result{centroid, normal};
            if (a > 1.0e-6) {  // tryNormalizeInPlaceWithAreaWeight（面积权重门）
                result.direction.Normalize();
                return result;
            }
            return std::nullopt;
        }
        // n > 3：参考 accumulateCross(0,i-1,i) 面法向 + 面积加权重心。
        dqGeom::Vector3d areaNormal = dqGeom::Vector3d::FromZero();
        for (size_t i = 2; i < n; ++i) {
            dqGeom::Vector3d const c = dqGeom::Vector3d::FromCrossProduct(
                points[i - 1].x - points[0].x, points[i - 1].y - points[0].y,
                points[i - 1].z - points[0].z,
                points[i].x - points[0].x, points[i].y - points[0].y,
                points[i].z - points[0].z);
            areaNormal.x += c.x;
            areaNormal.y += c.y;
            areaNormal.z += c.z;
        }
        areaNormal.Normalize();
        dqGeom::Point3d const& origin = points[0];
        dqGeom::Vector3d vector0 = dqGeom::Vector3d::From(
            points[1].x - origin.x, points[1].y - origin.y, points[1].z - origin.z);
        dqGeom::Vector3d centroidSum = dqGeom::Vector3d::FromZero();
        dqGeom::Vector3d normal = dqGeom::Vector3d::FromZero();
        for (size_t i = 2; i < n; ++i) {
            dqGeom::Vector3d const vector1 = dqGeom::Vector3d::From(
                points[i].x - origin.x, points[i].y - origin.y, points[i].z - origin.z);
            dqGeom::Vector3d const cross = dqGeom::Vector3d::FromCrossProduct(vector0, vector1);
            double const signedTriangleArea = areaNormal.DotProduct(cross);  // twice the area
            normal.x += cross.x;
            normal.y += cross.y;
            normal.z += cross.z;
            double const b = signedTriangleArea / 6.0;
            centroidSum.x += vector0.x * b + vector1.x * b;
            centroidSum.y += vector0.y * b + vector1.y * b;
            centroidSum.z += vector0.z * b + vector1.z * b;
            vector0 = vector1;
        }
        double const area = 0.5 * std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (area > 1.0e-14) {  // conditionalDivideFraction(1, area) 面
            double const inverseArea = 1.0 / area;
            dqGeom::Point3d const centroid = dqGeom::Point3d::From(
                origin.x + centroidSum.x * inverseArea,
                origin.y + centroidSum.y * inverseArea,
                origin.z + centroidSum.z * inverseArea);
            dqGeom::Ray3d result{centroid, normal};
            if (area > 1.0e-6) {
                result.direction.Normalize();
                return result;
            }
            return result;
        }
        return std::nullopt;
    }

    /// Test convexity/orientation of the xy polygon: 1 = CCW convex, -1 = CW
    /// convex, 0 = concave or degenerate. Trailing duplicates of the first
    /// point are ignored.
    /// Ported from: PolygonOps.testXYPolygonTurningDirections (PolygonOps.ts)
    // §3.4 适配：Point2d[]|Point3d[] 联合 → Point3d 形态（消费方 ClipShape 均传 Point3d）。
    static double testXYPolygonTurningDirections(const std::vector<Point3d>& points) noexcept
    {
        // Reduce count by trailing duplicates; leaves iLast at final index
        size_t numPoint = points.size();
        size_t iLast = numPoint - 1;
        while (iLast > 1 && points[iLast].x == points[0].x && points[iLast].y == points[0].y) {
            numPoint = iLast--;
        }
        if (numPoint > 2) {
            // vector0 = P[iLast-1] -> P[iLast], vector1 = P[iLast] -> P[0]（xy only）
            double v0x = points[iLast].x - points[iLast - 1].x;
            double v0y = points[iLast].y - points[iLast - 1].y;
            double v1x = points[0].x - points[iLast].x;
            double v1y = points[0].y - points[iLast].y;
            double const baseArea = v0x * v1y - v0y * v1x;
            // In a convex polygon, all successive-vector cross products have
            // the same sign as the base area, hence all products are positive.
            for (size_t i1 = 1; i1 < numPoint; ++i1) {
                double const v0xPrev = v1x, v0yPrev = v1y;
                v1x = points[i1].x - points[i1 - 1].x;
                v1y = points[i1].y - points[i1 - 1].y;
                double const currArea = v0xPrev * v1y - v0yPrev * v1x;
                if (currArea * baseArea <= 0.0)
                    return 0;
                v0x = v0xPrev;
                v0y = v0yPrev;
            }
            // Fall out with all signs same as base area
            return baseArea > 0.0 ? 1 : -1;
        }
        return 0;
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
