// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Layer command lists implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/LayerCommands.ts
#include "LayerCommands.h"

#include <algorithm>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// clear
// (Ported from: itwinjs-core LayerCommands.ts clear)
// ---------------------------------------------------------------------------
void LayerCommandLists::clear()
{
    m_layers.clear();
}

// ---------------------------------------------------------------------------
// setCurrentLayer
// (Ported from: itwinjs-core LayerCommands.ts setCurrentLayer)
// ---------------------------------------------------------------------------
void LayerCommandLists::setCurrentLayer(char const* layerId, int priority,
                                         float elevation)
{
    m_currentPriority = priority;
    m_currentElevation = elevation;

    // Ensure the layer exists.
    if (layerId && layerId[0] != '\0') {
        findOrCreateLayer(layerId, priority, elevation);
    }
}

// ---------------------------------------------------------------------------
// addCommands
// (Ported from: itwinjs-core LayerCommands.ts addCommands)
// ---------------------------------------------------------------------------
void LayerCommandLists::addCommands(std::vector<uint32_t> const& cmds)
{
    if (cmds.empty()) return;

    // Find the most recently set layer.
    if (m_layers.empty()) {
        findOrCreateLayer("", m_currentPriority, m_currentElevation);
    }

    auto& layer = m_layers.back();
    layer.commands.insert(layer.commands.end(), cmds.begin(), cmds.end());
}

// ---------------------------------------------------------------------------
// outputCommands
// (Ported from: itwinjs-core LayerCommands.ts outputCommands)
//
// Merges all layer command lists into a single output, sorted by priority.
// Layers with equal priority are merged in insertion order.
// ---------------------------------------------------------------------------
void LayerCommandLists::outputCommands(std::vector<uint32_t>& output)
{
    // Sort layers by priority (stable to preserve insertion order for ties).
    std::stable_sort(m_layers.begin(), m_layers.end(),
                     [](LayerCommands const& a, LayerCommands const& b) {
                         return a.priority < b.priority;
                     });

    for (auto const& layer : m_layers) {
        output.insert(output.end(), layer.commands.begin(), layer.commands.end());
    }
}

// ---------------------------------------------------------------------------
// hasCommands
// (Ported from: itwinjs-core LayerCommands.ts hasCommands)
// ---------------------------------------------------------------------------
bool LayerCommandLists::hasCommands() const noexcept
{
    for (auto const& layer : m_layers) {
        if (!layer.commands.empty()) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// findOrCreateLayer (private)
// ---------------------------------------------------------------------------
LayerCommands& LayerCommandLists::findOrCreateLayer(char const* layerId,
                                                     int priority, float elevation)
{
    // Look for existing layer with matching ID.
    for (auto& layer : m_layers) {
        if (layer.layerId == (layerId ? layerId : "")) {
            return layer;
        }
    }

    // Create new layer.
    m_layers.emplace_back();
    auto& newLayer = m_layers.back();
    newLayer.layerId = layerId ? layerId : "";
    newLayer.priority = priority;
    newLayer.elevation = elevation;
    return newLayer;
}

END_DQ_RENDER_NAMESPACE
