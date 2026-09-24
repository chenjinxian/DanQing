// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Color info for feature rendering
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ColorInfo.ts
//
// Manages feature color/alpha information for rendering.
#pragma once

#include <array>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ColorInfo — feature color and alpha
// (Ported from: itwinjs-core ColorInfo.ts)
// ---------------------------------------------------------------------------
class ColorInfo {
public:
    enum class Type : uint8_t {
        Uniform,     // Single color for all features
        PerVertex,   // Per-vertex colors
        PerFeature,  // Per-feature override colors
        Monochrome,  // Single monochrome color
    };

    ColorInfo() = default;

    static ColorInfo fromUniform(uint32_t rgb, uint8_t alpha = 255)
    {
        ColorInfo info;
        info.m_type = Type::Uniform;
        info.m_rgb = rgb;
        info.m_alpha = alpha;
        return info;
    }

    static ColorInfo fromPerVertex()
    {
        ColorInfo info;
        info.m_type = Type::PerVertex;
        return info;
    }

    static ColorInfo fromPerFeature()
    {
        ColorInfo info;
        info.m_type = Type::PerFeature;
        return info;
    }

    Type getType() const noexcept { return m_type; }
    uint32_t getRgb() const noexcept { return m_rgb; }
    uint8_t getAlpha() const noexcept { return m_alpha; }
    bool isUniform() const noexcept { return m_type == Type::Uniform; }
    bool isPerVertex() const noexcept { return m_type == Type::PerVertex; }
    bool isPerFeature() const noexcept { return m_type == Type::PerFeature; }

    /// Get the color as float RGBA (0-1 range).
    std::array<float, 4> getFloatRgba() const
    {
        return {{
            ((m_rgb >> 16) & 0xFF) / 255.0f,
            ((m_rgb >> 8) & 0xFF) / 255.0f,
            (m_rgb & 0xFF) / 255.0f,
            m_alpha / 255.0f,
        }};
    }

private:
    Type m_type = Type::Uniform;
    uint32_t m_rgb = 0xFFFFFF;
    uint8_t m_alpha = 255;
};

END_DQ_RENDER_NAMESPACE
