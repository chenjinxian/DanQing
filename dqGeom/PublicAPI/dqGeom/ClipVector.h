// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipVector (collection of ClipPrimitives, intersection semantics)
// Ported from: itwinjs-core core/geometry/src/clipping/ClipVector.ts
//
// M-P（Sectioning 剖切）P-A 落地。ClipVector 定义成员 ClipPrimitive 区域的交集；常态
// 用法 = 一个外区域 + 若干 mask 洞。§3.4 适配：TS GC 引用语义 → RefCounted/RefPtr；
/// bvector → std::vector；Segment1d 直用（dqGeom 既有）；try/catch fromJSON 容错 →
/// 宿主预解析（无异常路径）。
//
// TODO（参考有、依赖未移植，随依赖落地补齐）：
//  - announceClippedSegmentIntervals / announceClippedArcIntervals /
//    announceClippedCurveIntervals / appendPolygonClip（经 BooleanClipNodeIntersection
//    代理与 GrowableXYZArrayCache 池——BooleanClipNode 族未移植；M-P 剖切链无消费方）
//  - multiplyPlanesByMatrix4d（Matrix4d 未移植）
#pragma once

#include <dqBase/RefCounted.h>

#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/Export.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Segment1d.h>
#include <dqGeom/Transform.h>

#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

/// Wire format describing a ClipVector (array of ClipPrimitiveProps).
/// Ported from: itwinjs-core ClipVectorProps (ClipVector.ts:29-33)
using ClipVectorProps = std::vector<ClipPrimitiveProps>;

/// Class holding an array structure of shapes defined by ClipPrimitive. The
/// ClipVector defines an intersection of the member ClipPrimitive regions.
/// Ported from: itwinjs-core ClipVector (ClipVector.ts:36-484)
class DQ_GEOM_EXPORT ClipVector : public dqBase::RefCounted<ClipVector> {
public:
    using Ptr = dqBase::RefPtr<ClipVector>;

    ~ClipVector() = default;

    /// Range acting as first filter (overall range limit, not precise planes).
    /// Ported from: ClipVector.boundingRange (:50, public field)
    Range3d boundingRange;

    /// Returns a reference to the array of ClipPrimitives.
    /// Ported from: ClipVector.clips (:52-54)
    std::vector<ClipPrimitive::Ptr> const& clips() const noexcept { return m_clips; }
    std::vector<ClipPrimitive::Ptr>& clips() noexcept { return m_clips; }

    /// Returns true if this ClipVector contains a ClipPrimitive.
    /// Ported from: ClipVector.isValid (:59-61)
    bool isValid() const noexcept { return !m_clips.empty(); }

    /// Create a ClipVector with an empty set of ClipShapes.
    /// Ported from: ClipVector.createEmpty (:63-69)
    static Ptr createEmpty();

    /// Create a ClipVector from an array of ClipPrimitives (capture the
    /// pointers — RefPtr 共享，appendReference 语义等价).
    /// Ported from: ClipVector.createCapture (:71-77)
    static Ptr createCapture(std::vector<ClipPrimitive::Ptr> clips);

    /// Create a ClipVector from (clones of) an array of ClipPrimitives.
    /// Ported from: ClipVector.create (:79-84)
    static Ptr create(std::vector<ClipPrimitive::Ptr> const& clips);

    /// Create a deep copy of another ClipVector.
    /// Ported from: ClipVector.clone (:86-94)
    Ptr clone() const;

    /// Parse this ClipVector into a JSON object form.
    /// Ported from: ClipVector.toJSON (:96-101)
    ClipVectorProps toJSON() const;

    /// Parse a JSON props array into a new ClipVector (invalid members are
    /// skipped by ClipPrimitive::fromJSON).
    /// Ported from: ClipVector.fromJSON (:103-118)
    static Ptr fromJSON(ClipVectorProps const* json);

    /// Empties out the array of ClipShapes.
    /// Ported from: ClipVector.clear (:120-122)
    void clear() noexcept { m_clips.clear(); }

    /// Append a deep copy of the given ClipPrimitive to this ClipVector.
    /// Ported from: ClipVector.appendClone (:124-126)
    void appendClone(ClipPrimitive const& clip) { m_clips.push_back(clip.clone()); }

