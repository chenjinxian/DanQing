// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contour lines
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Contours.ts
//
// Contour line rendering on surfaces (elevation contours).
// Generates contour lines at regular intervals on surfaces.
#pragma once

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ContourLines — contour line configuration
// (Ported from: itwinjs-core Contours.ts)
// ---------------------------------------------------------------------------
class ContourLines {
public:
    ContourLines() = default;

    /// Set the contour interval (world units between lines).
    void setInterval(float interval) noexcept { m_interval = interval; }

    /// Get the contour interval.
    float getInterval() const noexcept { return m_interval; }

    /// Set the contour origin (reference elevation).
    void setOrigin(float origin) noexcept { m_origin = origin; }

    /// Get the contour origin.
    float getOrigin() const noexcept { return m_origin; }

    /// Set the contour line color.
    void setColor(uint32_t color) noexcept { m_color = color; }

    /// Get the contour line color.
    uint32_t getColor() const noexcept { return m_color; }

    /// Set the contour line width.
    void setWidth(float width) noexcept { m_width = width; }

    /// Get the contour line width.
    float getWidth() const noexcept { return m_width; }

    /// Check if contour lines are enabled.
    bool isEnabled() const noexcept { return m_enabled; }

    /// Enable/disable contour lines.
    void setEnabled(bool enabled) noexcept { m_enabled = enabled; }

    /// Build the contour LUT texture data.
    /// Returns a 1D RGBA texture where each pixel represents a contour level.
    std::vector<uint8_t> buildContourLutData(uint32_t numLevels) const;

private:
    float m_interval = 1.0f;
    float m_origin = 0.0f;
    uint32_t m_color = 0x000000;  // black
    float m_width = 1.0f;
    bool m_enabled = false;
};

END_DQ_RENDER_NAMESPACE
