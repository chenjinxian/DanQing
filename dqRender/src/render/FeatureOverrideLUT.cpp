// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — FeatureOverrideLUT implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/FeatureOverrides.ts
#include "FeatureOverrideLUT.h"
#include "dqRender/rhi/BufferDescriptor.h"
#include "dqRender/rhi/Driver.h"
#include "gl/GL.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

FeatureOverrideLUT::FeatureOverrideLUT() = default;
FeatureOverrideLUT::~FeatureOverrideLUT() = default;

// ---------------------------------------------------------------------------
// computeDimensions — compute 2D texture dimensions for N features
// Ported from: itwinjs-core computeDimensions() in VertexTable.ts
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::computeDimensions(uint32_t numFeatures, uint32_t& width, uint32_t& height)
{
    // Each feature needs 3 texels. Try to make the texture roughly square.
    uint32_t numTexels = numFeatures * 3;
    uint32_t maxSize = 4096;

    width = std::min(numTexels, maxSize);
    if (width == 0) width = 1;
    height = (numTexels + width - 1) / width;
    if (height == 0) height = 1;
}

// ---------------------------------------------------------------------------
// initialize — set up LUT data for a feature table
// Ported from: itwinjs-core FeatureOverrides._initialize()
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::initialize(const dqCommon::PackedFeatureTable& featureTable)
{
    m_numFeatures = featureTable.getNumFeatures();
    if (m_numFeatures == 0)
        return;

    computeDimensions(m_numFeatures, m_width, m_height);

    // Allocate RGBA8 data (4 bytes per texel).
    size_t totalBytes = static_cast<size_t>(m_width) * m_height * 4;
    m_data.resize(totalBytes, 0);

    // Initialize override data (all features visible, no overrides).
    m_overrides.resize(m_numFeatures);
    for (uint32_t i = 0; i < m_numFeatures; ++i) {
        m_overrides[i].flags16 = dqCommon::OvrFlags16::Visibility;
    }

    buildLookupTable();
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// setFeatureOverride — set override for a specific feature
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::setFeatureOverride(uint32_t featureIndex, const FeatureOverrideData& data)
{
    if (featureIndex >= m_numFeatures)
        return;

    m_overrides[featureIndex] = data;
    writeFeatureTexels(featureIndex);
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// setFeatureVisibility — toggle feature visibility
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::setFeatureVisibility(uint32_t featureIndex, bool visible)
{
    if (featureIndex >= m_numFeatures)
        return;

    auto& flags16 = m_overrides[featureIndex].flags16;
    if (visible)
        flags16 = flags16 | dqCommon::OvrFlags16::Visibility;
    else
        flags16 = static_cast<dqCommon::OvrFlags16>(
            static_cast<uint8_t>(flags16) & ~static_cast<uint8_t>(dqCommon::OvrFlags16::Visibility));

    writeFeatureTexels(featureIndex);
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// setFeatureFlashed — toggle feature flash state
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::setFeatureFlashed(uint32_t featureIndex, bool flashed)
{
    if (featureIndex >= m_numFeatures)
        return;

    auto& flags = m_overrides[featureIndex].flags;
    if (flashed)
        flags = flags | dqCommon::OvrFlag::Flashed;
    else
        flags = static_cast<dqCommon::OvrFlag>(
            static_cast<uint8_t>(flags) & ~static_cast<uint8_t>(dqCommon::OvrFlag::Flashed));

    writeFeatureTexels(featureIndex);
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// setFeatureHilited — toggle feature hilite state
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::setFeatureHilited(uint32_t featureIndex, bool hilited)
{
    if (featureIndex >= m_numFeatures)
        return;

    auto& flags16 = m_overrides[featureIndex].flags16;
    if (hilited)
        flags16 = flags16 | dqCommon::OvrFlags16::Hilited;
    else
        flags16 = static_cast<dqCommon::OvrFlags16>(
            static_cast<uint8_t>(flags16) & ~static_cast<uint8_t>(dqCommon::OvrFlags16::Hilited));

    writeFeatureTexels(featureIndex);
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// buildLookupTable — build the full LUT byte array
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::buildLookupTable()
{
    for (uint32_t i = 0; i < m_numFeatures; ++i) {
        writeFeatureTexels(i);
    }
}

// ---------------------------------------------------------------------------
// writeFeatureTexels — write 3 RGBA texels for one feature
// Ported from: itwinjs-core FeatureOverrides.buildLookupTable()
//
// LUT layout per feature (3 texels = 12 bytes):
//   Texel[0]: R=OvrFlags, G=OvrFlags16, B=lineCode, A=lineWeight
//   Texel[1]: R=r, G=g, B=b, A=alpha
//   Texel[2]: R=lineR, G=lineG, B=lineB, A=lineAlpha
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::writeFeatureTexels(uint32_t featureIndex)
{
    if (featureIndex >= m_numFeatures || m_width == 0)
        return;

    const auto& ovr = m_overrides[featureIndex];

    // Each feature occupies 3 consecutive texels in row-major order.
    uint32_t texelBase = featureIndex * 3;

    // Texel[0]: flags, line code, line weight
    uint32_t tx0 = texelBase % m_width;
    uint32_t ty0 = texelBase / m_width;
    size_t offset0 = (static_cast<size_t>(ty0) * m_width + tx0) * 4;
    if (offset0 + 3 < m_data.size()) {
        m_data[offset0 + 0] = static_cast<uint8_t>(ovr.flags);
        m_data[offset0 + 1] = static_cast<uint8_t>(ovr.flags16);
        m_data[offset0 + 2] = ovr.lineCode;
        m_data[offset0 + 3] = ovr.lineWeight;
    }

    // Texel[1]: override RGB + alpha
    uint32_t tx1 = (texelBase + 1) % m_width;
    uint32_t ty1 = (texelBase + 1) / m_width;
    size_t offset1 = (static_cast<size_t>(ty1) * m_width + tx1) * 4;
    if (offset1 + 3 < m_data.size()) {
        m_data[offset1 + 0] = ovr.r;
        m_data[offset1 + 1] = ovr.g;
        m_data[offset1 + 2] = ovr.b;
        m_data[offset1 + 3] = ovr.alpha;
    }

    // Texel[2]: line RGB + line alpha
    uint32_t tx2 = (texelBase + 2) % m_width;
    uint32_t ty2 = (texelBase + 2) / m_width;
    size_t offset2 = (static_cast<size_t>(ty2) * m_width + tx2) * 4;
    if (offset2 + 3 < m_data.size()) {
        m_data[offset2 + 0] = ovr.lineR;
        m_data[offset2 + 1] = ovr.lineG;
        m_data[offset2 + 2] = ovr.lineB;
        m_data[offset2 + 3] = ovr.lineAlpha;
    }
}

// ---------------------------------------------------------------------------
// upload — create or update the GPU texture
// ---------------------------------------------------------------------------
void FeatureOverrideLUT::upload(rhi::Driver& driver)
{
    if (!m_dirty || m_numFeatures == 0)
        return;

    if (!m_textureCreated) {
        m_textureHandle = driver.createTexture(
            rhi::SamplerType::SAMPLER_2D,
            1,
            rhi::TextureFormat::RGBA8,
            m_width, m_height, 1,
            rhi::TextureUsage::DEFAULT);
        m_textureCreated = true;
    }

    rhi::PixelBufferDescriptor pbd(
        m_data.data(), m_data.size(),
        static_cast<GLenum>(GL::Texture::Format::Rgba),
        static_cast<GLenum>(GL::DataType::UnsignedByte));

    driver.setTextureData(
        m_textureHandle, 0,
        0, 0, 0,
        m_width, m_height, 1,
        std::move(pbd));

    m_dirty = false;
}

// ---------------------------------------------------------------------------
// anyHilited — check if any feature is hilited
// Ported from: itwinjs-core FeatureOverrides.anyHilited
// ---------------------------------------------------------------------------
bool FeatureOverrideLUT::anyHilited() const noexcept
{
    for (auto const& data : m_overrides) {
        if (dqCommon::HasFlag(data.flags16, dqCommon::OvrFlags16::Hilited))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// allHidden — check if all features are hidden
// Ported from: itwinjs-core FeatureOverrides.allHidden
// ---------------------------------------------------------------------------
bool FeatureOverrideLUT::allHidden() const noexcept
{
    for (auto const& data : m_overrides) {
        if (dqCommon::HasFlag(data.flags16, dqCommon::OvrFlags16::Visibility))
            return false;  // At least one visible
    }
    return true;
}

END_DQ_RENDER_NAMESPACE
