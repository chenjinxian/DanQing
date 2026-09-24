// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Layer rendering
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Layer.ts
//
// Map layer rendering: wraps graphic branches for overlay/underlay layers.
// Layers are rendered in a specific order (background, overlay, etc.).
#pragma once

#include "Graphic.h"

#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// LayerType — layer rendering order
// ---------------------------------------------------------------------------
enum class LayerType : uint8_t {
    Background = 0,     // Rendered first (behind everything)
    Overlay = 1,        // Rendered on top of geometry
    Underlay = 2,       // Rendered between background and geometry
    WorldOverlay = 3,   // World-space overlay
    ViewOverlay = 4,    // View-space overlay
};

// ---------------------------------------------------------------------------
// Layer — a single rendering layer
// (Ported from: itwinjs-core Layer.ts)
// ---------------------------------------------------------------------------
class Layer {
public:
    Layer(LayerType type, std::unique_ptr<Graphic> graphic)
        : m_type(type), m_graphic(std::move(graphic)) {}

    LayerType getType() const noexcept { return m_type; }
    Graphic* getGraphic() const noexcept { return m_graphic.get(); }

    void addCommands(RenderCommands& commands)
    {
        if (m_graphic) m_graphic->addCommands(commands);
    }

private:
    LayerType m_type;
    std::unique_ptr<Graphic> m_graphic;
};

// ---------------------------------------------------------------------------
// LayerContainer — manages multiple layers
// (Ported from: itwinjs-core LayerContainer.ts)
// ---------------------------------------------------------------------------
class LayerContainer {
public:
    LayerContainer() = default;

    void addLayer(std::unique_ptr<Layer> layer)
    {
        m_layers.push_back(std::move(layer));
    }

    void addCommands(RenderCommands& commands)
    {
        for (auto& layer : m_layers) {
            layer->addCommands(commands);
        }
    }

    size_t getLayerCount() const noexcept { return m_layers.size(); }
    void clear() { m_layers.clear(); }

private:
    std::vector<std::unique_ptr<Layer>> m_layers;
};

END_DQ_RENDER_NAMESPACE
