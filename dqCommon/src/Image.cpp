// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Image implementation
//
// Ported from: itwinjs-core core/common/src/Image.ts
#include "dqCommon/Image.h"

// stb_image implementation (exactly one TU).
// Authored: decode via stb_image (§8.3 approved deviation, 架构师签字 2026-09-15).
// Global scope (namespace opened below), same shape as cgltf in dqRender/src/gltf/GltfReader.cpp.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO    // we feed bytes, not file paths
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image/stb_image.h>

BEGIN_DQ_COMMON_NAMESPACE

// Ported from: itwinjs-core ImageBuffer.getNumBytesPerPixel()
int ImageBuffer::getNumBytesPerPixel(ImageBufferFormat fmt) noexcept
{
    switch (fmt) {
    case ImageBufferFormat::Alpha: return 1;
    case ImageBufferFormat::Rgb: return 3;
    default: return 4;  // Rgba
    }
}

// Ported from: itwinjs-core ImageBuffer.height
int ImageBuffer::getHeight() const noexcept
{
    const int bpp = getNumBytesPerPixel(format);
    if (bpp == 0 || width == 0)
        return 0;
    return static_cast<int>(data.size()) / (width * bpp);
}

// Ported from: itwinjs-core ImageBuffer.isValidData()
bool ImageBuffer::isValidData(const std::vector<uint8_t>& data, ImageBufferFormat format, int width) noexcept
{
    if (width <= 0)
        return false;
    const int bpp = getNumBytesPerPixel(format);
    if (bpp == 0)
        return false;
    const int bytesPerRow = width * bpp;
    if (bytesPerRow == 0)
        return false;
    // Data size must be an exact multiple of bytesPerRow
    if (static_cast<int>(data.size()) % bytesPerRow != 0)
        return false;
    const int height = static_cast<int>(data.size()) / bytesPerRow;
    return height > 0;
}

// Ported from: itwinjs-core ImageBuffer.create()
std::optional<ImageBuffer> ImageBuffer::create(std::vector<uint8_t> data, ImageBufferFormat format, int width)
{
    if (!isValidData(data, format, width))
        return std::nullopt;
    return ImageBuffer(std::move(data), format, width);
}

// Ported from: itwinjs-core nextHighestPowerOfTwo()
int NextHighestPowerOfTwo(int num) noexcept
{
    if (num <= 0)
        return 1;
    --num;
    for (int i = 1; i < 32; i <<= 1)
        num = num | (num >> i);
    return num + 1;
}

// Authored: see header comment.
std::optional<ImageBuffer> DecodeImage(ImageSource const& source)
{
    if (source.data.empty() || source.format == ImageSourceFormat::Svg)
        return std::nullopt;

    int w = 0, h = 0, n = 0;
    // Force 4 channels (RGBA8) regardless of source.
    auto* rgba = stbi_load_from_memory(source.data.data(),
                                       static_cast<int>(source.data.size()),
                                       &w, &h, &n, 4);
    if (!rgba || w <= 0 || h <= 0)
        return std::nullopt;

    std::vector<uint8_t> bytes(rgba, rgba + static_cast<size_t>(w) * h * 4);
    stbi_image_free(rgba);
    return ImageBuffer::create(std::move(bytes), ImageBufferFormat::Rgba, w);
}

END_DQ_COMMON_NAMESPACE
