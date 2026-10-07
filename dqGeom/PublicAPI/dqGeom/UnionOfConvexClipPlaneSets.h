// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — UnionOfConvexClipPlaneSets (union of convex clip regions)
// Ported from: itwinjs-core core/geometry/src/clipping/UnionOfConvexClipPlaneSets.ts
//
// M-P（Sectioning 剖切）P-A 落地。一个点在集合内 = 在任一 ConvexClipPlaneSet 内
// （union 布尔语义）。§3.4 适配（族先例）：bvector → std::vector；GrowableXYZArray →
// std::vector<Point3d>；LineSegment3d → 两点形参；Plane3dByOriginAndUnitNormal 重载缺席。
//
// TODO（参考有、依赖未移植，随依赖落地补齐）：
//  - hasIntersectionWithRay / fromSweptPolygon（Ray3d/Range1d 面）
//  - announceClippedArcIntervals / announceClippedCurveIntervals（曲线区间机械：
//    CurvePrimitive.appendPlaneIntersectionPoints）
//  - multiplyPlanesByMatrix4d（Matrix4d 未移植）
//  - appendPolygonClip（GrowableXYZArrayCache 池机械）
#pragma once

#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/CurvePrimitive.h>  // AnnounceNumberNumber
#include <dqGeom/Point3d.h>
#include <dqGeom/Segment1d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <optional>
#include <cmath>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

/// Wire format describing a UnionOfConvexClipPlaneSets.
/// Ported from: itwinjs-core UnionOfConvexClipPlaneSetsProps
/// (UnionOfConvexClipPlaneSets.ts:28-32)
using UnionOfConvexClipPlaneSetsProps = std::vector<ConvexClipPlaneSetProps>;

/// A collection of ConvexClipPlaneSets. A point is "in" the clip plane set if
/// it is "in" one or more of the ConvexClipPlaneSets (boolean logic: UNION).
/// Ported from: itwinjs-core UnionOfConvexClipPlaneSets
/// (UnionOfConvexClipPlaneSets.ts:34-389)
class DQ_GEOM_EXPORT UnionOfConvexClipPlaneSets {
public:
    /// (property accessor) Return the reference to the array of ConvexClipPlaneSet.
    /// Ported from: UnionOfConvexClipPlaneSets.convexSets (:43-45)
    std::vector<ConvexClipPlaneSet>& convexSets() { return m_convexSets; }
    std::vector<ConvexClipPlaneSet> const& convexSets() const { return m_convexSets; }

    /// Return the toJSON form of each ConvexClipPlaneSet.
    /// Ported from: UnionOfConvexClipPlaneSets.toJSON (:50-55)
    UnionOfConvexClipPlaneSetsProps toJSON() const noexcept {
        UnionOfConvexClipPlaneSetsProps val;
        for (ConvexClipPlaneSet const& convex : m_convexSets)
            val.push_back(convex.toJSON());
        return val;
    }
    /// Convert props to a union (setFromJSON semantics: cleared then appended).
    /// Ported from: UnionOfConvexClipPlaneSets.fromJSON (:57-68)
    static UnionOfConvexClipPlaneSets fromJSON(UnionOfConvexClipPlaneSetsProps const* json) noexcept {
        UnionOfConvexClipPlaneSets result;
        if (json == nullptr)
            return result;
        for (ConvexClipPlaneSetProps const& jsonSet : *json)
            result.m_convexSets.push_back(ConvexClipPlaneSet::fromJSON(&jsonSet));
        return result;
    }

    /// Create a union with no members.
    /// Ported from: UnionOfConvexClipPlaneSets.createEmpty (:70-76)
    static UnionOfConvexClipPlaneSets createEmpty() noexcept { return UnionOfConvexClipPlaneSets{}; }

    /// True if all member convex sets are almostEqual to corresponding members
    /// of other (same order).
    /// Ported from: UnionOfConvexClipPlaneSets.isAlmostEqual (:82-89)
    bool isAlmostEqual(UnionOfConvexClipPlaneSets const& other) const noexcept {
        if (m_convexSets.size() != other.m_convexSets.size())
            return false;
        for (size_t i = 0; i < m_convexSets.size(); ++i)
            if (!m_convexSets[i].isAlmostEqual(other.m_convexSets[i]))
                return false;
        return true;
    }

