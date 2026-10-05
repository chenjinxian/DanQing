// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ClipStack implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipStack.ts
#include "ClipStack.h"

#include "rhi/opengl/GlLoader.h"  // [CLIPDUMP] 探针（DANQING_CLIP_TRACE）

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dqRender {

// ---------------------------------------------------------------------------
// ClipStackFloatRgba
// ---------------------------------------------------------------------------

// Ported from: FloatRgba.setTbgr (FloatRGBA.ts:46-53)
void ClipStackFloatRgba::setTbgr(uint32_t tbgr) {
    // maskTbgr：tbgr 的 t 段为透明度（0xff<<24）；分量 c = (tbgr >> shift) & 0xff。
    uint32_t const cR = tbgr & 0xff;
    uint32_t const cG = (tbgr >> 8) & 0xff;
    uint32_t const cB = (tbgr >> 16) & 0xff;
    uint32_t const cT = (tbgr >> 24) & 0xff;
    r = cR / 255.0f;
    g = cG / 255.0f;
    b = cB / 255.0f;
    a = 1.0f - cT / 255.0f;
}

// Ported from: FloatRgba.setRgbColor (FloatRGBA.ts:42-44)
void ClipStackFloatRgba::setRgbColor(dqCommon::RgbColor const& rgb) {
    setTbgr(static_cast<uint32_t>(rgb.r | (rgb.g << 8) | (rgb.b << 16)));
}

// ---------------------------------------------------------------------------
// ClipStack
// ---------------------------------------------------------------------------

ClipStack::ClipStack(std::function<dqGeom::Transform const&()> getTransform,
                     std::function<bool()> wantViewClip)
    : m_getTransform(std::move(getTransform)), m_wantViewClip(std::move(wantViewClip)) {
    m_stack.push_back(ClipVolume::Ptr{});  // 栈底 = emptyClip 哨兵（§3.4 空 Ptr）
    // _prevTransform = Transform.createZero()（ClipStack.ts:67——首调必脏哨兵）。
    m_prevTransform = dqGeom::Transform(dqGeom::Point3d::From(0.0, 0.0, 0.0),
                                        dqGeom::Matrix3d::CreateZero());
}

size_t ClipStack::bytesUsed() const noexcept {
    return m_texture ? static_cast<size_t>(m_numTotalRows) * 4 * 4 : 0;
}

void ClipStack::setViewClip(dqGeom::ClipVector const* clip, dqCommon::ClipStyle const& style) {
    assert(m_stack.size() == 1);

    updateColor(style.insideColor.has_value() ? &*style.insideColor : nullptr, m_insideColor);
    updateColor(style.outsideColor.has_value() ? &*style.outsideColor : nullptr, m_outsideColor);
    updateIntersectionStyle(style.colorizeIntersection ? &style.colorizeIntersection : nullptr,
                            style.intersectionStyle.has_value() ? &*style.intersectionStyle : nullptr);

    dqGeom::Transform const& transform = m_getTransform();
    if (!transform.IsAlmostEqual(m_prevTransform)) {
        m_prevTransform = transform;
        m_isStackDirty = true;
    }

    ClipVolume::Ptr const& cur = m_stack[0];
    if (cur.IsNull()) {
        if (clip == nullptr)
            return;  // no change.
    } else if (clip == nullptr) {
        m_stack[0] = ClipVolume::Ptr{};
        m_numRowsInUse = 0;
        m_isStackDirty = true;
        return;
    } else {
        if (cur->clipVector() == clip) {
            // We assume that the active view's ClipVector is never mutated in
            // place, so if we are given the same ClipVector, we expect our
            // RenderClipVolume to match it.
            return;
        }
    }

    // ClipVector has changed.
    // §3.4 适配：参考经 IModelApp.renderSystem.createClipVolume（全局系统门面）；
    // DanQing 单后端直调 ClipVolume::create（同一工厂语义）。
    ClipVolume::Ptr const newClip = clip ? ClipVolume::create(*clip) : ClipVolume::Ptr{};
    if (newClip.IsNull()) {
        m_isStackDirty = m_stack[0].IsValid();
        m_stack[0] = ClipVolume::Ptr{};
        m_numRowsInUse = 0;
    } else {
        pop();
        push(newClip);
    }
}

void ClipStack::push(ClipVolume::Ptr clip) {
    assert(clip.IsValid());

    m_stack.push_back(std::move(clip));
    m_numRowsInUse += m_stack.back()->numRows();
    m_numTotalRows = std::max(m_numRowsInUse, m_numTotalRows);
    m_isStackDirty = true;
}

void ClipStack::pop() {
    assert(!m_stack.empty());
    ClipVolume::Ptr const clip = std::move(m_stack.back());
    m_stack.pop_back();
    m_numRowsInUse -= (clip.IsValid() ? clip->numRows() : 0);
}

rhi::TextureHandle ClipStack::texture() {
    updateTexture();
    return m_texture;
}

bool ClipStack::isRangeClipped(dqGeom::Range3d range, dqGeom::Transform const& transform) {
    if (hasOutsideColor() || !hasClip())
        return false;

    range = transform.MultiplyRange(range);
    std::array<dqGeom::Point3d, 8> const cornerArray = range.Corners();
    std::vector<dqGeom::Point3d> const corners(cornerArray.begin(), cornerArray.end());
    size_t const startIndex = (m_wantViewClip() && m_stack[0].IsValid()) ? 0 : 1;
    for (size_t i = startIndex; i < m_stack.size(); ++i) {
        ClipVolume::Ptr const& clip = m_stack[i];
        assert(clip.IsValid());
        if (clip.IsValid() && dqGeom::ClipPlaneContainment::StronglyOutside
                == clip->clipVector()->classifyPointContainment(corners))
            return true;
    }

    return false;
}

void ClipStack::updateTexture() {
    if (m_numTotalRows > 0 && (!m_textureAllocated || m_textureHeight < m_numTotalRows)) {
        // We need to resize the texture.
        assert(m_isStackDirty);
        m_isStackDirty = true;
        if (m_driver && m_texture)
            m_driver->destroyTexture(m_texture);
        m_texture.clear();
        m_textureAllocated = false;  // dispose(this._texture)——重建走 create 分支
        m_cpuBuffer.assign(static_cast<size_t>(m_numTotalRows) * 4 * 4, 0);
    }

    if (m_isStackDirty) {
        m_isStackDirty = false;
        recomputeTexture();
    }
}

void ClipStack::recomputeTexture() {
    // Copy each clip's data to the buffer, recording whether the buffer's
    // contents actually changed.
    bool bufferDirty = false;
    dqGeom::Transform const& transform = m_getTransform();
    size_t bufferIndex = 0;
    for (ClipVolume::Ptr const& clip : m_stack) {
        if (!clip)
            continue;
        std::vector<uint8_t> const& data = clip->getData(transform);
        for (size_t i = 0; i < data.size(); i++) {
            uint8_t const byte = data[i];
            bufferDirty = bufferDirty || byte != m_cpuBuffer[bufferIndex];
            m_cpuBuffer[bufferIndex++] = byte;
        }
    }

    // If the contents have changed, upload the new texture data to the GPU.
    if (bufferDirty) {
        uploadTexture();
    }
}

void ClipStack::uploadTexture() {
    m_lastUploadCreated = !m_textureAllocated;
    if (m_driver == nullptr) {
        // headless（无 GL）：跳过创建/上传；记账（generation/created/allocated）
        // 仍有效，测试以计数断言（见测试头注）。
        if (m_lastUploadCreated) {
            ++m_textureGeneration;
            m_textureAllocated = true;
        }
        m_textureHeight = m_numTotalRows;
        return;
    }

    if (!m_texture) {
        ++m_textureGeneration;
        m_textureAllocated = true;
        m_texture = m_driver->createTexture(
            rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA32F, 1, m_numTotalRows, 1,
            rhi::TextureUsage::DEFAULT);
        if (m_texture)
            m_driver->setTextureFilters(m_texture, 0x2600, 0x2600);  // NEAREST（数据纹理——见 Driver.h 注）
    }
    if (m_texture) {
        // 客户端格式 = GL_RGBA/GL_FLOAT（RGBA32F 行数据——参考 createForData
        // 的 Float32Array 语义；Driver 侧按 descriptor type 派发）。
        rhi::PixelBufferDescriptor pbd(
            m_cpuBuffer.data(), m_cpuBuffer.size(),
            0x1908 /* GL_RGBA */, 0x1406 /* GL_FLOAT */,
            0, 1, 0, 0, 1, m_numTotalRows, 1);
        m_driver->setTextureData(m_texture, 0, 0, 0, 0, 1, m_numTotalRows, 1, std::move(pbd));
        // [CLIPDUMP] 探针（DANQING_CLIP_TRACE=1，§13.1 族）：上传后回读前两行
        // 浮点 + 帧参数——与 CPU 编码值对拍（M-P P-D 剖切纹理上传链取证所加；
        // RGBA32F 映射 saga 的定音证据源）。
        if (std::getenv("DANQING_CLIP_TRACE")) {
            static int s_nClipDump = 0;
            if (s_nClipDump++ < 8) {
                // 绑到 unit 0 回读（不动既有单元占用——探针路径仅取证用）。
                m_driver->bindTexture(0, m_texture);
                float rows[8] = {-9, -9, -9, -9, -9, -9, -9, -9};
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, rows);
                float cpuRows[8] = {0};
                std::memcpy(cpuRows, m_cpuBuffer.data(),
                            std::min(m_cpuBuffer.size(), sizeof(cpuRows)));
                printf("[CLIPDUMP] rows=%u start=%u end=%u gpuR0=(%g,%g,%g,%g) gpuR1=(%g,%g,%g,%g) cpuR0=(%g,%g,%g,%g) cpuR1=(%g,%g,%g,%g)\n",
                       m_numTotalRows, startIndex(), endIndex(),
                       rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7],
                       cpuRows[0], cpuRows[1], cpuRows[2], cpuRows[3],
                       cpuRows[4], cpuRows[5], cpuRows[6], cpuRows[7]);
                fflush(stdout);
            }
        }
    }
    m_textureHeight = m_numTotalRows;
}

// Ported from: ClipStack.updateColor (:268-272)
void ClipStack::updateColor(dqCommon::RgbColor const* rgb, ClipStackFloatRgba& rgba) {
    rgba.a = (rgb != nullptr) ? 1.0f : 0.0f;
    if (rgb != nullptr)
        rgba.setRgbColor(*rgb);
}

// Ported from: ClipStack.updateIntersectionStyle (:274-283)
void ClipStack::updateIntersectionStyle(bool const* colorizeIntersection,
                                        dqCommon::ClipIntersectionStyle const* style) {
    m_colorizeIntersection = (colorizeIntersection != nullptr && *colorizeIntersection);

    if (style != nullptr) {
        // dqCommon 值承载（color/width 非 optional，默认黑/2.0）——参考的
        // undefined 分支等价于置默认。
        m_intersectionStyle.setRgbColor(style->color);
        m_intersectionStyle.a = static_cast<float>(style->width);
    }
}

}  // namespace dqRender
