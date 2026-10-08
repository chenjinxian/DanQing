// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Gradient thematic + produceImage tests
//
// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("getImage") / describe("produceImage")
#include <dqCommon/ColorDef.h>
#include <dqCommon/Gradient.h>
#include <dqCommon/Image.h>
#include <dqCommon/ThematicDisplay.h>

#include <gtest/gtest.h>

using namespace dqCommon;

namespace {
GradientKeyColorProps KeyColor(double value, uint32_t color)
{
    GradientKeyColorProps p;
    p.value = value;
    p.color = color;
    return p;
}
}  // namespace

// Ported from: itwinjs-core Gradient.test.ts
//              describe("getImage") it("produces an image of the specified dimensions")
TEST(GradientGetImageTest, ProducesImageOfSpecifiedDimensions)
{
    GradientSymbProps props;
    props.mode = GradientMode::Linear;
    props.keys = {KeyColor(0.65, 100), KeyColor(0.12, 100)};
    const auto symb = GradientSymb::fromJSON(props);

    const auto img = symb.getImage(123, 456);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 123);
    EXPECT_EQ(img->getHeight(), 456);
}

// Ported from: itwinjs-core Gradient.test.ts
//              it("constraints width of thematic image to 1")
TEST(GradientGetImageTest, ConstrainsThematicImageWidthToOne)
{
    GradientSymbProps props;
    props.mode = GradientMode::Thematic;
    props.keys = {KeyColor(0.65, 100), KeyColor(0.12, 100)};
    const auto symb = GradientSymb::fromJSON(props);

    const auto img = symb.getImage(123, 456);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 1);
    EXPECT_EQ(img->getHeight(), 456);
}

// Ported from: itwinjs-core Gradient.test.ts
//              describe("produceImage") it("produces an image of the specified dimensions")
TEST(GradientProduceImageTest, ProducesImageOfSpecifiedDimensions)
{
    GradientSymbProps props;
    props.mode = GradientMode::Linear;
    props.keys = {KeyColor(0.65, 100), KeyColor(0.12, 100)};
    const auto symb = GradientSymb::fromJSON(props);

    ProduceImageArgs args;
    args.width = 123;
    args.height = 456;
    const auto img = symb.produceImage(args);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 123);
    EXPECT_EQ(img->getHeight(), 456);
}

// Ported from: itwinjs-core Gradient.test.ts
//              it("does not constrain dimensions of thematic images") — produceImage keeps width.
TEST(GradientProduceImageTest, DoesNotConstrainThematicDimensions)
{
    GradientSymbProps props;
    props.mode = GradientMode::Thematic;
    props.keys = {KeyColor(0.65, 100), KeyColor(0.12, 100)};
    const auto symb = GradientSymb::fromJSON(props);

    ProduceImageArgs args;
    args.width = 50;
    args.height = 100;
    const auto img = symb.produceImage(args);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 50);   // produceImage does NOT force width=1
    EXPECT_EQ(img->getHeight(), 100);
}

namespace {
// 参考 Gradient.test.ts 的 getPixel 打包形：(a<<24)|(r<<16)|(g<<8)|b。
uint32_t GetPixelPacked(ImageBuffer const& img, int x, int y)
{
    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(img.width) +
                        static_cast<size_t>(x)) * 4;
    return (static_cast<uint32_t>(img.data[idx + 3]) << 24) |
           (static_cast<uint32_t>(img.data[idx + 0]) << 16) |
           (static_cast<uint32_t>(img.data[idx + 1]) << 8) |
           (static_cast<uint32_t>(img.data[idx + 2]));
}
}  // namespace

