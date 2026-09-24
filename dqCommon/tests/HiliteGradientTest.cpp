// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Hilite and Gradient unit tests
//
// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
#include "dqCommon/ColorDef.h"
#include "dqCommon/Gradient.h"
#include "dqCommon/Hilite.h"

#include <gtest/gtest.h>

using namespace dqCommon;

// Ported from: itwinjs-core core/common/src/test/Hilite.test.ts
//              describe("Hilite") it("should have default settings")
TEST(Hilite, DefaultSettings)
{
    const HiliteSettings settings;
    EXPECT_EQ(settings.color.getRgb(), ColorDef::from(0x23, 0xbb, 0xfc).getRgb());
    EXPECT_DOUBLE_EQ(settings.visibleRatio, 0.25);
    EXPECT_DOUBLE_EQ(settings.hiddenRatio, 0.0);
    EXPECT_EQ(settings.silhouette, HiliteSilhouette::Thin);
}

// Ported from: itwinjs-core core/common/src/test/Hilite.test.ts
//              describe("Hilite") it("should accept custom settings")
TEST(Hilite, CustomSettings)
{
    const HiliteSettings settings(ColorDef::red, 0.5, 0.3, HiliteSilhouette::Thick);
    EXPECT_TRUE(settings.color.equals(ColorDef::red));
    EXPECT_DOUBLE_EQ(settings.visibleRatio, 0.5);
    EXPECT_DOUBLE_EQ(settings.hiddenRatio, 0.3);
    EXPECT_EQ(settings.silhouette, HiliteSilhouette::Thick);
}

// Ported from: itwinjs-core core/common/src/test/Hilite.test.ts
//              describe("Hilite") it("should clamp ratios")
TEST(Hilite, Clamping)
{
    const HiliteSettings settings(ColorDef::white, 2.0, -1.0);
    EXPECT_DOUBLE_EQ(settings.visibleRatio, 1.0);
    EXPECT_DOUBLE_EQ(settings.hiddenRatio, 0.0);
}

