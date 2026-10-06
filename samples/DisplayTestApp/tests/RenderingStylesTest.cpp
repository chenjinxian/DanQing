// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-Q Q-b Rendering Style 14 预设表
//
// Authored: no reference test exists in display-test-app for renderingStyles
//           （ViewAttributes.ts 纯数据表无测试；断言锚 = 参考表逐字段的
//           字面值——ViewAttributes.ts:31-268）。
//
// 覆盖面：
//  - 名称序 14 项（参考数组序）。
//  - 代表字段精确值（每族至少一项）：Illustration[noCamera/noSource/noSolar/
//    visEdges + hline visible (0,Solid,1) + hidden (白,HiddenLine,0)]；
//    Moonlit[monochromeColor 7897479 + solar intensity 3 alwaysEnabled +
//    hemisphere lowerColor (83,100,87) + visible pattern Invalid width 0]；
//    Comic Book[numCels=2 + solar (0.7623,0.0505,-0.6453) 1.95 alwaysEnabled +
//    visible width=3]；Soft[ao 8 参 + lights 五段]；Thematic:Height[axis z +
//    SteppedWithDelimiter]；Thematic:Slope[Slope + range[0,90] + Custom 双色键
//    0x404040/0xffffff]；Sun-dappled[shadows 位 + solar dir]；Schematic[白底
//    16777215]；Ambient[fresnel 0.8 invert + ambient 0.55 + specular 0]；
//    Atmosphere[sky+ground display]；Default[env 四色 + solar dir]。
//  - applyRenderingStyle：None no-op（不触 viewport）；越界 false。
#include <gtest/gtest.h>

#include "Gui/RenderingStyles.h"

#include <dqApp/BlankConnection.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqCommon/LinePixels.h>
#include <dqCommon/ThematicDisplay.h>

#include <memory>

namespace {

using Gui::renderingStyles;
using Gui::applyRenderingStyle;

struct StyleFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    std::unique_ptr<dqApp::Viewport> vp;

    StyleFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100, -100, -100),
                                        dqGeom::Point3d::From(100, 100, 100));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
            dqGeom::Vector3d::From(200, 200, 200));
        vp.reset(dqApp::Viewport::Create(nullptr, view));
    }
};

}  // namespace

// Authored（参考数组序 :44-268）
TEST(RenderingStylesTest, FourteenNamesInReferenceOrder)
{
    auto const& styles = renderingStyles();
    ASSERT_EQ(styles.size(), 14u);
    char const* const kNames[] = {
        "None", "Default", "Ambient", "Illustration", "Sun-dappled", "Comic Book",
        "Outdoorsy", "Schematic", "Soft", "Moonlit", "Thematic: Height",
        "Thematic: Slope", "Gloss", "Atmosphere",
    };
    for (size_t i = 0; i < 14; ++i)
        EXPECT_EQ(styles[i].name, kNames[i]) << i;
}

// Authored（Illustration :75-93 + Gloss :232-246 的 hline 族 + Schematic 白底）
TEST(RenderingStylesTest, EdgeStylesCarryHlineOverrides)
{
    auto const& styles = renderingStyles();
    auto const& illustration = styles[3];
    ASSERT_TRUE(illustration.props.viewflags.has_value());
    EXPECT_TRUE(illustration.props.viewflags->noCameraLights.value_or(false));
    EXPECT_TRUE(illustration.props.viewflags->noSourceLights.value_or(false));
    EXPECT_TRUE(illustration.props.viewflags->noSolarLight.value_or(false));
    EXPECT_TRUE(illustration.props.viewflags->visEdges.value_or(false));
    ASSERT_TRUE(illustration.props.hline.has_value());
    ASSERT_TRUE(illustration.props.hline->visible.has_value());
    EXPECT_TRUE(illustration.props.hline->visible->ovrColor.value_or(false));
    ASSERT_TRUE(illustration.props.hline->visible->color.has_value());
    EXPECT_EQ(*illustration.props.hline->visible->color, 0u);
    EXPECT_EQ(illustration.props.hline->visible->pattern.value_or(dqCommon::LinePixels::Solid),
              dqCommon::LinePixels::Solid);
    EXPECT_EQ(illustration.props.hline->visible->width.value_or(-1), 1);
    ASSERT_TRUE(illustration.props.hline->hidden.has_value());
    ASSERT_TRUE(illustration.props.hline->hidden->color.has_value());
    EXPECT_EQ(*illustration.props.hline->hidden->color, 16777215u);
    EXPECT_EQ(illustration.props.hline->hidden->pattern.value_or(dqCommon::LinePixels::Solid),
              dqCommon::LinePixels::HiddenLine);  // 3435973836
    EXPECT_DOUBLE_EQ(illustration.props.hline->transThreshold.value_or(0.0), 1.0);

    // Gloss：visible 色 8026756。
    auto const& gloss = styles[12];
    ASSERT_TRUE(gloss.props.hline.has_value());
    ASSERT_TRUE(gloss.props.hline->visible.has_value());
    ASSERT_TRUE(gloss.props.hline->visible->color.has_value());
    EXPECT_EQ(*gloss.props.hline->visible->color, 8026756u);

    // Schematic：白底 16777215。
    ASSERT_TRUE(styles[7].props.backgroundColor.has_value());
    EXPECT_EQ(*styles[7].props.backgroundColor, 16777215u);
}

