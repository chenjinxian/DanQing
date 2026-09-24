// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Texture drape (reality mesh texture overlay)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/TextureDrape.ts
//
// Manages a texture that is draped over geometry (e.g., orthoimagery on
// terrain).  The drape texture has its own UV transform that maps world
// coordinates to texture coordinates.
//
// During rendering, the drape texture is bound as a secondary texture unit
// and the fragment shader blends it with the base material color.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <array>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TextureDrape — texture overlay for reality meshes
// (Ported from: itwinjs-core TextureDrape.ts)
// ---------------------------------------------------------------------------
class TextureDrape {
public:
    TextureDrape() = default;
    ~TextureDrape() = default;

    TextureDrape(TextureDrape const&) = delete;
    TextureDrape& operator=(TextureDrape const&) = delete;

    /// Create a drape texture from raw pixel data.
    /// @param driver RHI driver for texture creation.
    /// @param width Texture width in pixels.
    /// @param height Texture height in pixels.
    /// @param data RGBA8 pixel data.
    /// @return true on success.
    bool create(rhi::Driver& driver, uint32_t width, uint32_t height,
                void const* data, uint32_t dataSize);

    /// Destroy the drape texture.
    void destroy(rhi::Driver& driver);

    /// Set the UV transform (world-to-texture mapping).
    /// The transform maps (worldX, worldY) to (u, v) via:
    ///   u = scaleU * worldX + offsetU
    ///   v = scaleV * worldY + offsetV
    void setUvTransform(float scaleU, float scaleV, float offsetU, float offsetV);

    /// Get the UV transform as a vec4 (scaleU, scaleV, offsetU, offsetV).
    std::array<float, 4> const& getUvTransform() const noexcept { return m_uvTransform; }

    /// Get the RHI texture handle.
    rhi::TextureHandle getTextureHandle() const noexcept { return m_handle; }

    /// Check if the drape is valid.
    bool isValid() const noexcept { return static_cast<bool>(m_handle); }

    /// Get texture dimensions.
    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }

private:
    rhi::TextureHandle m_handle;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::array<float, 4> m_uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};  // identity
};

END_DQ_RENDER_NAMESPACE
