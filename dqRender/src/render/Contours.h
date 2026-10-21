// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contours (per-batch feature→contour-group LUT texture)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Contours.ts
//
// A Contours holds the per-batch lookup table mapping feature index → contour
// group index (4-bit nibble, 2 features per byte, RGBA texel = 8 features).
// The surface vertex shader (glsl/Contours.ts computeContourNdx) samples this
// LUT with the feature index; u_contourLUTWidth==0u is the runtime early-out
// (no LUT bound).
#pragma once

#include "TextureHandle.h"

#include <dqCommon/ContourDisplay.h>
#include <dqCommon/PackedFeatureTable.h>
#include <dqRender/rhi/Handle.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class TargetImpl;
class UniformHandle;

// ---------------------------------------------------------------------------
// Contours — per-batch contour group LUT
// Ported from: itwinjs-core Contours（Contours.ts:27-152）
//
// 参考 `_options: BatchOptions` 成员（:29/:113）在参考侧从未被读（死存管）——
// DanQing 不移植该死成员（DanQing Batch 无 options 载体），登记。
// ---------------------------------------------------------------------------
class Contours {
public:
    // Ported from: itwinjs-core Contours.createFromTarget（:117-119）。
    static std::unique_ptr<Contours> createFromTarget(TargetImpl& target);

    // Ported from: itwinjs-core Contours.initFromMap（:128-131——dispose 旧
    //   LUT 后 _initialize）。
    void initFromMap(dqCommon::PackedFeatureTable const& map);

    // Ported from: itwinjs-core Contours.update（:133-142——
    //   matchesSubCategories 早退；否则采 target.currentContours 重打包）。
    void update(dqCommon::PackedFeatureTable const& map);

    // Ported from: itwinjs-core Contours.matchesTargetAndFeatureCount
    //   （:37-40——target 同一性 + numFeatures 等）。
    bool matchesTargetAndFeatureCount(TargetImpl const& target,
                                      dqCommon::PackedFeatureTable const& map) const noexcept;

    // Ported from: itwinjs-core Contours.bindContourLUTWidth（:144-146——
    //   `uniform.setUniform1ui(this._lutWidth)`；无 LUT 时 _lutWidth=0=
    //   着色器早退门）。
    void bindContourLUTWidth(UniformHandle& uniform) const;

    // LUT GPU 纹理（绘制环绑定面——DanQing EQUIVALENCE：参考在 GraphicUniform
    //   内 bindSampler[Contours.ts:148-151]，DanQing 经绘制环 driver.bindTexture
    //   [s_sensorSampler 同族先例，SceneCompositorImpl 传感器段]；sampler 索引
    //   uniform 仍由 GraphicUniform 写）。
    rhi::TextureHandle getTexture() const noexcept
    {
        return m_lut.isValid() ? m_lut.getRhiHandle() : rhi::TextureHandle{};
    }

    uint32_t getLutWidth() const noexcept { return m_lutWidth; }
    uint32_t getNumFeatures() const noexcept { return m_numFeatures; }

    // CPU 侧 LUT 字节（测试面——FeatureOverrideLUT::getRawData 先例）。
    std::vector<uint8_t> const& getLutData() const noexcept { return m_data; }
    // bindContourLUTWidth 的上传值（getLutWidth 同值——测试对账面）。
    uint32_t getLutWidthForBind() const noexcept { return m_lutWidth; }

private:
    explicit Contours(TargetImpl& target);

    // Ported from: itwinjs-core Contours._initialize（:56-69）。
    void initialize(dqCommon::PackedFeatureTable const& map);
    // Ported from: itwinjs-core Contours._update（:71-76）。
    void updateLut(dqCommon::PackedFeatureTable const& map);
    // Ported from: itwinjs-core Contours.matchesSubCategories（:42-54）。
    bool matchesSubCategories() const;
    // Ported from: itwinjs-core Contours.buildLookupTable（:78-109——
    //   subCatMap[Id64.Uint32Map→uint64 键 EQUIVALENCE：lower|upper<<32 恒等]
    //   + defaultNdx=0xf 起、空 subCategories 组改写 defaultNdx[:85-86]
    //   + 每 RGBA 字节装 2 特征的 4-bit nibble 打包[:93-108]）。
    void buildLookupTable(dqCommon::PackedFeatureTable const& map);

    TargetImpl* m_target;
    // 参考 `_contours`（:30——TS 引用[GC]）；DanQing 值存管（栈态重定位安全——
    //   ContourUniforms.h m_contourDisplay 同注）。
    std::optional<dqCommon::ContourDisplay> m_contours;
    TextureHandle m_lut;
    std::vector<uint8_t> m_data;  // CPU 侧 LUT 字节（参考 Texture2DDataUpdater 自持面）
    uint32_t m_lutWidth = 0;
    uint32_t m_numFeatures = 0;
};

END_DQ_RENDER_NAMESPACE
