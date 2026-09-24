// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Target graphics management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/TargetGraphics.ts
//
// Manages the collection of graphics currently assigned to a Target for rendering.
#pragma once

#include "Graphic.h"
#include "dqRender/Scene.h"
#include "dqRender/Decorations.h"

#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TargetGraphics — manages graphics for a render target
// (Ported from: itwinjs-core TargetGraphics.ts)
//
// Holds the scene data broken down into the categories that
// RenderCommands.initForRender() needs: foreground, background, overlays,
// dynamics, and decorations.
// ---------------------------------------------------------------------------
class TargetGraphics {
public:
    TargetGraphics() = default;
    ~TargetGraphics() = default;

    TargetGraphics(TargetGraphics const&) = delete;
    TargetGraphics& operator=(TargetGraphics const&) = delete;

    /// Set the scene to render. Breaks down into foreground/background/overlay lists.
    /// Ported from: itwinjs-core TargetGraphics.setScene()
    void setScene(Scene const* scene) { m_scene = scene; }

    /// Get the current scene.
    Scene const* getScene() const noexcept { return m_scene; }

    /// Get foreground graphics (scene content with z-buffer).
    /// Ported from: itwinjs-core TargetGraphics.foreground
    std::vector<Graphic*> getForeground() const;

    /// Get background graphics (drawn behind everything).
    /// Ported from: itwinjs-core TargetGraphics.background
    std::vector<Graphic*> getBackground() const;

    /// Get overlay graphics (drawn on top of everything).
    /// Ported from: itwinjs-core TargetGraphics.overlays
    std::vector<Graphic*> getOverlays() const;

    /// Get foreground dynamics.
    /// Ported from: itwinjs-core TargetGraphics.foregroundDynamics
    std::vector<Graphic*> const& getForegroundDynamics() const { return m_foregroundDynamics; }

    /// Get overlay dynamics.
    /// Ported from: itwinjs-core TargetGraphics.overlayDynamics
    std::vector<Graphic*> const& getOverlayDynamics() const { return m_overlayDynamics; }

    /// Get decorations (may be nullptr).
    /// Ported from: itwinjs-core TargetGraphics.decorations
    Decorations const* getDecorations() const noexcept { return m_decorations; }
    void setDecorations(Decorations const* dec) { m_decorations = dec; }

    /// add a graphic to the target.
    void addGraphic(RenderGraphic* graphic)
    {
        if (graphic) m_graphics.push_back(graphic);
    }

    /// Remove a graphic from the target.
    void removeGraphic(RenderGraphic* graphic)
    {
        auto it = std::find(m_graphics.begin(), m_graphics.end(), graphic);
        if (it != m_graphics.end()) {
            m_graphics.erase(it);
        }
    }

    /// Get all graphics.
    std::vector<RenderGraphic*> const& getGraphics() const noexcept { return m_graphics; }

    /// Clear all graphics.
    void clear()
    {
        m_graphics.clear();
        m_scene = nullptr;
        m_decorations = nullptr;
        m_foregroundDynamics.clear();
        m_overlayDynamics.clear();
    }

    /// Check if any graphics are assigned.
    bool isEmpty() const noexcept { return m_graphics.empty() && !m_scene; }

private:
    Scene const* m_scene = nullptr;
    Decorations const* m_decorations = nullptr;
    std::vector<RenderGraphic*> m_graphics;
    std::vector<Graphic*> m_foregroundDynamics;
    std::vector<Graphic*> m_overlayDynamics;
};

END_DQ_RENDER_NAMESPACE
