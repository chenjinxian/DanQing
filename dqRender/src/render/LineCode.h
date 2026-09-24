// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Line code/line pattern texture
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/LineCode.ts
//
// Line pattern/line code texture management.  Generates the line code
// texture used for dashed/dotted line styles.
//
// The line code texture is a 1D texture where each pixel represents a
// segment of the line pattern.  The fragment shader samples this texture
// to determine if the current fragment should be drawn or discarded.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/LinePixels.h>

#include <array>
#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// LineCode — line pattern definitions
// (Ported from: itwinjs-core LineCode.ts)
// ---------------------------------------------------------------------------
class LineCode {
public:
    /// Built-in line code patterns.
    enum class Pattern : uint8_t {
        Solid = 0,          // Solid line
        Dashed = 1,         // Dashed
        Dotted = 2,         // Dotted
        DashDot = 3,        // Dash-dot
        DashDotDot = 4,     // Dash-dot-dot
        Count
    };

    LineCode() = default;
    ~LineCode() = default;

    LineCode(LineCode const&) = delete;
    LineCode& operator=(LineCode const&) = delete;

    /// Initialize the line code texture.
    /// @param driver RHI driver for texture creation.
    /// @return true on success.
    bool initialize(rhi::Driver& driver);

    /// Destroy the line code texture.
    void destroy(rhi::Driver& driver);

    /// Get the line code texture handle.
    rhi::TextureHandle getTexture() const noexcept { return m_texture; }

    /// Get the number of patterns.
    static constexpr uint32_t getPatternCount()
    {
        return static_cast<uint32_t>(Pattern::Count);
    }

    /// Get the texture width (pixels per pattern).
    static constexpr uint32_t getPatternWidth() { return 32; }

    /// Map a LinePixels value to a line code index.
    /// Ported from: itwinjs-core LineCode.valueFromLinePixels()
    ///
    /// The LinePixels enum uses bit patterns (not sequential integers).
    /// This maps them to sequential indices for texture row lookup.
    static int valueFromLinePixels(dqCommon::LinePixels pixels)
    {
        // Map known LinePixels values to sequential indices.
        // The reference uses a dynamic pattern registry; this is a static mapping.
        switch (pixels) {
            case dqCommon::LinePixels::Solid:      return 0;
            case dqCommon::LinePixels::Code1:      return 1;
            case dqCommon::LinePixels::Code2:      return 2;
            case dqCommon::LinePixels::Code3:      return 3;
            case dqCommon::LinePixels::Code4:      return 4;
            case dqCommon::LinePixels::Code5:      return 5;
            case dqCommon::LinePixels::Code6:      return 6;
            case dqCommon::LinePixels::Code7:      return 7;
            case dqCommon::LinePixels::HiddenLine: return 8;
            default:                               return 0;
        }
    }

private:
    /// Build the line code texture data.
    std::vector<uint8_t> buildTextureData() const;

    rhi::TextureHandle m_texture;
    bool m_initialized = false;
};

END_DQ_RENDER_NAMESPACE
