// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Auxiliary channel table implementation
// Ported from: itwinjs-core core/frontend/src/common/internal/render/AuxChannelTable.ts
#include "AuxChannelTable.h"
#include "dqRender/rhi/Driver.h"
#include "gl/RenderFlags.h"

BEGIN_DQ_RENDER_NAMESPACE

AuxChannelTable::AuxChannelTable(
    std::vector<uint8_t> data, uint32_t width, uint32_t height,
    uint32_t numVertices, uint32_t numBytesPerVertex,
    std::vector<AuxDisplacementChannel> displacements,
    std::vector<AuxChannel> normals,
    std::vector<AuxParamChannel> params)
    : m_data(std::move(data))
    , m_width(width)
    , m_height(height)
    , m_numVertices(numVertices)
    , m_numBytesPerVertex(numBytesPerVertex)
    , m_displacements(std::move(displacements))
    , m_normals(std::move(normals))
    , m_params(std::move(params))
{
}

std::unique_ptr<AuxChannelTable> AuxChannelTable::create(
    std::vector<uint8_t> data, uint32_t width, uint32_t height,
    uint32_t numVertices, uint32_t numBytesPerVertex,
    std::vector<AuxDisplacementChannel> displacements,
    std::vector<AuxChannel> normals,
    std::vector<AuxParamChannel> params)
{
    if (data.empty() || width == 0 || height == 0)
        return nullptr;

    return std::unique_ptr<AuxChannelTable>(new AuxChannelTable(
        std::move(data), width, height, numVertices, numBytesPerVertex,
        std::move(displacements), std::move(normals), std::move(params)));
}

rhi::TextureHandle AuxChannelTable::createLutTexture(rhi::Driver& driver) const
{
    if (m_texture)
        return m_texture;

    // Create RGBA8 texture with packed animation data
    // Ported from: itwinjs-core VertexLutTexture pattern
    m_texture = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D,
        1,  // levels
        rhi::TextureFormat::RGBA8,
        m_width,
        m_height,
        1,  // depth
        rhi::TextureUsage::DEFAULT);

    if (!m_texture)
        return m_texture;

    // Upload packed RGBA8 data
    rhi::PixelBufferDescriptor pbd(
        m_data.data(),
        m_data.size(),
        0,  // format (unused for RGBA8)
        0,  // type (unused for RGBA8)
        0,  // stride
        1,  // alignment
        0, 0,  // left, top
        m_width, m_height, 1);  // width, height, depth

    driver.setTextureData(m_texture, 0, 0, 0, 0, m_width, m_height, 0, std::move(pbd));

    return m_texture;
}

END_DQ_RENDER_NAMESPACE
