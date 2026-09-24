// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Compositor framebuffer management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SceneCompositor.ts
//              FrameBuffers inner class (lines 247-511)
//
// Manages all framebuffer object (FBO) configurations used by the scene
// compositor for multi-pass rendering. Each FBO defines which color
// attachments and depth buffer are bound for a specific render pass.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class CompositorTextures;

// ---------------------------------------------------------------------------
// CompositorFrameBuffers — FBO management for the scene compositor
// Ported from: itwinjs-core SceneCompositor.ts FrameBuffers class
// ---------------------------------------------------------------------------
class CompositorFrameBuffers {
public:
    CompositorFrameBuffers() = default;
    ~CompositorFrameBuffers() { dispose(); }

    CompositorFrameBuffers(CompositorFrameBuffers const&) = delete;
    CompositorFrameBuffers& operator=(CompositorFrameBuffers const&) = delete;

    /// Initialize core FBOs (always allocated).
    /// Ported from: itwinjs-core FrameBuffers.init() (lines 272-304)
    bool init(rhi::Driver& driver, CompositorTextures const& textures,
              rhi::TextureHandle depth, uint8_t samples = 1);

    /// Enable occlusion FBOs.
    /// Ported from: itwinjs-core FrameBuffers.enableOcclusion() (lines 356-387)
    bool enableOcclusion(rhi::Driver& driver, CompositorTextures const& textures,
                         rhi::TextureHandle depth);

    /// Disable occlusion FBOs.
    void disableOcclusion(rhi::Driver& driver);

    /// Enable volume classifier FBOs.
    /// Ported from: itwinjs-core FrameBuffers.enableVolumeClassifier() (lines 399-434)
    bool enableVolumeClassifier(rhi::Driver& driver, CompositorTextures const& textures,
                                rhi::TextureHandle depth, rhi::TextureHandle volClassDepth);

    /// Disable volume classifier FBOs.
    void disableVolumeClassifier(rhi::Driver& driver);

    /// Dispose all FBOs.
    void dispose(rhi::Driver* driver = nullptr);

    // --- FBO accessors ---
    rhi::RenderTargetHandle getOpaqueColor() const noexcept { return m_opaqueColor; }
    rhi::RenderTargetHandle getOpaqueAndCompositeColor() const noexcept { return m_opaqueAndCompositeColor; }
    rhi::RenderTargetHandle getOpaqueAll() const noexcept { return m_opaqueAll; }
    rhi::RenderTargetHandle getOpaqueAndCompositeAll() const noexcept { return m_opaqueAndCompositeAll; }
    rhi::RenderTargetHandle getDepthAndOrder() const noexcept { return m_depthAndOrder; }
    rhi::RenderTargetHandle getHilite() const noexcept { return m_hilite; }
    rhi::RenderTargetHandle getTranslucent() const noexcept { return m_translucent; }
    rhi::RenderTargetHandle getClearTranslucent() const noexcept { return m_clearTranslucent; }
    rhi::RenderTargetHandle getPingPong() const noexcept { return m_pingPong; }
    rhi::RenderTargetHandle getOcclusion() const noexcept { return m_occlusion; }
    rhi::RenderTargetHandle getOcclusionBlur() const noexcept { return m_occlusionBlur; }
    rhi::RenderTargetHandle getOpaqueAndCompositeAllHidden() const noexcept { return m_opaqueAndCompositeAllHidden; }
    rhi::RenderTargetHandle getEdlDrawCol() const noexcept { return m_edlDrawCol; }

    bool isInitialized() const noexcept { return m_initialized; }

private:
    // Helper to create a render target with color attachments + depth
    rhi::RenderTargetHandle createFBO(rhi::Driver& driver,
                                       rhi::TextureHandle const* colorAttachments,
                                       uint32_t colorCount,
                                       rhi::TextureHandle depth);

    uint8_t m_samples = 1;  // MSAA sample count for render targets

    // Always-on FBOs
    rhi::RenderTargetHandle m_opaqueColor;              // [boundColor] + depth
    rhi::RenderTargetHandle m_opaqueAndCompositeColor;   // [color] + depth
    rhi::RenderTargetHandle m_depthAndOrder;             // [depthAndOrder] + depth
    rhi::RenderTargetHandle m_hilite;                    // [hilite] + depth
    rhi::RenderTargetHandle m_opaqueAll;                 // [boundColor, featureId, depthAndOrder] + depth (MRT)
    rhi::RenderTargetHandle m_opaqueAndCompositeAll;     // [color, featureId, depthAndOrder] + depth (MRT)
    rhi::RenderTargetHandle m_translucent;               // [accumulation, revealage] + depth (MRT)
    rhi::RenderTargetHandle m_clearTranslucent;          // [accumulation, revealage] (no depth)
    rhi::RenderTargetHandle m_pingPong;                  // [accumulation, revealage] (no depth)
    rhi::RenderTargetHandle m_edlDrawCol;                // EDL draw color

    // Occlusion FBOs (on-demand)
    rhi::RenderTargetHandle m_occlusion;                 // [occlusion] (no depth)
    rhi::RenderTargetHandle m_occlusionBlur;             // [occlusionBlur] (no depth)
    rhi::RenderTargetHandle m_opaqueAndCompositeAllHidden; // [color, accumulation, revealage] + depth

    // Volume classifier FBOs (on-demand)
    rhi::RenderTargetHandle m_stencilSet;                // [] + depth (stencil only)
    rhi::RenderTargetHandle m_altZOnly;                  // [] + altDepth
    rhi::RenderTargetHandle m_volClassCreateBlend;       // [volClassBlend] + depth
    rhi::RenderTargetHandle m_volClassCreateBlendAltZ;   // [volClassBlend] + altDepth

    bool m_initialized = false;
    bool m_occlusionEnabled = false;
    bool m_volClassEnabled = false;
};

END_DQ_RENDER_NAMESPACE
