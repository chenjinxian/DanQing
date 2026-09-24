// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Compositor framebuffer management implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SceneCompositor.ts
//              FrameBuffers inner class (lines 247-511)
#include "CompositorFrameBuffers.h"
#include "CompositorTextures.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Helper: create a render target with MRT color attachments + depth
// ---------------------------------------------------------------------------
static rhi::RenderTargetHandle createFBOImpl(
    rhi::Driver& driver,
    rhi::TextureFormat const* colorFormats,
    uint8_t colorCount,
    rhi::TextureFormat depthFormat,
    uint32_t width, uint32_t height,
    uint8_t samples = 1)
{
    auto flags = rhi::TargetBufferFlags::DEPTH;
    if (colorCount > 0)
        flags = flags | rhi::TargetBufferFlags::COLOR_ALL;

    return driver.createRenderTargetMRT(
        flags, width, height, samples, 1,
        colorCount, colorFormats, depthFormat);
}

// ---------------------------------------------------------------------------
// init — create core FBOs
// Ported from: itwinjs-core FrameBuffers.init() (lines 272-304)
// ---------------------------------------------------------------------------
bool CompositorFrameBuffers::init(rhi::Driver& driver,
                                   CompositorTextures const& textures,
                                   rhi::TextureHandle depth,
                                   uint8_t samples)
{
    (void)depth;
    dispose(&driver);
    m_samples = samples;

    // FBOs must be sized to MATCH the compositor textures — the previous hard-coded
    // 1024x768 left the framebuffer incomplete whenever the viewport differed
    // (attaching a 100x100 color texture to a 1024x768 FBO → desktop GL
    // GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS → draw silently renders zero fragments).
    // Ported from: itwinjs-core FrameBuffers.init() (FBOs created at viewport size).
    uint32_t const w = textures.getWidth();
    uint32_t const h = textures.getHeight();
    if (w == 0 || h == 0)
        return false;

    // depthAndOrder FBO: [depthAndOrder] + depth (pick target, no MSAA)
    {
        rhi::TextureFormat fmts[] = {rhi::TextureFormat::RGBA8};
        m_depthAndOrder = createFBOImpl(driver, fmts, 1, rhi::TextureFormat::DEPTH24, w, h);
    }

    // hilite FBO: [hilite] + depth (no MSAA)
    {
        rhi::TextureFormat fmts[] = {rhi::TextureFormat::RGBA8};
        m_hilite = createFBOImpl(driver, fmts, 1, rhi::TextureFormat::DEPTH24, w, h);
    }

    // opaqueAll MRT FBO: [color, featureId, depthAndOrder] + depth
    // MSAA: opaque render targets use multisampled FBOs when samples > 1.
    {
        rhi::TextureFormat fmts[] = {
            rhi::TextureFormat::RGBA8,  // color
            rhi::TextureFormat::RGBA8,  // featureId
            rhi::TextureFormat::RGBA8   // depthAndOrder
        };
        m_opaqueAll = createFBOImpl(driver, fmts, 3, rhi::TextureFormat::DEPTH24, w, h, m_samples);
    }

    // opaqueAndCompositeAll MRT FBO: [color, featureId, depthAndOrder] + depth
    {
        rhi::TextureFormat fmts[] = {
            rhi::TextureFormat::RGBA8,  // color
            rhi::TextureFormat::RGBA8,  // featureId
            rhi::TextureFormat::RGBA8   // depthAndOrder
        };
        m_opaqueAndCompositeAll = createFBOImpl(driver, fmts, 3, rhi::TextureFormat::DEPTH24, w, h, m_samples);
    }

    // translucent MRT FBO: [accumulation, revealage] + depth
    {
        rhi::TextureFormat fmts[] = {
            rhi::TextureFormat::RGBA16F,  // accumulation
            rhi::TextureFormat::RGBA16F   // revealage
        };
        m_translucent = createFBOImpl(driver, fmts, 2, rhi::TextureFormat::DEPTH24, w, h);
    }

    // clearTranslucent FBO: [accumulation, revealage] (no depth)
    {
        rhi::TextureFormat fmts[] = {
            rhi::TextureFormat::RGBA16F,  // accumulation
            rhi::TextureFormat::RGBA16F   // revealage
        };
        m_clearTranslucent = createFBOImpl(driver, fmts, 2, rhi::TextureFormat::DEPTH24, w, h);
    }

    // pingPong FBO: [accumulation, revealage] (no depth)
    {
        rhi::TextureFormat fmts[] = {
            rhi::TextureFormat::RGBA16F,  // accumulation
            rhi::TextureFormat::RGBA16F   // revealage
        };
        m_pingPong = createFBOImpl(driver, fmts, 2, rhi::TextureFormat::DEPTH24, w, h);
    }

    // edlDrawCol FBO: single color + depth
    {
        rhi::TextureFormat fmts[] = {rhi::TextureFormat::RGBA8};
        m_edlDrawCol = createFBOImpl(driver, fmts, 1, rhi::TextureFormat::DEPTH24, w, h);
    }

    m_initialized = m_opaqueAll && m_opaqueAndCompositeAll &&
                   m_translucent && m_clearTranslucent;

    return m_initialized;
}

