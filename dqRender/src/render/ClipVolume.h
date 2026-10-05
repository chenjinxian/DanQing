// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ClipVolume (ClipVector → GPU 纹理行编码)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipVolume.ts
//
// M-P P-C：按参考语义整体重写（原简化数据面[本地 ClipPlane struct + bind 空桩]
// 移除——BatchClipTest 的 Authored 锁随换为参考测试移植，见 tests/）。
// §3.4 适配：Uint8Array/DataView → std::vector<uint8_t> + memcpy(float32 LE——
// Windows/x86 小端)；参考的 appendEncodedFloat（十进制字节编码）为参考源内
// 未被调用的死代码，不移植（登记）。
#pragma once

#include <dqGeom/ClipVector.h>
#include <dqGeom/Transform.h>
#include <dqRender/RenderClipVolume.h>

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

/// Maintains a buffer to serve as texture data for a ClipVector. The clip
/// planes are in view coordinates, so the data must be updated whenever the
/// view matrix changes.
/// Ported from: itwinjs-core ClipPlanesBuffer (ClipVolume.ts:26-159)
class ClipPlanesBuffer {
public:
    /// Create from the unions with a precomputed row count.
    /// Ported from: ClipPlanesBuffer.create (:53-56)
    static ClipPlanesBuffer create(std::vector<dqGeom::UnionOfConvexClipPlaneSets> clips, uint32_t numRows);

    /// The data in view coordinates (viewMatrix changed → re-encoded).
    /// Ported from: ClipPlanesBuffer.getData (:42-47)
    std::vector<uint8_t> const& getData(dqGeom::Transform const& viewMatrix);

    /// Ported from: ClipPlanesBuffer.byteLength (:49-51)
    size_t byteLength() const noexcept { return m_data.size(); }

    /// The number of rows of data. Each row corresponds to a clipping plane,
    /// or to a boundary between two ClipPlaneSets / Unions. The final row is
    /// *always* a union boundary (nested clip volumes concatenate).
    /// Ported from: ClipPlanesBuffer.numRows (:37-40)
    uint32_t numRows() const noexcept { return m_numRows; }

private:
    ClipPlanesBuffer(std::vector<dqGeom::UnionOfConvexClipPlaneSets> clips, uint32_t numRows);

    /// Ported from: ClipPlanesBuffer.updateData (:125-158)
    void updateData(dqGeom::Transform const& transform);

    // Row writers (:67-94)。
    void appendFloat(float value);
    void appendValues(float a, float b, float c, float d);
    void appendPlane(dqGeom::Vector3d const& normal, double distance);
    void appendSetBoundary();
    void appendUnionBoundary();

    dqGeom::Transform m_viewMatrix;  // most recently-applied view matrix (:28)
    std::vector<uint8_t> m_data;     // numRows * 4 * 4 字节
    size_t m_curPos = 0;             // 当前写位置 (:34)
    std::vector<dqGeom::UnionOfConvexClipPlaneSets> m_clips;
    uint32_t m_numRows = 0;
};

/// A ClipVector encoded for transmission to the GPU as a texture.
/// Ported from: itwinjs-core ClipVolume (ClipVolume.ts:164-218)
class ClipVolume final : public RenderClipVolume {
public:
    using Ptr = dqBase::RefPtr<ClipVolume>;

    /// Create from a ClipVector; returns null for empty/invalid/无有效凸集输入.
    /// Ported from: ClipVolume.create (:179-212)
    static Ptr create(dqGeom::ClipVector const& clip);

    /// Ported from: ClipVolume.numRows (:167-169)
    uint32_t numRows() const noexcept { return m_buffer.numRows(); }
    /// Ported from: ClipVolume.byteLength (:171-173)
    size_t byteLength() const noexcept { return m_buffer.byteLength(); }
    /// Ported from: ClipVolume.getData (:175-177)
    std::vector<uint8_t> const& getData(dqGeom::Transform const& viewMatrix) { return m_buffer.getData(viewMatrix); }

    /// Ported from: RenderClipVolume.clipVector
    dqGeom::ClipVector const* clipVector() const noexcept override { return m_clipVector.Get(); }

private:
    ClipVolume(dqGeom::ClipVector::Ptr clip, ClipPlanesBuffer buffer);

    dqGeom::ClipVector::Ptr m_clipVector;
    ClipPlanesBuffer m_buffer;
};

END_DQ_RENDER_NAMESPACE
