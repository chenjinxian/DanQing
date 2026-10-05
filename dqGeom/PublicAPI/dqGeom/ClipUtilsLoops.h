// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipUtilities 剖切轮廓/范围族（loops / rangeOf / does）
// Ported from: itwinjs-core core/geometry/src/clipping/ClipUtils.ts
//   announceLoopsOfConvexClipPlaneSetIntersectRange (:385-437) /
//   loopsOfConvexClipPlaneIntersectionWithRange (:448-472) /
//   rangeOfConvexClipPlaneSetIntersectionWithRange (:479-487) /
//   rangeOfClipperIntersectionWithRange (:501-537) /
//   doesClipperIntersectRange (:552-585) /
//   doesConvexClipPlaneSetIntersectRange (:597+)
//
// M-P（Sectioning 剖切）P-A 落地。ViewClipDecoration.getClipData 的轮廓来源。
// §3.4 适配：
//  - ClipUtils.h 只含无依赖子集（selectIntervals01 等）；本族函数消费
//    ConvexClipPlaneSet/UnionOf/ClipPrimitive/ClipVector，C++ 包含分层要求拆文件
//    （参考同文件 ClipUtils.ts——结构适配，函数逐行 1:1）。
//  - TS 联合类型形参（ConvexSet|UnionOf|ClipPrimitive|ClipVector|undefined）→ 重载；
//    undefined 分支由调用方判空（不设重载）。
//  - GrowableXYZArray → std::vector<Point3d>；Loop.createPolygon → Loop::CreatePolygon。
//
// TODO（参考有、依赖未移植）：createComplementaryClips（ClipUtils.ts:1092-1116——
// 依赖 CurveFactory.planePlaneIntersectionRay/Ray3d 面）。
#pragma once

#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipUtils.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/UnionOfConvexClipPlaneSets.h>
#include <dqGeom/Vector3d.h>

#include <functional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

/// Loop-announce callback: receives one loop's points.
using ClipLoopFunction = std::function<void(std::vector<Point3d> const&)>;

/// ClipUtilities 的剖切轮廓/范围族（subset 续）。
/// Ported from: itwinjs-core ClipUtilities (ClipUtils.ts, loops subset)
struct DQ_GEOM_EXPORT ClipUtilitiesLoops {
    /// Emit point loops for intersection of a convex set (or a single plane)
    /// with a range: convex-set plane faces (each clipped by the range and the
    /// other planes, planeToSkip=该面自身) plus the range faces clipped by the
    /// set.
    /// Ported from: ClipUtilities.announceLoopsOfConvexClipPlaneSetIntersectRange
    /// (ClipUtils.ts:385-437)
    static void announceLoopsOfConvexClipPlaneSetIntersectRange(
        ConvexClipPlaneSet const& convexSet, Range3d const& range,
        ClipLoopFunction const& loopFunction, bool includeConvexSetFaces = true,
        bool includeRangeFaces = true, bool ignoreInvisiblePlanes = false);
    /// Single-plane overload (ClipUtils.ts:408-416, 430-434 分支).
    static void announceLoopsOfConvexClipPlaneSetIntersectRange(
        ClipPlane const& plane, Range3d const& range, ClipLoopFunction const& loopFunction,
        bool includeConvexSetFaces = true, bool includeRangeFaces = true, bool ignoreInvisiblePlanes = false);

    /// Return loops (Loop polygons) that are facets of the intersection of the
    /// clipper with a range (union: one announce per convex set).
    /// Ported from: ClipUtilities.loopsOfConvexClipPlaneIntersectionWithRange
    /// (ClipUtils.ts:448-472)
    static std::vector<dqBase::RefPtr<Loop>> loopsOfConvexClipPlaneIntersectionWithRange(
        UnionOfConvexClipPlaneSets const& allClippers, Range3d const& range,
        bool includeConvexSetFaces = true, bool includeRangeFaces = true, bool ignoreInvisiblePlanes = false);
    /// ConvexClipPlaneSet / ClipPlane overload (ClipUtils.ts:464-470).
    static std::vector<dqBase::RefPtr<Loop>> loopsOfConvexClipPlaneIntersectionWithRange(
        ConvexClipPlaneSet const& allClippers, Range3d const& range,
        bool includeConvexSetFaces = true, bool includeRangeFaces = true, bool ignoreInvisiblePlanes = false);
    static std::vector<dqBase::RefPtr<Loop>> loopsOfConvexClipPlaneIntersectionWithRange(
        ClipPlane const& allClippers, Range3d const& range, bool includeConvexSetFaces = true,
        bool includeRangeFaces = true, bool ignoreInvisiblePlanes = false);

    /// Return the (possibly null) range of the intersection of the convex set
    /// with a range.
    /// Ported from: ClipUtilities.rangeOfConvexClipPlaneSetIntersectionWithRange
    /// (ClipUtils.ts:479-487)
    static Range3d rangeOfConvexClipPlaneSetIntersectionWithRange(ConvexClipPlaneSet const& convexSet,
                                                                  Range3d const& range);

    /// Return the range of various types of clippers intersected with a range
    /// (dispatch per reference).
    /// Ported from: ClipUtilities.rangeOfClipperIntersectionWithRange (ClipUtils.ts:501-537)
    static Range3d rangeOfClipperIntersectionWithRange(ConvexClipPlaneSet const& clipper,
                                                       Range3d const& range);
    static Range3d rangeOfClipperIntersectionWithRange(UnionOfConvexClipPlaneSets const& clipper,
                                                       Range3d const& range);
    static Range3d rangeOfClipperIntersectionWithRange(ClipPrimitive const& clipper, Range3d const& range,
                                                       bool observeInvisibleFlag = true);
    static Range3d rangeOfClipperIntersectionWithRange(ClipVector const& clipper, Range3d const& range,
                                                       bool observeInvisibleFlag = true);

    /// Test if various types of clippers have any intersection with a range.
    /// Ported from: ClipUtilities.doesClipperIntersectRange (ClipUtils.ts:552-585)
    static bool doesClipperIntersectRange(ConvexClipPlaneSet const& clipper, Range3d const& range);
    static bool doesClipperIntersectRange(UnionOfConvexClipPlaneSets const& clipper, Range3d const& range);
    static bool doesClipperIntersectRange(ClipPrimitive const& clipper, Range3d const& range,
                                          bool observeInvisibleFlag = true);
    static bool doesClipperIntersectRange(ClipVector const& clipper, Range3d const& range,
                                          bool observeInvisibleFlag = true);

    /// Early-exit variant of the loop emission for a single convex set.
    /// Ported from: ClipUtilities.doesConvexClipPlaneSetIntersectRange (ClipUtils.ts:597+)
    static bool doesConvexClipPlaneSetIntersectRange(ConvexClipPlaneSet const& convexSet,
                                                     Range3d const& range, bool includeConvexSetFaces = true,
                                                     bool includeRangeFaces = true,
                                                     bool ignoreInvisiblePlanes = false);
};

END_DQ_GEOM_NAMESPACE