// Authored（Moonlit :182-205）
TEST(RenderingStylesTest, MoonlitMonochromeAndLights)
{
    auto const& moonlit = renderingStyles()[9];
    ASSERT_TRUE(moonlit.props.monochromeColor.has_value());
    EXPECT_EQ(*moonlit.props.monochromeColor, 7897479u);
    ASSERT_TRUE(moonlit.props.monochromeMode.has_value());
    EXPECT_EQ(*moonlit.props.monochromeMode, dqCommon::MonochromeMode::Flat);
    ASSERT_TRUE(moonlit.props.lights.has_value());
    auto const& lights = *moonlit.props.lights;
    ASSERT_TRUE(lights.solar.has_value());
    EXPECT_DOUBLE_EQ(lights.solar->intensity.value_or(0.0), 3.0);
    EXPECT_TRUE(lights.solar->alwaysEnabled.value_or(false));
    ASSERT_TRUE(lights.hemisphere.has_value());
    ASSERT_TRUE(lights.hemisphere->lowerColor.has_value());
    EXPECT_EQ(lights.hemisphere->lowerColor->r, 83);
    EXPECT_EQ(lights.hemisphere->lowerColor->g, 100);
    EXPECT_EQ(lights.hemisphere->lowerColor->b, 87);
    ASSERT_TRUE(moonlit.props.hline.has_value());
    ASSERT_TRUE(moonlit.props.hline->visible.has_value());
    EXPECT_EQ(moonlit.props.hline->visible->pattern.value_or(dqCommon::LinePixels::Solid),
              dqCommon::LinePixels::Invalid);  // -1
    EXPECT_EQ(moonlit.props.hline->visible->width.value_or(-1), 0);
    // Moonlit sky 色族。
    ASSERT_TRUE(moonlit.props.environment.has_value());
    ASSERT_TRUE(moonlit.props.environment->sky.has_value());
    EXPECT_TRUE(moonlit.props.environment->sky->display);
    ASSERT_TRUE(moonlit.props.environment->sky->groundColor.has_value());
    EXPECT_EQ(*moonlit.props.environment->sky->groundColor, 2435876u);
    ASSERT_TRUE(moonlit.props.environment->sky->skyColor.has_value());
    EXPECT_EQ(*moonlit.props.environment->sky->skyColor, 3481088u);
}

