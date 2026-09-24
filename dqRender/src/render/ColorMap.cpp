// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/ColorMap.ts
// DanQing dqRender — ColorMap implementation (toColorIndex)
#include "ColorMap.h"

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 ColorMap.toColorIndex — populate `index` from this color table + the mesh's per-vertex indices.
void ColorMap::toColorIndex(dqCommon::ColorIndex& index, const std::vector<uint32_t>& indices) const
{
    index.reset();
    const int n = length();
    if (0 == n) {
        // 1:1 reference asserts ("empty color map") and returns; DanQing core is non-throwing — no-op.
        return;
    }

    if (1 == n) {
        // initUniform(this._array[0].value) — the single inserted color.
        index.initUniform(toArray()[0]);
        return;
    }

    // colors[entry.index] = entry.value for each entry — equivalent to toArray() (values ordered by
    // insertion index).
    const std::vector<uint32_t> colors = toArray();
    // indices: per-vertex color indices (fit in uint16; color table capped at 0xffff).
    std::vector<uint16_t> idx16;
    idx16.reserve(indices.size());
    for (const uint32_t v : indices)
        idx16.push_back(static_cast<uint16_t>(v));
    index.initNonUniform(colors, idx16, m_hasTransparency);
}

END_DQ_RENDER_NAMESPACE
