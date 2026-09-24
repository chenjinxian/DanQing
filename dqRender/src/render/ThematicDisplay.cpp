// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ThematicDisplay gradient LUT builder
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicDisplay.ts
#include "ThematicDisplay.h"

#include <algorithm>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ThematicDisplay::buildGradientLutData
// Ported from: itwinjs-core webgl/ThematicDisplay.ts
// Builds a 256-pixel wide RGBA8 texture for gradient lookup.
// ---------------------------------------------------------------------------
std::vector<uint8_t> ThematicDisplay::buildGradientLutData() const
{
    constexpr uint32_t kLutWidth = 256;
    std::vector<uint8_t> data(kLutWidth * 4, 0);

    if (m_gradientColors.empty()) {
        // Default: white to black gradient
        for (uint32_t i = 0; i < kLutWidth; ++i) {
            uint8_t v = static_cast<uint8_t>(255 - i);
            data[i * 4 + 0] = v;
            data[i * 4 + 1] = v;
            data[i * 4 + 2] = v;
            data[i * 4 + 3] = 255;
        }
        return data;
    }

    // Interpolate between gradient color stops
    for (uint32_t i = 0; i < kLutWidth; ++i) {
        float t = static_cast<float>(i) / (kLutWidth - 1);

        // Find the two color stops that bracket this position
        size_t lo = 0;
        size_t hi = m_gradientColors.size() - 1;
        for (size_t j = 0; j + 1 < m_gradientColors.size(); ++j) {
            if (t >= m_gradientColors[j].position && t <= m_gradientColors[j + 1].position) {
                lo = j;
                hi = j + 1;
                break;
            }
        }

        // Interpolate
        float range = m_gradientColors[hi].position - m_gradientColors[lo].position;
        float frac = (range > 0.0f) ? (t - m_gradientColors[lo].position) / range : 0.0f;

        uint32_t c0 = m_gradientColors[lo].color;
        uint32_t c1 = m_gradientColors[hi].color;

        auto lerp = [](uint8_t a, uint8_t b, float f) -> uint8_t {
            return static_cast<uint8_t>(a + (b - a) * f);
        };

        uint8_t r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
        uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;

        data[i * 4 + 0] = lerp(r0, r1, frac);
        data[i * 4 + 1] = lerp(g0, g1, frac);
        data[i * 4 + 2] = lerp(b0, b1, frac);
        data[i * 4 + 3] = 255;
    }

    return data;
}

END_DQ_RENDER_NAMESPACE
