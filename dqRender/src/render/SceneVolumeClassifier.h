// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scene volume classifier
// Ported from: itwinjs-core core/frontend/src/internal/render/SceneVolumeClassifier.ts
//
// Volume classification for scene rendering.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// VolumeClassifiedRealityData — volume-classified reality data
// ---------------------------------------------------------------------------
struct VolumeClassifiedRealityData {
    uint32_t modelId = 0;
    float geometricError = 1.0f;
    bool enabled = true;
};

// ---------------------------------------------------------------------------
// SceneVolumeClassifier — volume classification for scene
// (Ported from: itwinjs-core SceneVolumeClassifier.ts)
// ---------------------------------------------------------------------------
class SceneVolumeClassifier {
public:
    SceneVolumeClassifier() = default;

    /// Set the classifier model ID.
    void setModelId(uint32_t id) noexcept { m_modelId = id; }

    /// Get the classifier model ID.
    uint32_t getModelId() const noexcept { return m_modelId; }

    /// Set the classification type.
    void setType(uint8_t type) noexcept { m_type = type; }

    /// Get the classification type.
    uint8_t getType() const noexcept { return m_type; }

    /// Check if the classifier is active.
    bool isActive() const noexcept { return m_modelId != 0; }

    /// add a volume-classified reality data entry.
    void addRealityData(VolumeClassifiedRealityData const& data)
    {
        m_realityData.push_back(data);
    }

    /// Get the reality data entries.
    std::vector<VolumeClassifiedRealityData> const& getRealityData() const noexcept
    {
        return m_realityData;
    }

private:
    uint32_t m_modelId = 0;
    uint8_t m_type = 0;
    std::vector<VolumeClassifiedRealityData> m_realityData;
};

END_DQ_RENDER_NAMESPACE
