// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Layer command lists
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/LayerCommands.ts
//
// State machine for per-layer sorted command lists.  During scene graph
// traversal, draw commands are accumulated into per-layer buckets keyed by
// layer ID.  At render time the buckets are merged in priority order.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// LayerCommands — commands for a single layer
// (Ported from: itwinjs-core LayerCommands.ts)
// ---------------------------------------------------------------------------
struct LayerCommands {
    std::string layerId;
    int priority = 0;
    float elevation = 0.0f;
    std::vector<uint32_t> commands;  // command indices into the RenderCommands buffer
};

// ---------------------------------------------------------------------------
// LayerCommandLists — manages per-layer command accumulation
// (Ported from: itwinjs-core LayerCommands.ts LayerCommandLists)
// ---------------------------------------------------------------------------
class LayerCommandLists {
public:
    LayerCommandLists() = default;

    /// clear all layer commands.
    void clear();

    /// Set the current layer for subsequent addCommands calls.
    /// @param layerId    Unique identifier for the layer.
    /// @param priority   Rendering priority (lower = rendered first).
    /// @param elevation  Elevation offset for the layer.
    void setCurrentLayer(char const* layerId, int priority, float elevation);

    /// add commands to the current layer.
    void addCommands(std::vector<uint32_t> const& cmds);

    /// Merge all layer commands into a single output list, sorted by priority.
    /// Layers with lower priority values are emitted first.
    void outputCommands(std::vector<uint32_t>& output);

    /// Get the number of layers.
    size_t getLayerCount() const noexcept { return m_layers.size(); }

    /// Check if there are any commands across all layers.
    bool hasCommands() const noexcept;

private:
    /// Find or create a layer entry by ID.
    LayerCommands& findOrCreateLayer(char const* layerId, int priority, float elevation);

    std::vector<LayerCommands> m_layers;
    int m_currentPriority = 0;
    float m_currentElevation = 0.0f;
};

END_DQ_RENDER_NAMESPACE
