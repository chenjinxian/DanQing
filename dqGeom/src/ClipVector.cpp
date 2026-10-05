// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipVector implementation
// Ported from: itwinjs-core core/geometry/src/clipping/ClipVector.ts
#include <dqGeom/ClipVector.h>

#include <charconv>
#include <array>
#include <limits>
#include <string>

namespace dqGeom {
namespace {

// `${num.toString()}_` — JS Number.prototype.toString shortest round-trip form.
// std::to_chars 默认即最短往返表示（"0"/"-5"/"5e-11"）。
std::string formatNumber(double num) {
    char buf[32];
    auto const result = std::to_chars(buf, buf + sizeof(buf), num);
    return std::string(buf, result.ptr) + "_";
}

std::string formatVector3d(Vector3d const& vec) {
    return formatNumber(vec.x) + formatNumber(vec.y) + formatNumber(vec.z);
}

std::string formatFlags(int flags) {
    // assert(1 === f.length) — flags ∈ [0,3] single digit (1:1 reference).
    return std::to_string(flags);
}

std::string formatPlane(ClipPlane const& plane) {
    int flags = plane.invisible ? 1 : 0;
    flags |= (plane.interior ? 2 : 0);
    return formatFlags(flags) + formatVector3d(plane.inwardNormal) + formatNumber(plane.getDistanceFromOrigin());
}

std::string formatPlaneSet(ConvexClipPlaneSet const& set) {
    std::string planes;
    for (ClipPlane const& plane : set.planes)
        planes += formatPlane(plane);
    return planes + "_";
}

std::string formatPrimitive(ClipPrimitive const& prim) {
    int const flags = prim.invisible() ? 1 : 0;
    std::string str = std::to_string(flags);
    UnionOfConvexClipPlaneSets const* unionSets = prim.fetchClipPlanesRef();
    if (unionSets != nullptr) {
        for (ConvexClipPlaneSet const& s : unionSets->convexSets())
            str += formatPlaneSet(s);
    }
    return str + "_";
}

}  // namespace

ClipVector::Ptr ClipVector::createEmpty() { return Ptr(new ClipVector()); }

ClipVector::Ptr ClipVector::createCapture(std::vector<ClipPrimitive::Ptr> clips) {
    return Ptr(new ClipVector(std::move(clips)));
}

ClipVector::Ptr ClipVector::create(std::vector<ClipPrimitive::Ptr> const& clips) {
    std::vector<ClipPrimitive::Ptr> clipClones;
    clipClones.reserve(clips.size());
    for (ClipPrimitive::Ptr const& clip : clips)
        clipClones.push_back(clip->clone());
    return createCapture(std::move(clipClones));
}

ClipVector::Ptr ClipVector::clone() const {
    Ptr const retVal = createEmpty();
    retVal->m_clips.reserve(m_clips.size());
    for (ClipPrimitive::Ptr const& clip : m_clips)
        retVal->m_clips.push_back(clip->clone());
    retVal->boundingRange = boundingRange;
    return retVal;
}

ClipVectorProps ClipVector::toJSON() const {
    if (!isValid())
        return {};
    ClipVectorProps props;
    props.reserve(m_clips.size());
    for (ClipPrimitive::Ptr const& clip : m_clips)
        props.push_back(clip->toJSON());
    return props;
}

ClipVector::Ptr ClipVector::fromJSON(ClipVectorProps const* json) {
    Ptr const result = createEmpty();
    if (json == nullptr)
        return result;
    for (ClipPrimitiveProps const& clip : *json) {
        ClipPrimitive::Ptr const clipPrim = ClipPrimitive::fromJSON(&clip);
        if (clipPrim)
            result->m_clips.push_back(clipPrim);
    }
    return result;
}

bool ClipVector::appendShape(std::vector<Point3d> const& shape, std::optional<double> zLow,
                             std::optional<double> zHigh, Transform const* transform, bool isMask,
                             bool invisible) {
    ClipShape::Ptr const clip = ClipShape::createShape(shape, zLow, zHigh, transform, isMask, invisible);
    if (!clip)
        return false;
    m_clips.push_back(clip);
    return true;
}

bool ClipVector::pointInside(Point3d const& point, double onTolerance) const {
    return isPointOnOrInside(point, onTolerance);
}

bool ClipVector::isPointOnOrInside(Point3d const& point, double onTolerance) const {
    if (!boundingRange.isNull() && !boundingRange.ContainsPoint(point))
        return false;

    for (ClipPrimitive::Ptr const& clip : m_clips) {
        if (!clip->pointInside(point, onTolerance))
            return false;
    }
    return true;
}

bool ClipVector::transformInPlace(Transform const& transform) {
    for (ClipPrimitive::Ptr const& clip : m_clips) {
        if (clip->transformInPlace(transform) == false)
            return false;
    }
    if (!boundingRange.isNull())
        boundingRange = transform.MultiplyRange(boundingRange);

    return true;
}

std::vector<double> ClipVector::extractBoundaryLoops(std::vector<std::vector<Point3d>>& loopPoints,
                                                     Transform* transform) const {
    ClipMaskXYZRangePlanes clipM = ClipMaskXYZRangePlanes::None;
    double zBack = -std::numeric_limits<double>::max();
    double zFront = std::numeric_limits<double>::max();
    std::vector<double> retVal;
    size_t nLoops = 0;

    if (m_clips.empty())
        return retVal;
    ClipShape const* firstClipShape = nullptr;
    Transform fwdTrans = Transform::CreateIdentity();
    Transform invTrans = Transform::CreateIdentity();
    Transform deltaTrans = Transform::CreateIdentity();

    for (ClipPrimitive::Ptr const& clipPtr : m_clips) {
        ClipShape const* clip = clipPtr->asClipShape();  // instanceof ClipShape（§3.4 no-RTTI）
        if (clip != nullptr) {
            if (firstClipShape != nullptr && clip != firstClipShape) {  // not the first iteration
                fwdTrans = Transform::CreateIdentity();
                invTrans = Transform::CreateIdentity();

                if (firstClipShape->transformToClip() != nullptr) {
                    if (clip->transformFromClip() != nullptr) {
                        fwdTrans = *clip->transformFromClip();
                        invTrans = *firstClipShape->transformToClip();
                    }
                }
                deltaTrans = invTrans.MultiplyTransform(fwdTrans);
            }
            if (firstClipShape == nullptr)
                firstClipShape = clip;
            if (loopPoints.size() <= nLoops)
                loopPoints.resize(nLoops + 1);
            loopPoints[nLoops].clear();

            // clip.polygon is always defined for a ClipShape.
            clipM = ClipMaskXYZRangePlanes::XAndY;

            if (clip->hasZHigh()) {
                clipM = clipM | ClipMaskXYZRangePlanes::ZHigh;
                zFront = *clip->zHigh();
            }
            if (clip->hasZLow()) {
                clipM = clipM | ClipMaskXYZRangePlanes::ZLow;
                zBack = *clip->zLow();
            }

            loopPoints[nLoops] = clip->polygon();
            deltaTrans.MultiplyPoint3dArrayInPlace(loopPoints[nLoops]);
            ++nLoops;
        }
    }
    retVal.push_back(static_cast<double>(static_cast<uint32_t>(clipM)));
    retVal.push_back(zBack);
    retVal.push_back(zFront);
    if (transform != nullptr && firstClipShape != nullptr && firstClipShape->transformFromClip() != nullptr)
        *transform = *firstClipShape->transformFromClip();
    return retVal;
}

void ClipVector::setInvisible(bool invisible) {
    for (ClipPrimitive::Ptr const& clip : m_clips)
        clip->setInvisible(invisible);
}

void ClipVector::parseClipPlanes() {
    for (ClipPrimitive::Ptr const& clip : m_clips)
        clip->fetchClipPlanesRef();
}

ClipPlaneContainment ClipVector::classifyPointContainment(std::vector<Point3d> const& points,
                                                          bool ignoreMasks) const {
    ClipPlaneContainment currentContainment = ClipPlaneContainment::Ambiguous;

    for (ClipPrimitive::Ptr const& primitive : m_clips) {
        ClipPlaneContainment const thisContainment = primitive->classifyPointContainment(points, ignoreMasks);

        if (ClipPlaneContainment::Ambiguous == thisContainment)
            return ClipPlaneContainment::Ambiguous;

        if (ClipPlaneContainment::Ambiguous == currentContainment)
            currentContainment = thisContainment;
        else if (currentContainment != thisContainment)
            return ClipPlaneContainment::Ambiguous;
    }
    return currentContainment;
}

ClipPlaneContainment ClipVector::classifyRangeContainment(Range3d const& range, bool ignoreMasks) const {
    std::array<Point3d, 8> const cornerArray = range.Corners();
    std::vector<Point3d> const corners(cornerArray.begin(), cornerArray.end());
    return classifyPointContainment(corners, ignoreMasks);
}

bool ClipVector::isAnyLineStringPointInside(std::vector<Point3d> const& points) const {
    for (ClipPrimitive::Ptr const& clip : m_clips) {
        UnionOfConvexClipPlaneSets const* clipPlaneSet = clip->fetchClipPlanesRef();
        if (clipPlaneSet != nullptr) {
            for (size_t i = 0; i + 1 < points.size(); ++i) {
                if (clipPlaneSet->isAnyPointInOrOnFromSegment(points[i], points[i + 1]))
                    return true;
            }
        }
    }
    return false;
}

double ClipVector::sumSizes(std::vector<Segment1d> const& intervals, size_t begin, size_t end) noexcept {
    double s = 0.0;
    for (size_t i = begin; i < end; ++i)
        s += (intervals[i].x1 - intervals[i].x0);
    return s;
}

bool ClipVector::isLineStringCompletelyContained(std::vector<Point3d> const& points) const {
    // _TARGET_FRACTION_SUM (ClipVector.ts:391)
    constexpr double kTargetFractionSum = 0.99999999;
    std::vector<Segment1d> clipIntervals;

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        double fractionSum = 0.0;
        size_t index0 = 0;

        for (ClipPrimitive::Ptr const& clip : m_clips) {
            UnionOfConvexClipPlaneSets const* clipPlaneSet = clip->fetchClipPlanesRef();
            if (clipPlaneSet != nullptr) {
                clipPlaneSet->appendIntervalsFromSegment(points[i], points[i + 1], clipIntervals);
                size_t const index1 = clipIntervals.size();
                fractionSum += sumSizes(clipIntervals, index0, index1);
                index0 = index1;
                // ASSUME primitives are non-overlapping...
                if (fractionSum >= kTargetFractionSum)
                    break;
            }
        }
        if (fractionSum < kTargetFractionSum)
            return false;
    }
    return true;
}

std::string ClipVector::toCompactString() const {
    std::string result;
    for (ClipPrimitive::Ptr const& primitive : m_clips)
        result += formatPrimitive(*primitive);
    return result + "_";
}

std::optional<StringifiedClipVector> StringifiedClipVector::fromClipVector(ClipVector::Ptr const& clip) {
    if (!clip || !clip->isValid())
        return std::nullopt;

    StringifiedClipVector stringified;
    stringified.clip = clip;
    stringified.clipString = clip->toCompactString();
    return stringified;
}

}  // namespace dqGeom
