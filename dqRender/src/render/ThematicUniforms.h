// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicUniforms.ts
//
// Maintains state for uniforms related to thematic display (height maps, slopes, etc.).
//
// M-S S-c 归位：update(target) 参考全语义（ThematicUniforms.ts:90-152——快路径
// 视图相关刷新/clear 路径/渐变纹理重建）；渐变纹理经 GradientSymb.
// getThematicImageForRenderer（1×N 列向 RGBA8，Gradient.ts:307-359）+ NEAREST/
// ClampToEdge（Texture.ts:293-309 ThematicGradient 形态）。
#pragma once

#include "UniformHandle.h"
#include "FloatRGBA.h"
#include "TextureHandle.h"
#include "ThematicSensors.h"
#include "Sync.h"

#include <dqCommon/ThematicDisplay.h>
#include <dqCommon/Gradient.h>
#include <dqCommon/Image.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class TargetImpl;

/// Default gradient dimension for thematic textures.
/// Ported from: itwinjs-core ThematicUniforms._getGradientDimension()
///（:209-213——min(8192, maxTextureSize)；DanQing 的 maxTextureSize 接线待
/// 落地（TD-23 同族登记），恒 8192）。
constexpr int kDefaultGradientDimension = 8192;

// ---------------------------------------------------------------------------
// ThematicUniforms — thematic display uniform handler
// Ported from: itwinjs-core ThematicUniforms
//
// EQUIVALENCE（§11.10）：参考 bind* 以 sync(this, uniform) 做逐程序上传去重
//（UniformHandle.syncToken 面）；DanQing UniformHandle 无 syncToken 且 set*
// 自带 dirty 检查直派 GL（TD-15）——bind* 恒上传（幂等写；去重为纯性能
// 优化，语义无差）。验证法：ThematicUniformsTest 经 UniformHandle::getData
// 读回值断言。
// ---------------------------------------------------------------------------
class ThematicUniforms : public SyncTarget {
public:
    ThematicUniforms() = default;

    /// Get the current thematic display settings (nullptr = 关闭/未设置).
    /// Ported from: itwinjs-core ThematicUniforms.thematicDisplay getter
    dqCommon::ThematicDisplay const* getThematicDisplay() const noexcept
    {
        return m_thematicDisplay ? &m_thematicDisplay.value() : nullptr;
    }

    /// Check if iso lines are wanted.
    /// Ported from: itwinjs-core ThematicUniforms.wantIsoLines（:49-53）
    bool wantIsoLines() const noexcept
    {
        return m_thematicDisplay &&
               m_thematicDisplay->displayMode == dqCommon::ThematicDisplayMode::Height &&
               m_thematicDisplay->gradientSettings.mode == dqCommon::ThematicGradientMode::IsoLines;
    }

    /// Check if slope mode is wanted.
    /// Ported from: itwinjs-core ThematicUniforms.wantSlopeMode（:55-57）
    bool wantSlopeMode() const noexcept
    {
        return m_thematicDisplay &&
               m_thematicDisplay->displayMode == dqCommon::ThematicDisplayMode::Slope;
    }

    /// Check if hill shade mode is wanted.
    /// Ported from: itwinjs-core ThematicUniforms.wantHillShadeMode（:59-61）
    bool wantHillShadeMode() const noexcept
    {
        return m_thematicDisplay &&
               m_thematicDisplay->displayMode == dqCommon::ThematicDisplayMode::HillShade;
    }

    /// Whether the global (shared) sensor texture is used (no distance cutoff).
    /// Ported from: itwinjs-core ThematicUniforms.wantGlobalSensorTexture（:63-65）
    bool wantGlobalSensorTexture() const noexcept { return !(m_fragSettings[1] > 0.0f); }

    /// Number of sensors（参考 :37 `_numSensors`）。
    int getNumSensors() const noexcept { return m_numSensors; }

    /// Update thematic uniforms from the target's render plan.
    /// Ported from: itwinjs-core ThematicUniforms.update(target)（:90-152）。
    /// 实现入 .cpp（TargetImpl 完全体所需——头文件循环包含规避）。
    void update(TargetImpl& target);

    /// Bind range uniform (vec2).
    /// Ported from: itwinjs-core ThematicUniforms.bindRange()
    void bindRange(UniformHandle& uniform) const
    {
        uniform.setUniform2fv(m_range.data());
    }

