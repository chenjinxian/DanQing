// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/common/src/DisplayStyle.ts
// DanQing dqCommon — LightSettings and HiddenLine unit tests
#include "dqCommon/DisplayStyleSettings.h"
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

// DisplayStyle3dSettings.lights application（M-M(1)：saved display style 的
// 灯光段此前 applyOverrides3d 静默丢弃——渲染恒用默认 rig）。
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              (lights round-trip cases :723-728 —
//               {numCels:2, solar:{intensity:4, alwaysEnabled:true},
//                ambient:{intensity:2, color:{r:12, g:24, b:48}}})
TEST(DisplayStyle3dSettings, LightsOverrideApplies)
{
    DisplayStyle3dSettings settings;  // defaults == reference LightSettings{}
    EXPECT_DOUBLE_EQ(settings.getLights().portraitIntensity, 0.3);

    DisplayStyle3dSettingsProps props;
    LightSettingsProps lights;
    lights.numCels = 2;
    SolarLightProps solar;
    solar.intensity = 4.0;
    solar.alwaysEnabled = true;
    lights.solar = solar;
    AmbientLightProps ambient;
    ambient.intensity = 2.0;
    ambient.color = RgbColorProps{12, 24, 48};
    lights.ambient = ambient;
    props.lights = lights;

    settings.applyOverrides3d(props);

    EXPECT_EQ(settings.getLights().numCels, 2);
    EXPECT_DOUBLE_EQ(settings.getLights().solar.intensity, 4.0);
    EXPECT_TRUE(settings.getLights().solar.alwaysEnabled);
    EXPECT_DOUBLE_EQ(settings.getLights().ambient.intensity, 2.0);
    EXPECT_EQ(settings.getLights().ambient.color.r, 12);
    EXPECT_EQ(settings.getLights().ambient.color.g, 24);
    EXPECT_EQ(settings.getLights().ambient.color.b, 48);
    // Unset fields keep the reference defaults.
    EXPECT_DOUBLE_EQ(settings.getLights().portraitIntensity, 0.3);
    EXPECT_DOUBLE_EQ(settings.getLights().specularIntensity, 1.0);
}

// Absent lights props leave the default rig untouched.
TEST(DisplayStyle3dSettings, LightsAbsentKeepsDefaults)
{
    DisplayStyle3dSettings settings;
    DisplayStyle3dSettingsProps props;  // no lights
    settings.applyOverrides3d(props);
    EXPECT_DOUBLE_EQ(settings.getLights().ambient.intensity, 0.2);
    EXPECT_DOUBLE_EQ(settings.getLights().solar.intensity, 1.0);
    EXPECT_FALSE(settings.getLights().solar.alwaysEnabled);
    EXPECT_DOUBLE_EQ(settings.getLights().hemisphere.intensity, 0.0);
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

// M-Q Q-a：applyOverrides 的 viewflags 合并语义（缺席位保持现值）。
// Ported from: itwinjs-core core/common/src/test/DisplayStyle.test.ts
//              "overrides selected settings" (:554-588 ——
//               应用键输出=overrides 值 + 缺席键=原值；参考以
//               ViewFlags.fromJSON({...current.toJSON(), ...overrides.viewflags})
//               承载。analysisStyle/scheduleScript 两例随消费面立项登记。）
TEST(DisplayStyle3dSettings, OverridesSelectedSettings)
{
    auto makeBase = [] {
        DisplayStyle3dSettings settings;
        auto p = settings.getViewFlags().Properties();
        p.grid = true;                          // 偏离默认（默认 false）
        p.weights = false;                      // 偏离默认（默认 true）
        p.renderMode = RenderMode::SmoothShade;
        settings.setViewFlags(dqCommon::ViewFlags(p));
        settings.setBackgroundColor(dqCommon::ColorDef::from(255, 0, 0));
        settings.setMonochromeColor(dqCommon::ColorDef::from(1, 2, 3));
        return settings;
    };

    // Case 1（:583）：viewflags 仅 renderMode=SolidFill —— grid/weights 保持现值。
    {
        DisplayStyle3dSettings settings = makeBase();
        DisplayStyle3dSettingsProps o;
        ViewFlagProps vf;
        vf.renderMode = RenderMode::SolidFill;
        o.viewflags = vf;
        settings.applyOverrides3d(o);
        auto const out = settings.getViewFlags().Properties();
        EXPECT_EQ(out.renderMode, RenderMode::SolidFill);  // 应用键=override 值
        EXPECT_TRUE(out.grid);                             // 缺席键=原值（合并）
        EXPECT_FALSE(out.weights);                         // 缺席键=原值（合并）
        EXPECT_EQ(settings.getBackgroundColor().getTbgr(),
                  dqCommon::ColorDef::from(255, 0, 0).getTbgr());
    }

    // Case 2（:584）：viewflags + backgroundColor=honeydew。
    {
        DisplayStyle3dSettings settings = makeBase();
        DisplayStyle3dSettingsProps o;
        ViewFlagProps vf;
        vf.renderMode = RenderMode::SolidFill;
        o.viewflags = vf;
        o.backgroundColor = 0xF0FFF0u;  // ColorByName.honeydew
        settings.applyOverrides3d(o);
        EXPECT_EQ(settings.getBackgroundColor().getTbgr(), 0xF0FFF0u);
        EXPECT_TRUE(settings.getViewFlags().Properties().grid);  // 仍保持
    }

    // Case 3（:585）：viewflags + monochromeColor=hotPink。
    {
        DisplayStyle3dSettings settings = makeBase();
        DisplayStyle3dSettingsProps o;
        ViewFlagProps vf;
        vf.monochrome = true;
        o.viewflags = vf;
        o.monochromeColor = 0xFF69B4u;  // ColorByName.hotPink
        settings.applyOverrides3d(o);
        EXPECT_EQ(settings.getMonochromeColor().getTbgr(), 0xFF69B4u);
        EXPECT_TRUE(settings.getViewFlags().Properties().monochrome);
        EXPECT_TRUE(settings.getViewFlags().Properties().grid);   // 仍保持
        EXPECT_FALSE(settings.getViewFlags().Properties().weights);
    }

    // Case 4（:586）：viewflags + monochromeMode=Flat + timePoint（:587 段
    // analysisFraction 的可移植面；analysisStyle 未移植登记）。
    {
        DisplayStyle3dSettings settings = makeBase();
        DisplayStyle3dSettingsProps o;
        ViewFlagProps vf;
        vf.monochrome = true;
        o.viewflags = vf;
        o.monochromeMode = MonochromeMode::Flat;
        o.timePoint = 87654321.0;
        o.analysisFraction = 0.8;
        settings.applyOverrides3d(o);
        EXPECT_EQ(settings.getMonochromeMode(), MonochromeMode::Flat);
        ASSERT_TRUE(settings.getTimePoint().has_value());
        EXPECT_DOUBLE_EQ(*settings.getTimePoint(), 87654321.0);
        EXPECT_DOUBLE_EQ(settings.getAnalysisFraction(), 0.8);
        EXPECT_TRUE(settings.getViewFlags().Properties().grid);
    }

    // 3d 层缺席保持（:554-588 的 absent 键面——lights/hline/environment 原样）。
    {
        DisplayStyle3dSettings settings = makeBase();
        DisplayStyle3dSettingsProps o;
        ViewFlagProps vf;
        vf.renderMode = RenderMode::SolidFill;
        o.viewflags = vf;
        settings.applyOverrides3d(o);
        EXPECT_DOUBLE_EQ(settings.getLights().portraitIntensity, 0.3);  // 默认 rig 未动
        EXPECT_DOUBLE_EQ(settings.getHiddenLineSettings().transparencyThreshold, 1.0);
        EXPECT_FALSE(settings.getEnvironment().displaySky);
    }
}
