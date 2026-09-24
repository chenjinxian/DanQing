// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Display style uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/StyleUniforms.ts
//
// Maintains uniforms associated with the DisplayStyleState: background color
// (RGB + RGBA), monochrome color, white-on-white reversal flag, and the
// background intensity (luminance) used by edge/line rendering.
//
// NOTE: the reference StyleUniforms.update() takes the internal render-webgl
// RenderPlan interface (ColorDef fields). dqRender currently exposes a single
// public SDK RenderPlan (PublicAPI/dqRender/RenderPlan.h) consumed by
// OpenGLRenderTarget and dqApp::Viewport, whose bgColor/monoColor are packed
// 0xTTBBGGRR uint32_t (ColorDef.tbgr-compatible) and whiteOnWhiteReversal is a
// bool. We adapt to that public type until the internal/public RenderPlan
// duplication is reconciled.
#pragma once

#include "ColorInfo.h"
#include "FloatRGBA.h"
#include "UniformHandle.h"
#include "dqRender/RenderPlan.h"

#include <dqCommon/ColorDef.h>

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// StyleUniforms — DisplayStyle uniform state
// Ported from: itwinjs-core StyleUniforms
// ---------------------------------------------------------------------------
class StyleUniforms {
public:
    StyleUniforms() noexcept
    {
        // Reference initializes from ColorDef.white and computes intensity.
        updateBackgroundColor(m_bgColor);
        m_monoRgb.setColorDef(m_monoColor);
    }

    /// Update from a render plan.
    /// Ported from: itwinjs-core StyleUniforms.update()
    void update(RenderPlan const& plan) noexcept
    {
        dqCommon::ColorDef const bg(dqCommon::ColorDef::fromTbgr(plan.backgroundColor));
        dqCommon::ColorDef const mono(dqCommon::ColorDef::fromTbgr(plan.monochromeColor));
        bool const wowIgnore = plan.whiteOnWhiteReversal;

        if (m_bgColor.equals(bg)
            && m_monoColor.equals(mono)
            && m_wowIgnoreBgColor == wowIgnore)
            return;

        m_monoColor = mono;
        m_monoRgb.setColorDef(mono);
        m_wowIgnoreBgColor = wowIgnore;
        updateBackgroundColor(bg);
    }

    /// Change only the background color.
    /// Ported from: itwinjs-core StyleUniforms.changeBackgroundColor()
    void changeBackgroundColor(dqCommon::ColorDef const& bgColor) noexcept
    {
        if (bgColor.equals(m_bgColor))
            return;
        updateBackgroundColor(bgColor);
    }

    // vec4
    // Ported from: itwinjs-core StyleUniforms.bindBackgroundRgba()
    void bindBackgroundRgba(UniformHandle& uniform) const { m_bgRgba.bind(uniform); }

    // vec3
    // Ported from: itwinjs-core StyleUniforms.bindBackgroundRgb()
    void bindBackgroundRgb(UniformHandle& uniform) const { m_bgRgb.bind(uniform); }

    // vec3
    // Ported from: itwinjs-core StyleUniforms.bindMonochromeRgb()
    void bindMonochromeRgb(UniformHandle& uniform) const { m_monoRgb.bind(uniform); }

    // Ported from: itwinjs-core StyleUniforms.backgroundIntensity
    float getBackgroundIntensity() const noexcept { return m_bgIntensity; }

    // Ported from: itwinjs-core StyleUniforms.backgroundTbgr
    uint32_t getBackgroundTbgr() const noexcept { return m_bgColor.getTbgr(); }

    // Ported from: itwinjs-core StyleUniforms.backgroundHexString
    std::string getBackgroundHexString() const { return m_bgColor.toHexString(); }

    // Ported from: itwinjs-core StyleUniforms.backgroundAlpha
    float getBackgroundAlpha() const noexcept { return m_bgRgba.alpha(); }

    // Ported from: itwinjs-core StyleUniforms.backgroundColor
    dqCommon::ColorDef const& getBackgroundColor() const noexcept { return m_bgColor; }

    // Ported from: itwinjs-core StyleUniforms.cloneBackgroundRgba()
    void cloneBackgroundRgba(FloatRgba& out) const noexcept { out = m_bgRgba.clone(); }

    // Ported from: itwinjs-core StyleUniforms.wantWoWReversal
    // (ignoreBackgroundColor || bgRgb.isWhite)
    bool getWantWoWReversal() const noexcept { return m_wowIgnoreBgColor || m_bgRgb.isWhite(); }

    // Ported from: itwinjs-core StyleUniforms.backgroundColorInfo
    ColorInfo getBackgroundColorInfo() const noexcept
    {
        return ColorInfo::fromUniform(m_bgColor.getRgb(),
                                      static_cast<uint8_t>(m_bgRgba.alpha() * 255.0f + 0.5f));
    }

private:
    // Ported from: itwinjs-core StyleUniforms.updateBackgroundColor()
    void updateBackgroundColor(dqCommon::ColorDef const& bgColor) noexcept
    {
        m_bgColor = bgColor;
        m_bgRgba.setColorDef(bgColor);
        m_bgRgb.setColorDef(bgColor);
        m_bgIntensity = m_bgRgb.red() * 0.3f + m_bgRgb.green() * 0.59f + m_bgRgb.blue() * 0.11f;
    }

    dqCommon::ColorDef m_bgColor = dqCommon::ColorDef::white;     // reference default: ColorDef.white
    FloatRgba m_bgRgba;
    FloatRgb m_bgRgb;
    dqCommon::ColorDef m_monoColor = dqCommon::ColorDef::white;   // reference default: ColorDef.white
    FloatRgb m_monoRgb;
    bool m_wowIgnoreBgColor = true;  // reference default: WhiteOnWhiteReversalSettings.fromJSON() -> ignore=false, but bgWhite->want true
    float m_bgIntensity = 0.0f;
};

END_DQ_RENDER_NAMESPACE
