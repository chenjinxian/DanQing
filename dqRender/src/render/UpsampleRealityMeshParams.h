// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Upsample reality mesh parameters
// Ported from: itwinjs-core core/frontend/src/internal/render/UpsampleRealityMeshParams.ts
//
// Parameters for reality mesh upsampling.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// UpsampleRealityMeshParams — parameters for reality mesh upsampling
// (Ported from: itwinjs-core UpsampleRealityMeshParams.ts)
// ---------------------------------------------------------------------------
class UpsampleRealityMeshParams {
public:
    UpsampleRealityMeshParams() = default;

    /// Set the target geometric error.
    void setTargetGeometricError(float error) noexcept { m_targetGeometricError = error; }

    /// Get the target geometric error.
    float getTargetGeometricError() const noexcept { return m_targetGeometricError; }

    /// Set the maximum subdivision level.
    void setMaxSubdivisionLevel(uint32_t level) noexcept { m_maxSubdivisionLevel = level; }

    /// Get the maximum subdivision level.
    uint32_t getMaxSubdivisionLevel() const noexcept { return m_maxSubdivisionLevel; }

    /// Set the screen space error threshold.
    void setScreenSpaceError(float sse) noexcept { m_screenSpaceError = sse; }

    /// Get the screen space error threshold.
    float getScreenSpaceError() const noexcept { return m_screenSpaceError; }

private:
    float m_targetGeometricError = 0.5f;
    uint32_t m_maxSubdivisionLevel = 20;
    float m_screenSpaceError = 16.0f;
};

END_DQ_RENDER_NAMESPACE
