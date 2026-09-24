// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clip volume implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipVolume.ts
#include "ClipVolume.h"

BEGIN_DQ_RENDER_NAMESPACE

void ClipVolume::setPlanes(std::vector<ClipPlane> const& planes)
{
    m_planes = planes;
}

std::vector<float> ClipVolume::buildTextureData() const
{
    // pack planes into RGBA float texture (4 floats per plane)
    std::vector<float> data;
    data.reserve(m_planes.size() * 4);

    for (auto const& plane : m_planes) {
        data.push_back(plane.nx);
        data.push_back(plane.ny);
        data.push_back(plane.nz);
        data.push_back(plane.d);
    }

    // Pad to at least 4 floats (minimum texture size)
    if (data.empty()) {
        data.resize(4, 0.0f);
    }

    return data;
}

void ClipVolume::uploadToGpu(rhi::Driver& driver)
{
    if (m_planes.empty()) return;

    auto data = buildTextureData();
    uint32_t width = static_cast<uint32_t>(m_planes.size());
    if (width < 1) width = 1;

    // Create or update the texture
    // Each plane is one texel (RGBA32F)
    if (!m_textureHandle) {
        m_textureHandle = driver.createTexture(
            rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA32F,
            width, 1, 1, rhi::TextureUsage::SAMPLEABLE);
    }

    rhi::PixelBufferDescriptor pbd(data.data(), data.size() * sizeof(float),
                                    0, 0);  // format, type (unused for float data)

    driver.setTextureData(m_textureHandle, 0, 0, 0, 0, width, 1, 1, std::move(pbd));
}

void ClipVolume::bind(rhi::Driver& driver, uint32_t textureUnit) const
{
    (void)driver;
    (void)textureUnit;
    // Phase 2+: bind the clip texture to the given texture unit
    // This requires descriptor set support, which will be wired in the compositor.
}

END_DQ_RENDER_NAMESPACE