    /// Bind axis uniform (vec3).
    /// Ported from: itwinjs-core ThematicUniforms.bindAxis()
    void bindAxis(UniformHandle& uniform) const
    {
        uniform.setUniform3fv(m_axis.data());
    }

    /// Bind sun direction uniform (vec3).
    /// Ported from: itwinjs-core ThematicUniforms.bindSunDirection()
    void bindSunDirection(UniformHandle& uniform) const
    {
        uniform.setUniform3fv(m_sunDirection.data());
    }

    /// Bind margin color uniform (vec4).
    /// Ported from: itwinjs-core ThematicUniforms.bindMarginColor()
    void bindMarginColor(UniformHandle& uniform) const
    {
        float v[4] = {m_marginColor.r, m_marginColor.g, m_marginColor.b, m_marginColor.a};
        uniform.setUniform4fv(v);
    }

    /// Bind display mode uniform (float).
    /// Ported from: itwinjs-core ThematicUniforms.bindDisplayMode()
    void bindDisplayMode(UniformHandle& uniform) const
    {
        uniform.setUniform1fv(m_displayMode.data());
    }

    /// Bind fragment settings uniform (vec4 — gradientMode, distanceCutoff,
    /// stepCount, multiplyGradientAlpha).
    /// Ported from: itwinjs-core ThematicUniforms.bindFragSettings()
    void bindFragSettings(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_fragSettings.data());
    }

    /// Bind the number of sensors.
    /// Ported from: itwinjs-core ThematicUniforms.bindNumSensors()
    void bindNumSensors(UniformHandle& uniform) const
    {
        uniform.setUniform1i(m_numSensors);
    }

    /// Check if disposed（参考 :199-201——纹理与传感器均释放）。
    bool isDisposed() const noexcept { return !m_texture.isValid() && !m_sensors.has_value(); }

    /// Dispose — release GPU resources（参考 :203-206）。
    void dispose() {
        m_texture = {};
        m_sensors.reset();
    }

    /// Bind gradient texture to a texture unit（GL 级绑定——S-d 的着色器
    /// s_texture 注册面消费；参考 bindTexture :184-187 的 sampler 绑单位移）。
    void bindGradientTexture(rhi::Driver& driver, uint32_t unit) const {
        if (m_texture.isValid()) {
            driver.bindTexture(unit, m_texture.getRhiHandle());
        }
    }

    /// Get the gradient texture handle.
    rhi::TextureHandle getGradientTexture() const noexcept { return m_texture.getRhiHandle(); }

private:
    /// Ported from: itwinjs-core ThematicUniforms._updateAxis（:73-79——
    /// 视矩阵旋转向量[可选] + 恒 normalize）。
    void updateAxis(dqGeom::Vector3d const& axis, dqGeom::Transform const* viewMatrix);

    /// Ported from: itwinjs-core ThematicUniforms._updateSunDirection（:81-88——
    /// 视矩阵变换 + negate + normalize）。
    void updateSunDirection(dqGeom::Vector3d const& sunDir, dqGeom::Transform const& viewMatrix);

    /// Create gradient texture from thematic display settings.
    /// Ported from: itwinjs-core ThematicUniforms.update() :149-151——
    /// Gradient.Symb.createThematic + getThematicImageForRenderer +
    /// createForImageBuffer(ThematicGradient 型：NEAREST/ClampToEdge/无 mipmap)。
    void createGradientTexture(rhi::Driver& driver);

    std::optional<dqCommon::ThematicDisplay> m_thematicDisplay;

    // CPU state
    std::array<float, 2> m_range{};
    float m_colorMix = 0.0f;
    std::array<float, 3> m_axis{};
    std::array<float, 3> m_sunDirection{};
    FloatRgba m_marginColor;
    std::array<float, 1> m_displayMode{};
    std::array<float, 4> m_fragSettings{};

    // GPU resources
    TextureHandle m_texture;
    int m_numSensors = 0;
    int m_gradientDimension = kDefaultGradientDimension;

    // 全局共享传感器纹理的 CPU 数据面（GPU 上传随 S-e 接线——
    // ThematicSensors GPU 化前 update 的 wantThematicSensors 臂仅持 CPU 态）。
    std::optional<ThematicSensors> m_sensors;
};

END_DQ_RENDER_NAMESPACE
