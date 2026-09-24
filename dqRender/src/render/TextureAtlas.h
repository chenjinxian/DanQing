// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Texture atlas (rect-packing)
// Ported from: itwinjs-core core/frontend/src/render/TextureAtlas.ts
//
// Packs multiple small textures into a single atlas texture to reduce
// texture bind calls.  Uses a simple Skyline Bottom-Left bin-packing
// algorithm suitable for BIM material swatches and feature override strips.
//
// Note: itwinjs-core has no texture atlas (noted in exploration).  This is a
// DanQing-specific optimization using standard rect-packing.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// AtlasRect — a rectangle within the atlas
// ---------------------------------------------------------------------------
struct AtlasRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    /// Get UV coordinates (normalized 0-1).
    float getU0(float atlasWidth) const noexcept { return static_cast<float>(x) / atlasWidth; }
    float getV0(float atlasHeight) const noexcept { return static_cast<float>(y) / atlasHeight; }
    float getU1(float atlasWidth) const noexcept
    {
        return static_cast<float>(x + width) / atlasWidth;
    }
    float getV1(float atlasHeight) const noexcept
    {
        return static_cast<float>(y + height) / atlasHeight;
    }
};

// ---------------------------------------------------------------------------
// TextureAtlas — rect-packing texture atlas
// ---------------------------------------------------------------------------
class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas() = default;

    TextureAtlas(TextureAtlas const&) = delete;
    TextureAtlas& operator=(TextureAtlas const&) = delete;

    /// Initialize the atlas with the given dimensions.
    /// @param width Atlas width in pixels (should be power-of-two).
    /// @param height Atlas height in pixels (should be power-of-two).
    void init(uint32_t width, uint32_t height);

    /// add a rectangle to the atlas (Skyline Bottom-Left packing).
    /// @param width Rectangle width in pixels.
    /// @param height Rectangle height in pixels.
    /// @return The packed rectangle position, or {0,0,0,0} if it doesn't fit.
    AtlasRect addRect(uint32_t width, uint32_t height);

    /// Upload the atlas texture to the GPU.
    /// @param driver RHI driver for texture creation.
    /// @param data Pixel data (RGBA8 format, width*height*4 bytes).
    /// @return true on success.
    bool uploadToGpu(rhi::Driver& driver, uint8_t const* data);

    /// Get the RHI texture handle.
    rhi::TextureHandle getTextureHandle() const noexcept { return m_texture; }

    /// Get atlas dimensions.
    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }

    /// Check if the atlas is valid (uploaded to GPU).
    bool isValid() const noexcept { return static_cast<bool>(m_texture); }

    /// Get the number of packed rectangles.
    size_t getRectCount() const noexcept { return m_rects.size(); }

    /// Get a packed rectangle by index.
    AtlasRect const& getRect(size_t index) const { return m_rects[index]; }

private:
    // Skyline node for bin-packing
    struct SkylineNode {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
    };

    /// Find the best skyline position for a rectangle.
    bool findPosition(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY);

    /// Update the skyline after placing a rectangle.
    void addSkylineNode(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<SkylineNode> m_skyline;
    std::vector<AtlasRect> m_rects;
    rhi::TextureHandle m_texture;
};

END_DQ_RENDER_NAMESPACE
