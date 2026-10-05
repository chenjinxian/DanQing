// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ClipVolume implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipVolume.ts
#include "ClipVolume.h"

#include <bit>
#include <cassert>
#include <cstring>

namespace dqRender {
namespace {

// Transform.createZero() 对应物（全零矩阵）——ClipPlanesBuffer._viewMatrix 的
// 初值哨兵（保证首次 getData 必编码，ClipVolume.ts:28）。
dqGeom::Transform createZeroTransform() {
    return dqGeom::Transform(dqGeom::Point3d::From(0.0, 0.0, 0.0), dqGeom::Matrix3d::CreateZero());
}

}  // namespace

// ---------------------------------------------------------------------------
// ClipPlanesBuffer
// ---------------------------------------------------------------------------

ClipPlanesBuffer ClipPlanesBuffer::create(std::vector<dqGeom::UnionOfConvexClipPlaneSets> clips,
                                          uint32_t numRows) {
    assert(numRows > 1);  // at least one plane, plus a union boundary.
    return ClipPlanesBuffer(std::move(clips), numRows);
}

ClipPlanesBuffer::ClipPlanesBuffer(std::vector<dqGeom::UnionOfConvexClipPlaneSets> clips, uint32_t numRows)
    : m_viewMatrix(createZeroTransform()), m_clips(std::move(clips)), m_numRows(numRows) {
    m_data.assign(static_cast<size_t>(numRows) * 4 * 4, 0);
}

std::vector<uint8_t> const& ClipPlanesBuffer::getData(dqGeom::Transform const& viewMatrix) {
    if (!viewMatrix.IsAlmostEqual(m_viewMatrix))
        updateData(viewMatrix);

    return m_data;
}

void ClipPlanesBuffer::appendFloat(float value) {
    // DataView.setFloat32(curPos, value, true) —— 小端 float32。
    static_assert(std::endian::native == std::endian::little, "little-endian assumed (Windows/x86)");
    std::memcpy(m_data.data() + m_curPos, &value, sizeof(float));
    m_curPos += 4;
}

void ClipPlanesBuffer::appendValues(float a, float b, float c, float d) {
    appendFloat(a);
    appendFloat(b);
    appendFloat(c);
    appendFloat(d);
}

void ClipPlanesBuffer::appendPlane(dqGeom::Vector3d const& normal, double distance) {
    appendValues(static_cast<float>(normal.x), static_cast<float>(normal.y),
                 static_cast<float>(normal.z), static_cast<float>(distance));
}

void ClipPlanesBuffer::appendSetBoundary() {
    appendValues(0, 0, 0, 0);
}

void ClipPlanesBuffer::appendUnionBoundary() {
    appendValues(2, 2, 2, 0);
}

void ClipPlanesBuffer::updateData(dqGeom::Transform const& transform) {
    m_curPos = 0;
    m_viewMatrix = transform;

    for (dqGeom::UnionOfConvexClipPlaneSets const& clip : m_clips) {
        for (size_t j = 0; j < clip.convexSets().size(); j++) {
            dqGeom::ConvexClipPlaneSet const& set = clip.convexSets()[j];
            if (0 == set.planes.size())
                continue;

            // NOTE 参考怪癖 1:1：界行判定用凸集索引 j（非已发射计数）——首凸集为
            // 空时，后续凸集前仍会输出 set 边界行（ClipVolume.ts:136）。
            if (j > 0)
                appendSetBoundary();

            for (dqGeom::ClipPlane const& plane : set.planes) {
                dqGeom::Vector3d normal = plane.inwardNormal;
                double distance = plane.getDistanceFromOrigin();

                dqGeom::Vector3d dir = normal;
                transform.matrix.MultiplyVector(dir);
                dir.Normalize();  // normalizeInPlace

                dqGeom::Point3d const pos = transform.MultiplyPoint3d(
                    dqGeom::Point3d::From(normal.x * distance, normal.y * distance, normal.z * distance));

                normal = dir;
                distance = -(pos.x * dir.x + pos.y * dir.y + pos.z * dir.z);  // v0.dotProduct(dir)
                appendPlane(normal, distance);
            }
        }

        appendUnionBoundary();
    }
}

// ---------------------------------------------------------------------------
// ClipVolume
// ---------------------------------------------------------------------------

ClipVolume::Ptr ClipVolume::create(dqGeom::ClipVector const& clip) {
    if (!clip.isValid())
        return nullptr;

    // Compute how many rows of data we need.
    std::vector<dqGeom::UnionOfConvexClipPlaneSets> unions;
    uint32_t numRows = 0;
    for (dqGeom::ClipPrimitive::Ptr const& primitive : clip.clips()) {
        dqGeom::UnionOfConvexClipPlaneSets const* unionPtr = primitive->fetchClipPlanesRef();
        if (unionPtr == nullptr)
            continue;

        uint32_t numSets = 0;
        for (dqGeom::ConvexClipPlaneSet const& set : unionPtr->convexSets()) {
            size_t const setLength = set.planes.size();
            if (setLength > 0) {
                ++numSets;
                numRows += static_cast<uint32_t>(setLength);
            }
        }

        if (numSets > 0) {
            unions.push_back(*unionPtr);
            numRows += numSets - 1;  // Additional boundary rows between sets.
        }
    }

    if (unions.empty())
        return nullptr;

    numRows += static_cast<uint32_t>(unions.size());  // boundary row after each union - *including* the last.
    ClipPlanesBuffer buffer = ClipPlanesBuffer::create(std::move(unions), numRows);
    return Ptr(new ClipVolume(dqGeom::ClipVector::Ptr(const_cast<dqGeom::ClipVector*>(&clip)), std::move(buffer)));
}

ClipVolume::ClipVolume(dqGeom::ClipVector::Ptr clip, ClipPlanesBuffer buffer)
    : m_clipVector(std::move(clip)), m_buffer(std::move(buffer)) {}

}  // namespace dqRender
