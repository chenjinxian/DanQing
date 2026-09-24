// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TargetGraphics implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/TargetGraphics.ts
#include "TargetGraphics.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// getForeground — scene foreground graphics
// Ported from: itwinjs-core TargetGraphics.foreground
// ---------------------------------------------------------------------------
std::vector<Graphic*> TargetGraphics::getForeground() const
{
    std::vector<Graphic*> result;
    if (m_scene) {
        result.reserve(m_scene->foreground.size());
        for (auto* rg : m_scene->foreground) {
            if (rg) {
                // Scene stores RenderGraphic*, but internal pipeline needs Graphic*.
                // Graphic inherits from RenderGraphic, so static_cast is safe.
                result.push_back(static_cast<Graphic*>(rg));
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// getBackground — scene background graphics
// Ported from: itwinjs-core TargetGraphics.background
// ---------------------------------------------------------------------------
std::vector<Graphic*> TargetGraphics::getBackground() const
{
    std::vector<Graphic*> result;
    if (m_scene) {
        result.reserve(m_scene->background.size());
        for (auto* rg : m_scene->background) {
            if (rg)
                result.push_back(static_cast<Graphic*>(rg));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// getOverlays — scene overlay graphics
// Ported from: itwinjs-core TargetGraphics.overlays
// ---------------------------------------------------------------------------
std::vector<Graphic*> TargetGraphics::getOverlays() const
{
    std::vector<Graphic*> result;
    if (m_scene) {
        result.reserve(m_scene->overlay.size());
        for (auto* rg : m_scene->overlay) {
            if (rg)
                result.push_back(static_cast<Graphic*>(rg));
        }
    }
    return result;
}

END_DQ_RENDER_NAMESPACE
