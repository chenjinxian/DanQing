// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Cubemap texture handle implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Texture.ts
#include "CubeTextureHandle.h"

BEGIN_DQ_RENDER_NAMESPACE

bool CubeTextureHandle::create(rhi::Driver& driver, uint32_t faceSize,
                                rhi::TextureFormat format)
{
    if (faceSize == 0) return false;

    destroy(driver);

    // Create cubemap texture: 6 faces, 1 mip level
    m_handle = driver.createTexture(
        rhi::SamplerType::SAMPLER_CUBEMAP, 1, format,
        faceSize, faceSize, 1, rhi::TextureUsage::SAMPLEABLE);

    if (m_handle) {
        m_faceSize = faceSize;
        return true;
    }
    return false;
}

bool CubeTextureHandle::setFaceData(rhi::Driver& driver, CubeFace face,
                                     void const* data, uint32_t dataSize)
{
    if (!m_handle || !data || dataSize == 0) return false;

    // Upload to the specific face (layer = face index)
    uint32_t layer = static_cast<uint32_t>(face);
    rhi::PixelBufferDescriptor pbd(data, dataSize, 0, 0);
    driver.setTextureData(m_handle, 0, 0, 0, layer, m_faceSize, m_faceSize, 1, std::move(pbd));
    return true;
}

void CubeTextureHandle::destroy(rhi::Driver& driver)
{
    if (m_handle) {
        driver.destroyTexture(m_handle);
        m_handle = {};
        m_faceSize = 0;
    }
}

END_DQ_RENDER_NAMESPACE
