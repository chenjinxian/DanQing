// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clip volume (GPU-based clip planes)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipVolume.ts
//
// Encodes clip planes into a float texture for GPU-based clipping.
// Supports nested clip volumes via ClipStack.
//
// Each clip plane is stored as (nx, ny, nz, d) in view coordinates.
// The fragment shader samples the clip texture to determine if a fragment
// should be clipped.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <array>
#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ClipPlane — a single clip plane (nx, ny, nz, d)
// ---------------------------------------------------------------------------
struct ClipPlane {
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
    float d = 0.0f;

    ClipPlane() = default;
    ClipPlane(float inNx, float inNy, float inNz, float inD)
        : nx(inNx), ny(inNy), nz(inNz), d(inD) {}
};

// ---------------------------------------------------------------------------
// ClipVolume — encodes clip planes into a GPU texture
// (Ported from: itwinjs-core ClipVolume.ts)
// ---------------------------------------------------------------------------
class ClipVolume {
public:
    ClipVolume() = default;
    ~ClipVolume() = default;

    /// Set the clip planes for this volume.
    void setPlanes(std::vector<ClipPlane> const& planes);

    /// Get the clip planes.
    std::vector<ClipPlane> const& getPlanes() const noexcept { return m_planes; }

    /// Get the number of clip planes.
    size_t getPlaneCount() const noexcept { return m_planes.size(); }

    /// Check if this volume has any planes.
    bool isEmpty() const noexcept { return m_planes.empty(); }

    /// Build the packed float texture data for GPU upload.
    /// Returns a vector of floats (4 floats per plane).
    std::vector<float> buildTextureData() const;

    /// Create or update the GPU texture.
    void uploadToGpu(rhi::Driver& driver);

    /// Get the RHI texture handle (valid after uploadToGpu).
    rhi::TextureHandle getTextureHandle() const noexcept { return m_textureHandle; }

    /// Bind the clip texture to the given texture unit.
    void bind(rhi::Driver& driver, uint32_t textureUnit) const;

private:
    std::vector<ClipPlane> m_planes;
    rhi::TextureHandle m_textureHandle;
};

END_DQ_RENDER_NAMESPACE
