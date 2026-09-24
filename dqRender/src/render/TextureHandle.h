// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Texture management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Texture.ts
//
// Manages GPU textures.  Wraps the RHI TextureHandle.
//
// Integration point: itwinjs held WebGLTexture and called gl.texImage2D() etc.
// In the RHI approach, textures are created via driver.createTexture() and
// uploaded via driver.update3DImage().
#pragma once

#include "gl/GL.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <functional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TextureHandle — wraps an RHI texture
// (Ported from: itwinjs-core Texture.ts TextureHandle)
// ---------------------------------------------------------------------------
class TextureHandle {
public:
    TextureHandle() = default;
    ~TextureHandle() = default;

    /// Create a 2D texture from raw pixel data.
    /// @param wrap Wrap mode applied after upload (default ClampToEdge, matching the
    ///             RHI creation default; pass Repeat for glTF Normal textures —
    ///             Ported from: itwinjs-core Texture.ts:87-88/300).
    static TextureHandle create2D(rhi::Driver& driver, uint32_t width, uint32_t height,
                                  rhi::TextureFormat format, void const* data,
                                  uint32_t dataSize,
                                  GL::Texture::WrapMode wrap = GL::Texture::WrapMode::ClampToEdge);

    /// Get the RHI texture handle.
    rhi::TextureHandle getRhiHandle() const noexcept { return m_handle; }

    /// Check if the texture is valid.
    bool isValid() const noexcept { return static_cast<bool>(m_handle); }

    uint32_t getWidth() const noexcept { return m_width; }
    uint32_t getHeight() const noexcept { return m_height; }

private:
    rhi::TextureHandle m_handle;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

// ---------------------------------------------------------------------------
// computeBytesUsed — texture memory accounting
// Ported from: itwinjs-core Texture.ts computeBytesUsed
// ---------------------------------------------------------------------------
inline uint32_t computeBytesUsed(uint32_t width, uint32_t height,
                                  GL::Texture::Format format,
                                  GL::Texture::DataType dataType) {
    const uint32_t bytesPerComponent = (dataType == GL::Texture::DataType::UnsignedByte) ? 1 : 4;
    uint32_t componentsPerPixel = 1;
    switch (format) {
        case GL::Texture::Format::Rgb:   componentsPerPixel = 3; break;
        case GL::Texture::Format::Rgba:  componentsPerPixel = 4; break;
        default: break;
    }
    return width * height * componentsPerPixel * bytesPerComponent;
}

// ---------------------------------------------------------------------------
// Texture2DCreateParams — texture creation parameters
// Ported from: itwinjs-core Texture.ts Texture2DCreateParams
// ---------------------------------------------------------------------------
using Load2DImageData = std::function<void(rhi::Driver&, rhi::TextureHandle, uint32_t, uint32_t)>;

class Texture2DCreateParams {
public:
    uint32_t width;
    uint32_t height;
    GL::Texture::Format format;
    GL::Texture::DataType dataType;
    GL::Texture::WrapMode wrapMode;
    Load2DImageData loadImageData;
    bool useMipMaps = false;
    bool interpolate = true;
    int32_t anisotropicFilter = 0;

    Texture2DCreateParams(uint32_t w, uint32_t h,
                          GL::Texture::Format fmt, GL::Texture::DataType dt,
                          GL::Texture::WrapMode wrap,
                          Load2DImageData loader,
                          bool mipmaps = false, bool interp = true, int32_t aniso = 0)
        : width(w), height(h), format(fmt), dataType(dt), wrapMode(wrap)
        , loadImageData(std::move(loader))
        , useMipMaps(mipmaps), interpolate(interp), anisotropicFilter(aniso)
    {
    }

    /// Create params for raw pixel data.
    /// Ported from: itwinjs-core Texture2DCreateParams.createForData
    static Texture2DCreateParams createForData(uint32_t width, uint32_t height,
                                               void const* data, uint32_t dataSize,
                                               GL::Texture::WrapMode wrapMode = GL::Texture::WrapMode::ClampToEdge,
                                               GL::Texture::Format format = GL::Texture::Format::Rgba) {
        auto loader = [data, dataSize, format](rhi::Driver& driver, rhi::TextureHandle texHandle, uint32_t w, uint32_t h) {
            rhi::PixelBufferDescriptor pbd(data, dataSize,
                static_cast<uint32_t>(format), 0);
            driver.setTextureData(texHandle, 0, 0, 0, 0, w, h, 1, std::move(pbd));
        };
        return Texture2DCreateParams(width, height, format,
            GL::Texture::DataType::UnsignedByte, wrapMode, std::move(loader));
    }

    /// Create params for a render target attachment (no initial data).
    /// Ported from: itwinjs-core Texture2DCreateParams.createForAttachment
    static Texture2DCreateParams createForAttachment(uint32_t width, uint32_t height,
                                                     GL::Texture::Format format,
                                                     GL::Texture::DataType dataType) {
        auto loader = [](rhi::Driver&, rhi::TextureHandle, uint32_t, uint32_t) {
            // No data upload for attachments — content rendered by FBO.
        };
        return Texture2DCreateParams(width, height, format, dataType,
            GL::Texture::WrapMode::ClampToEdge, std::move(loader));
    }
};

END_DQ_RENDER_NAMESPACE
