// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Texture drape implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/TextureDrape.ts
#include "TextureDrape.h"

BEGIN_DQ_RENDER_NAMESPACE

bool TextureDrape::create(rhi::Driver& driver, uint32_t width, uint32_t height,
                           void const* data, uint32_t dataSize)
{
    if (width == 0 || height == 0 || !data || dataSize == 0) return false;

    destroy(driver);

    m_handle = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
        width, height, 1, rhi::TextureUsage::SAMPLEABLE);

    if (!m_handle) return false;

    m_width = width;
    m_height = height;

    rhi::PixelBufferDescriptor pbd(data, dataSize, 0, 0);
    driver.setTextureData(m_handle, 0, 0, 0, 0, width, height, 1, std::move(pbd));
    return true;
}

void TextureDrape::destroy(rhi::Driver& driver)
{
    if (m_handle) {
        driver.destroyTexture(m_handle);
        m_handle = {};
        m_width = 0;
        m_height = 0;
        m_uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
    }
}

void TextureDrape::setUvTransform(float scaleU, float scaleV, float offsetU, float offsetV)
{
    m_uvTransform = {scaleU, scaleV, offsetU, offsetV};
}

END_DQ_RENDER_NAMESPACE
