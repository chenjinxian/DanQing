// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ThematicDisplay / ThematicGradientSettings tests
//
// Ported from: itwinjs-core core/common/src/test/ThematicDisplay.test.ts
//              describe("ThematicDisplay")
// Ported from: itwinjs-core core/common/src/test/ThematicGradientSettings.test.ts
//              describe("ThematicGradientSettings")
#include <dqCommon/ColorDef.h>
#include <dqCommon/SolarCalculate.h>
#include <dqCommon/TextureProps.h>
#include <dqCommon/ThematicDisplay.h>

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <cmath>

#include <gtest/gtest.h>

using namespace dqCommon;

namespace {

// Ported from: ThematicDisplay.test.ts verifyDefaults() (:13-28)
void verifyThematicDefaults(ThematicDisplay const& td)
{
    EXPECT_DOUBLE_EQ(td.axis.x, 0.0);
    EXPECT_DOUBLE_EQ(td.axis.y, 0.0);
    EXPECT_DOUBLE_EQ(td.axis.z, 0.0);
    EXPECT_EQ(td.displayMode, ThematicDisplayMode::Height);
    EXPECT_EQ(td.gradientSettings.mode, ThematicGradientMode::Smooth);
    EXPECT_EQ(td.gradientSettings.stepCount, 10);
    EXPECT_EQ(td.gradientSettings.colorScheme, ThematicGradientColorScheme::BlueRed);
    auto mc = td.gradientSettings.marginColor.getColors();
    EXPECT_EQ(mc.r, 0);
    EXPECT_EQ(mc.g, 0);
    EXPECT_EQ(mc.b, 0);
    EXPECT_EQ(mc.t, 0);
    EXPECT_TRUE(td.gradientSettings.customKeys.empty());
    // 参考：range 默认 = Range1d.createNull()（ThematicDisplay.test.ts:24）。
    EXPECT_TRUE(td.range.isNull());
    EXPECT_TRUE(td.sensorSettings.sensors.empty());
    EXPECT_DOUBLE_EQ(td.sensorSettings.distanceCutoff, 0.0);
}

// Ported from: ThematicDisplay.test.ts verifyBackAndForth() (:31-34)
void verifyThematicBackAndForth(ThematicDisplay const& a)
{
    auto props = a.toJSON();
    auto copy = ThematicDisplay::fromJSON(&props);
    EXPECT_TRUE(copy.equals(a));
}

} // namespace

