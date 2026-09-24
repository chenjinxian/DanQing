// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instance rendering parameters implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderInstancesParamsImpl.ts
#include "RenderInstancesParamsImpl.h"

#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderInstancesParamsBuilder::add
// (Ported from: itwinjs-core RenderInstancesParamsImpl.ts builder.add)
// ---------------------------------------------------------------------------
void RenderInstancesParamsBuilder::add(float const* transform, uint32_t featureId)
{
    if (!transform) return;

    m_transforms.insert(m_transforms.end(), transform, transform + 16);
    m_featureIds.push_back(featureId);
}

// ---------------------------------------------------------------------------
// RenderInstancesParamsBuilder::finish
// (Ported from: itwinjs-core RenderInstancesParamsImpl.ts builder.finish)
// ---------------------------------------------------------------------------
RenderInstancesParamsImpl RenderInstancesParamsBuilder::finish()
{
    RenderInstancesParamsImpl result;
    result.instanceTransforms = std::move(m_transforms);
    result.instanceCount = static_cast<uint32_t>(result.instanceTransforms.size() / 16);

    result.features.modelId = m_modelId;
    result.features.data = std::move(m_featureIds);
    result.features.count = result.instanceCount;

    // Reset builder state.
    m_transforms.clear();
    m_featureIds.clear();
    m_modelId = 0;

    return result;
}

END_DQ_RENDER_NAMESPACE