// Authored（Comic Book :109-131 + Sun-dappled :94-108 + Ambient :61-74 + Soft :164-181）
TEST(RenderingStylesTest, LightRigsMatchReference)
{
    auto const& styles = renderingStyles();

    // Comic Book：numCels=2 + solar (0.7623,0.0505,-0.6453) 1.95 alwaysEnabled
    // + visible width=3。
    auto const& comic = styles[5];
    ASSERT_TRUE(comic.props.lights.has_value());
    ASSERT_TRUE(comic.props.lights->numCels.has_value());
    EXPECT_EQ(*comic.props.lights->numCels, 2);
    ASSERT_TRUE(comic.props.lights->solar.has_value());
    EXPECT_DOUBLE_EQ(comic.props.lights->solar->dirX.value_or(0.0), 0.7623);
    EXPECT_DOUBLE_EQ(comic.props.lights->solar->dirY.value_or(0.0), 0.0505);
    EXPECT_DOUBLE_EQ(comic.props.lights->solar->dirZ.value_or(0.0), -0.6453);
    EXPECT_DOUBLE_EQ(comic.props.lights->solar->intensity.value_or(0.0), 1.95);
    ASSERT_TRUE(comic.props.hline.has_value());
    ASSERT_TRUE(comic.props.hline->visible.has_value());
    EXPECT_EQ(comic.props.hline->visible->width.value_or(-1), 3);

    // Sun-dappled：shadows 位 + solar dir。
    auto const& dappled = styles[4];
    ASSERT_TRUE(dappled.props.viewflags.has_value());
    EXPECT_TRUE(dappled.props.viewflags->shadows.value_or(false));
    ASSERT_TRUE(dappled.props.lights.has_value());
    ASSERT_TRUE(dappled.props.lights->solar.has_value());
    EXPECT_DOUBLE_EQ(dappled.props.lights->solar->dirX.value_or(0.0), 0.9391245716329828);
    EXPECT_DOUBLE_EQ(dappled.props.lights->solar->dirZ.value_or(0.0), -0.3281931795832247);

    // Ambient：fresnel 0.8 invert + ambient 0.55 + specular 0 + bg 10921638。
    auto const& ambient = styles[2];
    ASSERT_TRUE(ambient.props.backgroundColor.has_value());
    EXPECT_EQ(*ambient.props.backgroundColor, 10921638u);
    ASSERT_TRUE(ambient.props.lights.has_value());
    ASSERT_TRUE(ambient.props.lights->fresnel.has_value());
    EXPECT_DOUBLE_EQ(ambient.props.lights->fresnel->intensity.value_or(0.0), 0.8);
    EXPECT_TRUE(ambient.props.lights->fresnel->invert.value_or(false));
    ASSERT_TRUE(ambient.props.lights->ambient.has_value());
    EXPECT_DOUBLE_EQ(ambient.props.lights->ambient->intensity.value_or(0.0), 0.55);
    ASSERT_TRUE(ambient.props.lights->specularIntensity.has_value());
    EXPECT_DOUBLE_EQ(*ambient.props.lights->specularIntensity, 0.0);

    // Soft：ao 8 参。
    auto const& soft = styles[8];
    ASSERT_TRUE(soft.props.ao.has_value());
    EXPECT_DOUBLE_EQ(soft.props.ao->bias.value_or(0.0), 0.25);
    EXPECT_DOUBLE_EQ(soft.props.ao->zLengthCap.value_or(0.0), 0.0025);
    EXPECT_DOUBLE_EQ(soft.props.ao->maxDistance.value_or(0.0), 100.0);
    EXPECT_DOUBLE_EQ(soft.props.ao->blurDelta.value_or(0.0), 1.5);
    EXPECT_DOUBLE_EQ(soft.props.ao->blurSigma.value_or(0.0), 2.0);
    ASSERT_TRUE(soft.props.viewflags.has_value());
    EXPECT_TRUE(soft.props.viewflags->ambientOcclusion.value_or(false));
}

