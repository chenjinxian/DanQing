// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon tests — Environment.atmosphere 载体（M-W 集成面）
//
// 行为锚定：itwinjs-core Environment.ts（atmosphere Props 字段 :25 /
// displayAtmosphere 成员 :53 / ctor 缺省 :70-71 / toJSON 恒携 display :100-106 /
// fromJSON 读 atmosphere.display :113-120）× Atmosphere.ts（Settings.toJSON(display?)
// :172-185 / equals :135-153）。
// 参考测试面：core/common/src/test/DisplayStyle.test.ts:345-360/:655-680 的
// environment.atmosphere round-trip 夹具腿（DisplayStyleSettings 级）——本文件锁
// Environment/Atmosphere 载体级同语义。
//
// Authored: no reference test exists in itwinjs-core for Environment.atmosphere
//           carrier in isolation（DisplayStyle.test.ts 仅经样式级夹具覆盖——
//           场景按参考实现语义构造，§5(f) 标注）。
//
// RED（落地前）：EnvironmentProps 无 atmosphere 字段、Atmosphere::Settings::toJSON
// 无 display 形参、equals 缺 depthBelowEarthForMaxDensity 维——编译期缺 API 红。
#include <gtest/gtest.h>

#include <dqCommon/Atmosphere.h>
#include <dqCommon/Environment.h>

namespace Atmosphere = dqCommon::Atmosphere;  // Atmosphere 是 dqCommon 内命名空间（非类——C2873 不得 using 声明）
using dqCommon::Environment;
using dqCommon::EnvironmentProps;

// 载体缺省：displayAtmosphere=false + atmosphere=defaults（Environment.ts:70-71）。
TEST(EnvironmentAtmosphereTest, DefaultsCarryAtmosphereDefaults)
{
    Environment env;
    EXPECT_FALSE(env.displayAtmosphere);
    EXPECT_TRUE(env.atmosphere.equals(Atmosphere::Settings::defaults()));
}

// toJSON 恒携 atmosphere+display（Environment.ts:100-106 × Atmosphere.ts:172-185）。
TEST(EnvironmentAtmosphereTest, ToJsonAlwaysWritesAtmosphereWithDisplay)
{
    Environment env;
    auto props = env.toJSON();
    ASSERT_TRUE(props.atmosphere.has_value());
    ASSERT_TRUE(props.atmosphere->display.has_value());
    EXPECT_FALSE(*props.atmosphere->display);
    // 默认参数全字段写出（Atmosphere.ts:172-181——无条件写，非省略语义）。
    ASSERT_TRUE(props.atmosphere->exposure.has_value());
    EXPECT_DOUBLE_EQ(20.0, *props.atmosphere->exposure);
    ASSERT_TRUE(props.atmosphere->wavelengths.has_value());
    EXPECT_DOUBLE_EQ(680.0, props.atmosphere->wavelengths->r);
}

// round-trip：改值+display=true → toJSON → fromJSON 逐字段守恒（DisplayStyle.test.ts
// :345-360 夹具腿的同语义载体级锁）。
TEST(EnvironmentAtmosphereTest, RoundTripPreservesValuesAndDisplay)
{
    Environment env;
    env.displayAtmosphere = true;
    env.atmosphere.exposure = 7.5;
    env.atmosphere.numViewRaySamples = 64;
    env.atmosphere.wavelengths.r = 700.0;

    auto written = env.toJSON();
    auto back = Environment::fromJSON(&written);
    EXPECT_TRUE(back.displayAtmosphere);
    EXPECT_DOUBLE_EQ(7.5, back.atmosphere.exposure);
    EXPECT_EQ(64, back.atmosphere.numViewRaySamples);
    EXPECT_DOUBLE_EQ(700.0, back.atmosphere.wavelengths.r);
}

// fromJSON 缺席语义：无 atmosphere 段 → defaults + display false（Environment.ts
// :113-120 的 props?.atmosphere 缺席路径）。
TEST(EnvironmentAtmosphereTest, FromJsonAbsentLegKeepsDefaults)
{
    EnvironmentProps props;  // atmosphere 缺席
    auto env = Environment::fromJSON(&props);
    EXPECT_FALSE(env.displayAtmosphere);
    EXPECT_TRUE(env.atmosphere.equals(Atmosphere::Settings::defaults()));
}

// equals 全字段维：depthBelowEarthForMaxDensity 差异必须判不等（归位前缺该维——
// Atmosphere.ts:140-141）。
TEST(EnvironmentAtmosphereTest, EqualsCoversDepthBelowEarthForMaxDensity)
{
    auto a = Atmosphere::Settings::defaults();
    auto b = Atmosphere::Settings::defaults();
    EXPECT_TRUE(a.equals(b));
    b.depthBelowEarthForMaxDensity = -1000.0;  // 默认 -2000
    EXPECT_FALSE(a.equals(b));
}

// toJSON 无参 → display 不写出（Atmosphere.ts:172-185 的 undefined 分支）。
TEST(EnvironmentAtmosphereTest, ToJsonWithoutDisplayArgOmitsDisplay)
{
    Atmosphere::Settings s;
    auto props = s.toJSON();
    EXPECT_FALSE(props.display.has_value());
    auto propsOn = s.toJSON(true);
    ASSERT_TRUE(propsOn.display.has_value());
    EXPECT_TRUE(*propsOn.display);
}
