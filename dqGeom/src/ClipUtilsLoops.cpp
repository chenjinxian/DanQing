// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipUtilities 剖切轮廓/范围族实现
// Ported from: itwinjs-core core/geometry/src/clipping/ClipUtils.ts
#include <dqGeom/ClipUtilsLoops.h>

#include <algorithm>

namespace dqGeom {

void ClipUtilitiesLoops::announceLoopsOfConvexClipPlaneSetIntersectRange(
    ConvexClipPlaneSet const& convexSet, Range3d const& range, ClipLoopFunction const& loopFunction,
    bool includeConvexSetFaces, bool includeRangeFaces, bool ignoreInvisiblePlanes) {
    std::vector<Point3d> work;
    if (includeConvexSetFaces) {
        // Clip convexSet planes to the range and to the rest of the convexSet . .
        for (ClipPlane const& plane : convexSet.planes) {
            if (ignoreInvisiblePlanes && plane.invisible)
                continue;
            std::optional<std::vector<Point3d>> const pointsClippedToRange = plane.intersectRange(range, true);
            if (pointsClippedToRange.has_value()) {
                std::vector<Point3d> finalPoints;
                convexSet.polygonClip(*pointsClippedToRange, finalPoints, work, &plane);
                if (!finalPoints.empty())
                    loopFunction(finalPoints);
            }
        }
    }

    if (includeRangeFaces) {
        // clip range faces to the convex set . . .
        std::array<Point3d, 8> const corners = range.Corners();
        for (size_t i = 0; i < 6; ++i) {
            std::array<size_t, 4> const indices = Range3d::FaceCornerIndices(i);
            std::vector<Point3d> const lineString = {corners[indices[0]], corners[indices[1]],
                                                     corners[indices[2]], corners[indices[3]]};
            std::vector<Point3d> finalPoints;
            convexSet.polygonClip(lineString, finalPoints, work);
            if (!finalPoints.empty())
                loopFunction(finalPoints);
        }
    }
}

void ClipUtilitiesLoops::announceLoopsOfConvexClipPlaneSetIntersectRange(
    ClipPlane const& plane, Range3d const& range, ClipLoopFunction const& loopFunction,
    bool includeConvexSetFaces, bool includeRangeFaces, bool ignoreInvisiblePlanes) {
    std::vector<Point3d> work;
    if (includeConvexSetFaces) {
        // `convexSet` is just one plane ...
        if (ignoreInvisiblePlanes && plane.invisible) {
            // skip it !
        } else {
            std::optional<std::vector<Point3d>> const pointsClippedToRange = plane.intersectRange(range, true);
            if (pointsClippedToRange.has_value())
                loopFunction(*pointsClippedToRange);
        }
    }

    if (includeRangeFaces) {
        std::array<Point3d, 8> const corners = range.Corners();
        for (size_t i = 0; i < 6; ++i) {
            std::array<size_t, 4> const indices = Range3d::FaceCornerIndices(i);
            std::vector<Point3d> lineString = {corners[indices[0]], corners[indices[1]],
                                               corners[indices[2]], corners[indices[3]]};
            // §3.4 适配：单面分支的 ClipPlane.clipConvexPolygonInPlace → 单面集合的
            // clipConvexPolygonInPlace（同一数学）。
            ConvexClipPlaneSet const singlePlaneSet = ConvexClipPlaneSet::createPlanes({plane});
            singlePlaneSet.clipConvexPolygonInPlace(lineString, work);
            if (!lineString.empty())
                loopFunction(lineString);
        }
    }
}

std::vector<dqBase::RefPtr<Loop>> ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(
    UnionOfConvexClipPlaneSets const& allClippers, Range3d const& range, bool includeConvexSetFaces,
    bool includeRangeFaces, bool ignoreInvisiblePlanes) {
    std::vector<dqBase::RefPtr<Loop>> result;
    for (ConvexClipPlaneSet const& clipper : allClippers.convexSets()) {
        announceLoopsOfConvexClipPlaneSetIntersectRange(
            clipper, range,
            [&](std::vector<Point3d> const& points) {
                if (!points.empty())
                    result.push_back(Loop::CreatePolygon(points));
            },
            includeConvexSetFaces, includeRangeFaces, ignoreInvisiblePlanes);
    }
    return result;
}

std::vector<dqBase::RefPtr<Loop>> ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(
    ConvexClipPlaneSet const& allClippers, Range3d const& range, bool includeConvexSetFaces,
    bool includeRangeFaces, bool ignoreInvisiblePlanes) {
    std::vector<dqBase::RefPtr<Loop>> result;
    announceLoopsOfConvexClipPlaneSetIntersectRange(
        allClippers, range,
        [&](std::vector<Point3d> const& points) {
            if (!points.empty())
                result.push_back(Loop::CreatePolygon(points));
        },
        includeConvexSetFaces, includeRangeFaces, ignoreInvisiblePlanes);
    return result;
}

std::vector<dqBase::RefPtr<Loop>> ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(
    ClipPlane const& allClippers, Range3d const& range, bool includeConvexSetFaces, bool includeRangeFaces,
    bool ignoreInvisiblePlanes) {
    std::vector<dqBase::RefPtr<Loop>> result;
    announceLoopsOfConvexClipPlaneSetIntersectRange(
        allClippers, range,
        [&](std::vector<Point3d> const& points) {
            if (!points.empty())
                result.push_back(Loop::CreatePolygon(points));
        },
        includeConvexSetFaces, includeRangeFaces, ignoreInvisiblePlanes);
    return result;
}

Range3d ClipUtilitiesLoops::rangeOfConvexClipPlaneSetIntersectionWithRange(ConvexClipPlaneSet const& convexSet,
                                                                           Range3d const& range) {
    Range3d result = Range3d::CreateNull();
    announceLoopsOfConvexClipPlaneSetIntersectRange(
        convexSet, range,
        [&](std::vector<Point3d> const& points) {
            for (Point3d const& p : points)
                result.ExtendPoint(p);
        },
        true, true, false);
    return result;
}

Range3d ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(ConvexClipPlaneSet const& clipper,
                                                                Range3d const& range) {
    return rangeOfConvexClipPlaneSetIntersectionWithRange(clipper, range);
}

Range3d ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(UnionOfConvexClipPlaneSets const& clipper,
                                                                Range3d const& range) {
    Range3d rangeUnion = Range3d::CreateNull();
    for (ConvexClipPlaneSet const& c : clipper.convexSets()) {
        Range3d const rangeC = rangeOfConvexClipPlaneSetIntersectionWithRange(c, range);
        rangeUnion.ExtendRange(rangeC);
    }
    return rangeUnion;
}

Range3d ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(ClipPrimitive const& clipper,
                                                                Range3d const& range, bool observeInvisibleFlag) {
    if (observeInvisibleFlag && clipper.invisible())
        return range;
    UnionOfConvexClipPlaneSets const* planes = clipper.fetchClipPlanesRef();
    if (planes == nullptr)
        return range;  // undefined clipper → the input range (1:1 reference)
    return rangeOfClipperIntersectionWithRange(*planes, range);
}

Range3d ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(ClipVector const& clipper, Range3d const& range,
                                                                bool observeInvisibleFlag) {
    Range3d rangeIntersection = range;
    for (ClipPrimitive::Ptr const& c : clipper.clips()) {
        if (observeInvisibleFlag && c->invisible()) {
            // trivial range tests do not expose the effects. Assume the hole allows everything.
        } else {
            Range3d const rangeC = rangeOfClipperIntersectionWithRange(*c, range, observeInvisibleFlag);
            rangeIntersection = rangeIntersection.Intersect(rangeC);
        }
    }
    return rangeIntersection;
}

bool ClipUtilitiesLoops::doesClipperIntersectRange(ConvexClipPlaneSet const& clipper, Range3d const& range) {
    return doesConvexClipPlaneSetIntersectRange(clipper, range);
}

bool ClipUtilitiesLoops::doesClipperIntersectRange(UnionOfConvexClipPlaneSets const& clipper,
                                                   Range3d const& range) {
    for (ConvexClipPlaneSet const& c : clipper.convexSets()) {
        if (doesConvexClipPlaneSetIntersectRange(c, range))
            return true;
    }
    return false;
}

bool ClipUtilitiesLoops::doesClipperIntersectRange(ClipPrimitive const& clipper, Range3d const& range,
                                                   bool observeInvisibleFlag) {
    if (observeInvisibleFlag && clipper.invisible())
        return true;
    UnionOfConvexClipPlaneSets const* planes = clipper.fetchClipPlanesRef();
    if (planes == nullptr)
        return true;  // undefined clipper → non-null range intersects (1:1)
    return doesClipperIntersectRange(*planes, range);
}

bool ClipUtilitiesLoops::doesClipperIntersectRange(ClipVector const& clipper, Range3d const& range,
                                                   bool observeInvisibleFlag) {
    Range3d rangeIntersection = range;
    for (ClipPrimitive::Ptr const& c : clipper.clips()) {
        if (observeInvisibleFlag && c->invisible()) {
            // trivial range tests do not expose the effects. Assume the hole allows everything.
        } else {
            Range3d const rangeC = rangeOfClipperIntersectionWithRange(*c, range, observeInvisibleFlag);
            rangeIntersection = rangeIntersection.Intersect(rangeC);
        }
    }
    return !rangeIntersection.isNull();
}

bool ClipUtilitiesLoops::doesConvexClipPlaneSetIntersectRange(ConvexClipPlaneSet const& convexSet,
                                                              Range3d const& range, bool includeConvexSetFaces,
                                                              bool includeRangeFaces, bool ignoreInvisiblePlanes) {
    std::vector<Point3d> work;
    if (includeConvexSetFaces) {
        // Clip convexSet planes to the range and to the rest of the convexSet . .
        for (ClipPlane const& plane : convexSet.planes) {
            if (ignoreInvisiblePlanes && plane.invisible)
                continue;
            std::optional<std::vector<Point3d>> const pointsClippedToRange = plane.intersectRange(range, true);
            if (pointsClippedToRange.has_value()) {
                std::vector<Point3d> finalPoints;
                convexSet.polygonClip(*pointsClippedToRange, finalPoints, work, &plane);
                if (!finalPoints.empty())
                    return true;
            }
        }
    }

    if (includeRangeFaces) {
        std::array<Point3d, 8> const corners = range.Corners();
        for (size_t i = 0; i < 6; ++i) {
            std::array<size_t, 4> const indices = Range3d::FaceCornerIndices(i);
            std::vector<Point3d> const lineString = {corners[indices[0]], corners[indices[1]],
                                                     corners[indices[2]], corners[indices[3]]};
            std::vector<Point3d> finalPoints;
            convexSet.polygonClip(lineString, finalPoints, work);
            if (!finalPoints.empty())
                return true;
        }
    }
    return false;
}

}  // namespace dqGeom
