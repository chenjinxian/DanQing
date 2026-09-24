// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — EdgeSettings implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/EdgeSettings.ts
#include "EdgeSettings.h"

#include <algorithm>

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// EdgeSettings
// (Ported from: itwinjs-core EdgeSettings.ts, line 19-128)
// ===========================================================================

EdgeSettings EdgeSettings::create(dqCommon::HiddenLineSettings const* hline)
{
    EdgeSettings settings;
    settings.init(hline);
    return settings;
}

void EdgeSettings::init(dqCommon::HiddenLineSettings const* hline)
{
    clear();
    if (!hline)
        return;

    // Ported from: itwinjs-core EdgeSettings.ts line 43-46
    // The threshold is a transparency value. Convert to alpha and clamp to [0..1].
    float threshold = static_cast<float>(hline->transparencyThreshold);
    threshold = std::min(1.0f, std::max(0.0f, threshold));
    m_transparencyThreshold = 1.0f - threshold;

    // Ported from: itwinjs-core EdgeSettings.ts line 48-55
    auto const& vis = hline->visible;
    if (vis.color.has_value()) {
        m_colorOverridden = true;
        auto rgb = vis.color->getRgb();
        auto alpha = vis.color->getAlpha();
        m_color = FloatRgba(
            ((rgb >> 16) & 0xFF) / 255.0f,
            ((rgb >> 8) & 0xFF) / 255.0f,
            (rgb & 0xFF) / 255.0f,
            alpha / 255.0f
        );
    }

    m_visibleLineCode = vis.pattern.has_value()
        ? std::make_optional(LineCode::valueFromLinePixels(*vis.pattern))
        : std::nullopt;
    m_visibleWeight = vis.width;

    // Ported from: itwinjs-core EdgeSettings.ts line 58-65
    // Hidden edge settings default to matching visible edge settings.
    auto const& hid = hline->hidden;
    m_hiddenLineCode = hid.pattern.has_value()
        ? std::make_optional(LineCode::valueFromLinePixels(*hid.pattern))
        : m_visibleLineCode;
    m_hiddenWeight = hid.width.has_value() ? hid.width : m_visibleWeight;

    if (m_hiddenWeight.has_value() && m_visibleWeight.has_value()) {
        // Hidden edges cannot be wider than visible edges.
        m_hiddenWeight = std::min(*m_visibleWeight, *m_hiddenWeight);
    }
}

void EdgeSettings::clear()
{
    m_colorOverridden = false;
    m_visibleLineCode = std::nullopt;
    m_visibleWeight = std::nullopt;
    m_hiddenLineCode = std::nullopt;
    m_hiddenWeight = std::nullopt;
    m_transparencyThreshold = 0.0f;
}

// ---------------------------------------------------------------------------
// computeOvrFlags
// Ported from: itwinjs-core EdgeSettings.ts line 68-83
// ---------------------------------------------------------------------------
dqCommon::OvrFlag EdgeSettings::computeOvrFlags(RenderPass pass, dqCommon::ViewFlags const& vf) const
{
    if (!isOverridden(vf))
        return dqCommon::OvrFlag::None;

    // Alpha always overridden — transparent edges only supported in wireframe mode.
    dqCommon::OvrFlag flags = dqCommon::OvrFlag::None;
    if (getColor(vf)) {
        flags = dqCommon::OvrFlag::Rgb | dqCommon::OvrFlag::Alpha | dqCommon::OvrFlag::LineRgb | dqCommon::OvrFlag::LineAlpha;
    } else {
        flags = dqCommon::OvrFlag::Alpha | dqCommon::OvrFlag::LineAlpha;
    }

    if (getLineCode(pass, vf).has_value())
        flags = flags | dqCommon::OvrFlag::LineCode;

    if (getWeight(pass, vf).has_value())
        flags = flags | dqCommon::OvrFlag::Weight;

    return flags;
}

// ---------------------------------------------------------------------------
// getColor
// Ported from: itwinjs-core EdgeSettings.ts line 89-91
// ---------------------------------------------------------------------------
FloatRgba const* EdgeSettings::getColor(dqCommon::ViewFlags const& vf) const
{
    if (m_colorOverridden && isOverridden(vf))
        return &m_color;
    return nullptr;
}

// ---------------------------------------------------------------------------
// getLineCode
// Ported from: itwinjs-core EdgeSettings.ts line 93-98
// ---------------------------------------------------------------------------
std::optional<int> EdgeSettings::getLineCode(RenderPass pass, dqCommon::ViewFlags const& vf) const
{
    if (!isOverridden(vf))
        return std::nullopt;

    return (pass == RenderPass::HiddenEdge) ? m_hiddenLineCode : m_visibleLineCode;
}

// ---------------------------------------------------------------------------
// getWeight
// Ported from: itwinjs-core EdgeSettings.ts line 100-105
// ---------------------------------------------------------------------------
std::optional<int> EdgeSettings::getWeight(RenderPass pass, dqCommon::ViewFlags const& vf) const
{
    if (!isOverridden(vf))
        return std::nullopt;

    return (pass == RenderPass::HiddenEdge) ? m_hiddenWeight : m_visibleWeight;
}

// ---------------------------------------------------------------------------
// wantContrastingColor
// Ported from: itwinjs-core EdgeSettings.ts line 114-116
// ---------------------------------------------------------------------------
bool EdgeSettings::wantContrastingColor(dqCommon::RenderMode renderMode) const
{
    return !m_colorOverridden && renderMode == dqCommon::RenderMode::SolidFill;
}

// ---------------------------------------------------------------------------
// isOverridden
// Ported from: itwinjs-core EdgeSettings.ts line 118-127
// ---------------------------------------------------------------------------
bool EdgeSettings::isOverridden(dqCommon::ViewFlags const& vf) const
{
    switch (vf.renderMode()) {
        case dqCommon::RenderMode::Wireframe:
            return false;  // edge overrides don't apply in wireframe mode
        case dqCommon::RenderMode::SmoothShade:
            return vf.visibleEdges();
        default:
            return true;  // edges always displayed in solid fill and hidden line modes
    }
}

END_DQ_RENDER_NAMESPACE