// Ported from: itwinjs-core core/common/src/test/ThematicDisplay.test.ts
//              it("Ensures ThematicDisplay derives values properly from JSON,
//              including handling defaults and incorrect values")
TEST(ThematicDisplayTest, DerivesValuesFromJsonWithDefaultsAndIncorrectValues)
{
    // create default ThematicDisplay object and verify the default values
    auto defaultThematicDisplay = ThematicDisplay::fromJSON();
    verifyThematicDefaults(defaultThematicDisplay);
    verifyThematicBackAndForth(defaultThematicDisplay);

    // bad displayMode / gradient mode / color scheme → expected defaults
    // （C++ 适配：枚举底层 uint8_t，参考的 99999 越界值以 255 承载——同语义）。
    {
        ThematicDisplayProps bad;
        bad.displayMode = static_cast<ThematicDisplayMode>(255);
        ThematicGradientSettingsProps bg;
        bg.mode = static_cast<ThematicGradientMode>(255);
        bg.colorScheme = static_cast<ThematicGradientColorScheme>(255);
        bad.gradientSettings = bg;
        auto td = ThematicDisplay::fromJSON(&bad);
        EXPECT_TRUE(td.equals(defaultThematicDisplay));
        verifyThematicBackAndForth(td);
    }

    // sensor settings propagate; value clamped to [0,1]
    {
        ThematicDisplayProps props;
        ThematicDisplaySensorSettingsProps ss;
        ss.sensors = std::vector<ThematicDisplaySensorProps>();
        const double pos[5][3] = {{1,2,3},{4,5,6},{7,8,9},{10,11,12},{13,14,15}};
        const double val[5] = {0.25, 0.5, 0.75, -1.0, 2.0};
        for (int i = 0; i < 5; ++i) {
            ThematicDisplaySensorProps sp;
            sp.position = dqGeom::Point3d::From(pos[i][0], pos[i][1], pos[i][2]);
            sp.value = val[i];
            ss.sensors->push_back(sp);
        }
        ss.distanceCutoff = 5.0;
        props.sensorSettings = ss;
        auto td = ThematicDisplay::fromJSON(&props);
        ASSERT_EQ(td.sensorSettings.sensors.size(), 5u);
        for (int i = 0; i < 3; ++i) {
            EXPECT_TRUE(td.sensorSettings.sensors[i].position.IsEqual(
                dqGeom::Point3d::From(pos[i][0], pos[i][1], pos[i][2])));
            EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[i].value, val[i]);
        }
        // 'bad' values clamped: -1 → 0；2 → 1（ThematicDisplay.ts:308-311）
        EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[3].value, 0.0);
        EXPECT_DOUBLE_EQ(td.sensorSettings.sensors[4].value, 1.0);
        EXPECT_DOUBLE_EQ(td.sensorSettings.distanceCutoff, 5.0);
        verifyThematicBackAndForth(td);
    }

    // Custom color scheme with a single key → two manufactured keys (white→black)
    {
        ThematicDisplayProps props;
        ThematicGradientSettingsProps g;
        g.colorScheme = ThematicGradientColorScheme::Custom;
        GradientKeyColorProps single;
        single.value = 0.0;
        single.color = 0;
        g.customKeys = std::vector<GradientKeyColorProps>{single};
        props.gradientSettings = g;
        auto td = ThematicDisplay::fromJSON(&props);
        ASSERT_EQ(td.gradientSettings.customKeys.size(), 2u);
        EXPECT_TRUE(td.gradientSettings.customKeys[0].color.equals(ColorDef::from(255, 255, 255, 0)));
        EXPECT_DOUBLE_EQ(td.gradientSettings.customKeys[0].value, 0.0);
        EXPECT_TRUE(td.gradientSettings.customKeys[1].color.equals(ColorDef::from(0, 0, 0, 0)));
        EXPECT_DOUBLE_EQ(td.gradientSettings.customKeys[1].value, 1.0);
        verifyThematicBackAndForth(td);
    }

    // IsoLines / SteppedWithDelimiter forced back to Smooth in non-Height modes
    // （ThematicDisplay.ts:514-523 构造校验——参考四例逐项：IsoLines+Sensors /
    // SWD+Sensors / SWD+Slope / SWD+HillShade，test.ts:96-138）。
    const struct { ThematicGradientMode gm; ThematicDisplayMode dm; } badCombos[4] = {
        {ThematicGradientMode::IsoLines, ThematicDisplayMode::InverseDistanceWeightedSensors},
        {ThematicGradientMode::SteppedWithDelimiter, ThematicDisplayMode::InverseDistanceWeightedSensors},
        {ThematicGradientMode::SteppedWithDelimiter, ThematicDisplayMode::Slope},
        {ThematicGradientMode::SteppedWithDelimiter, ThematicDisplayMode::HillShade}};
    for (auto const& c : badCombos) {
        ThematicDisplayProps props;
        props.displayMode = c.dm;
        ThematicGradientSettingsProps g;
        g.mode = c.gm;
        props.gradientSettings = g;
        auto td = ThematicDisplay::fromJSON(&props);
        EXPECT_EQ(td.gradientSettings.mode, ThematicGradientMode::Smooth)
            << "gradient " << static_cast<int>(c.gm) << " display "
            << static_cast<int>(c.dm);
        verifyThematicBackAndForth(td);
    }
}

