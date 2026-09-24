// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TextureHandle implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Texture.ts
#include "TextureHandle.h"

BEGIN_DQ_RENDER_NAMESPACE

TextureHandle TextureHandle::create2D(rhi::Driver& driver, uint32_t width, uint32_t height,
                                      rhi::TextureFormat format, void const* data,
                                      uint32_t dataSize,
                                      GL::Texture::WrapMode wrap)
{
    TextureHandle tex;
    tex.m_width = width;
    tex.m_height = height;

    if (width == 0 || height == 0)
        return tex;

    // Create the GPU texture (1 mip level, 2D, default usage).
    tex.m_handle = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D,
        1,  // levels
        format,
        width, height, 1,  // depth = 1 for 2D
        rhi::TextureUsage::DEFAULT);

    if (!static_cast<bool>(tex.m_handle))
        return tex;

    // Upload pixel data if provided.
    if (data && dataSize > 0) {
        rhi::PixelBufferDescriptor pbd(data, dataSize,
            static_cast<uint32_t>(format),  // format enum as uint32
            0);  // type = 0 (auto)
        driver.setTextureData(
            tex.m_handle, 0,  // level
            0, 0, 0,         // x, y, z offset
            width, height, 1, // depth = 1
            std::move(pbd));
    }

    // Apply the wrap mode explicitly (the RHI clamps at creation).
    // Ported from: itwinjs-core Texture.ts:87-88 — texParameteri(TEXTURE_WRAP_S/T, wrapMode).
    if (tex.m_handle) {
        uint32_t const glWrap = static_cast<uint32_t>(wrap);
        driver.setTextureWrapMode(tex.m_handle, glWrap, glWrap);
    }

    return tex;
}

END_DQ_RENDER_NAMESPACE
