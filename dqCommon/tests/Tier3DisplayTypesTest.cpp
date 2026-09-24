// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Tier 3 display types unit tests
//
// Authored: no reference tests exist in itwinjs-core for these types
#include <dqCommon/AnalysisStyle.h>
#include <dqCommon/Atmosphere.h>
#include <dqCommon/MapLayerSettings.h>

#include <gtest/gtest.h>

using namespace dqCommon;

// ---------------------------------------------------------------------------
// Atmosphere
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AtmosphereSettingsTest, defaults)
{
    const auto& d = Atmosphere::Settings::defaults();
    EXPECT_NEAR(d.atmosphereHeightAboveEarth, 100000.0, 1e-10);
    EXPECT_NEAR(d.exposure, 20.0, 1e-10);
    EXPECT_EQ(d.numViewRaySamples, 16);
    EXPECT_EQ(d.numSunRaySamples, 4);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AtmosphereSettingsTest, highQuality)
{
    const auto& hq = Atmosphere::Settings::highQuality();
    EXPECT_EQ(hq.numViewRaySamples, 64);
    EXPECT_EQ(hq.numSunRaySamples, 16);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AtmosphereSettingsTest, Roundtrip)
{
    auto s = Atmosphere::Settings::defaults();
    auto j = s.toJSON();
    auto s2 = Atmosphere::Settings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// AnalysisStyle
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AnalysisStyleTest, defaults)
{
    const auto& d = AnalysisStyle::defaults();
    EXPECT_FALSE(d.displacement.has_value());
    EXPECT_FALSE(d.scalar.has_value());
    EXPECT_FALSE(d.normalChannelName.has_value());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AnalysisStyleTest, WithDisplacement)
{
    AnalysisStyleProps props;
    AnalysisStyleDisplacementProps disp;
    disp.channelName = "displacement";
    disp.scale = 2.5;
    props.displacement = disp;

    auto s = AnalysisStyle::fromJSON(&props);
    ASSERT_TRUE(s.displacement.has_value());
    EXPECT_EQ(s.displacement->channelName, "displacement");
    EXPECT_NEAR(s.displacement->scale, 2.5, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AnalysisStyleTest, WithThematic)
{
    AnalysisStyleProps props;
    AnalysisStyleThematicProps scalar;
    scalar.channelName = "temperature";
    scalar.rangeMin = -40.0;
    scalar.rangeMax = 60.0;
    props.scalar = scalar;

    auto s = AnalysisStyle::fromJSON(&props);
    ASSERT_TRUE(s.scalar.has_value());
    EXPECT_EQ(s.scalar->channelName, "temperature");
    EXPECT_NEAR(s.scalar->rangeMin, -40.0, 1e-10);
    EXPECT_NEAR(s.scalar->rangeMax, 60.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(AnalysisStyleTest, Roundtrip)
{
    AnalysisStyleProps props;
    AnalysisStyleDisplacementProps disp;
    disp.channelName = "test";
    props.displacement = disp;
    props.normalChannelName = "normals";

    auto s = AnalysisStyle::fromJSON(&props);
    auto j = s.toJSON();
    auto s2 = AnalysisStyle::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// MapLayerSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(MapSubLayerSettingsTest, fromJSON)
{
    MapSubLayerProps props;
    props.name = "Layer1";
    props.visible = false;
    auto s = MapSubLayerSettings::fromJSON(props);
    EXPECT_EQ(s.name, "Layer1");
    EXPECT_FALSE(s.visible);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ImageMapLayerSettingsTest, fromJSON)
{
    ImageMapLayerProps props;
    props.name = "Bing Maps";
    props.url = "https://dev.virtualearth.net";
    props.formatId = "BingMaps";
    props.visible = true;
    props.transparency = 0.3;

    auto s = ImageMapLayerSettings::fromJSON(props);
    EXPECT_EQ(s.name, "Bing Maps");
    EXPECT_EQ(s.formatId, "BingMaps");
    EXPECT_TRUE(s.visible);
    EXPECT_NEAR(s.transparency, 0.3, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ImageMapLayerSettingsTest, WithSubLayers)
{
    ImageMapLayerProps props;
    props.name = "WMS";
    props.url = "https://example.com/wms";
    props.formatId = "WMS";

    MapSubLayerProps sl1;
    sl1.name = "roads";
    sl1.visible = true;
    MapSubLayerProps sl2;
    sl2.name = "buildings";
    sl2.visible = false;
    props.subLayers = {sl1, sl2};

    auto s = ImageMapLayerSettings::fromJSON(props);
    EXPECT_EQ(s.subLayers.size(), 2u);
    EXPECT_EQ(s.subLayers[0].name, "roads");
    EXPECT_TRUE(s.subLayers[0].visible);
    EXPECT_EQ(s.subLayers[1].name, "buildings");
    EXPECT_FALSE(s.subLayers[1].visible);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ImageMapLayerSettingsTest, Roundtrip)
{
    ImageMapLayerSettings s;
    s.name = "Test";
    s.url = "https://example.com";
    s.formatId = "WMS";
    s.transparency = 0.5;

    auto j = s.toJSON();
    auto s2 = ImageMapLayerSettings::fromJSON(j);
    EXPECT_TRUE(s.equals(s2));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ModelMapLayerSettingsTest, fromJSON)
{
    ModelMapLayerProps props;
    props.name = "Reality Model";
    props.modelId = 12345;
    props.drapeTarget = ModelMapLayerDrapeTarget::RealityData;

    auto s = ModelMapLayerSettings::fromJSON(props);
    EXPECT_EQ(s.name, "Reality Model");
    EXPECT_EQ(s.modelId, 12345u);
    EXPECT_EQ(s.drapeTarget, ModelMapLayerDrapeTarget::RealityData);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ModelMapLayerDrapeTargetTest, Values)
{
    EXPECT_EQ(static_cast<int>(ModelMapLayerDrapeTarget::Globe), 1);
    EXPECT_EQ(static_cast<int>(ModelMapLayerDrapeTarget::RealityData), 2);
    EXPECT_EQ(static_cast<int>(ModelMapLayerDrapeTarget::IModel), 4);
}