// Authored（Thematic :206-231 + Atmosphere :247-268 + Default :46-60）
TEST(RenderingStylesTest, ThematicAndAtmosphereAndDefault)
{
    auto const& styles = renderingStyles();

    auto const& height = styles[10];
    ASSERT_TRUE(height.props.viewflags.has_value());
    EXPECT_TRUE(height.props.viewflags->thematicDisplay.value_or(false));
    ASSERT_TRUE(height.props.thematic.has_value());
    ASSERT_TRUE(height.props.thematic->axis.has_value());
    EXPECT_DOUBLE_EQ(height.props.thematic->axis->z, 1.0);
    ASSERT_TRUE(height.props.thematic->gradientSettings.has_value());
    EXPECT_EQ(height.props.thematic->gradientSettings->mode.value_or(dqCommon::ThematicGradientMode::Smooth),
              dqCommon::ThematicGradientMode::SteppedWithDelimiter);

    auto const& slope = styles[11];
    ASSERT_TRUE(slope.props.thematic.has_value());
    EXPECT_EQ(slope.props.thematic->displayMode.value_or(dqCommon::ThematicDisplayMode::Height),
              dqCommon::ThematicDisplayMode::Slope);
    EXPECT_DOUBLE_EQ(slope.props.thematic->rangeMin.value_or(-1.0), 0.0);
    EXPECT_DOUBLE_EQ(slope.props.thematic->rangeMax.value_or(-1.0), 90.0);
    ASSERT_TRUE(slope.props.thematic->gradientSettings.has_value());
    EXPECT_EQ(slope.props.thematic->gradientSettings->colorScheme.value_or(dqCommon::ThematicGradientColorScheme::BlueRed),
              dqCommon::ThematicGradientColorScheme::Custom);
    ASSERT_TRUE(slope.props.thematic->gradientSettings->customKeys.has_value());
    ASSERT_EQ(slope.props.thematic->gradientSettings->customKeys->size(), 2u);
    EXPECT_EQ((*slope.props.thematic->gradientSettings->customKeys)[0].color, 0x404040u);
    EXPECT_EQ((*slope.props.thematic->gradientSettings->customKeys)[1].color, 0xffffffu);

    auto const& atmosphere = styles[13];
    ASSERT_TRUE(atmosphere.props.environment.has_value());
    ASSERT_TRUE(atmosphere.props.environment->sky.has_value());
    EXPECT_TRUE(atmosphere.props.environment->sky->display);
    ASSERT_TRUE(atmosphere.props.environment->ground.has_value());
    EXPECT_TRUE(atmosphere.props.environment->ground->display);

    auto const& def = styles[1];
    ASSERT_TRUE(def.props.environment.has_value());
    ASSERT_TRUE(def.props.environment->sky.has_value());
    EXPECT_EQ(*def.props.environment->sky->groundColor, 8228728u);
    EXPECT_EQ(*def.props.environment->sky->zenithColor, 16741686u);
    EXPECT_EQ(*def.props.environment->sky->nadirColor, 3880u);
    EXPECT_EQ(*def.props.environment->sky->skyColor, 16764303u);
    ASSERT_TRUE(def.props.lights.has_value());
    ASSERT_TRUE(def.props.lights->solar.has_value());
    EXPECT_DOUBLE_EQ(def.props.lights->solar->dirX.value_or(0.0), -0.9833878378071199);
}

// Authored（applyRenderingStyle :283-286——None no-op + 应用面 + 越界）
TEST(RenderingStylesTest, ApplyRenderingStyleNoneNoOpAndApplies)
{
    StyleFixture f;

    // None：no-op（不改变任何设置）。
    uint32_t const bgBefore = f.view->GetDisplayStyle().getSettings().getBackgroundColor().getTbgr();
    EXPECT_FALSE(applyRenderingStyle(*f.vp, 0));
    EXPECT_EQ(f.view->GetDisplayStyle().getSettings().getBackgroundColor().getTbgr(), bgBefore);

    // Illustration：应用后 viewflags/hline/lights 生效 + merge 保位（grid）。
    {
        auto p = f.view->GetDisplayStyle().getViewFlags().Properties();
        p.grid = true;
        f.view->GetDisplayStyle().setViewFlags(dqCommon::ViewFlags(p));
    }
    EXPECT_TRUE(applyRenderingStyle(*f.vp, 3));
    auto const& settings = f.view->GetDisplayStyle().getSettings();
    EXPECT_TRUE(settings.getViewFlags().visibleEdges());
    EXPECT_FALSE(settings.getViewFlags().lighting());  // noCamera/noSource/noSolar
    EXPECT_TRUE(settings.getViewFlags().grid());       // merge 保位
    EXPECT_DOUBLE_EQ(settings.getLights().solar.direction.x, -0.9833878378071199);
    EXPECT_DOUBLE_EQ(settings.getHiddenLineSettings().transparencyThreshold, 1.0);

    // 越界：false。
    EXPECT_FALSE(applyRenderingStyle(*f.vp, 14));
    EXPECT_FALSE(applyRenderingStyle(*f.vp, static_cast<size_t>(-1)));
}