// Ported from: itwinjs-core Gradient.test.ts
//              it("includes thematic margin color")
TEST(GradientGetImageTest, IncludesThematicMarginColor)
{
    GradientSymbProps props;
    props.mode = GradientMode::Thematic;
    props.keys = {KeyColor(0.65, 100), KeyColor(0.12, 100)};
    ThematicGradientSettingsProps ts;
    ts.marginColor = 0x00ff00;
    props.thematicSettings = ts;
    const auto symb = GradientSymb::fromJSON(props);

    const auto img = symb.getImage(1, 8192);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(GetPixelPacked(*img, 0, 8191), 0xff00ff00u);
    EXPECT_NE(GetPixelPacked(*img, 0, 127), 0xff00ff00u);
    EXPECT_EQ(GetPixelPacked(*img, 0, 0), 0xff00ff00u);
}

// Ported from: itwinjs-core Gradient.test.ts
//              it("allows thematic margin color to be included or omitted")
TEST(GradientProduceImageTest, ThematicMarginColorIncludedOrOmitted)
{
    GradientSymbProps props;
    props.mode = GradientMode::Thematic;
    props.keys = {KeyColor(0.65, 100), KeyColor(0.12, 100)};
    ThematicGradientSettingsProps ts;
    ts.marginColor = 0x00ff00;
    props.thematicSettings = ts;
    const auto symb = GradientSymb::fromJSON(props);

    ProduceImageArgs args;
    args.width = 1;
    args.height = 8192;
    args.includeThematicMargin = true;
    auto img = symb.produceImage(args);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(GetPixelPacked(*img, 0, 8191), 0xff00ff00u);
    EXPECT_NE(GetPixelPacked(*img, 0, 127), 0xff00ff00u);
    EXPECT_EQ(GetPixelPacked(*img, 0, 0), 0xff00ff00u);

    args.includeThematicMargin = false;
    img = symb.produceImage(args);
    ASSERT_TRUE(img.has_value());
    EXPECT_NE(GetPixelPacked(*img, 0, 8191), 0xff00ff00u);
    EXPECT_NE(GetPixelPacked(*img, 0, 127), 0xff00ff00u);
    EXPECT_NE(GetPixelPacked(*img, 0, 0), 0xff00ff00u);

    args.includeThematicMargin = false;  // ProduceImageArgs 默认 false（缺席语义）
    img = symb.produceImage(args);
    ASSERT_TRUE(img.has_value());
    EXPECT_NE(GetPixelPacked(*img, 0, 8191), 0xff00ff00u);
    EXPECT_NE(GetPixelPacked(*img, 0, 127), 0xff00ff00u);
    EXPECT_NE(GetPixelPacked(*img, 0, 0), 0xff00ff00u);
}

// Authored: createThematic produces a thematic-mode symb with fixed-scheme keys.
// (ref Gradient.Symb.createThematic lines 140-158)
TEST(GradientCreateThematicTest, CreateThematicWithFixedScheme)
{
    auto settings = ThematicGradientSettings::defaults();
    settings.colorScheme = ThematicGradientColorScheme::BlueRed;  // index 0 → Blue-Red fixed scheme
    const auto symb = GradientSymb::createThematic(settings);
    EXPECT_EQ(symb.mode, GradientMode::Thematic);
    EXPECT_EQ(symb.keys.size(), 5u);  // Blue-Red fixed scheme has 5 keys
}

