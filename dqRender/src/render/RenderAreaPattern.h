// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render area pattern
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderAreaPattern.ts
//
// Generates pattern geometry for area fills (cross-hatch, etc.).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// AreaPatternType — type of area fill pattern
// ---------------------------------------------------------------------------
enum class AreaPatternType : uint8_t {
    None = 0,
    CrossHatch = 1,
    DiagonalUp = 2,
    DiagonalDown = 3,
    Horizontal = 4,
    Vertical = 5,
    Dots = 6,
};

// ---------------------------------------------------------------------------
// RenderAreaPattern — area fill pattern configuration
// (Ported from: itwinjs-core RenderAreaPattern.ts)
// ---------------------------------------------------------------------------
class RenderAreaPattern {
public:
    RenderAreaPattern() = default;

    /// Set the pattern type.
    void setType(AreaPatternType type) noexcept { m_type = type; }

    /// Get the pattern type.
    AreaPatternType getType() const noexcept { return m_type; }

    /// Set the pattern scale.
    void setScale(float scale) noexcept { m_scale = scale; }

    /// Get the pattern scale.
    float getScale() const noexcept { return m_scale; }

    /// Set the pattern color.
    void setColor(uint32_t color) noexcept { m_color = color; }

    /// Get the pattern color.
    uint32_t getColor() const noexcept { return m_color; }

    /// Set the pattern weight (line width).
    void setWeight(float weight) noexcept { m_weight = weight; }

    /// Get the pattern weight.
    float getWeight() const noexcept { return m_weight; }

    /// Check if a pattern is active.
    bool isActive() const noexcept { return m_type != AreaPatternType::None; }

private:
    AreaPatternType m_type = AreaPatternType::None;
    float m_scale = 1.0f;
    uint32_t m_color = 0x000000;
    float m_weight = 1.0f;
};

END_DQ_RENDER_NAMESPACE
