// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — FeatureAppearance unit tests
//
// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
#include "dqCommon/ColorDef.h"
#include "dqCommon/FeatureSymbology.h"
#include "dqCommon/LinePixels.h"
#include "dqCommon/RgbColor.h"

#include <gtest/gtest.h>

using namespace dqCommon;

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("default constructor works as expected")
TEST(FeatureAppearance, DefaultConstructor)
{
    const auto& app = FeatureAppearance::defaults();
    EXPECT_FALSE(app.getRgb().has_value());
    EXPECT_FALSE(app.getWeight().has_value());
    EXPECT_FALSE(app.getTransparency().has_value());
    EXPECT_FALSE(app.getLinePixels().has_value());
    EXPECT_FALSE(app.getIgnoresMaterial());
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("AppearanceProps passed in constructor works as expected")
TEST(FeatureAppearance, FromJSONProps)
{
    FeatureAppearanceProps props1;
    props1.rgb = RgbColorProps{100, 100, 100};
    props1.weight = 1.0;
    props1.transparency = 200.0 / 255.0;
    props1.linePixels = LinePixels::Code2;
    props1.ignoresMaterial = true;

    auto app = FeatureAppearance::fromJSON(&props1);
    EXPECT_TRUE(app.overridesRgb());
    EXPECT_TRUE(app.overridesWeight());
    EXPECT_TRUE(app.overridesTransparency());
    EXPECT_TRUE(app.overridesLinePixels());
    EXPECT_TRUE(app.getIgnoresMaterial());

    FeatureAppearanceProps props2;
    props2.rgb = RgbColorProps{100, 100, 100};
    props2.weight = 1.0;
    props2.transparency = 200.0 / 255.0;
    props2.linePixels = LinePixels::Code2;

    app = FeatureAppearance::fromJSON(&props2);
    EXPECT_FALSE(app.getIgnoresMaterial());
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("extend works as expected")
TEST(FeatureAppearance, extendAppearance)
{
    FeatureAppearanceProps props1;
    props1.rgb = RgbColorProps{100, 100, 100};
    props1.linePixels = LinePixels::Code2;
    props1.ignoresMaterial = true;

    FeatureAppearanceProps props2;
    props2.rgb = RgbColorProps{250, 180, 150};
    props2.weight = 1.0;
    props2.transparency = 200.0 / 255.0;
    props2.linePixels = LinePixels::Code3;

    auto app1 = FeatureAppearance::fromJSON(&props1);
    const auto app2 = FeatureAppearance::fromJSON(&props2);
    app1 = app2.extendAppearance(app1);

    // app1's rgb and linePixels should be preserved (they were already set)
    // app2's weight and transparency should be added
    EXPECT_TRUE(app1.overridesRgb());
    EXPECT_TRUE(app1.overridesWeight());
    EXPECT_TRUE(app1.overridesTransparency());
    EXPECT_TRUE(app1.getIgnoresMaterial());
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("compares for equality")
TEST(FeatureAppearance, Equality)
{
    const auto& appA = FeatureAppearance::defaults();

    FeatureAppearanceProps lineRgbProps;
    lineRgbProps.lineRgb = RgbColorProps{255, 255, 255};
    const auto appB = FeatureAppearance::fromJSON(&lineRgbProps);

    EXPECT_TRUE(appA.equals(appA));
    EXPECT_TRUE(appB.equals(appB));
    EXPECT_FALSE(appA.equals(appB));
    EXPECT_FALSE(appB.equals(appA));
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("creates from rgb")
TEST(FeatureAppearance, fromRgb)
{
    const auto app = FeatureAppearance::fromRgb(ColorDef::from(255, 0, 0));
    EXPECT_TRUE(app.overridesRgb());
    ASSERT_TRUE(app.getRgb().has_value());
    EXPECT_EQ(app.getRgb()->r, 255);
    EXPECT_EQ(app.getRgb()->g, 0);
    EXPECT_EQ(app.getRgb()->b, 0);
    EXPECT_FALSE(app.overridesTransparency());
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("creates from rgba")
TEST(FeatureAppearance, fromRgba)
{
    const auto app = FeatureAppearance::fromRgba(ColorDef::from(0, 255, 0, 128));
    EXPECT_TRUE(app.overridesRgb());
    ASSERT_TRUE(app.getRgb().has_value());
    EXPECT_EQ(app.getRgb()->g, 255);
    EXPECT_TRUE(app.overridesTransparency());
    ASSERT_TRUE(app.getTransparency().has_value());
    EXPECT_NEAR(app.getTransparency().value(), 128.0 / 255.0, 0.01);
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("creates from transparency")
TEST(FeatureAppearance, fromTransparency)
{
    const auto app = FeatureAppearance::fromTransparency(0.5);
    EXPECT_FALSE(app.overridesRgb());
    EXPECT_TRUE(app.overridesTransparency());
    ASSERT_TRUE(app.getTransparency().has_value());
    EXPECT_NEAR(app.getTransparency().value(), 0.5, 1e-10);
}

// Ported from: itwinjs-core FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("view-dependent transparency serialization")
TEST(FeatureAppearance, ViewDependentTransparency)
{
    // No transparency → viewDependent should be false
    {
        FeatureAppearanceProps props;
        const auto app = FeatureAppearance::fromJSON(&props);
        EXPECT_FALSE(app.getViewDependentTransparency());
    }
    // Transparency without viewDependent → false
    {
        FeatureAppearanceProps props;
        props.transparency = 1.0;
        const auto app = FeatureAppearance::fromJSON(&props);
        EXPECT_FALSE(app.getViewDependentTransparency());
    }
    // Transparency with viewDependent → true
    {
        FeatureAppearanceProps props;
        props.transparency = 1.0;
        props.viewDependentTransparency = true;
        const auto app = FeatureAppearance::fromJSON(&props);
        EXPECT_TRUE(app.getViewDependentTransparency());
    }
    // viewDependent without transparency → false
    {
        FeatureAppearanceProps props;
        props.viewDependentTransparency = true;
        const auto app = FeatureAppearance::fromJSON(&props);
        EXPECT_FALSE(app.getViewDependentTransparency());
    }
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("isFullyTransparent")
TEST(FeatureAppearance, isFullyTransparent)
{
    EXPECT_FALSE(FeatureAppearance::defaults().isFullyTransparent());

    const auto opaque = FeatureAppearance::fromTransparency(0.0);
    EXPECT_FALSE(opaque.isFullyTransparent());

    const auto fullTransp = FeatureAppearance::fromTransparency(1.0);
    EXPECT_TRUE(fullTransp.isFullyTransparent());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("overridesSymbology")
TEST(FeatureAppearance, overridesSymbology)
{
    EXPECT_FALSE(FeatureAppearance::defaults().overridesSymbology());
    EXPECT_TRUE(FeatureAppearance::fromRgb(ColorDef::red).overridesSymbology());
    EXPECT_TRUE(FeatureAppearance::fromTransparency(0.5).overridesSymbology());
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("toJSON roundtrip")
TEST(FeatureAppearance, ToJSONRoundtrip)
{
    const auto app1 = FeatureAppearance::fromRgb(ColorDef::from(100, 200, 50));
    const auto json = app1.toJSON();
    const auto app2 = FeatureAppearance::fromJSON(&json);
    EXPECT_TRUE(app1.equals(app2));
}

// Ported from: itwinjs-core core/common/src/test/FeatureSymbology.test.ts
//              describe("FeatureAppearance") it("Clone with overrides")
TEST(FeatureAppearance, CloneWithOverrides)
{
    const auto base = FeatureAppearance::fromRgba(ColorDef::white);
    FeatureAppearanceProps overrides;
    overrides.weight = 5.0;
    auto json = base.toJSON();
    json.weight = overrides.weight;
    json.transparency = std::nullopt;  // Remove transparency
    const auto clone = FeatureAppearance::fromJSON(&json);
    EXPECT_FALSE(clone.overridesTransparency());
    EXPECT_TRUE(clone.overridesWeight());
    ASSERT_TRUE(clone.getWeight().has_value());
    EXPECT_NEAR(clone.getWeight().value(), 5.0, 1e-10);
    EXPECT_TRUE(clone.overridesRgb());
}
