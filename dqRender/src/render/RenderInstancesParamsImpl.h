// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instance rendering parameters
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderInstancesParamsImpl.ts
//
// Accumulates per-instance transforms and feature IDs for instanced
// rendering.  The builder collects instances and produces an immutable
// RenderInstancesParamsImpl that can be bound to the GPU.
#pragma once

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// InstancedFeaturesParams — per-instance feature data
// (Ported from: itwinjs-core RenderInstancesParamsImpl.ts)
// ---------------------------------------------------------------------------
struct InstancedFeaturesParams {
    uint32_t modelId = 0;
    std::vector<uint32_t> data;   // per-instance feature IDs
    uint32_t count = 0;
};

// ---------------------------------------------------------------------------
// RenderInstancesParamsImpl — final instance parameters for GPU upload
// (Ported from: itwinjs-core RenderInstancesParamsImpl.ts)
// ---------------------------------------------------------------------------
struct RenderInstancesParamsImpl {
    /// Instance transforms: 16 floats per instance (4x4 matrix, row-major).
    std::vector<float> instanceTransforms;

    /// Number of instances.
    uint32_t instanceCount = 0;

    /// Per-instance feature data.
    InstancedFeaturesParams features;

    /// Check if this instance batch is valid.
    bool isValid() const noexcept { return instanceCount > 0; }

    /// Get the transform for a specific instance.
    /// @param index Instance index (0-based).
    /// @return Pointer to 16 floats (4x4 matrix), or nullptr if out of range.
    float const* getTransform(uint32_t index) const noexcept
    {
        if (index >= instanceCount) return nullptr;
        return instanceTransforms.data() + index * 16;
    }
};

// ---------------------------------------------------------------------------
// RenderInstancesParamsBuilder — accumulates instances before finalizing
// (Ported from: itwinjs-core RenderInstancesParamsImpl.ts
//   RenderInstancesParamsBuilder)
// ---------------------------------------------------------------------------
class RenderInstancesParamsBuilder {
public:
    RenderInstancesParamsBuilder() = default;

    /// add a single instance.
    /// @param transform  4x4 transform matrix (16 floats, row-major).
    /// @param featureId  Feature ID for this instance.
    void add(float const* transform, uint32_t featureId);

    /// finish building and return the immutable params.
    RenderInstancesParamsImpl finish();

    /// Get the number of instances added so far.
    uint32_t getCount() const noexcept { return static_cast<uint32_t>(m_transforms.size() / 16); }

    /// Check if any instances have been added.
    bool hasInstances() const noexcept { return !m_transforms.empty(); }

    /// Set the model ID for the features.
    void setModelId(uint32_t modelId) { m_modelId = modelId; }

private:
    std::vector<float> m_transforms;
    std::vector<uint32_t> m_featureIds;
    uint32_t m_modelId = 0;
};

END_DQ_RENDER_NAMESPACE
