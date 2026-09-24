// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Background map drape implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BackgroundMapDrape.ts
#include "BackgroundMapDrape.h"

#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// create
// (Ported from: itwinjs-core BackgroundMapDrape.ts create)
// ---------------------------------------------------------------------------
std::unique_ptr<BackgroundMapDrape> BackgroundMapDrape::create()
{
    auto drape = std::make_unique<BackgroundMapDrape>();

    // Initialize projection matrix to identity.
    std::memset(drape->m_projectionMatrix, 0, sizeof(drape->m_projectionMatrix));
    drape->m_projectionMatrix[0] = 1.0f;
    drape->m_projectionMatrix[5] = 1.0f;
    drape->m_projectionMatrix[10] = 1.0f;
    drape->m_projectionMatrix[15] = 1.0f;

    return drape;
}

// ---------------------------------------------------------------------------
// addGraphic
// (Ported from: itwinjs-core BackgroundMapDrape.ts addGraphic)
// ---------------------------------------------------------------------------
void BackgroundMapDrape::addGraphic(RenderGraphic* graphic)
{
    if (graphic && !m_disposed) {
        m_graphics.push_back(graphic);
    }
}

// ---------------------------------------------------------------------------
// collectGraphics
// (Ported from: itwinjs-core BackgroundMapDrape.ts collectGraphics)
// ---------------------------------------------------------------------------
void BackgroundMapDrape::collectGraphics(/* SceneContext context */)
{
    // Graphics collection is driven by the caller (SceneContext) which
    // populates the graphics list via addGraphic before Draw is called.
    // This method provides the collection phase hook for consistency with
    // the itwinjs lifecycle.
}

// ---------------------------------------------------------------------------
// Draw
// (Ported from: itwinjs-core BackgroundMapDrape.ts draw)
// ---------------------------------------------------------------------------
void BackgroundMapDrape::draw(rhi::Driver& driver)
{
    if (m_disposed || m_graphics.empty()) return;

    // Ensure FBO is allocated.
    if (!m_renderTarget) {
        if (!createFbo(driver, m_width, m_height)) return;
    }

    // Begin render pass to the drape FBO.
    rhi::RenderPassParams params;
    params.clearColor.f[0] = 0.0f;
    params.clearColor.f[1] = 0.0f;
    params.clearColor.f[2] = 0.0f;
    params.clearColor.f[3] = 0.0f;
    params.clearDepth = 1.0f;
    params.flags.clear = rhi::TargetBufferFlags::COLOR_ALL | rhi::TargetBufferFlags::DEPTH;
    driver.beginRenderPass(m_renderTarget, params);

    // Ported from: itwinjs-core BackgroundMapDrape._drawGraphics()
    // Draw each graphic into the FBO using the projection matrix.
    // Each graphic's addCommands() should be called with the drape
    // projection matrix to render into the drape texture.

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// Dispose
// (Ported from: itwinjs-core BackgroundMapDrape.ts dispose)
// ---------------------------------------------------------------------------
void BackgroundMapDrape::dispose(rhi::Driver& driver)
{
    if (m_disposed) return;

    if (m_colorTexture) {
        driver.destroyTexture(m_colorTexture);
        m_colorTexture = {};
    }
    if (m_renderTarget) {
        driver.destroyRenderTarget(m_renderTarget);
        m_renderTarget = {};
    }

    m_graphics.clear();
    m_disposed = true;
}

// ---------------------------------------------------------------------------
// setProjectionMatrix
// (Ported from: itwinjs-core BackgroundMapDrape.ts setProjectionMatrix)
// ---------------------------------------------------------------------------
void BackgroundMapDrape::setProjectionMatrix(float const* matrix)
{
    if (matrix) {
        std::memcpy(m_projectionMatrix, matrix, sizeof(m_projectionMatrix));
    }
}

// ---------------------------------------------------------------------------
// createFbo (private)
// ---------------------------------------------------------------------------
bool BackgroundMapDrape::createFbo(rhi::Driver& driver, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return false;

    m_colorTexture = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
        width, height, 1, rhi::TextureUsage::SAMPLEABLE);

    if (!m_colorTexture) return false;

    // create render target with a color attachment and depth.
    m_renderTarget = driver.createRenderTarget(
        rhi::TargetBufferFlags::COLOR0 | rhi::TargetBufferFlags::DEPTH,
        width, height, 1, 1);

    if (!m_renderTarget) {
        driver.destroyTexture(m_colorTexture);
        m_colorTexture = {};
        return false;
    }

    m_width = width;
    m_height = height;
    return true;
}

END_DQ_RENDER_NAMESPACE
