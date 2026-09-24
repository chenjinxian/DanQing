// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Compositor texture management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SceneCompositor.ts
//              Textures inner class (lines 60-244)
//
// Manages all texture handles used by the scene compositor for multi-pass
// rendering: OIT accumulation/revealage, pick data (featureId, depthAndOrder),
// hilite, color, occlusion, and volume classification.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CompositorTextures — texture handle management for the scene compositor
// Ported from: itwinjs-core SceneCompositor.ts Textures class
// ---------------------------------------------------------------------------
class CompositorTextures {
public:
    CompositorTextures() = default;
    ~CompositorTextures() { dispose(); }

    CompositorTextures(CompositorTextures const&) = delete;
    CompositorTextures& operator=(CompositorTextures const&) = delete;

    /// Initialize core textures (always allocated).
    /// Ported from: itwinjs-core Textures.init() (lines 139-182)
    /// Creates: accumulation, revealage, hilite, color, featureId, depthAndOrder
    bool init(rhi::Driver& driver, uint32_t width, uint32_t height);

    /// Enable occlusion textures (AO pipeline).
    /// Ported from: itwinjs-core Textures.enableOcclusion() (lines 184-195)
    bool enableOcclusion(rhi::Driver& driver, uint32_t width, uint32_t height);

    /// Disable occlusion textures.
    /// Ported from: itwinjs-core Textures.disableOcclusion() (lines 197-202)
    void disableOcclusion(rhi::Driver& driver);

    /// Enable volume classifier textures.
    /// Ported from: itwinjs-core Textures.enableVolumeClassifier() (lines 204-213)
    bool enableVolumeClassifier(rhi::Driver& driver, uint32_t width, uint32_t height);

    /// Disable volume classifier textures.
    /// Ported from: itwinjs-core Textures.disableVolumeClassifier() (lines 215-218)
    void disableVolumeClassifier(rhi::Driver& driver);

    /// Dispose all textures.
    void dispose(rhi::Driver* driver = nullptr);

    // --- Texture accessors ---
    rhi::TextureHandle getAccumulation() const noexcept { return m_accumulation; }
    rhi::TextureHandle getRevealage() const noexcept { return m_revealage; }
    rhi::TextureHandle getColor() const noexcept { return m_color; }
    rhi::TextureHandle getFeatureId() const noexcept { return m_featureId; }
    rhi::TextureHandle getDepthAndOrder() const noexcept { return m_depthAndOrder; }
    rhi::TextureHandle getHilite() const noexcept { return m_hilite; }
    rhi::TextureHandle getOcclusion() const noexcept { return m_occlusion; }
    rhi::TextureHandle getOcclusionBlur() const noexcept { return m_occlusionBlur; }
    rhi::TextureHandle getVolClassBlend() const noexcept { return m_volClassBlend; }

    bool isInitialized() const noexcept { return m_initialized; }
    bool isOcclusionEnabled() const noexcept { return m_occlusionEnabled; }
    bool isVolClassEnabled() const noexcept { return m_volClassEnabled; }

    /// Texture dimensions (the viewport size they were initialized with).
    /// CompositorFrameBuffers.init must size its FBOs to MATCH these textures —
    /// attaching a 100x100 color texture to a hard-coded 1024x768 FBO leaves the
    /// framebuffer incomplete (GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS on desktop GL)
    /// and the draw silently renders zero fragments.
    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    // Core textures (always allocated)
    rhi::TextureHandle m_accumulation;    // RGBA float/half/ubyte — OIT accumulation
    rhi::TextureHandle m_revealage;       // RGBA float/half/ubyte — OIT revealage
    rhi::TextureHandle m_color;           // RGBA ubyte — opaque composite color
    rhi::TextureHandle m_featureId;       // RGBA ubyte — pick: feature ID per pixel
    rhi::TextureHandle m_depthAndOrder;   // RGBA ubyte — pick: encoded depth + render order
    rhi::TextureHandle m_hilite;          // RGBA ubyte — hilite overlay accumulation

    // Occlusion textures (on-demand)
    rhi::TextureHandle m_occlusion;       // RGBA ubyte — raw AO output
    rhi::TextureHandle m_occlusionBlur;   // RGBA ubyte — blurred AO result

    // Volume classifier textures (on-demand)
    rhi::TextureHandle m_volClassBlend;   // RGBA ubyte — volume classifier blend

    bool m_initialized = false;
    bool m_occlusionEnabled = false;
    bool m_volClassEnabled = false;
};

END_DQ_RENDER_NAMESPACE
