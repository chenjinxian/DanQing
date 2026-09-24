// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clip stack implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ClipStack.ts
#include "ClipStack.h"

BEGIN_DQ_RENDER_NAMESPACE

void ClipStack::push(ClipVolume const& volume)
{
    m_stack.push_back(volume);
}

void ClipStack::pop()
{
    if (!m_stack.empty()) {
        m_stack.pop_back();
    }
}

std::vector<ClipPlane> ClipStack::getCombinedPlanes() const
{
    // Combine all planes from all active volumes
    std::vector<ClipPlane> combined;
    for (auto const& volume : m_stack) {
        auto const& planes = volume.getPlanes();
        combined.insert(combined.end(), planes.begin(), planes.end());
    }
    return combined;
}

size_t ClipStack::getActivePlaneCount() const
{
    size_t count = 0;
    for (auto const& volume : m_stack) {
        count += volume.getPlaneCount();
    }
    return count;
}

std::vector<float> ClipStack::buildCombinedTextureData() const
{
    // Combine all planes into a single texture
    std::vector<float> data;
    for (auto const& volume : m_stack) {
        auto volumeData = volume.buildTextureData();
        data.insert(data.end(), volumeData.begin(), volumeData.end());
    }

    // Pad to at least 4 floats
    if (data.empty()) {
        data.resize(4, 0.0f);
    }

    return data;
}

void ClipStack::uploadToGpu(rhi::Driver& driver)
{
    auto data = buildCombinedTextureData();
    uint32_t width = static_cast<uint32_t>(getActivePlaneCount());
    if (width < 1) width = 1;

    if (!m_combinedTexture) {
        m_combinedTexture = driver.createTexture(
            rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA32F,
            width, 1, 1, rhi::TextureUsage::SAMPLEABLE);
    }

    rhi::PixelBufferDescriptor pbd(data.data(), data.size() * sizeof(float),
                                    0, 0);  // format, type (unused for float data)

    driver.setTextureData(m_combinedTexture, 0, 0, 0, 0, width, 1, 1, std::move(pbd));
}

END_DQ_RENDER_NAMESPACE
