// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clip stack (nested clip volume management)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipStack.ts
//
// Manages a stack of nested ClipVolumes.  When rendering, all active clip
// planes are combined into a single texture for the fragment shader.
// Supports inside/outside coloring and intersection highlighting.
#pragma once

#include "ClipVolume.h"

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ClipStack — manages nested clip volumes
// (Ported from: itwinjs-core ClipStack.ts)
// ---------------------------------------------------------------------------
class ClipStack {
public:
    ClipStack() = default;
    ~ClipStack() = default;

    /// Push a clip volume onto the stack.
    void push(ClipVolume const& volume);

    /// Pop the top clip volume.
    void pop();

    /// Get the combined clip planes from all active volumes.
    std::vector<ClipPlane> getCombinedPlanes() const;

    /// Get the total number of active clip planes.
    size_t getActivePlaneCount() const;

    /// Check if any clip volumes are active.
    bool isEmpty() const noexcept { return m_stack.empty(); }

    /// Build the combined texture data for all active clip volumes.
    std::vector<float> buildCombinedTextureData() const;

    /// Upload the combined clip texture to the GPU.
    void uploadToGpu(rhi::Driver& driver);

    /// Get the combined texture handle.
    rhi::TextureHandle getTextureHandle() const noexcept { return m_combinedTexture; }

private:
    std::vector<ClipVolume> m_stack;
    rhi::TextureHandle m_combinedTexture;
};

END_DQ_RENDER_NAMESPACE
