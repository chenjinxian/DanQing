// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ContourLines LUT builder
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Contours.ts
#include "ContourLines.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ContourLines::buildContourLutData
// Ported from: itwinjs-core webgl/Contours.ts
// Builds a 1D RGBA texture for contour level lookup.
// ---------------------------------------------------------------------------
std::vector<uint8_t> ContourLines::buildContourLutData(uint32_t numLevels) const
{
    std::vector<uint8_t> data(numLevels * 4, 0);

    uint8_t r = (m_color >> 16) & 0xFF;
    uint8_t g = (m_color >> 8) & 0xFF;
    uint8_t b = m_color & 0xFF;

    for (uint32_t i = 0; i < numLevels; ++i) {
        data[i * 4 + 0] = r;
        data[i * 4 + 1] = g;
        data[i * 4 + 2] = b;
        data[i * 4 + 3] = 255;  // opaque for contour lines
    }

    return data;
}

END_DQ_RENDER_NAMESPACE
