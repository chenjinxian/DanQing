// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Tier 2 display types unit tests
//
// Authored: no reference tests exist in itwinjs-core for these types
#include <dqCommon/BackgroundMapSettings.h>
#include <dqCommon/ContourDisplay.h>
#include <dqCommon/PlanProjectionSettings.h>
#include <dqCommon/SolarShadowSettings.h>
#include <dqCommon/TerrainSettings.h>

#include <gtest/gtest.h>

using namespace dqCommon;

// ---------------------------------------------------------------------------
// SolarShadowSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SolarShadowSettingsTest, defaults)
{
    const auto& d = SolarShadowSettings::defaults();
    EXPECT_NEAR(d.bias, 0.001, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SolarShadowSettingsTest, Roundtrip)
{
    auto s = SolarShadowSettings::defaults();
    auto j = s.toJSON();
    auto s2 = SolarShadowSettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// TerrainSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(TerrainSettingsTest, defaults)
{
    const auto& d = TerrainSettings::defaults();
    EXPECT_EQ(d.providerName, "CesiumWorldTerrain");
    EXPECT_NEAR(d.exaggeration, 1.0, 1e-10);
    EXPECT_EQ(d.heightOriginMode, TerrainHeightOriginMode::Geodetic);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(TerrainSettingsTest, fromJSON)
{
    TerrainProps props;
    props.exaggeration = 2.5;
    props.applyLighting = true;
    auto s = TerrainSettings::fromJSON(&props);
    EXPECT_NEAR(s.exaggeration, 2.5, 1e-10);
    EXPECT_TRUE(s.applyLighting);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(TerrainSettingsTest, ExaggerationClamp)
{
    TerrainProps props;
    props.exaggeration = 500.0;  // exceeds max 100
    auto s = TerrainSettings::fromJSON(&props);
    EXPECT_NEAR(s.exaggeration, 100.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(TerrainSettingsTest, Roundtrip)
{
    auto s = TerrainSettings::defaults();
    auto j = s.toJSON();
    auto s2 = TerrainSettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// PlanProjectionSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(PlanProjectionSettingsTest, defaults)
{
    PlanProjectionSettings s;
    EXPECT_FALSE(s.overlay);
    EXPECT_FALSE(s.elevation.has_value());
    EXPECT_FALSE(s.transparency.has_value());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(PlanProjectionSettingsTest, fromJSON)
{
    PlanProjectionSettingsProps props;
    props.elevation = 100.0;
    props.transparency = 0.5;
    props.overlay = true;
    auto s = PlanProjectionSettings::fromJSON(&props);
    EXPECT_NEAR(*s.elevation, 100.0, 1e-10);
    EXPECT_NEAR(*s.transparency, 0.5, 1e-10);
    EXPECT_TRUE(s.overlay);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(PlanProjectionSettingsTest, Roundtrip)
{
    PlanProjectionSettingsProps props;
    props.elevation = 50.0;
    auto s = PlanProjectionSettings::fromJSON(&props);
    auto j = s.toJSON();
    auto s2 = PlanProjectionSettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// ContourDisplay
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourStyleTest, defaults)
{
    ContourStyle s;
    EXPECT_NEAR(s.pixelWidth, 1.0, 1e-10);
    EXPECT_EQ(s.pattern, LinePixels::Solid);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourStyleTest, Roundtrip)
{
    ContourStyle s;
    s.pixelWidth = 3.0;
    auto j = s.toJSON();
    auto s2 = ContourStyle::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
    EXPECT_NEAR(s2.pixelWidth, 3.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourTest, defaults)
{
    const auto& d = Contour::defaults();
    EXPECT_NEAR(d.majorStyle.pixelWidth, 2.0, 1e-10);
    EXPECT_NEAR(d.minorInterval, 1.0, 1e-10);
    EXPECT_EQ(d.majorIntervalCount, 5);
    EXPECT_TRUE(d.showGeometry);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourTest, Roundtrip)
{
    auto c = Contour::defaults();
    auto j = c.toJSON();
    auto c2 = Contour::fromJSON(&j);
    EXPECT_TRUE(c.equals(c2));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourGroupTest, defaults)
{
    ContourGroup g;
    EXPECT_TRUE(g.name.empty());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourGroupTest, fromJSON)
{
    ContourGroupProps props;
    props.name = "TestGroup";
    auto g = ContourGroup::fromJSON(&props);
    EXPECT_EQ(g.name, "TestGroup");
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourDisplayTest, defaults)
{
    const auto& d = ContourDisplay::defaults();
    EXPECT_FALSE(d.displayContours);
    EXPECT_TRUE(d.groups.empty());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourDisplayTest, fromJSON)
{
    ContourDisplayProps props;
    props.displayContours = true;
    ContourGroupProps gp;
    gp.name = "Group1";
    props.groups = {gp};
    auto d = ContourDisplay::fromJSON(&props);
    EXPECT_TRUE(d.displayContours);
    EXPECT_EQ(d.groups.size(), 1u);
    EXPECT_EQ(d.groups[0].name, "Group1");
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ContourDisplayTest, MaxContourGroups)
{
    EXPECT_EQ(ContourDisplay::MaxContourGroups, 5);
}

// ---------------------------------------------------------------------------
// BackgroundMapSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(BackgroundMapSettingsTest, defaults)
{
    const auto& d = BackgroundMapSettings::defaults();
    EXPECT_NEAR(d.groundBias, 0.0, 1e-10);
    EXPECT_FALSE(d.transparencyEnabled);
    EXPECT_FALSE(d.useDepthBuffer);
    EXPECT_FALSE(d.applyTerrain);
    EXPECT_EQ(d.globeMode, GlobeMode::Ellipsoid);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(BackgroundMapSettingsTest, fromJSON)
{
    BackgroundMapProps props;
    props.groundBias = 10.0;
    props.transparency = 0.5;
    props.applyTerrain = true;
    props.globeMode = GlobeMode::Plane;
    auto s = BackgroundMapSettings::fromJSON(&props);
    EXPECT_NEAR(s.groundBias, 10.0, 1e-10);
    EXPECT_TRUE(s.transparencyEnabled);
    EXPECT_NEAR(s.transparencyValue, 0.5, 1e-10);
    EXPECT_TRUE(s.applyTerrain);
    EXPECT_EQ(s.globeMode, GlobeMode::Plane);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(BackgroundMapSettingsTest, TransparencyFalse)
{
    BackgroundMapProps props;
    props.transparency = false;
    auto s = BackgroundMapSettings::fromJSON(&props);
    EXPECT_FALSE(s.transparencyEnabled);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(BackgroundMapSettingsTest, Roundtrip)
{
    auto s = BackgroundMapSettings::defaults();
    s.groundBias = 5.0;
    s.transparencyEnabled = true;
    s.transparencyValue = 0.3;
    auto j = s.toJSON();
    auto s2 = BackgroundMapSettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}
