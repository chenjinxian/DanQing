// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Compositor texture management implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SceneCompositor.ts
//              Textures inner class (lines 60-244)
#include "CompositorTextures.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Helper: create a 2D RGBA texture
// ---------------------------------------------------------------------------
static rhi::TextureHandle createTexture2D(rhi::Driver& driver,
                                           uint32_t width, uint32_t height,
                                           rhi::TextureFormat texFormat)
{
    return driver.createTexture(
        rhi::SamplerType::SAMPLER_2D,  // target
        1,                              // levels (mip 0 only)
        texFormat,                      // format
        width, height, 1,               // dimensions
        rhi::TextureUsage::DEFAULT      // usage
    );
}

// ---------------------------------------------------------------------------
// init — create core textures
// Ported from: itwinjs-core Textures.init() (lines 139-182)
// ---------------------------------------------------------------------------
bool CompositorTextures::init(rhi::Driver& driver, uint32_t width, uint32_t height)
{
    dispose(&driver);

    // Track the texture dimensions so CompositorFrameBuffers.init sizes its FBOs
    // to match (a hard-coded 1024x768 FBO with 100x100 textures is incomplete).
    m_width = width;
    m_height = height;

    // OIT textures: RGBA float for accumulation and revealage
    // Ported from: itwinjs-core Textures.init() pixelDataType selection
    m_accumulation = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA16F);
    m_revealage = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA16F);

    // Core pick/display textures: RGBA unsigned byte
    m_hilite = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);
    m_color = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);
    m_featureId = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);
    m_depthAndOrder = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);

    m_initialized = static_cast<bool>(m_accumulation) && static_cast<bool>(m_revealage) &&
                   static_cast<bool>(m_hilite) && static_cast<bool>(m_color) &&
                   static_cast<bool>(m_featureId) && static_cast<bool>(m_depthAndOrder);

    return m_initialized;
}

// ---------------------------------------------------------------------------
// enableOcclusion — create AO textures
// Ported from: itwinjs-core Textures.enableOcclusion() (lines 184-195)
// ---------------------------------------------------------------------------
bool CompositorTextures::enableOcclusion(rhi::Driver& driver,
                                          uint32_t width, uint32_t height)
{
    if (m_occlusionEnabled)
        return true;

    m_occlusion = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);
    m_occlusionBlur = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);

    m_occlusionEnabled = static_cast<bool>(m_occlusion) && static_cast<bool>(m_occlusionBlur);
    return m_occlusionEnabled;
}

// ---------------------------------------------------------------------------
// disableOcclusion — dispose AO textures
// Ported from: itwinjs-core Textures.disableOcclusion() (lines 197-202)
// ---------------------------------------------------------------------------
void CompositorTextures::disableOcclusion(rhi::Driver& driver)
{
    if (!m_occlusionEnabled)
        return;

    if (m_occlusion) driver.destroyTexture(m_occlusion);
    if (m_occlusionBlur) driver.destroyTexture(m_occlusionBlur);
    m_occlusion = {};
    m_occlusionBlur = {};
    m_occlusionEnabled = false;
}

// ---------------------------------------------------------------------------
// enableVolumeClassifier — create volume classifier textures
// Ported from: itwinjs-core Textures.enableVolumeClassifier() (lines 204-213)
// ---------------------------------------------------------------------------
bool CompositorTextures::enableVolumeClassifier(rhi::Driver& driver,
                                                 uint32_t width, uint32_t height)
{
    if (m_volClassEnabled)
        return true;

    m_volClassBlend = createTexture2D(driver, width, height, rhi::TextureFormat::RGBA8);

    m_volClassEnabled = static_cast<bool>(m_volClassBlend);
    return m_volClassEnabled;
}

// ---------------------------------------------------------------------------
// disableVolumeClassifier — dispose volume classifier textures
// Ported from: itwinjs-core Textures.disableVolumeClassifier() (lines 215-218)
// ---------------------------------------------------------------------------
void CompositorTextures::disableVolumeClassifier(rhi::Driver& driver)
{
    if (!m_volClassEnabled)
        return;

    if (m_volClassBlend) driver.destroyTexture(m_volClassBlend);
    m_volClassBlend = {};
    m_volClassEnabled = false;
}

// ---------------------------------------------------------------------------
// dispose — destroy all textures
// ---------------------------------------------------------------------------
void CompositorTextures::dispose(rhi::Driver* driver)
{
    if (driver) {
        if (m_accumulation) driver->destroyTexture(m_accumulation);
        if (m_revealage) driver->destroyTexture(m_revealage);
        if (m_color) driver->destroyTexture(m_color);
        if (m_featureId) driver->destroyTexture(m_featureId);
        if (m_depthAndOrder) driver->destroyTexture(m_depthAndOrder);
        if (m_hilite) driver->destroyTexture(m_hilite);
        if (m_occlusion) driver->destroyTexture(m_occlusion);
        if (m_occlusionBlur) driver->destroyTexture(m_occlusionBlur);
        if (m_volClassBlend) driver->destroyTexture(m_volClassBlend);
    }
    m_accumulation = {};
    m_revealage = {};
    m_color = {};
    m_featureId = {};
    m_depthAndOrder = {};
    m_hilite = {};
    m_occlusion = {};
    m_occlusionBlur = {};
    m_volClassBlend = {};
    m_initialized = false;
    m_occlusionEnabled = false;
    m_volClassEnabled = false;
}

END_DQ_RENDER_NAMESPACE
