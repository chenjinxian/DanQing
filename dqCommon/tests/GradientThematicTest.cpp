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