    /// Append a reference of the given ClipPrimitive to this ClipVector.
    /// Ported from: ClipVector.appendReference (:128-130)
    void appendReference(ClipPrimitive::Ptr clip) { m_clips.push_back(std::move(clip)); }

    /// Create and append a new ClipPrimitive given a shape as an array of
    /// points. Returns true if successful.
    /// Ported from: ClipVector.appendShape (:132-139)
    bool appendShape(std::vector<Point3d> const& shape, std::optional<double> zLow = std::nullopt,
                     std::optional<double> zHigh = std::nullopt, Transform const* transform = nullptr,
                     bool isMask = false, bool invisible = false);

    /// True if the given point lies inside all of this ClipVector's members.
    /// Ported from: ClipVector.pointInside (:141-143)
    bool pointInside(Point3d const& point, double onTolerance = kSmallMetricDistanceSquared) const;

    /// Method from the Clipper interface.
    /// Ported from: ClipVector.isPointOnOrInside (:148-157)
    bool isPointOnOrInside(Point3d const& point, double onTolerance = kSmallMetricDistanceSquared) const;

    /// Transform this ClipVector to a new coordinate-system (boundingRange
    /// expands under rotation). Returns true if successful.
    /// Ported from: ClipVector.transformInPlace (:221-230)
    bool transformInPlace(Transform const& transform);

    /// Package this ClipVector's ClipShape points into loopPoints, returning
    /// {clipMask, zBack, zFront} (see reference doc for the transform rules).
    /// Ported from: ClipVector.extractBoundaryLoops (:242-299)
    std::vector<double> extractBoundaryLoops(std::vector<std::vector<Point3d>>& loopPoints,
                                             Transform* transform = nullptr) const;

    /// Sets this ClipVector and all of its members to the visibility given.
    /// Ported from: ClipVector.setInvisible (:301-304)
    void setInvisible(bool invisible);

    /// For every clip, parse the member point array into the member clip
    /// plane object.
    /// Ported from: ClipVector.parseClipPlanes (:309-312)
    void parseClipPlanes();

    /// Determines whether the given points fall inside or outside this set of
    /// ClipPrimitives.
    /// Ported from: ClipVector.classifyPointContainment (:342-357)
    ClipPlaneContainment classifyPointContainment(std::vector<Point3d> const& points,
                                                  bool ignoreMasks = false) const;

    /// Determines whether a 3d range lies inside or outside this set.
    /// Ported from: ClipVector.classifyRangeContainment (:362-365)
    ClipPlaneContainment classifyRangeContainment(Range3d const& range, bool ignoreMasks) const;

    /// For an array of points making up a LineString, true if any segment
    /// lies inside this ClipVector.
    /// Ported from: ClipVector.isAnyLineStringPointInside (:371-383)
    bool isAnyLineStringPointInside(std::vector<Point3d> const& points) const;

    /// For an array of points making up a LineString, true if all segments
    /// lie inside this ClipVector.
    /// Ported from: ClipVector.isLineStringCompletelyContained (:396-420)
    bool isLineStringCompletelyContained(std::vector<Point3d> const& points) const;

    /// Sum of the lengths of intervals[begin..end).
    /// Ported from: ClipVector.sumSizes (:385-390)
    static double sumSizes(std::vector<Segment1d> const& intervals, size_t begin, size_t end) noexcept;

    /// Serializes this ClipVector to a compact string representation
    /// (section-cut tile requests).
    /// Ported from: ClipVector.toCompactString (:443-483)
    std::string toCompactString() const;

private:
    ClipVector() = default;
    explicit ClipVector(std::vector<ClipPrimitive::Ptr> clips) : m_clips(std::move(clips)) {}

    std::vector<ClipPrimitive::Ptr> m_clips;
};

/// Bundles a ClipVector with its compact string representation (computed
/// once; the ClipVector is assumed not to be subsequently modified).
/// Ported from: itwinjs-core StringifiedClipVector (ClipVector.ts:486-518)
// §3.4 适配：TS intersection type（ClipVector & {clipString}）→ 值捆绑结构。
struct StringifiedClipVector {
    ClipVector::Ptr clip;
    std::string clipString;

    /// Create from a ClipVector; returns nullopt for null/empty input.
    /// Ported from: StringifiedClipVector.fromClipVector (:506-517)
    static std::optional<StringifiedClipVector> fromClipVector(ClipVector::Ptr const& clip);
};

END_DQ_GEOM_NAMESPACE
