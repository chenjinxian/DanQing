// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — LineCode texture management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/LineCode.ts
#include "LineCode.h"
#include "dqRender/rhi/DriverEnums.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// LineCode::initialize
// Ported from: itwinjs-core webgl/LineCode.ts
// Creates the line code texture with all built-in patterns.
// ---------------------------------------------------------------------------
bool LineCode::initialize(rhi::Driver& driver)
{
    if (m_initialized) return true;

    auto data = buildTextureData();
    uint32_t width = getPatternWidth() * getPatternCount();

    m_texture = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::R8,
        width, 1, 1, rhi::TextureUsage::SAMPLEABLE);

    if (!m_texture) return false;

    rhi::PixelBufferDescriptor pbd(data.data(), data.size(), 0, 0);
    driver.setTextureData(m_texture, 0, 0, 0, 0, width, 1, 1, std::move(pbd));

    m_initialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// LineCode::destroy
// Ported from: itwinjs-core webgl/LineCode.ts
// ---------------------------------------------------------------------------
void LineCode::destroy(rhi::Driver& driver)
{
    if (m_texture) {
        driver.destroyTexture(m_texture);
        m_texture = {};
        m_initialized = false;
    }
}

// ---------------------------------------------------------------------------
// LineCode::buildTextureData
// Ported from: itwinjs-core webgl/LineCode.ts
// Builds the raw pixel data for all line patterns.
// ---------------------------------------------------------------------------
std::vector<uint8_t> LineCode::buildTextureData() const
{
    uint32_t width = getPatternWidth() * getPatternCount();
    std::vector<uint8_t> data(width, 0);

    // Pattern 0: Solid (all on)
    for (uint32_t i = 0; i < getPatternWidth(); ++i) {
        data[i] = 255;
    }

    // Pattern 1: Dashed (8 on, 4 off)
    uint32_t base = getPatternWidth();
    for (uint32_t i = 0; i < getPatternWidth(); ++i) {
        data[base + i] = (i < 8 || (i >= 12 && i < 20) || (i >= 24 && i < 32)) ? 255 : 0;
    }

    // Pattern 2: Dotted (2 on, 2 off)
    base = getPatternWidth() * 2;
    for (uint32_t i = 0; i < getPatternWidth(); ++i) {
        data[base + i] = (i % 4 < 2) ? 255 : 0;
    }

    // Pattern 3: Dash-dot (8 on, 2 off, 2 on, 2 off)
    base = getPatternWidth() * 3;
    for (uint32_t i = 0; i < getPatternWidth(); ++i) {
        uint32_t mod = i % 14;
        data[base + i] = (mod < 8 || (mod >= 10 && mod < 12)) ? 255 : 0;
    }

    // Pattern 4: Dash-dot-dot (8 on, 2 off, 2 on, 2 off, 2 on, 2 off)
    base = getPatternWidth() * 4;
    for (uint32_t i = 0; i < getPatternWidth(); ++i) {
        uint32_t mod = i % 18;
        data[base + i] = (mod < 8 || (mod >= 10 && mod < 12) || (mod >= 14 && mod < 16)) ? 255 : 0;
    }

    return data;
}

END_DQ_RENDER_NAMESPACE