// Ported from: itwinjs-core core/common/src/test/ThematicGradientSettings.test.ts
//              it("compares")
TEST(ThematicGradientSettingsTest, Compares)
{
    ThematicGradientSettingsProps propsA;
    propsA.mode = ThematicGradientMode::Stepped;
    propsA.stepCount = 5;
    propsA.marginColor = ColorDef::blue.getTbgr();
    propsA.colorScheme = ThematicGradientColorScheme::Topographic;
    propsA.customKeys = std::vector<GradientKeyColorProps>();
    const struct { double v; uint32_t c; } keys[3] = {
        {0.0, ColorDef::green.getTbgr()},
        {0.5, ColorDef::red.getTbgr()},
        {1.0, ColorDef::white.getTbgr()}};
    for (auto const& k : keys) {
        GradientKeyColorProps kp;
        kp.value = k.v;
        kp.color = k.c;
        propsA.customKeys->push_back(kp);
    }
    propsA.colorMix = 0.5;
    propsA.transparencyMode = ThematicGradientTransparencyMode::MultiplySurfaceAndGradient;

    auto settingsA = ThematicGradientSettings::fromJSON(&propsA);

    // A equals B when B is fully copied from A.
    {
        auto propsB = settingsA.toJSON();
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_EQ(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // A > B when B lowers the stepCount; A < B when B raises it.
    {
        auto propsB = settingsA.toJSON();
        propsB.stepCount = 4;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_GT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    {
        auto propsB = settingsA.toJSON();
        propsB.stepCount = 6;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_LT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // A > B when B lowers the mode; A < B when B raises it.
    {
        auto propsB = settingsA.toJSON();
        propsB.mode = ThematicGradientMode::Smooth;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_GT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    {
        auto propsB = settingsA.toJSON();
        propsB.mode = ThematicGradientMode::SteppedWithDelimiter;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_LT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // colorScheme lower/higher.
    {
        auto propsB = settingsA.toJSON();
        propsB.colorScheme = ThematicGradientColorScheme::Monochrome;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_GT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    {
        auto propsB = settingsA.toJSON();
        propsB.colorScheme = ThematicGradientColorScheme::SeaMountain;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_LT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // colorMix lower/higher.
    {
        auto propsB = settingsA.toJSON();
        propsB.colorMix = 0.4;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_GT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    {
        auto propsB = settingsA.toJSON();
        propsB.colorMix = 0.6;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_LT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // transparencyMode lower.
    {
        auto propsB = settingsA.toJSON();
        propsB.transparencyMode = ThematicGradientTransparencyMode::SurfaceOnly;
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_GT(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // A != B when marginColor changes.
    {
        auto propsB = settingsA.toJSON();
        propsB.marginColor = ColorDef::red.getTbgr();
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_NE(ThematicGradientSettings::compare(settingsA, b), 0);
    }
    // A != B when customKeys change.
    {
        auto propsB = settingsA.toJSON();
        propsB.customKeys = std::vector<GradientKeyColorProps>();
        const struct { double v; uint32_t c; } keys2[3] = {
            {0.0, ColorDef::black.getTbgr()},
            {0.5, ColorDef::white.getTbgr()},
            {1.0, ColorDef::green.getTbgr()}};
        for (auto const& k : keys2) {
            GradientKeyColorProps kp;
            kp.value = k.v;
            kp.color = k.c;
            propsB.customKeys->push_back(kp);
        }
        auto b = ThematicGradientSettings::fromJSON(&propsB);
        EXPECT_NE(ThematicGradientSettings::compare(settingsA, b), 0);
    }
}

// Ported from: itwinjs-core core/common/src/test/ThematicGradientSettings.test.ts
//              it("computes texture transparency")
TEST(ThematicGradientSettingsTest, ComputesTextureTransparency)
{
    auto const op = ColorDef::red;
    auto const tr = ColorDef::blue.withTransparency(127);

    auto expectTransparency = [](TextureTransparency expected, ColorDef const& margin,
                                 std::vector<ColorDef> const& custom) {
        ThematicGradientSettingsProps props;
        props.colorScheme = !custom.empty() ? ThematicGradientColorScheme::Custom
                                            : ThematicGradientColorScheme::BlueRed;
        props.marginColor = margin.getTbgr();
        if (!custom.empty()) {
            props.customKeys = std::vector<GradientKeyColorProps>();
            for (auto const& c : custom) {
                GradientKeyColorProps kp;
                kp.value = 0.5;
                kp.color = c.getTbgr();
                props.customKeys->push_back(kp);
            }
        }
        auto settings = ThematicGradientSettings::fromJSON(&props);
        EXPECT_EQ(settings.textureTransparency(), expected);
    };

    const std::vector<ColorDef> none;
    expectTransparency(TextureTransparency::Opaque, op, none);
    expectTransparency(TextureTransparency::Mixed, tr, none);

    expectTransparency(TextureTransparency::Opaque, op, {op, op});
    expectTransparency(TextureTransparency::Translucent, tr, {tr, tr});
    expectTransparency(TextureTransparency::Mixed, op, {tr, tr});
    expectTransparency(TextureTransparency::Mixed, tr, {op, op});
    expectTransparency(TextureTransparency::Mixed, op, {tr, op});
    expectTransparency(TextureTransparency::Mixed, tr, {tr, op});
}

// Authored: no reference test exists for ThematicDisplay::equals field
// completeness — reference equals compares axis/sunDirection/sensorSettings
// (ThematicDisplay.ts:479-494); this lock guards the C++ port against
// field-omission regression (M-S 勘察断点 G9).
TEST(ThematicDisplayTest, EqualsCoversAllFields)
{
    auto base = ThematicDisplay::fromJSON();
    {
        auto other = base;
        other.axis = dqGeom::Vector3d::From(0, 0, 1);
        EXPECT_FALSE(base.equals(other));
    }
    {
        auto other = base;
        other.sunDirection = dqGeom::Vector3d::From(0, 0, -1);
        EXPECT_FALSE(base.equals(other));
    }
    {
        auto other = base;
        other.sensorSettings.distanceCutoff = 5.0;
        EXPECT_FALSE(base.equals(other));
    }
    {
        auto other = base;
        ThematicDisplaySensor s;
        s.position = dqGeom::Point3d::From(1, 2, 3);
        s.value = 0.5;
        other.sensorSettings.sensors.push_back(s);
        EXPECT_FALSE(base.equals(other));
    }
    {
        auto other = base;
        other.range = dqGeom::Range1d(0.0, 10.0);
        EXPECT_FALSE(base.equals(other));
    }
    {
        auto other = base;
        other.gradientSettings.marginColor = ColorDef::red;
        EXPECT_FALSE(base.equals(other));
    }
}

// Authored: no reference test exists in itwinjs-core for
// calculateSolarDirectionFromAngles（SolarCalculate.ts:191-197——参考仓无对应
// 测试文件）。值断言=轴正向三态 + DTA 面板默认（azimuth 315/elevation 45，
// ThematicDisplay.ts:21-35）。
TEST(SolarCalculateTest, DirectionFromAngles)
{
    auto d0 = calculateSolarDirectionFromAngles(0.0, 0.0);
    EXPECT_NEAR(d0.x, 0.0, 1e-12);
    EXPECT_NEAR(d0.y, -1.0, 1e-12);
    EXPECT_NEAR(d0.z, 0.0, 1e-12);

    auto d90 = calculateSolarDirectionFromAngles(90.0, 0.0);
    EXPECT_NEAR(d90.x, -1.0, 1e-12);
    EXPECT_NEAR(d90.y, 0.0, 1e-12);
    EXPECT_NEAR(d90.z, 0.0, 1e-12);

    auto dEl = calculateSolarDirectionFromAngles(0.0, 90.0);
    EXPECT_NEAR(dEl.x, 0.0, 1e-12);
    EXPECT_NEAR(dEl.y, 0.0, 1e-12);
    EXPECT_NEAR(dEl.z, -1.0, 1e-12);

    auto dDta = calculateSolarDirectionFromAngles(315.0, 45.0);
    EXPECT_NEAR(dDta.x, 0.5, 1e-12);
    EXPECT_NEAR(dDta.y, -0.5, 1e-12);
    EXPECT_NEAR(dDta.z, -std::sin(3.141592653589793 / 4.0), 1e-12);
}
