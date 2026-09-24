// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Cubemap texture handle
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Texture.ts
//              (TextureCubeHandle, TextureCubeCreateParams)
//
// Manages a cubemap texture (6 faces) for skybox rendering.
// Each face is uploaded separately via setTextureData with layer index.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CubeFace — cubemap face indices
// ---------------------------------------------------------------------------
enum class CubeFace : uint8_t {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};

// ---------------------------------------------------------------------------
// CubeTextureHandle — wraps a cubemap RHI texture
// (Ported from: itwinjs-core Texture.ts TextureCubeHandle)
// ---------------------------------------------------------------------------
class CubeTextureHandle {
public:
    CubeTextureHandle() = default;
    ~CubeTextureHandle() = default;

    CubeTextureHandle(CubeTextureHandle const&) = delete;
    CubeTextureHandle& operator=(CubeTextureHandle const&) = delete;

    /// Create a cubemap texture.
    /// @param driver RHI driver for texture creation.
    /// @param faceSize Width/height of each face in pixels.
    /// @param format Texture format (default: RGBA8).
    /// @return true on success.
    bool create(rhi::Driver& driver, uint32_t faceSize,
                rhi::TextureFormat format = rhi::TextureFormat::RGBA8);

    /// Upload a single face's pixel data.
    /// @param face Which face to upload.
    /// @param data Pixel data (faceSize*faceSize*4 bytes for RGBA8).
    bool setFaceData(rhi::Driver& driver, CubeFace face, void const* data, uint32_t dataSize);

    /// Destroy the cubemap texture.
    void destroy(rhi::Driver& driver);

    /// Get the RHI texture handle.
    rhi::TextureHandle getRhiHandle() const noexcept { return m_handle; }

    /// Get the face size.
    uint32_t getFaceSize() const noexcept { return m_faceSize; }

    /// Check if the cubemap is valid.
    bool isValid() const noexcept { return static_cast<bool>(m_handle); }

private:
    rhi::TextureHandle m_handle;
    uint32_t m_faceSize = 0;
};

END_DQ_RENDER_NAMESPACE
