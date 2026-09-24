// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Vertex LUT Texture implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/VertexLUT.ts
#include "VertexLutTexture.h"

#include <cmath>
#include <cstring>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

VertexLutTexture::~VertexLutTexture()
{
    // Note: caller must call destroy() before destruction
}

bool VertexLutTexture::create(rhi::Driver& driver, uint8_t const* data,
                               uint32_t width, uint32_t height,
                               uint32_t numVertices, uint32_t numRgbaPerVert)
{
    if (!data || numVertices == 0 || numRgbaPerVert == 0 || width == 0 || height == 0) return false;

    // Use the VertexTableBuilder's EXACT (width, height). The builder emits a
    // transposed SoA layout where width % numRgbaPerVert == 0 (a vertex's texels
    // stay on one row); the shader's samplePosition keys off u_vertParams.xy =
    // (width, height). Recomputing pow-2 dims here (the prior code) changed the
    // geometry of that layout, so the shader sampled the wrong texels and every
    // Polyline produced garbage positions → 0 fragments (ACS outlines + labels
    // invisible; only Surface fills, which don't use the LUT, rendered). data is
    // exactly width*height*4 bytes, so uploading to a width×height texture is
    // exact (no overread — the reason the prior pow-2 zero-pad existed).
    uint32_t const texWidth = width;
    uint32_t const texHeight = height;
    uint32_t const texBytes = texWidth * texHeight * 4u;

    m_texture = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
        texWidth, texHeight, 1, rhi::TextureUsage::DEFAULT);

    if (!m_texture) return false;

    rhi::PixelBufferDescriptor pbd(data, texBytes,
                                    0, 0, 0, 0, 0, 0, texWidth, texHeight, 0);
    driver.setTextureData(m_texture, 0, 0, 0, 0, texWidth, texHeight, 0,
                          std::move(pbd));

    m_params.texWidth = texWidth;
    m_params.texHeight = texHeight;
    m_params.numRgbaPerVert = numRgbaPerVert;
    m_params.numVertices = numVertices;

    return true;
}

void VertexLutTexture::destroy(rhi::Driver& driver)
{
    if (m_texture) {
        driver.destroyTexture(m_texture);
        m_texture = {};
    }
}

void VertexLutTexture::setQuantization(float originX, float originY, float originZ,
                                        float scaleX, float scaleY, float scaleZ)
{
    m_qOrigin[0] = originX;
    m_qOrigin[1] = originY;
    m_qOrigin[2] = originZ;
    m_qScale[0] = scaleX;
    m_qScale[1] = scaleY;
    m_qScale[2] = scaleZ;
}

END_DQ_RENDER_NAMESPACE
