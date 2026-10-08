// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Display types unit tests
//
// Authored: no reference tests exist in itwinjs-core for these types
#include <dqCommon/AmbientOcclusion.h>
#include <dqCommon/ClipStyle.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/Environment.h>
#include <dqCommon/GroundPlane.h>
#include <dqCommon/SkyBox.h>
#include <dqCommon/ThematicDisplay.h>
#include <dqCommon/WhiteOnWhiteReversalSettings.h>

#include <gtest/gtest.h>

using namespace dqCommon;

// ---------------------------------------------------------------------------
// WhiteOnWhiteReversalSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(WhiteOnWhiteReversalTest, DefaultsIsNoIgnore)
{
    const auto& d = WhiteOnWhiteReversalSettings::defaults();
    EXPECT_FALSE(d.ignoreBackgroundColor);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(WhiteOnWhiteReversalTest, FromJSONNullptr)
{
    auto s = WhiteOnWhiteReversalSettings::fromJSON(nullptr);
    EXPECT_FALSE(s.ignoreBackgroundColor);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(WhiteOnWhiteReversalTest, FromJSONIgnore)
{
    WhiteOnWhiteReversalProps props;
    props.ignoreBackgroundColor = true;
    auto s = WhiteOnWhiteReversalSettings::fromJSON(&props);
    EXPECT_TRUE(s.ignoreBackgroundColor);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(WhiteOnWhiteReversalTest, toJSON)
{
    WhiteOnWhiteReversalProps props;
    props.ignoreBackgroundColor = true;
    auto s = WhiteOnWhiteReversalSettings::fromJSON(&props);
    auto j = s.toJSON();
    EXPECT_TRUE(j.ignoreBackgroundColor);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(WhiteOnWhiteReversalTest, SingletonIdentity)
{
    const auto& a = WhiteOnWhiteReversalSettings::fromJSON(nullptr);
    const auto& b = WhiteOnWhiteReversalSettings::fromJSON(nullptr);
    EXPECT_TRUE(a.equals(b));  // same instance via static locals
}

// ---------------------------------------------------------------------------
// GroundPlane
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(GroundPlaneTest, defaults)
{
    const auto& d = GroundPlane::defaults();
    EXPECT_NEAR(d.elevation, -0.01, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(GroundPlaneTest, fromJSON)
{
    GroundPlaneProps props;
    props.elevation = 100.0;
    auto gp = GroundPlane::fromJSON(&props);
    EXPECT_NEAR(gp.elevation, 100.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(GroundPlaneTest, Roundtrip)
{
    GroundPlane gp;
    gp.elevation = 50.0;
    gp.aboveColor = ColorDef::red;
    auto j = gp.toJSON(true);
    EXPECT_TRUE(j.display);
    EXPECT_NEAR(*j.elevation, 50.0, 1e-10);
}

// ---------------------------------------------------------------------------
// AmbientOcclusion
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AmbientOcclusionTest, defaults)
{
    const auto& d = AmbientOcclusion::Settings::defaults();
    EXPECT_NEAR(d.bias, 0.25, 1e-10);
    EXPECT_NEAR(d.intensity, 1.0, 1e-10);
    EXPECT_NEAR(d.maxDistance, 10000.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AmbientOcclusionTest, fromJSON)
{
    AmbientOcclusion::Props props;
    props.bias = 0.5;
    props.intensity = 2.0;
    auto s = AmbientOcclusion::Settings::fromJSON(&props);
    EXPECT_NEAR(s.bias, 0.5, 1e-10);
    EXPECT_NEAR(s.intensity, 2.0, 1e-10);
    // Other values should be defaults
    EXPECT_NEAR(s.maxDistance, 10000.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AmbientOcclusionTest, Roundtrip)
{
    auto s = AmbientOcclusion::Settings::defaults();
    auto j = s.toJSON();
    auto s2 = AmbientOcclusion::Settings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// SkyBox / SkyGradient
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SkyGradientTest, defaults)
{
    const auto& d = SkyGradient::defaults();
    EXPECT_FALSE(d.twoColor);
    EXPECT_NEAR(d.skyExponent, 4.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SkyGradientTest, fromJSON)
{
    SkyBoxProps props;
    props.twoColor = true;
    props.skyExponent = 8.0;
    auto g = SkyGradient::fromJSON(&props);
    EXPECT_TRUE(g.twoColor);
    EXPECT_NEAR(g.skyExponent, 8.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SkyGradientTest, Roundtrip)
{
    auto g = SkyGradient::defaults();
    auto j = g.toJSON();
    auto g2 = SkyGradient::fromJSON(&j);
    EXPECT_TRUE(g.equals(g2));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SkyBoxTest, defaults)
{
    const auto& d = SkyBox::defaults();
    EXPECT_FALSE(d.gradient.twoColor);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SkyBoxTest, fromJSON)
{
    SkyBoxProps props;
    props.skyExponent = 6.0;
    auto sb = SkyBox::fromJSON(&props);
    EXPECT_NEAR(sb.gradient.skyExponent, 6.0, 1e-10);
}

// ---------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(EnvironmentTest, defaults)
{
    const auto& d = Environment::defaults();
    EXPECT_FALSE(d.displaySky);
    EXPECT_FALSE(d.displayGround);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(EnvironmentTest, fromJSON)
{
    EnvironmentProps props;
    GroundPlaneProps gp;
    gp.display = true;
    gp.elevation = 50.0;
    props.ground = gp;
    auto env = Environment::fromJSON(&props);
    EXPECT_TRUE(env.displayGround);
    EXPECT_NEAR(env.ground.elevation, 50.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(EnvironmentTest, Roundtrip)
{
    auto env = Environment::defaults();
    env.displaySky = true;
    auto j = env.toJSON();
    EXPECT_TRUE(j.sky.has_value());
    EXPECT_TRUE(j.sky->display);
}

// ---------------------------------------------------------------------------
// ClipStyle
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(CutStyleTest, defaults)
{
    const auto& d = CutStyle::defaults();
    EXPECT_TRUE(d.matchesDefaults());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ClipIntersectionStyleTest, defaults)
{
    const auto& d = ClipIntersectionStyle::defaults();
    EXPECT_NEAR(d.width, 2.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ClipStyleTest, defaults)
{
    const auto& d = ClipStyle::defaults();
    EXPECT_FALSE(d.produceCutGeometry);
    EXPECT_FALSE(d.colorizeIntersection);
    EXPECT_TRUE(d.matchesDefaults());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ClipStyleTest, fromJSON)
{
    ClipStyleProps props;
    props.produceCutGeometry = true;
    auto cs = ClipStyle::fromJSON(&props);
    EXPECT_TRUE(cs.produceCutGeometry);
}

// ---------------------------------------------------------------------------
// ThematicDisplay
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ThematicGradientSettingsTest, defaults)
{
    const auto& d = ThematicGradientSettings::defaults();
    EXPECT_EQ(d.mode, ThematicGradientMode::Smooth);
    EXPECT_EQ(d.colorScheme, ThematicGradientColorScheme::BlueRed);
    EXPECT_NEAR(ThematicGradientSettings::margin(), 0.001, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ThematicGradientSettingsTest, fromJSON)
{
    ThematicGradientSettingsProps props;
    props.mode = ThematicGradientMode::Stepped;
    props.stepCount = 10;
    auto s = ThematicGradientSettings::fromJSON(&props);
    EXPECT_EQ(s.mode, ThematicGradientMode::Stepped);
    EXPECT_EQ(s.stepCount, 10);
}

// Authored: no reference tests exist in itwinjs-core for these types
// （M-S 订正：value 越界钳 [0,1]——ThematicDisplay.ts:308-311；42.0→1.0。
// 原断言 42.0 系发散形态，参考测试 ThematicDisplay.test.ts:75-77 同款钳制）。
TEST(ThematicDisplaySensorTest, fromJSON)
{
    ThematicDisplaySensorProps props;
    props.position = dqGeom::Point3d::From(1, 2, 3);
    props.value = 42.0;
    auto s = ThematicDisplaySensor::fromJSON(&props);
    EXPECT_TRUE(s.position.IsEqual(dqGeom::Point3d::From(1, 2, 3)));
    EXPECT_NEAR(s.value, 1.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
// （M-S 订正：range 默认归位参考 null range——ThematicDisplay.ts:505
// `Range1d.fromJSON()` 无参=null；原 0/1 断言系发散形态）。
TEST(ThematicDisplayTest, defaults)
{
    const auto& d = ThematicDisplay::defaults();
    EXPECT_EQ(d.displayMode, ThematicDisplayMode::Height);
    EXPECT_TRUE(d.range.isNull());
    EXPECT_DOUBLE_EQ(d.axis.x, 0.0);
    EXPECT_DOUBLE_EQ(d.axis.y, 0.0);
    EXPECT_DOUBLE_EQ(d.axis.z, 0.0);
    EXPECT_DOUBLE_EQ(d.sunDirection.x, 0.0);
    EXPECT_DOUBLE_EQ(d.sunDirection.y, 0.0);
    EXPECT_DOUBLE_EQ(d.sunDirection.z, 0.0);
}

// Authored: no reference tests exist in itwinjs-core for these types
// （M-S 订正：Slope range 钳 [0,90]——ThematicDisplay.ts:524-527 构造校验）。
TEST(ThematicDisplayTest, fromJSON)
{
    ThematicDisplayProps props;
    props.displayMode = ThematicDisplayMode::Slope;
    props.range = dqGeom::Range1d(-100.0, 100.0);
    auto td = ThematicDisplay::fromJSON(&props);
    EXPECT_EQ(td.displayMode, ThematicDisplayMode::Slope);
    EXPECT_NEAR(td.range.low, 0.0, 1e-10);
    EXPECT_NEAR(td.range.high, 90.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ThematicDisplayTest, Roundtrip)
{
    auto td = ThematicDisplay::defaults();
    auto j = td.toJSON();
    auto td2 = ThematicDisplay::fromJSON(&j);
    EXPECT_TRUE(td.equals(td2));
}