// ---------------------------------------------------------------------------
// enableOcclusion — create AO FBOs
// Ported from: itwinjs-core FrameBuffers.enableOcclusion() (lines 356-387)
// ---------------------------------------------------------------------------
bool CompositorFrameBuffers::enableOcclusion(rhi::Driver& driver,
                                              CompositorTextures const& /*textures*/,
                                              rhi::TextureHandle /*depth*/)
{
    if (m_occlusionEnabled)
        return true;

    uint32_t w = 1024, h = 768;  // Will be set from target

    // occlusion FBO: [occlusion] (no depth)
    {
        rhi::TextureFormat fmts[] = {rhi::TextureFormat::RGBA8};
        m_occlusion = createFBOImpl(driver, fmts, 0, rhi::TextureFormat::DEPTH24, w, h);
    }

    // occlusionBlur FBO: [occlusionBlur] (no depth)
    {
        rhi::TextureFormat fmts[] = {rhi::TextureFormat::RGBA8};
        m_occlusionBlur = createFBOImpl(driver, fmts, 0, rhi::TextureFormat::DEPTH24, w, h);
    }

    // opaqueAndCompositeAllHidden FBO: [color, accumulation, revealage] + depth
    {
        rhi::TextureFormat fmts[] = {
            rhi::TextureFormat::RGBA8,    // color
            rhi::TextureFormat::RGBA16F,  // accumulation
            rhi::TextureFormat::RGBA16F   // revealage
        };
        m_opaqueAndCompositeAllHidden = createFBOImpl(driver, fmts, 3, rhi::TextureFormat::DEPTH24, w, h);
    }

    m_occlusionEnabled = true;
    return true;
}

void CompositorFrameBuffers::disableOcclusion(rhi::Driver& driver)
{
    if (!m_occlusionEnabled)
        return;

    if (m_occlusion) driver.destroyRenderTarget(m_occlusion);
    if (m_occlusionBlur) driver.destroyRenderTarget(m_occlusionBlur);
    if (m_opaqueAndCompositeAllHidden) driver.destroyRenderTarget(m_opaqueAndCompositeAllHidden);
    m_occlusion = {};
    m_occlusionBlur = {};
    m_opaqueAndCompositeAllHidden = {};
    m_occlusionEnabled = false;
}

// ---------------------------------------------------------------------------
// enableVolumeClassifier — create volume classifier FBOs
// Ported from: itwinjs-core FrameBuffers.enableVolumeClassifier() (lines 399-434)
// ---------------------------------------------------------------------------
bool CompositorFrameBuffers::enableVolumeClassifier(rhi::Driver& driver,
                                                     CompositorTextures const& textures,
                                                     rhi::TextureHandle depth,
                                                     rhi::TextureHandle volClassDepth)
{
    (void)textures;
    (void)depth;
    (void)volClassDepth;
    if (m_volClassEnabled)
        return true;

    uint32_t w = 1024, h = 768;

    // stencilSet FBO: depth only (stencil, no color attachments)
    {
        m_stencilSet = driver.createRenderTarget(
            rhi::TargetBufferFlags::DEPTH, w, h, 1, 1);
    }

    // volClassCreateBlend FBO: [volClassBlend] + depth
    {
        rhi::TextureFormat fmts[] = {rhi::TextureFormat::RGBA8};
        m_volClassCreateBlend = createFBOImpl(driver, fmts, 1, rhi::TextureFormat::DEPTH24, w, h);
    }

    m_volClassEnabled = true;
    return true;
}

void CompositorFrameBuffers::disableVolumeClassifier(rhi::Driver& driver)
{
    if (!m_volClassEnabled)
        return;

    if (m_stencilSet) driver.destroyRenderTarget(m_stencilSet);
    if (m_altZOnly) driver.destroyRenderTarget(m_altZOnly);
    if (m_volClassCreateBlend) driver.destroyRenderTarget(m_volClassCreateBlend);
    if (m_volClassCreateBlendAltZ) driver.destroyRenderTarget(m_volClassCreateBlendAltZ);
    m_stencilSet = {};
    m_altZOnly = {};
    m_volClassCreateBlend = {};
    m_volClassCreateBlendAltZ = {};
    m_volClassEnabled = false;
}

// ---------------------------------------------------------------------------
// dispose — destroy all FBOs
// ---------------------------------------------------------------------------
void CompositorFrameBuffers::dispose(rhi::Driver* driver)
{
    if (driver) {
        auto destroy = [&](rhi::RenderTargetHandle& h) {
            if (h) driver->destroyRenderTarget(h);
            h = {};
        };
        destroy(m_opaqueColor);
        destroy(m_opaqueAndCompositeColor);
        destroy(m_depthAndOrder);
        destroy(m_hilite);
        destroy(m_opaqueAll);
        destroy(m_opaqueAndCompositeAll);
        destroy(m_translucent);
        destroy(m_clearTranslucent);
        destroy(m_pingPong);
        destroy(m_edlDrawCol);
        destroy(m_occlusion);
        destroy(m_occlusionBlur);
        destroy(m_opaqueAndCompositeAllHidden);
        destroy(m_stencilSet);
        destroy(m_altZOnly);
        destroy(m_volClassCreateBlend);
        destroy(m_volClassCreateBlendAltZ);
    }
    m_initialized = false;
    m_occlusionEnabled = false;
    m_volClassEnabled = false;
}

END_DQ_RENDER_NAMESPACE
