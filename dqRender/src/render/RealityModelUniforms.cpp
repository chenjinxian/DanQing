// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Reality model uniforms implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RealityModelUniforms.ts
#include "RealityModelUniforms.h"
#include "Uniforms.h"  // FrustumUniforms

#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RealityModelUniforms::updateRange — update scale factor based on range
// Ported from: itwinjs-core RealityModelUniforms.updateRange() (line 56-80)
// ---------------------------------------------------------------------------
void RealityModelUniforms::updateRange(dqGeom::Range3d const* range,
                                        FrustumUniforms const& frustum,
                                        float const* transformScale,
                                        bool is3d,
                                        uint32_t viewWidth)
{
    float rangeFactor = 8.0f;  // default to min scale factor of 8
    float near = frustum.getNearPlane();
    float far = frustum.getFarPlane();
    float viewDepth = far - near;

    if (range && !range->isNull()) {
        // Calculate range scale from transform
        float rangeScale = 1.0f;
        if (transformScale) {
            float xLen = static_cast<float>(range->high.x - range->low.x);
            rangeScale = std::max({std::abs(transformScale[0]) * xLen,
                                   std::abs(transformScale[4]) * xLen,
                                   std::abs(transformScale[8]) * xLen});
        }
        // Limit the viewDepth/rangeScale ratio to min of 10
        rangeFactor = std::log(std::max(10.0f, viewDepth / rangeScale));
    }

    float zoomFactor = std::log(far / near);
    float winSizeFactor = std::pow(1.8440033f, std::log2(2226.0f / viewWidth));
    float scaleFactor = (rangeFactor + zoomFactor) / winSizeFactor;

    if (m_scaleFactor == scaleFactor && m_is3d == is3d)
        return;
    m_scaleFactor = scaleFactor;
    m_is3d = is3d;
}

END_DQ_RENDER_NAMESPACE