// Authored: no reference test pins the fixed-scheme key colors——值锚
// Gradient.ts:131-136 _fixedSchemeKeys 逐字（rbg 序）。M-S S-d 取证修复：
// DanQing 原表归一 (r,g,b) 存储却保留参考 (1,3,2) 调用序 → 非对称行 g/b
// 互换（BlueRed 0 值端绿而非蓝——渐变带内容错误）。本锁钉 BlueRed 全 5 键
// 与 RedBlue/Monochrome 端点。
TEST(GradientCreateThematicTest, FixedSchemeKeyColorsAreReferenceExact)
{
    // tbgr = 0xTTBBGGRR。BlueRed 参考五行 rbg（[v,r,b,g]）经 (kv[1],kv[3],
    // kv[2]) 调用：[0,0,255,0]→蓝(0,0,255)=0x00FF0000；[0.25,0,255,255]→
    // 青(0,255,255)=0x00FFFF00；[0.5,0,0,255]→绿(0,255,0)=0x0000FF00；
    // [0.75,255,0,255]→黄(255,255,0)=0x0000FFFF；[1,255,0,0]→红=0x000000FF。
    auto settings = ThematicGradientSettings::defaults();
    settings.colorScheme = ThematicGradientColorScheme::BlueRed;
    const auto blueRed = GradientSymb::createThematic(settings);
    ASSERT_EQ(blueRed.keys.size(), 5u);
    const uint32_t expectBlueRed[5] = {
        0x00FF0000u,  // 蓝
        0x00FFFF00u,  // 青
        0x0000FF00u,  // 绿
        0x0000FFFFu,  // 黄
        0x000000FFu,  // 红
    };
    const double expectValues[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_DOUBLE_EQ(blueRed.keys[i].value, expectValues[i]);
        EXPECT_EQ(blueRed.keys[i].color.getTbgr(), expectBlueRed[i]) << "key " << i;
    }

    // RedBlue 端点（rbg 行 [0,255,0,0]→红 / [1,0,255,0]→蓝）。
    settings.colorScheme = ThematicGradientColorScheme::RedBlue;
    const auto redBlue = GradientSymb::createThematic(settings);
    ASSERT_EQ(redBlue.keys.size(), 5u);
    EXPECT_EQ(redBlue.keys[0].color.getTbgr(), 0x000000FFu);  // 红
    EXPECT_EQ(redBlue.keys[4].color.getTbgr(), 0x00FF0000u);  // 蓝

    settings.colorScheme = ThematicGradientColorScheme::Monochrome;
    const auto mono = GradientSymb::createThematic(settings);
    ASSERT_EQ(mono.keys.size(), 2u);
    EXPECT_EQ(mono.keys[0].color.getTbgr(), 0x00000000u);  // 黑
    EXPECT_EQ(mono.keys[1].color.getTbgr(), 0x00FFFFFFu);  // 白
}

// Authored: createThematic with custom color scheme uses the provided customKeys.
TEST(GradientCreateThematicTest, CreateThematicWithCustomKeys)
{
    ThematicGradientSettings settings;
    settings.colorScheme = ThematicGradientColorScheme::Custom;
    settings.customKeys = {
        GradientKeyColor(0.0, ColorDef::fromTbgr(0x0000FF)),
        GradientKeyColor(1.0, ColorDef::fromTbgr(0xFF0000)),
    };
    const auto symb = GradientSymb::createThematic(settings);
    EXPECT_EQ(symb.mode, GradientMode::Thematic);
    EXPECT_EQ(symb.keys.size(), 2u);
}

// Authored: getThematicImageForRenderer produces a 1-pixel-wide image for thematic mode.
// (ref getThematicImageForRenderer lines 307-359)
TEST(GradientThematicImageTest, GetThematicImageForRendererProducesSinglePixelWidth)
{
    ThematicGradientSettings settings;
    settings.colorScheme = ThematicGradientColorScheme::BlueRed;
    settings.mode = ThematicGradientMode::Smooth;
    const auto symb = GradientSymb::createThematic(settings);

    const auto img = symb.getThematicImageForRenderer(64);
    ASSERT_TRUE(img.has_value());
    EXPECT_EQ(img->width, 1);
    EXPECT_GE(img->getHeight(), 1);
}

// Authored: compareSymb includes thematicSettings (ref compareSymb lines 225-234).
TEST(GradientCompareTest, CompareIncludesThematicSettings)
{
    GradientSymbProps props;
    props.mode = GradientMode::Thematic;
    props.keys = {KeyColor(0.0, 0), KeyColor(1.0, 0xFFFFFF)};
    auto a = GradientSymb::fromJSON(props);
    auto b = GradientSymb::fromJSON(props);
    EXPECT_EQ(GradientSymb::compare(a, b), 0);

    // Give `a` thematic settings; `b` has none → not equal.
    a.thematicSettings = ThematicGradientSettings::defaults();
    EXPECT_NE(GradientSymb::compare(a, b), 0);
}
