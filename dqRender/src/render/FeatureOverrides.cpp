// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature overrides implementation
// Ported from: itwinjs-core core/frontend/src/render/FeatureSymbology.ts
#include "FeatureOverrides.h"

BEGIN_DQ_RENDER_NAMESPACE

void FeatureOverrides::setAppearance(uint32_t featureId, FeatureGpuAppearance const& appearance)
{
    m_overrides[featureId] = appearance;
    if (featureId > m_maxFeatureId) {
        m_maxFeatureId = featureId;
    }
}

void FeatureOverrides::removeAppearance(uint32_t featureId)
{
    m_overrides.erase(featureId);
}

void FeatureOverrides::clear()
{
    m_overrides.clear();
    m_maxFeatureId = 0;
}

FeatureGpuAppearance FeatureOverrides::getAppearance(uint32_t featureId) const
{
    auto it = m_overrides.find(featureId);
    if (it != m_overrides.end()) {
        return it->second;
    }
    return FeatureGpuAppearance::Default();
}

// Build packed RGBA texture data for GPU upload
// Each feature ID maps to 4 bytes: R=color.r, G=color.g, B=color.b, A=alpha
std::vector<uint8_t> FeatureOverrides::buildTextureData() const
{
    if (m_overrides.empty()) return {};

    // Size the texture to cover all feature IDs up to max
    size_t textureSize = static_cast<size_t>(m_maxFeatureId) + 1;
    std::vector<uint8_t> data(textureSize * 4, 255);  // default: white, opaque

    for (auto const& [id, appearance] : m_overrides) {
        size_t offset = static_cast<size_t>(id) * 4;
        if (offset + 3 < data.size()) {
            data[offset + 0] = static_cast<uint8_t>((appearance.rgb >> 16) & 0xFF);  // R
            data[offset + 1] = static_cast<uint8_t>((appearance.rgb >> 8) & 0xFF);   // G
            data[offset + 2] = static_cast<uint8_t>(appearance.rgb & 0xFF);           // B
            data[offset + 3] = appearance.alpha;                                       // A
        }
    }

    return data;
}

END_DQ_RENDER_NAMESPACE
