// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature override LUT texture (3-texel-per-feature layout)
//
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/FeatureOverrides.ts
// Manages a LUT texture mapping feature indices to override flags and colors.
// Each feature occupies 3 RGBA texels (12 bytes) in the texture.
//
// LUT layout per feature (3 RGBA texels = 12 bytes):
//   Texel[0]: R=OvrFlags low byte, G=OvrFlags16 high byte, B=line code, A=line weight
//   Texel[1]: R=override red, G=override green, B=override blue, A=override alpha
//   Texel[2]: R=line red, G=line green, B=line blue, A=line alpha
#pragma once

#include "dqRender/Export.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/OvrFlags.h>
#include <dqCommon/PackedFeatureTable.h>

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace rhi {
class Driver;
struct HwTexture;
using TextureHandle = Handle<HwTexture>;
}

// Per-feature override data (CPU side).
// Ported from: itwinjs-core FeatureOverrides feature data
struct FeatureOverrideData {
    dqCommon::OvrFlag flags = dqCommon::OvrFlag::None;
    dqCommon::OvrFlags16 flags16 = dqCommon::OvrFlags16::Visibility; // visible by default
    uint8_t lineCode = 0;
    uint8_t lineWeight = 0;
    uint8_t r = 0, g = 0, b = 0;       // override surface color
    uint8_t alpha = 255;                 // override surface alpha (255=opaque)
    uint8_t lineR = 0, lineG = 0, lineB = 0; // override line color
    uint8_t lineAlpha = 255;             // override line alpha
};

// ---------------------------------------------------------------------------
// FeatureOverrideLUT — manages the 3-texel-per-feature LUT texture
// Ported from: itwinjs-core FeatureOverrides class
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT FeatureOverrideLUT {
public:
    FeatureOverrideLUT();
    ~FeatureOverrideLUT();

    FeatureOverrideLUT(FeatureOverrideLUT const&) = delete;
    FeatureOverrideLUT& operator=(FeatureOverrideLUT const&) = delete;

    /// Initialize with a feature table (creates the LUT data).
    void initialize(const dqCommon::PackedFeatureTable& featureTable);

    /// Set override for a specific feature.
    void setFeatureOverride(uint32_t featureIndex, const FeatureOverrideData& data);

    /// Set visibility for a specific feature.
    void setFeatureVisibility(uint32_t featureIndex, bool visible);

    /// Set flash state for a specific feature.
    void setFeatureFlashed(uint32_t featureIndex, bool flashed);

    /// Set hilite state for a specific feature.
    void setFeatureHilited(uint32_t featureIndex, bool hilited);

    /// Upload the LUT data to a GPU texture (creates or updates).
    void upload(rhi::Driver& driver);

    /// Get the GPU texture handle (invalid if not yet uploaded).
    rhi::TextureHandle getTextureHandle() const noexcept { return m_textureHandle; }

    /// Get texture width.
    uint32_t getWidth() const noexcept { return m_width; }

    /// Get texture height.
    uint32_t getHeight() const noexcept { return m_height; }

    /// Get number of features.
    uint32_t getNumFeatures() const noexcept { return m_numFeatures; }

    /// Check if any feature is hilited.
    /// Ported from: itwinjs-core FeatureOverrides.anyHilited
    bool anyHilited() const noexcept;

    /// Check if all features are hidden.
    /// Ported from: itwinjs-core FeatureOverrides.allHidden
    bool allHidden() const noexcept;

    /// Check if data has been modified since last upload.
    bool isDirty() const noexcept { return m_dirty; }

    /// Get the raw LUT data (for debugging/testing).
    const uint8_t* getRawData() const noexcept { return m_data.data(); }
    size_t getRawDataSize() const noexcept { return m_data.size(); }

    // --- WebGL FeatureOverrides surface ---
    // Ported from: itwinjs-core internal/render/webgl/FeatureOverrides.ts
    // (dqRender's FeatureOverrideLUT holds the LUT that the reference's
    //  FeatureOverrides manages; these accessors expose the webgl surface that
    //  BatchUniforms consumes.)

    /// True when the LUT holds a single feature packed into a 3-texel × 1-row
    /// texture (the uniform-override case — no texture bind needed by the shader).
    /// Ported from: itwinjs-core FeatureOverrides.isUniform (line 70)
    bool isUniform() const noexcept { return m_width == 3u && m_height == 1u; }

    /// The uniform override LUT data (valid when isUniform()). Returns the full
    /// 3-texel × RGBA8 = 12-byte feature record — the same byte array the
    /// multi-feature texture uses, just a single 3-texel row. The uniform shader
    /// path (BatchUniforms) consumes bytes [0..7]: texel 0 = OvrFlags low /
    /// OvrFlags16 high / lineCode / lineWeight; texel 1 = r / g / b / alpha.
    /// Texel 2 ([8..11] = line rgb/alpha) is unused by the uniform path. 1:1 with
    /// the reference, which returns the full _lut.dataBytes.
    /// Ported from: itwinjs-core FeatureOverrides.getUniformOverrides (line 95)
    uint8_t const* getUniformOverrides() const noexcept { return m_data.data(); }

    /// LUT dimensions as {width, height}, for the u_lutParams uniform.
    /// Ported from: itwinjs-core FeatureOverrides.lutParams / bindLUTParams
    void getLutParams(float out[2]) const noexcept
    {
        out[0] = static_cast<float>(m_width);
        out[1] = static_cast<float>(m_height);
    }

    /// True if any feature carries a non-default override.
    /// Ported from: itwinjs-core FeatureOverrides.anyOverridden
    bool anyOverridden() const noexcept
    {
        for (FeatureOverrideData const& o : m_overrides)
            if (o.flags != dqCommon::OvrFlag::None) return true;
        return false;
    }

private:
    /// Compute texture dimensions for the given number of features.
    static void computeDimensions(uint32_t numFeatures, uint32_t& width, uint32_t& height);

    /// Build the LUT byte array from override data.
    void buildLookupTable();

    /// Write one feature's data (3 texels = 12 bytes) into the LUT.
    void writeFeatureTexels(uint32_t featureIndex);

    // CPU-side LUT data (RGBA8).
    std::vector<uint8_t> m_data;

    // Per-feature override data.
    std::vector<FeatureOverrideData> m_overrides;

    // GPU texture handle.
    rhi::TextureHandle m_textureHandle;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_numFeatures = 0;
    bool m_dirty = false;
    bool m_textureCreated = false;
};

END_DQ_RENDER_NAMESPACE
