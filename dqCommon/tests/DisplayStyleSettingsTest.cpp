// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — DisplayStyleSettings unit tests
//
// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
#include <dqCommon/DisplayStyleSettings.h>

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqBase;

// ---------------------------------------------------------------------------
// DisplayStyleSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, DefaultConstruction)
{
    DisplayStyleSettings settings;
    EXPECT_FALSE(settings.is3d());
    EXPECT_NEAR(settings.getAnalysisFraction(), 0.0, 1e-10);
    EXPECT_FALSE(settings.hasRenderTimeline());
    EXPECT_FALSE(settings.getTimePoint().has_value());
    EXPECT_FALSE(settings.hasSubCategoryOverride());
    EXPECT_FALSE(settings.hasModelAppearanceOverride());
    EXPECT_TRUE(settings.getExcludedElements().empty());
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, ViewFlags)
{
    DisplayStyleSettings settings;
    ViewFlagsProperties props;
    props.renderMode = RenderMode::SmoothShade;
    ViewFlags flags(props);
    settings.setViewFlags(flags);
    EXPECT_EQ(settings.getViewFlags().renderMode(), RenderMode::SmoothShade);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, BackgroundColor)
{
    DisplayStyleSettings settings;
    settings.setBackgroundColor(ColorDef::red);
    EXPECT_TRUE(settings.getBackgroundColor().equals(ColorDef::red));
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, SubCategoryOverrides)
{
    DisplayStyleSettings settings;
    DqId subCatId(42);
    SubCategoryOverride ovr;
    ovr.invisible = true;

    settings.overrideSubCategory(subCatId, ovr);
    EXPECT_TRUE(settings.hasSubCategoryOverride());

    const auto* result = settings.getSubCategoryOverride(subCatId);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->invisible.has_value());

    settings.dropSubCategoryOverride(subCatId);
    EXPECT_FALSE(settings.hasSubCategoryOverride());
    EXPECT_EQ(settings.getSubCategoryOverride(subCatId), nullptr);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, ModelAppearanceOverrides)
{
    DisplayStyleSettings settings;
    DqId modelId(100);
    auto app = FeatureAppearance::fromTransparency(0.5, false);

    settings.overrideModelAppearance(modelId, app);
    EXPECT_TRUE(settings.hasModelAppearanceOverride());

    const auto* result = settings.getModelAppearanceOverride(modelId);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->overridesTransparency());

    settings.dropModelAppearanceOverride(modelId);
    EXPECT_FALSE(settings.hasModelAppearanceOverride());
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, ExcludedElements)
{
    DisplayStyleSettings settings;
    DqId elemId(200);

    settings.addExcludedElement(elemId);
    EXPECT_TRUE(settings.isExcluded(elemId));
    EXPECT_EQ(settings.getExcludedElements().size(), 1u);

    settings.dropExcludedElement(elemId);
    EXPECT_FALSE(settings.isExcluded(elemId));

    settings.addExcludedElement(DqId(1));
    settings.addExcludedElement(DqId(2));
    settings.clearExcludedElements();
    EXPECT_TRUE(settings.getExcludedElements().empty());
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, ClipStyle)
{
    DisplayStyleSettings settings;
    const auto& cs = settings.getClipStyle();
    EXPECT_FALSE(cs.produceCutGeometry);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, WhiteOnWhiteReversal)
{
    DisplayStyleSettings settings;
    EXPECT_FALSE(settings.getWhiteOnWhiteReversal().ignoreBackgroundColor);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, TimePoint)
{
    DisplayStyleSettings settings;
    EXPECT_FALSE(settings.getTimePoint().has_value());
    settings.setTimePoint(42.0);
    ASSERT_TRUE(settings.getTimePoint().has_value());
    EXPECT_NEAR(*settings.getTimePoint(), 42.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyleSettingsTest, Roundtrip)
{
    DisplayStyleSettings settings;
    settings.setBackgroundColor(ColorDef::from(128, 64, 32));
    settings.setAnalysisFraction(0.5);

    auto j = settings.toJSON();
    EXPECT_EQ(j.backgroundColor, ColorDef::from(128, 64, 32).getTbgr());
    EXPECT_NEAR(*j.analysisFraction, 0.5, 1e-10);
}

// ---------------------------------------------------------------------------
// DisplayStyle3dSettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyle3dSettingsTest, DefaultConstruction)
{
    DisplayStyle3dSettings settings;
    EXPECT_TRUE(settings.is3d());
    EXPECT_FALSE(settings.getEnvironment().displaySky);
    EXPECT_EQ(settings.getThematic().displayMode, ThematicDisplayMode::Height);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyle3dSettingsTest, Environment)
{
    DisplayStyle3dSettings settings;
    settings.toggleSkyBox(true);
    EXPECT_TRUE(settings.getEnvironment().displaySky);

    settings.toggleGroundPlane(true);
    EXPECT_TRUE(settings.getEnvironment().displayGround);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyle3dSettingsTest, Thematic)
{
    DisplayStyle3dSettings settings;
    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Slope;
    settings.setThematic(td);
    EXPECT_EQ(settings.getThematic().displayMode, ThematicDisplayMode::Slope);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyle3dSettingsTest, AmbientOcclusion)
{
    DisplayStyle3dSettings settings;
    auto ao = AmbientOcclusion::Settings::defaults();
    ao.intensity = 2.0;
    settings.setAmbientOcclusionSettings(ao);
    EXPECT_NEAR(settings.getAmbientOcclusionSettings().intensity, 2.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for DisplayStyleSettings
TEST(DisplayStyle3dSettingsTest, Roundtrip3d)
{
    DisplayStyle3dSettings settings;
    settings.setBackgroundColor(ColorDef::from(10, 20, 30));
    settings.toggleSkyBox(true);

    auto j = settings.toJSON3d();
    EXPECT_EQ(j.backgroundColor, ColorDef::from(10, 20, 30).getTbgr());
    ASSERT_TRUE(j.environment.has_value());
    EXPECT_TRUE(j.environment->sky.has_value());
    EXPECT_TRUE(j.environment->sky->display);
}
