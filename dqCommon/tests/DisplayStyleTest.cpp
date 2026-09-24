// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/common/src/DisplayStyle.ts
// DanQing dqCommon — LightSettings and HiddenLine unit tests
#include "dqCommon/HiddenLine.h"
#include "dqCommon/LightSettings.h"

#include <gtest/gtest.h>

using namespace dqCommon;

// SolarLight tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(SolarLight, defaults)
TEST(SolarLight, defaults)
{
    const SolarLight light;
    EXPECT_DOUBLE_EQ(light.intensity, 1.0);
    EXPECT_FALSE(light.alwaysEnabled);
    EXPECT_FALSE(light.timePoint.has_value());
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(SolarLight, FromProps)
TEST(SolarLight, FromProps)
{
    SolarLightProps props;
    props.intensity = 0.5;
    props.alwaysEnabled = true;
    const SolarLight light(props);
    EXPECT_DOUBLE_EQ(light.intensity, 0.5);
    EXPECT_TRUE(light.alwaysEnabled);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(SolarLight, Equality)
TEST(SolarLight, Equality)
{
    const SolarLight a;
    const SolarLight b;
    EXPECT_TRUE(a.equals(b));

    SolarLightProps props;
    props.intensity = 2.0;
    const SolarLight c(props);
    EXPECT_FALSE(a.equals(c));
}

// AmbientLight tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(AmbientLight, defaults)
TEST(AmbientLight, defaults)
{
    const AmbientLight light;
    EXPECT_DOUBLE_EQ(light.intensity, 0.2);
    EXPECT_EQ(light.color.r, 0);
    EXPECT_EQ(light.color.g, 0);
    EXPECT_EQ(light.color.b, 0);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(AmbientLight, FromProps)
TEST(AmbientLight, FromProps)
{
    AmbientLightProps props;
    props.intensity = 0.5;
    props.color = RgbColorProps{255, 128, 64};
    const AmbientLight light(props);
    EXPECT_DOUBLE_EQ(light.intensity, 0.5);
    EXPECT_EQ(light.color.r, 255);
    EXPECT_EQ(light.color.g, 128);
    EXPECT_EQ(light.color.b, 64);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(AmbientLight, Equality)
TEST(AmbientLight, Equality)
{
    const AmbientLight a;
    const AmbientLight b;
    EXPECT_TRUE(a.equals(b));
}

// HemisphereLights tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HemisphereLights, defaults)
TEST(HemisphereLights, defaults)
{
    const HemisphereLights light;
    EXPECT_DOUBLE_EQ(light.intensity, 0.0);
    EXPECT_EQ(light.upperColor.r, 143);
    EXPECT_EQ(light.upperColor.g, 205);
    EXPECT_EQ(light.upperColor.b, 255);
    EXPECT_EQ(light.lowerColor.r, 120);
    EXPECT_EQ(light.lowerColor.g, 143);
    EXPECT_EQ(light.lowerColor.b, 125);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HemisphereLights, FromProps)
TEST(HemisphereLights, FromProps)
{
    HemisphereLightsProps props;
    props.intensity = 0.3;
    const HemisphereLights light(props);
    EXPECT_DOUBLE_EQ(light.intensity, 0.3);
}

// FresnelSettings tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(FresnelSettings, defaults)
TEST(FresnelSettings, defaults)
{
    const FresnelSettings settings;
    EXPECT_DOUBLE_EQ(settings.intensity, 0.0);
    EXPECT_FALSE(settings.invert);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(FresnelSettings, fromJSON)
TEST(FresnelSettings, fromJSON)
{
    FresnelSettingsProps props;
    props.intensity = 0.5;
    props.invert = true;
    const auto settings = FresnelSettings::fromJSON(props);
    EXPECT_DOUBLE_EQ(settings.intensity, 0.5);
    EXPECT_TRUE(settings.invert);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(FresnelSettings, Equality)
TEST(FresnelSettings, Equality)
{
    const FresnelSettings a;
    const FresnelSettings b;
    EXPECT_TRUE(a.equals(b));

    const FresnelSettings c(0.5, true);
    EXPECT_FALSE(a.equals(c));
}

// LightSettings tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(LightSettings, defaults)
TEST(LightSettings, defaults)
{
    const LightSettings settings;
    EXPECT_DOUBLE_EQ(settings.portraitIntensity, 0.3);
    EXPECT_DOUBLE_EQ(settings.specularIntensity, 1.0);
    EXPECT_EQ(settings.numCels, 0);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(LightSettings, fromJSON)
TEST(LightSettings, fromJSON)
{
    LightSettingsProps props;
    props.portraitIntensity = 0.5;
    props.specularIntensity = 2.0;
    props.numCels = 3;
    const auto settings = LightSettings::fromJSON(props);
    EXPECT_DOUBLE_EQ(settings.portraitIntensity, 0.5);
    EXPECT_DOUBLE_EQ(settings.specularIntensity, 2.0);
    EXPECT_EQ(settings.numCels, 3);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(LightSettings, Equality)
TEST(LightSettings, Equality)
{
    const LightSettings a;
    const LightSettings b;
    EXPECT_TRUE(a.equals(b));
}

// HiddenLineStyle tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineStyle, defaults)
TEST(HiddenLineStyle, defaults)
{
    const auto& vis = HiddenLineStyle::defaultVisible();
    EXPECT_FALSE(vis.color.has_value());
    EXPECT_FALSE(vis.pattern.has_value());
    EXPECT_FALSE(vis.width.has_value());

    const auto& hid = HiddenLineStyle::defaultHidden();
    EXPECT_FALSE(hid.color.has_value());
    ASSERT_TRUE(hid.pattern.has_value());
    EXPECT_EQ(hid.pattern.value(), LinePixels::HiddenLine);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineStyle, fromJSON)
TEST(HiddenLineStyle, fromJSON)
{
    HiddenLineStyleProps props;
    props.color = ColorDef::red.getTbgr();
    props.ovrColor = true;
    props.pattern = LinePixels::Code2;
    props.width = 3;

    const auto style = HiddenLineStyle::fromJSON(&props);
    ASSERT_TRUE(style.color.has_value());
    EXPECT_TRUE(style.color->equals(ColorDef::red));
    ASSERT_TRUE(style.pattern.has_value());
    EXPECT_EQ(style.pattern.value(), LinePixels::Code2);
    ASSERT_TRUE(style.width.has_value());
    EXPECT_EQ(style.width.value(), 3);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineStyle, Equality)
TEST(HiddenLineStyle, Equality)
{
    const auto& a = HiddenLineStyle::defaultVisible();
    const auto& b = HiddenLineStyle::defaultVisible();
    EXPECT_TRUE(a.equals(b));

    const auto& c = HiddenLineStyle::defaultHidden();
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineStyle, overrideColor)
TEST(HiddenLineStyle, overrideColor)
{
    const auto& base = HiddenLineStyle::defaultVisible();
    const auto overridden = base.overrideColor(ColorDef::red);
    ASSERT_TRUE(overridden.color.has_value());
    EXPECT_TRUE(overridden.color->equals(ColorDef::red));
}

// HiddenLineSettings tests
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineSettings, defaults)
TEST(HiddenLineSettings, defaults)
{
    const auto& def = HiddenLineSettings::defaults();
    EXPECT_DOUBLE_EQ(def.transparencyThreshold, 1.0);
    EXPECT_TRUE(def.matchesDefaults());
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineSettings, fromJSON)
TEST(HiddenLineSettings, fromJSON)
{
    HiddenLineSettingsProps props;
    props.transThreshold = 0.5;
    const auto settings = HiddenLineSettings::fromJSON(props);
    EXPECT_DOUBLE_EQ(settings.transparencyThreshold, 0.5);
}

// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              TEST(HiddenLineSettings, Equality)
TEST(HiddenLineSettings, Equality)
{
    const auto& a = HiddenLineSettings::defaults();
    const auto& b = HiddenLineSettings::defaults();
    EXPECT_TRUE(a.equals(b));
}
