// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render terrain
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderTerrain.ts
//
// Terrain mesh rendering support.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderTerrain — terrain rendering configuration
// (Ported from: itwinjs-core RenderTerrain.ts)
// ---------------------------------------------------------------------------
class RenderTerrain {
public:
    RenderTerrain() = default;

    /// Set the terrain height range.
    void setHeightRange(float min, float max) noexcept
    {
        m_heightMin = min;
        m_heightMax = max;
    }

    /// Get the terrain height range.
    float getHeightMin() const noexcept { return m_heightMin; }
    float getHeightMax() const noexcept { return m_heightMax; }

    /// Set the terrain resolution.
    void setResolution(uint32_t resolution) noexcept { m_resolution = resolution; }

    /// Get the terrain resolution.
    uint32_t getResolution() const noexcept { return m_resolution; }

    /// Set whether terrain is enabled.
    void setEnabled(bool enabled) noexcept { m_enabled = enabled; }

    /// Check if terrain is enabled.
    bool isEnabled() const noexcept { return m_enabled; }

private:
    float m_heightMin = 0.0f;
    float m_heightMax = 1000.0f;
    uint32_t m_resolution = 256;
    bool m_enabled = false;
};

END_DQ_RENDER_NAMESPACE