    /// Create a union with the given ConvexClipPlaneSet members.
    /// Ported from: UnionOfConvexClipPlaneSets.createConvexSets (:91-98)
    static UnionOfConvexClipPlaneSets createConvexSets(std::vector<ConvexClipPlaneSet> const& convexSets) noexcept {
        UnionOfConvexClipPlaneSets result;
        for (ConvexClipPlaneSet const& set : convexSets)
            result.m_convexSets.push_back(set);
        return result;
    }

    /// Return a deep copy.
    /// Ported from: UnionOfConvexClipPlaneSets.clone (:100-106)
    UnionOfConvexClipPlaneSets clone() const noexcept {
        UnionOfConvexClipPlaneSets result;
        for (ConvexClipPlaneSet const& convexSet : m_convexSets)
            result.m_convexSets.push_back(convexSet.clone());
        return result;
    }

    /// Append toAdd to the array of ConvexClipPlaneSet.
    /// Ported from: UnionOfConvexClipPlaneSets.addConvexSet (:111-114)
    void addConvexSet(ConvexClipPlaneSet const& toAdd) { m_convexSets.push_back(toAdd); }

    /// True if any contained convex set has the point strictly inside.
    /// Ported from: UnionOfConvexClipPlaneSets.isPointInside (:144-151)
    bool isPointInside(Point3d const& point) const noexcept {
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            if (convexSet.isPointInside(point))
                return true;
        }
        return false;
    }
    /// True if any contained convex set has the point on or inside.
    /// Ported from: UnionOfConvexClipPlaneSets.isPointOnOrInside (:156-162)
    bool isPointOnOrInside(Point3d const& point, double tolerance = 1.0e-6) const noexcept {
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            if (convexSet.isPointOnOrInside(point, tolerance))
                return true;
        }
        return false;
    }
    /// True if any contained convex set has the sphere on or inside.
    /// Ported from: UnionOfConvexClipPlaneSets.isSphereInside (:167-173)
    bool isSphereInside(Point3d const& point, double radius) const noexcept {
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            if (convexSet.isSphereInside(point, radius))
                return true;
        }
        return false;
    }

    /// Test if any part of a line segment is within the volume.
    /// Ported from: UnionOfConvexClipPlaneSets.isAnyPointInOrOnFromSegment (:175-181)
    // §3.4 适配：LineSegment3d → 两点形参（等价调用 announce(0,1,p0,p1)）。
    bool isAnyPointInOrOnFromSegment(Point3d const& point0, Point3d const& point1) const noexcept {
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            if (convexSet.announceClippedSegmentIntervals(0.0, 1.0, point0, point1))
                return true;
        }
        return false;
    }

    /// Collect the fractions of the segment that pass through the set regions,
    /// as 1d segments (multiple intervals per set possible).
    /// Ported from: UnionOfConvexClipPlaneSets.appendIntervalsFromSegment (:185-191)
    // §3.4 适配：LineSegment3d → 两点形参。
    void appendIntervalsFromSegment(Point3d const& point0, Point3d const& point1,
                                    std::vector<Segment1d>& intervals) const noexcept {
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            convexSet.announceClippedSegmentIntervals(0.0, 1.0, point0, point1,
                                                      [&](double fraction0, double fraction1) {
                                                          intervals.push_back(Segment1d(fraction0, fraction1));
                                                      });
        }
    }

    /// Announce the fractional interval of the segment inside the union (any
    /// set). Returns true if any set announced.
    /// Ported from: UnionOfConvexClipPlaneSets.announceClippedSegmentIntervals (:243-252)
    bool announceClippedSegmentIntervals(
        double f0, double f1, Point3d const& pointA, Point3d const& pointB,
        AnnounceNumberNumber const& announce = nullptr) const noexcept {
        size_t numAnnounce = 0;
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            if (convexSet.announceClippedSegmentIntervals(f0, f1, pointA, pointB, announce))
                numAnnounce++;
        }
        return numAnnounce > 0;
    }

    /// Apply transform to all the ConvexClipPlaneSets.
    /// Ported from: UnionOfConvexClipPlaneSets.transformInPlace (:193-197)
    void transformInPlace(Transform const& transform) noexcept {
        for (ConvexClipPlaneSet& convexSet : m_convexSets)
            convexSet.transformInPlace(transform);
    }

    /// Returns 1, 2, or 3 based on whether points are strongly inside,
    /// ambiguous, or strongly outside respectively (union: first non-outside
    /// status wins).
    /// Ported from: UnionOfConvexClipPlaneSets.classifyPointContainment (:199-206)
    ClipPlaneContainment classifyPointContainment(std::vector<Point3d> const& points,
                                                  bool onIsOutside) const noexcept {
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            ClipPlaneContainment const thisStatus = convexSet.classifyPointContainment(points, onIsOutside);
            if (thisStatus != ClipPlaneContainment::StronglyOutside)
                return thisStatus;
        }
        return ClipPlaneContainment::StronglyOutside;
    }

    /// Clip a polygon to the planes of the clip sets, returning new polygon
    /// boundaries (inside pieces only, one per convex set).
    /// Ported from: UnionOfConvexClipPlaneSets.polygonClip (:223-241)
    // §3.4 适配：GrowableXYZArray → std::vector<Point3d>（输出 vector<vector<Point3d>>，
    // 调用前清空 output —— 1:1 output.length = 0）。
    void polygonClip(std::vector<Point3d> const& input, std::vector<std::vector<Point3d>>& output,
                     std::vector<Point3d>& work, ClipPlane const* planeToSkip = nullptr,
                     double tolerance = 1.0e-6) const noexcept {
        output.clear();
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            std::vector<Point3d> convexSetOutput;
            convexSet.polygonClip(input, convexSetOutput, work, planeToSkip, tolerance);
            if (!convexSetOutput.empty())
                output.push_back(std::move(convexSetOutput));
        }
    }

    /// Collect the output from computePlanePlanePlaneIntersections in all the
    /// contained convex sets. Returns number of points.
    /// Ported from: UnionOfConvexClipPlaneSets.computePlanePlanePlaneIntersectionsInAllConvexSets
    /// (:285-293)
    size_t computePlanePlanePlaneIntersectionsInAllConvexSets(
        std::vector<Point3d>* points, Range3d* rangeToExtend, Transform const* transform = nullptr,
        bool testContainment = true) const noexcept {
        size_t n = 0;
        for (ConvexClipPlaneSet const& convexSet : m_convexSets) {
            n += convexSet.computePlanePlanePlaneIntersections(points, rangeToExtend, transform, testContainment);
        }
        return n;
    }

    /// Recursively call setInvisible on all member convex sets.
    /// Ported from: UnionOfConvexClipPlaneSets.setInvisible (:318-323)
    void setInvisible(bool invisible) noexcept {
        for (ConvexClipPlaneSet& convexSet : m_convexSets)
            convexSet.setInvisible(invisible);
    }

    /// Add convex sets that accept points below zLow and above zHigh.
    /// Ported from: UnionOfConvexClipPlaneSets.addOutsideZClipSets (:325-336)
    // NOTE 参考怪癖 1:1：`if (zLow)` truthiness——0/-0/NaN 均 falsy 跳过
    //（C++ NaN != 0.0 为 true——显式 isnan 门对齐，审计 E-5）。
    void addOutsideZClipSets(bool invisible, std::optional<double> zLow = std::nullopt,
                             std::optional<double> zHigh = std::nullopt) noexcept {
        if (zLow.has_value() && *zLow != 0.0 && !std::isnan(*zLow)) {
            ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createEmpty();
            convexSet.addZClipPlanes(invisible, std::nullopt, *zLow);
            m_convexSets.push_back(convexSet);
        }
        if (zHigh.has_value() && *zHigh != 0.0 && !std::isnan(*zHigh)) {
            ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createEmpty();
            convexSet.addZClipPlanes(invisible, *zHigh);
            m_convexSets.push_back(convexSet);
        }
    }

    /// Move convex sets from source.
    /// Ported from: UnionOfConvexClipPlaneSets.takeConvexSets (:338-343)
    void takeConvexSets(UnionOfConvexClipPlaneSets& source) noexcept {
        while (!source.m_convexSets.empty()) {
            m_convexSets.push_back(std::move(source.m_convexSets.back()));
            source.m_convexSets.pop_back();
        }
    }

private:
    std::vector<ConvexClipPlaneSet> m_convexSets;
};

END_DQ_GEOM_NAMESPACE
