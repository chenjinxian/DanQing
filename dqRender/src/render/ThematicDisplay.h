// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicSensors.ts
//
// Gradient-based thematic visualization (height, slope, sensor values).
// Colors geometry based on a gradient lookup from per-vertex scalar values.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ThematicDisplayMode — thematic coloring mode
// ---------------------------------------------------------------------------
enum class ThematicDisplayMode : uint8_t {
    Height = 0,      // Color by elevation
    Slope = 1,       // Color by slope angle
    HillShade = 2,   // Hill shading
    InverseDistanceWeighted = 3,  // IDW interpolation from sensors
};

// ---------------------------------------------------------------------------
// ThematicGradientColor — a single color stop in the gradient
// ---------------------------------------------------------------------------
struct ThematicGradientColor {
    float position = 0.0f;  // 0-1
    uint32_t color = 0xFFFFFF;
};

// ---------------------------------------------------------------------------
// ThematicDisplay — thematic display configuration
// (Ported from: itwinjs-core ThematicSensors.ts)
// ---------------------------------------------------------------------------
class ThematicDisplay {
public:
    ThematicDisplay() = default;

    /// Set the display mode.
    void setMode(ThematicDisplayMode mode) noexcept { m_mode = mode; }

    /// Get the display mode.
    ThematicDisplayMode getMode() const noexcept { return m_mode; }

    /// Set the gradient colors.
    void setGradientColors(std::vector<ThematicGradientColor> const& colors)
    {
        m_gradientColors = colors;
    }

    /// Get the gradient colors.
    std::vector<ThematicGradientColor> const& getGradientColors() const noexcept
    {
        return m_gradientColors;
    }

    /// Set the range (min/max values for gradient mapping).
    void setRange(float min, float max) noexcept
    {
        m_rangeMin = min;
        m_rangeMax = max;
    }

    /// Get the range.
    float getRangeMin() const noexcept { return m_rangeMin; }
    float getRangeMax() const noexcept { return m_rangeMax; }

    /// Check if thematic display is enabled.
    bool isEnabled() const noexcept { return m_enabled; }

    /// Enable/disable thematic display.
    void setEnabled(bool enabled) noexcept { m_enabled = enabled; }

    /// Build the gradient LUT texture data (RGBA8, 256 pixels wide).
    std::vector<uint8_t> buildGradientLutData() const;

private:
    ThematicDisplayMode m_mode = ThematicDisplayMode::Height;
    std::vector<ThematicGradientColor> m_gradientColors;
    float m_rangeMin = 0.0f;
    float m_rangeMax = 100.0f;
    bool m_enabled = false;
};

END_DQ_RENDER_NAMESPACE
