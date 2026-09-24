// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Edge display settings
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/EdgeSettings.ts
//
// Controls symbology of edges based on ViewFlags and HiddenLine.Settings.
// Typically these come from the Target's RenderPlan, but a GraphicBranch
// may override those settings.
#pragma once

#include "FloatRGBA.h"
#include "LineCode.h"
#include "gl/RenderFlags.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/HiddenLine.h>
#include <dqCommon/OvrFlags.h>
#include <dqCommon/RenderMode.h>
#include <dqCommon/ViewFlags.h>

#include <cstdint>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// EdgeSettings — edge display configuration
// (Ported from: itwinjs-core EdgeSettings.ts, line 19-128)
//
// Controls how edges are rendered based on ViewFlags and HiddenLine.Settings.
// Supports visible/hidden edge line codes, weights, and color overrides.
// ---------------------------------------------------------------------------
class EdgeSettings {
public:
    EdgeSettings() = default;

    /// Create from HiddenLine.Settings.
    /// Ported from: itwinjs-core EdgeSettings.ts line 32-36
    static EdgeSettings create(dqCommon::HiddenLineSettings const* hline);

    /// Initialize from HiddenLine.Settings.
    /// Ported from: itwinjs-core EdgeSettings.ts line 38-66
    void init(dqCommon::HiddenLineSettings const* hline);

    /// Compute override flags for the given render pass and view flags.
    /// Ported from: itwinjs-core EdgeSettings.ts line 68-83
    dqCommon::OvrFlag computeOvrFlags(RenderPass pass, dqCommon::ViewFlags const& vf) const;

    /// Get the transparency threshold (alpha value).
    /// Ported from: itwinjs-core EdgeSettings.ts line 85-87
    float getTransparencyThreshold() const noexcept { return m_transparencyThreshold; }

    /// Get the color override, or nullptr if not overridden.
    /// Ported from: itwinjs-core EdgeSettings.ts line 89-91
    FloatRgba const* getColor(dqCommon::ViewFlags const& vf) const;

    /// Get the line code override for the given pass.
    /// Ported from: itwinjs-core EdgeSettings.ts line 93-98
    std::optional<int> getLineCode(RenderPass pass, dqCommon::ViewFlags const& vf) const;

    /// Get the weight override for the given pass.
    /// Ported from: itwinjs-core EdgeSettings.ts line 100-105
    std::optional<int> getWeight(RenderPass pass, dqCommon::ViewFlags const& vf) const;

    /// Whether a contrasting color should be used in solid fill mode.
    /// Ported from: itwinjs-core EdgeSettings.ts line 114-116
    bool wantContrastingColor(dqCommon::RenderMode renderMode) const;

    // --- Simple accessors (backward compatible with old API) ---
    void setVisibleEdges(bool enabled) noexcept { m_visibleEdges = enabled; }
    void setHiddenEdges(bool enabled) noexcept { m_hiddenEdges = enabled; }
    void setSilhouetteEdges(bool enabled) noexcept { m_silhouetteEdges = enabled; }
    void setEdgeWeight(float weight) noexcept { m_edgeWeight = weight; }
    void setEdgeCode(uint32_t code) noexcept { m_edgeCode = code; }

    bool getVisibleEdges() const noexcept { return m_visibleEdges; }
    bool getHiddenEdges() const noexcept { return m_hiddenEdges; }
    bool getSilhouetteEdges() const noexcept { return m_silhouetteEdges; }
    float getEdgeWeight() const noexcept { return m_edgeWeight; }
    uint32_t getEdgeCode() const noexcept { return m_edgeCode; }

    bool hasAnyEdges() const noexcept
    {
        return m_visibleEdges || m_hiddenEdges || m_silhouetteEdges;
    }

private:
    /// Check if edge overrides apply based on render mode and view flags.
    /// Ported from: itwinjs-core EdgeSettings.ts line 118-127
    bool isOverridden(dqCommon::ViewFlags const& vf) const;

    void clear();

    // HiddenLine-derived settings
    FloatRgba m_color = {1.0f, 1.0f, 1.0f, 1.0f};  // white default
    bool m_colorOverridden = false;
    std::optional<int> m_visibleLineCode;
    std::optional<int> m_visibleWeight;
    std::optional<int> m_hiddenLineCode;
    std::optional<int> m_hiddenWeight;
    float m_transparencyThreshold = 0.0f;

    // Simple edge flags (backward compatible)
    bool m_visibleEdges = true;
    bool m_hiddenEdges = false;
    bool m_silhouetteEdges = false;
    float m_edgeWeight = 1.0f;
    uint32_t m_edgeCode = 0;
};

END_DQ_RENDER_NAMESPACE