// Ported from: itwinjs-core core/common/src/test/Hilite.test.ts
//              describe("Hilite") it("should compare equality")
TEST(Hilite, Equality)
{
    const HiliteSettings a;
    const HiliteSettings b;
    EXPECT_TRUE(a.equals(b));

    const HiliteSettings c(ColorDef::red);
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/Hilite.test.ts
//              describe("Hilite") it("should have correct silhouette values")
TEST(Hilite, Silhouette)
{
    EXPECT_EQ(static_cast<int>(HiliteSilhouette::None), 0);
    EXPECT_EQ(static_cast<int>(HiliteSilhouette::Thin), 1);
    EXPECT_EQ(static_cast<int>(HiliteSilhouette::Thick), 2);
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should have correct flags")
TEST(Gradient, Flags)
{
    EXPECT_EQ(static_cast<int>(GradientFlags::None), 0);
    EXPECT_EQ(static_cast<int>(GradientFlags::Invert), 1);
    EXPECT_EQ(static_cast<int>(GradientFlags::Outline), 2);
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should have correct modes")
TEST(Gradient, Mode)
{
    EXPECT_EQ(static_cast<int>(GradientMode::None), 0);
    EXPECT_EQ(static_cast<int>(GradientMode::Linear), 1);
    EXPECT_EQ(static_cast<int>(GradientMode::Curved), 2);
    EXPECT_EQ(static_cast<int>(GradientMode::Cylindrical), 3);
    EXPECT_EQ(static_cast<int>(GradientMode::Spherical), 4);
    EXPECT_EQ(static_cast<int>(GradientMode::Hemispherical), 5);
    EXPECT_EQ(static_cast<int>(GradientMode::Thematic), 6);
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should construct key color")
TEST(Gradient, KeyColor)
{
    const GradientKeyColor kc(0.5, ColorDef::red);
    EXPECT_DOUBLE_EQ(kc.value, 0.5);
    EXPECT_TRUE(kc.color.equals(ColorDef::red));
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should construct key color from props")
TEST(Gradient, KeyColorFromProps)
{
    GradientKeyColorProps props;
    props.value = 0.75;
    props.color = ColorByName::blue;
    const GradientKeyColor kc(props);
    EXPECT_DOUBLE_EQ(kc.value, 0.75);
    EXPECT_EQ(kc.color.getTbgr(), ColorByName::blue);
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should compare key color equality")
TEST(Gradient, KeyColorEquality)
{
    const GradientKeyColor a(0.5, ColorDef::red);
    const GradientKeyColor b(0.5, ColorDef::red);
    const GradientKeyColor c(0.5, ColorDef::blue);
    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should create from JSON")
TEST(Gradient, SymbFromJSON)
{
    GradientSymbProps props;
    props.mode = GradientMode::Linear;
    props.flags = GradientFlags::Outline;
    props.angleDegrees = 45.5;
    props.tint = 0.6;
    props.shift = 1.0;
    props.keys.push_back({0.65, 100});
    props.keys.push_back({0.12, 100});

    const auto symb = GradientSymb::fromJSON(props);
    EXPECT_EQ(symb.mode, GradientMode::Linear);
    EXPECT_EQ(symb.flags, GradientFlags::Outline);
    EXPECT_DOUBLE_EQ(symb.angleDegrees, 45.5);
    ASSERT_TRUE(symb.tint.has_value());
    EXPECT_DOUBLE_EQ(symb.tint.value(), 0.6);
    EXPECT_DOUBLE_EQ(symb.shift, 1.0);
    EXPECT_EQ(symb.keys.size(), 2u);
    EXPECT_TRUE(symb.isOutlined());
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should roundtrip JSON")
TEST(Gradient, SymbToJSONRoundtrip)
{
    GradientSymbProps props;
    props.mode = GradientMode::Cylindrical;
    props.flags = GradientFlags::Invert;
    props.angleDegrees = 90.0;
    props.shift = 0.5;
    props.keys.push_back({0.0, ColorDef::red.getTbgr()});
    props.keys.push_back({1.0, ColorDef::blue.getTbgr()});

    const auto symb = GradientSymb::fromJSON(props);
    const auto json = symb.toJSON();
    const auto symb2 = GradientSymb::fromJSON(json);

    EXPECT_TRUE(symb.equals(symb2));
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should clone")
TEST(Gradient, SymbClone)
{
    GradientSymbProps props;
    props.mode = GradientMode::Linear;
    props.keys.push_back({0.0, ColorDef::red.getTbgr()});
    props.keys.push_back({1.0, ColorDef::blue.getTbgr()});

    const auto symb = GradientSymb::fromJSON(props);
    const auto clone = symb.clone();
    EXPECT_TRUE(symb.equals(clone));
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should compare symbols")
TEST(Gradient, SymbCompare)
{
    GradientSymbProps propsA;
    propsA.mode = GradientMode::Linear;
    propsA.keys.push_back({0.0, 100});
    propsA.keys.push_back({1.0, 200});

    GradientSymbProps propsB;
    propsB.mode = GradientMode::Linear;
    propsB.keys.push_back({0.0, 100});
    propsB.keys.push_back({1.0, 200});

    const auto a = GradientSymb::fromJSON(propsA);
    const auto b = GradientSymb::fromJSON(propsB);
    EXPECT_EQ(a.compareTo(b), 0);

    GradientSymbProps propsC;
    propsC.mode = GradientMode::Curved;
    propsC.keys.push_back({0.0, 100});
    propsC.keys.push_back({1.0, 200});
    const auto c = GradientSymb::fromJSON(propsC);
    EXPECT_LT(a.compareTo(c), 0);
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should sort symbols by mode and flags")
TEST(Gradient, SymbSortOrder)
{
    std::vector<GradientSymb> symbArr;

    // Same mode, different flags
    {
        GradientSymbProps p;
        p.mode = GradientMode::Cylindrical;
        p.flags = GradientFlags::None;
        p.angleDegrees = 92.9;
        p.tint = 0.042;
        p.shift = 3.46;
        p.keys.push_back({0.68, 610});
        p.keys.push_back({0.73, 230});
        symbArr.push_back(GradientSymb::fromJSON(p));
    }
    {
        GradientSymbProps p;
        p.mode = GradientMode::Cylindrical;
        p.flags = GradientFlags::Invert;
        p.angleDegrees = 92.9;
        p.tint = 0.042;
        p.shift = 3.46;
        p.keys.push_back({0.68, 610});
        p.keys.push_back({0.73, 230});
        symbArr.push_back(GradientSymb::fromJSON(p));
    }
    {
        GradientSymbProps p;
        p.mode = GradientMode::Cylindrical;
        p.flags = GradientFlags::Outline;
        p.angleDegrees = 92.9;
        p.tint = 0.042;
        p.shift = 3.46;
        p.keys.push_back({0.68, 610});
        p.keys.push_back({0.73, 230});
        symbArr.push_back(GradientSymb::fromJSON(p));
    }

    // Sort and verify order
    std::sort(symbArr.begin(), symbArr.end(),
              [](const GradientSymb& a, const GradientSymb& b) { return GradientSymb::compare(a, b) < 0; });

    for (size_t i = 1; i < symbArr.size(); i++) {
        EXPECT_TRUE(GradientSymb::compare(symbArr[i - 1], symbArr[i]) <= 0);
        EXPECT_TRUE(static_cast<int>(symbArr[i].flags) >= static_cast<int>(symbArr[i - 1].flags));
    }
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should map color")
TEST(Gradient, mapColor)
{
    GradientSymbProps props;
    props.mode = GradientMode::Linear;
    props.keys.push_back({0.0, ColorDef::black.getTbgr()});
    props.keys.push_back({1.0, ColorDef::white.getTbgr()});

    const auto symb = GradientSymb::fromJSON(props);

    const auto c0 = symb.mapColor(0.0);
    EXPECT_EQ(c0.getRgb(), ColorDef::black.getRgb());

    const auto c1 = symb.mapColor(1.0);
    EXPECT_EQ(c1.getRgb(), ColorDef::white.getRgb());

    const auto cMid = symb.mapColor(0.5);
    const auto midColors = cMid.getColors();
    EXPECT_NEAR(midColors.r, 127, 1);
    EXPECT_NEAR(midColors.g, 127, 1);
    EXPECT_NEAR(midColors.b, 127, 1);
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should detect translucency")
TEST(Gradient, hasTranslucency)
{
    GradientSymbProps props;
    props.mode = GradientMode::Linear;
    props.keys.push_back({0.0, ColorDef::black.getTbgr()});
    props.keys.push_back({1.0, ColorDef::white.getTbgr()});

    auto symb = GradientSymb::fromJSON(props);
    EXPECT_FALSE(symb.hasTranslucency());

    // add translucent color
    props.keys.push_back({0.5, ColorDef::from(128, 128, 128, 128).getTbgr()});
    symb = GradientSymb::fromJSON(props);
    EXPECT_TRUE(symb.hasTranslucency());
}

// Ported from: itwinjs-core core/common/src/test/Gradient.test.ts
//              describe("Gradient") it("should detect outline flag")
TEST(Gradient, isOutlined)
{
    GradientSymb symb;
    symb.flags = GradientFlags::None;
    EXPECT_FALSE(symb.isOutlined());
    symb.flags = GradientFlags::Outline;
    EXPECT_TRUE(symb.isOutlined());
}
