// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Image types
//
// Ported from: itwinjs-core core/common/src/Image.ts
// ImageBuffer (uncompressed) and ImageSource (compressed JPEG/PNG).
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Format of an ImageBuffer.
// Ported from: itwinjs-core ImageBufferFormat
enum class ImageBufferFormat : uint8_t {
    Rgba = 0,   // 4 bytes per pixel
    Rgb = 2,    // 3 bytes per pixel
    Alpha = 5,  // 1 byte per pixel
};

// Uncompressed rectangular bitmap image data.
// Ported from: itwinjs-core core/common/src/Image.ts
class DQ_COMMON_EXPORT ImageBuffer {
public:
    std::vector<uint8_t> data;
    ImageBufferFormat format;
    int width;

    ImageBuffer(std::vector<uint8_t> imageData, ImageBufferFormat fmt, int w)
        : data(std::move(imageData)), format(fmt), width(w)
    {
    }

    // Number of bytes per pixel.
    // Ported from: itwinjs-core ImageBuffer.numBytesPerPixel
    int getNumBytesPerPixel() const noexcept { return getNumBytesPerPixel(format); }
    static int getNumBytesPerPixel(ImageBufferFormat fmt) noexcept;

    // height in pixels (computed from data length, format, width).
    // Ported from: itwinjs-core ImageBuffer.height
    int getHeight() const noexcept;

    // create a validated ImageBuffer. Returns empty optional if data length doesn't match.
    // Ported from: itwinjs-core ImageBuffer.create()
    static std::optional<ImageBuffer> create(std::vector<uint8_t> data, ImageBufferFormat format, int width);

    // Validate data length matches width/format.
    static bool isValidData(const std::vector<uint8_t>& data, ImageBufferFormat format, int width) noexcept;
};

// Whether a number is a power of two.
// Ported from: itwinjs-core isPowerOfTwo()
inline bool IsPowerOfTwo(int num) noexcept { return num > 0 && (num & (num - 1)) == 0; }

// First power-of-two value >= input.
// Ported from: itwinjs-core nextHighestPowerOfTwo()
int NextHighestPowerOfTwo(int num) noexcept;

// Format of an ImageSource (compressed).
// Ported from: itwinjs-core ImageSourceFormat
enum class ImageSourceFormat : uint8_t {
    Jpeg = 0,
    Png = 2,
    Svg = 3,
};

// True if format is a valid ImageSourceFormat.
// Ported from: itwinjs-core isValidImageSourceFormat()
inline bool IsValidImageSourceFormat(int format) noexcept
{
    return format == static_cast<int>(ImageSourceFormat::Jpeg) ||
           format == static_cast<int>(ImageSourceFormat::Png) ||
           format == static_cast<int>(ImageSourceFormat::Svg);
}

// Image data encoded in JPEG or PNG format.
// Ported from: itwinjs-core core/common/src/Image.ts
class DQ_COMMON_EXPORT ImageSource {
public:
    std::vector<uint8_t> data;
    ImageSourceFormat format;

    ImageSource(std::vector<uint8_t> imageData, ImageSourceFormat fmt)
        : data(std::move(imageData)), format(fmt)
    {
    }

    // True if this is a binary (non-SVG) image source.
    bool isBinary() const noexcept { return format != ImageSourceFormat::Svg; }
};

// Decode a compressed ImageSource (PNG/JPEG) into an RGBA8 ImageBuffer.
// stb_image forces 4 channels regardless of source format.
// Returns std::nullopt on empty input or decode failure.
// Authored: itwinjs decodes via browser createImageBitmap/Image element (no C++
//           equivalent); stb_image stands in per §8.3 approved deviation.
std::optional<ImageBuffer> DecodeImage(ImageSource const& source);

END_DQ_COMMON_NAMESPACE
