// SPDX-License-Identifier: Apache-2.0
// Authored: no reference decoder test exists in itwinjs-core/imodel-native for
//           DanQing's stb_image wrapper; stb_image behavior is the spec (spec §7.1).
#include <gtest/gtest.h>

#include <dqCommon/Image.h>

#include <cstdint>
#include <fstream>
#include <vector>

namespace {
std::vector<uint8_t> ReadAllBytes(std::string const& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(size);
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    return bytes;
}
}  // namespace

TEST(ImageDecodeTest, DecodesPngToRgba)
{
    auto png = ReadAllBytes("dqCommon/tests/assets/red1x1.png");
    ASSERT_FALSE(png.empty()) << "test asset missing";

    dqCommon::ImageSource src{ png, dqCommon::ImageSourceFormat::Png };
    auto img = dqCommon::DecodeImage(src);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 1);
    EXPECT_EQ(img->getHeight(), 1);
    ASSERT_EQ(img->data.size(), 4u);  // 1×1 RGBA8
    EXPECT_EQ(img->data[0], 255);     // R
    EXPECT_EQ(img->data[1], 0);       // G
    EXPECT_EQ(img->data[2], 0);       // B
    EXPECT_EQ(img->data[3], 255);     // A
}

TEST(ImageDecodeTest, DecodesJpegToRgba)
{
    // JPEG 红像素存在有损偏差——只断言尺寸/通道数与红色占优。
    // 用 PNG 资产转码一次：System.Drawing 无 JPEG 直写 1x1 稳定路径，
    // 故本用例改用解码后的再编码由执行者以 PowerShell 生成 red1x1.jpg（Step 1b）。
    auto jpg = ReadAllBytes("dqCommon/tests/assets/red1x1.jpg");
    ASSERT_FALSE(jpg.empty()) << "test asset missing";
    dqCommon::ImageSource src{ jpg, dqCommon::ImageSourceFormat::Jpeg };
    auto img = dqCommon::DecodeImage(src);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 1);
    EXPECT_EQ(img->getHeight(), 1);
    EXPECT_GT(img->data[0], img->data[1]);  // R > G
    EXPECT_GT(img->data[0], img->data[2]);  // R > B
}

TEST(ImageDecodeTest, InvalidBytesReturnNullopt)
{
    dqCommon::ImageSource src{ std::vector<uint8_t>{0, 1, 2, 3}, dqCommon::ImageSourceFormat::Png };
    auto img = dqCommon::DecodeImage(src);
    EXPECT_FALSE(img.has_value());
}

TEST(ImageDecodeTest, EmptyBytesReturnNullopt)
{
    dqCommon::ImageSource src{ {}, dqCommon::ImageSourceFormat::Png };
    auto img = dqCommon::DecodeImage(src);
    EXPECT_FALSE(img.has_value());
}
