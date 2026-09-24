// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Image, Material, TextureMapping unit tests
//
// Ported from: itwinjs-core core/common/src/test/Image.test.ts
#include "dqCommon/Image.h"
#include "dqCommon/MaterialProps.h"
#include "dqCommon/TextureMapping.h"

#include <gtest/gtest.h>

using namespace dqCommon;

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageBuffer") it("should create valid buffer")
TEST(ImageBuffer, CreateValid)
{
    // RGBA, 2x2 = 16 bytes
    std::vector<uint8_t> data(16, 0);
    auto buf = ImageBuffer::create(data, ImageBufferFormat::Rgba, 2);
    ASSERT_TRUE(buf.has_value());
    EXPECT_EQ(buf->width, 2);
    EXPECT_EQ(buf->getHeight(), 2);
    EXPECT_EQ(buf->getNumBytesPerPixel(), 4);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageBuffer") it("should reject invalid buffer")
TEST(ImageBuffer, CreateInvalid)
{
    // Wrong data length
    std::vector<uint8_t> data(10, 0);
    auto buf = ImageBuffer::create(data, ImageBufferFormat::Rgba, 2);
    EXPECT_FALSE(buf.has_value());
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageBuffer") it("should create RGB buffer")
TEST(ImageBuffer, CreateRgb)
{
    // RGB, 3x2 = 18 bytes
    std::vector<uint8_t> data(18, 0);
    auto buf = ImageBuffer::create(data, ImageBufferFormat::Rgb, 3);
    ASSERT_TRUE(buf.has_value());
    EXPECT_EQ(buf->width, 3);
    EXPECT_EQ(buf->getHeight(), 2);
    EXPECT_EQ(buf->getNumBytesPerPixel(), 3);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageBuffer") it("should create alpha buffer")
TEST(ImageBuffer, CreateAlpha)
{
    // Alpha, 4x4 = 16 bytes
    std::vector<uint8_t> data(16, 0);
    auto buf = ImageBuffer::create(data, ImageBufferFormat::Alpha, 4);
    ASSERT_TRUE(buf.has_value());
    EXPECT_EQ(buf->width, 4);
    EXPECT_EQ(buf->getHeight(), 4);
    EXPECT_EQ(buf->getNumBytesPerPixel(), 1);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageBuffer") it("should compute bytes per pixel")
TEST(ImageBuffer, NumBytesPerPixel)
{
    EXPECT_EQ(ImageBuffer::getNumBytesPerPixel(ImageBufferFormat::Alpha), 1);
    EXPECT_EQ(ImageBuffer::getNumBytesPerPixel(ImageBufferFormat::Rgb), 3);
    EXPECT_EQ(ImageBuffer::getNumBytesPerPixel(ImageBufferFormat::Rgba), 4);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageSource") it("should construct")
TEST(ImageSource, Construction)
{
    std::vector<uint8_t> data = {0xFF, 0xD8, 0xFF, 0xE0};  // JPEG magic
    ImageSource src(data, ImageSourceFormat::Jpeg);
    EXPECT_EQ(src.format, ImageSourceFormat::Jpeg);
    EXPECT_TRUE(src.isBinary());
    EXPECT_EQ(src.data.size(), 4u);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageSource") it("SVG should not be binary")
TEST(ImageSource, SvgNotBinary)
{
    std::vector<uint8_t> data = {'<', 's', 'v', 'g', '>'};
    ImageSource src(data, ImageSourceFormat::Svg);
    EXPECT_FALSE(src.isBinary());
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("ImageSource") it("should validate format")
TEST(ImageSource, IsValidFormat)
{
    EXPECT_TRUE(IsValidImageSourceFormat(0));  // Jpeg
    EXPECT_TRUE(IsValidImageSourceFormat(2));  // Png
    EXPECT_TRUE(IsValidImageSourceFormat(3));  // Svg
    EXPECT_FALSE(IsValidImageSourceFormat(1));
    EXPECT_FALSE(IsValidImageSourceFormat(4));
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("Image") it("should detect power of two")
TEST(Image, IsPowerOfTwo)
{
    EXPECT_TRUE(IsPowerOfTwo(1));
    EXPECT_TRUE(IsPowerOfTwo(2));
    EXPECT_TRUE(IsPowerOfTwo(4));
    EXPECT_TRUE(IsPowerOfTwo(256));
    EXPECT_FALSE(IsPowerOfTwo(0));
    EXPECT_FALSE(IsPowerOfTwo(3));
    EXPECT_FALSE(IsPowerOfTwo(5));
    EXPECT_FALSE(IsPowerOfTwo(255));
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("Image") it("should compute next highest power of two")
TEST(Image, NextHighestPowerOfTwo)
{
    EXPECT_EQ(NextHighestPowerOfTwo(0), 1);
    EXPECT_EQ(NextHighestPowerOfTwo(1), 1);
    EXPECT_EQ(NextHighestPowerOfTwo(2), 2);
    EXPECT_EQ(NextHighestPowerOfTwo(3), 4);
    EXPECT_EQ(NextHighestPowerOfTwo(4), 4);
    EXPECT_EQ(NextHighestPowerOfTwo(5), 8);
    EXPECT_EQ(NextHighestPowerOfTwo(255), 256);
    EXPECT_EQ(NextHighestPowerOfTwo(256), 256);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("MaterialProps") it("should have correct texture map units")
TEST(MaterialProps, TextureMapUnits)
{
    EXPECT_EQ(static_cast<int>(TextureMapUnits::Relative), 0);
    EXPECT_EQ(static_cast<int>(TextureMapUnits::Meters), 3);
    EXPECT_EQ(static_cast<int>(TextureMapUnits::Millimeters), 4);
    EXPECT_EQ(static_cast<int>(TextureMapUnits::Feet), 5);
    EXPECT_EQ(static_cast<int>(TextureMapUnits::Inches), 6);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("MaterialProps") it("should have correct normal map flags")
TEST(MaterialProps, NormalMapFlags)
{
    EXPECT_EQ(static_cast<int>(NormalMapFlags::None), 0);
    EXPECT_EQ(static_cast<int>(NormalMapFlags::GreenUp), 1);
    EXPECT_EQ(static_cast<int>(NormalMapFlags::UseConstantLod), 2);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("MaterialProps") it("should have correct texture mapping mode")
TEST(MaterialProps, TextureMappingMode)
{
    EXPECT_EQ(static_cast<int>(TextureMappingMode::None), -1);
    EXPECT_EQ(static_cast<int>(TextureMappingMode::Parametric), 0);
    EXPECT_EQ(static_cast<int>(TextureMappingMode::ElevationDrape), 1);
    EXPECT_EQ(static_cast<int>(TextureMappingMode::Planar), 2);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("TextureMapping") it("Trans2x3 identity")
TEST(TextureMapping, Trans2x3Identity)
{
    const auto& id = TextureTrans2x3::identity();
    // identity should have default values
    EXPECT_EQ(id.compare(id), 0);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("TextureMapping") it("Trans2x3 compare")
TEST(TextureMapping, Trans2x3Compare)
{
    const TextureTrans2x3 a;
    const TextureTrans2x3 b;
    EXPECT_EQ(a.compare(b), 0);

    const TextureTrans2x3 c(2.0, 0, 0, 0, 2.0, 0);
    EXPECT_NE(a.compare(c), 0);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("TextureMapping") it("params default")
TEST(TextureMapping, ParamsDefault)
{
    const TextureMappingParams params;
    EXPECT_DOUBLE_EQ(params.weight, 1.0);
    EXPECT_EQ(params.mode, TextureMappingMode::Parametric);
    EXPECT_FALSE(params.worldMapping);
    EXPECT_FALSE(params.useConstantLod);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("TextureMapping") it("params compare")
TEST(TextureMapping, ParamsCompare)
{
    const TextureMappingParams a;
    const TextureMappingParams b;
    EXPECT_EQ(a.compare(b), 0);

    TextureMappingParams c;
    c.weight = 0.5;
    EXPECT_NE(a.compare(c), 0);
}

// Ported from: itwinjs-core core/common/src/test/Image.test.ts
//              describe("TextureMapping") it("compare")
TEST(TextureMapping, Compare)
{
    const TextureMapping a;
    const TextureMapping b;
    EXPECT_EQ(a.compare(b), 0);
}
