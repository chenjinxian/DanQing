// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Hilite/emphasis uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/HiliteUniforms.ts
//
// Maintains uniform state for hilite (selection) and emphasis display.
// Packs colors and ratios into two mat3 uniforms plus a vec2 of silhouette
// widths, matching the reference layout exactly.
#pragma once

#include "FloatRGBA.h"
#include "Matrix.h"
#include "UniformHandle.h"

#include <dqCommon/Hilite.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// HiliteUniforms — hilite/emphasis uniform state
// Ported from: itwinjs-core HiliteUniforms
//
// Two mat3 uniforms encode the hilite and emphasis colors plus per-mode ratios:
//
//   compositeSettings (mat3):
//     row 0: hilite.red    hilite.green  hilite.blue
//     row 1: emph.red      emph.green    emph.blue
//     row 2: hilite.hidden emph.hidden   unused
//
//   featureSettings (mat3):
//     row 0: hilite.red    hilite.green  hilite.blue
//     row 1: emph.red      emph.green    emph.blue
//     row 2: hilite.visible emph.visible unused
//
//   compositeWidths (vec2): hilite.silhouette, emphasis.silhouette
// ---------------------------------------------------------------------------
class HiliteUniforms {
public:
    HiliteUniforms() noexcept
    {
        // Reference: _hiliteRgb = FloatRgb.fromColorDef(hiliteSettings.color)
        m_hiliteRgb.setColorDef(m_hiliteSettings.color);
    }

    dqCommon::HiliteSettings const& getHiliteSettings() const noexcept { return m_hiliteSettings; }
    dqCommon::HiliteSettings const& getEmphasisSettings() const noexcept { return m_emphasisSettings; }
    FloatRgb const& getHiliteColor() const noexcept { return m_hiliteRgb; }

    Matrix3 const& getCompositeSettings() const noexcept { return m_composite; }
    Matrix3 const& getFeatureSettings() const noexcept { return m_feature; }

    /// Update from new hilite/emphasis settings.
    /// Ported from: itwinjs-core HiliteUniforms.update()
    void update(dqCommon::HiliteSettings const& hilite,
                dqCommon::HiliteSettings const& emphasis) noexcept
    {
        // Ported from: itwinjs-core Hilite.equalSettings(...)
        if (hilite.equals(m_hiliteSettings) && emphasis.equals(m_emphasisSettings))
            return;

        m_hiliteSettings = hilite;
        m_emphasisSettings = emphasis;

        FloatRgb rgb;

        // Emphasis color -> row 1 (indices 3,4,5).
        // NB: reference sets emphasis first, then hilite last (hilite is exposed via getter).
        rgb.setColorDef(emphasis.color);
        m_composite.data[3] = m_feature.data[3] = rgb.red();
        m_composite.data[4] = m_feature.data[4] = rgb.green();
        m_composite.data[5] = m_feature.data[5] = rgb.blue();

        // Hilite color -> row 0 (indices 0,1,2). Set last.
        rgb.setColorDef(hilite.color);
        m_composite.data[0] = m_feature.data[0] = rgb.red();
        m_composite.data[1] = m_feature.data[1] = rgb.green();
        m_composite.data[2] = m_feature.data[2] = rgb.blue();
        m_hiliteRgb = rgb;

        // Row 2 ratios.
        m_composite.data[6] = static_cast<float>(hilite.hiddenRatio);
        m_composite.data[7] = static_cast<float>(emphasis.hiddenRatio);

        m_feature.data[6] = static_cast<float>(hilite.visibleRatio);
        m_feature.data[7] = static_cast<float>(emphasis.visibleRatio);

        // Silhouette widths (enum value as float).
        m_compositeWidths[0] = silhouetteToFloat(hilite.silhouette);
        m_compositeWidths[1] = silhouetteToFloat(emphasis.silhouette);
    }

    // mat3 composite settings.
    // Ported from: itwinjs-core HiliteUniforms.bindCompositeSettings()
    void bindCompositeSettings(UniformHandle& uniform) const
    {
        uniform.setMatrix3(m_composite.data);
    }

    // vec2 silhouette widths.
    // Ported from: itwinjs-core HiliteUniforms.bindCompositeWidths()
    void bindCompositeWidths(UniformHandle& uniform) const
    {
        uniform.setUniform2fv(m_compositeWidths);
    }

    // mat3 feature settings.
    // Ported from: itwinjs-core HiliteUniforms.bindFeatureSettings()
    void bindFeatureSettings(UniformHandle& uniform) const
    {
        uniform.setMatrix3(m_feature.data);
    }

private:
    // HiliteSilhouette enum -> float (matches reference numeric enum storage).
    static float silhouetteToFloat(dqCommon::HiliteSilhouette s) noexcept
    {
        return static_cast<float>(static_cast<int>(s));
    }

    Matrix3 m_composite;
    Matrix3 m_feature;
    float m_compositeWidths[2] = {0.0f, 0.0f};
    dqCommon::HiliteSettings m_hiliteSettings;
    dqCommon::HiliteSettings m_emphasisSettings;
    FloatRgb m_hiliteRgb;
};

END_DQ_RENDER_NAMESPACE
