// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Monochrome display mode
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Monochrome.ts
//
// MonochromeMode enum is defined in Material.h (from webgl/Material.ts).
// This header provides the MonochromeSettings class for runtime configuration.
#pragma once

#include "Material.h"  // for MonochromeMode enum

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MonochromeSettings — runtime monochrome rendering configuration
// (Ported from: itwinjs-core webgl/glsl/Monochrome.ts)
// ---------------------------------------------------------------------------
class MonochromeSettings {
public:
    MonochromeSettings() = default;

    /// Set the monochrome color (RGB).
    void setColor(uint32_t color) noexcept { m_color = color; }

    /// Get the monochrome color.
    uint32_t getColor() const noexcept { return m_color; }

    /// Set the monochrome alpha.
    void setAlpha(float alpha) noexcept { m_alpha = alpha; }

    /// Get the monochrome alpha.
    float getAlpha() const noexcept { return m_alpha; }

    /// Check if monochrome mode is enabled.
    bool isEnabled() const noexcept { return m_enabled; }

    /// Enable/disable monochrome mode.
    void setEnabled(bool enabled) noexcept { m_enabled = enabled; }

    /// Get color as float RGB (0-1).
    void getColorFloat(float* rgb) const
    {
        rgb[0] = ((m_color >> 16) & 0xFF) / 255.0f;
        rgb[1] = ((m_color >> 8) & 0xFF) / 255.0f;
        rgb[2] = (m_color & 0xFF) / 255.0f;
    }

private:
    uint32_t m_color = 0x808080;  // gray
    float m_alpha = 1.0f;
    bool m_enabled = false;
};

END_DQ_RENDER_NAMESPACE
