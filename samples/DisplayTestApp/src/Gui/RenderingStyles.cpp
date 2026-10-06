// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Rendering Style 14 预设表实现
// Ported from: itwinjs-core test-apps/display-test-app ViewAttributes.ts
//              renderingStyleViewFlags (:31-42) + renderingStyles (:44-268)
#include "RenderingStyles.h"

#include <dqApp/Viewport.h>

#include <dqCommon/AmbientOcclusion.h>
#include <dqCommon/Environment.h>
#include <dqCommon/GroundPlane.h>
#include <dqCommon/HiddenLine.h>
#include <dqCommon/LightSettings.h>
#include <dqCommon/RenderMode.h>
#include <dqCommon/SkyBox.h>
#include <dqCommon/ThematicDisplay.h>
#include <dqCommon/ViewFlags.h>

namespace Gui {
namespace {

using dqCommon::AmbientLightProps;
using dqCommon::DisplayStyle3dSettingsProps;
using dqCommon::EnvironmentProps;
using dqCommon::FresnelSettingsProps;
using dqCommon::GradientKeyColorProps;
using dqCommon::GroundPlaneProps;
using dqCommon::HemisphereLightsProps;
using dqCommon::HiddenLineSettingsProps;
using dqCommon::HiddenLineStyleProps;
using dqCommon::LightSettingsProps;
using dqCommon::LinePixels;
using dqCommon::SkyBoxProps;
using dqCommon::SolarLightProps;
using dqCommon::ThematicDisplayProps;
using dqCommon::ThematicGradientSettingsProps;
using dqCommon::ViewFlagProps;

// renderingStyleViewFlags（:31-42——十位基底，SmoothShade 渲染模式）。
ViewFlagProps renderingStyleViewFlags()
{
    ViewFlagProps vf;
    vf.noCameraLights = false;
    vf.noSourceLights = false;
    vf.noSolarLight = false;
    vf.visEdges = false;
    vf.hidEdges = false;
    vf.shadows = false;
    vf.monochrome = false;
    vf.ambientOcclusion = false;
    vf.thematicDisplay = false;
    vf.renderMode = dqCommon::RenderMode::SmoothShade;
    return vf;
}

// 参考 sky 段（Default/Sun-dappled/Comic Book/Outdoorsy/Soft/Gloss 共用）。
SkyBoxProps defaultSky()
{
    SkyBoxProps sky;
    sky.display = true;
    sky.groundColor = 8228728;
    sky.zenithColor = 16741686;
    sky.nadirColor = 3880;
    sky.skyColor = 16764303;
    return sky;
}

// 参考 ground 段（同上共用——display:false + elevation/above/below）。
GroundPlaneProps defaultGround()
{
    GroundPlaneProps ground;
    ground.display = false;
    ground.elevation = -0.01;
    ground.aboveColor = 32768;
    ground.belowColor = 1262987;
    return ground;
}

EnvironmentProps defaultEnvironment()
{
    EnvironmentProps env;
    env.sky = defaultSky();
    env.ground = defaultGround();
    return env;
}

// 参考 solar direction（Default/Illustration/Outdoorsy/Soft/Moonlit/Gloss 共用）。
SolarLightProps defaultSolarDir()
{
    SolarLightProps solar;
    solar.dirX = -0.9833878378071199;
    solar.dirY = -0.18098510351728977;
    solar.dirZ = 0.013883542698953828;
    return solar;
}

// Illustration/Schematic/Gloss/Moonlit 的 hline 基底（hidden 段一致）。
HiddenLineSettingsProps hlineBase()
{
    HiddenLineSettingsProps hline;
    HiddenLineStyleProps hidden;
    hidden.ovrColor = false;
    hidden.color = 16777215;
    hidden.pattern = LinePixels::HiddenLine;  // 3435973836
    hidden.width = 0;
    hline.hidden = hidden;
    hline.transThreshold = 1.0;
    return hline;
}

std::vector<RenderingStyle> makeRenderingStyles()
{
    std::vector<RenderingStyle> styles;

    // 1. None（:44-45——无字段）。
    styles.push_back(RenderingStyle{"None", DisplayStyle3dSettingsProps{}});

    // 2. Default（:46-60）。
    {
        RenderingStyle s{"Default", DisplayStyle3dSettingsProps{}};
        s.props.environment = defaultEnvironment();
        s.props.viewflags = renderingStyleViewFlags();
        LightSettingsProps lights;
        lights.solar = defaultSolarDir();
        s.props.lights = lights;
        styles.push_back(std::move(s));
    }

    // 3. Ambient（:61-74）。
    {
        RenderingStyle s{"Ambient", DisplayStyle3dSettingsProps{}};
        s.props.backgroundColor = 10921638;
        {
            EnvironmentProps env;
            SkyBoxProps sky;
            sky.display = false;
            env.sky = sky;
            GroundPlaneProps ground;
            ground.display = false;
            env.ground = ground;
            s.props.environment = env;
        }
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.ambientOcclusion = true;
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            SolarLightProps solar;
            solar.intensity = 0.0;
            lights.solar = solar;
            lights.portraitIntensity = 0.0;
            AmbientLightProps ambient;
            ambient.intensity = 0.55;
            lights.ambient = ambient;
            FresnelSettingsProps fresnel;
            fresnel.intensity = 0.8;
            fresnel.invert = true;
            lights.fresnel = fresnel;
            lights.specularIntensity = 0.0;
            s.props.lights = lights;
        }
        styles.push_back(std::move(s));
    }

    // 4. Illustration（:75-93）。
    {
        RenderingStyle s{"Illustration", DisplayStyle3dSettingsProps{}};
        s.props.environment = EnvironmentProps{};  // environment: {}
        s.props.backgroundColor = 10921638;
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.noCameraLights = true;
            vf.noSourceLights = true;
            vf.noSolarLight = true;
            vf.visEdges = true;
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            lights.solar = defaultSolarDir();
            s.props.lights = lights;
        }
        {
            HiddenLineSettingsProps hline = hlineBase();
            HiddenLineStyleProps visible;
            visible.ovrColor = true;
            visible.color = 0;
            visible.pattern = LinePixels::Solid;  // 0
            visible.width = 1;
            hline.visible = visible;
            s.props.hline = hline;
        }
        styles.push_back(std::move(s));
    }

    // 5. Sun-dappled（:94-108）。
    {
        RenderingStyle s{"Sun-dappled", DisplayStyle3dSettingsProps{}};
        s.props.environment = defaultEnvironment();
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.shadows = true;  // solarShadows 视觉面未移植（计划文档 E3）
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            SolarLightProps solar;
            solar.dirX = 0.9391245716329828;
            solar.dirY = 0.10165764029437066;
            solar.dirZ = -0.3281931795832247;
            lights.solar = solar;
            HemisphereLightsProps hemi;
            hemi.intensity = 0.2;
            lights.hemisphere = hemi;
            lights.portraitIntensity = 0.0;
            s.props.lights = lights;
        }
        styles.push_back(std::move(s));
    }

    // 6. Comic Book（:109-131）。
    {
        RenderingStyle s{"Comic Book", DisplayStyle3dSettingsProps{}};
        s.props.environment = defaultEnvironment();
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.noWeight = false;
            vf.visEdges = true;
            s.props.viewflags = vf;
        }
        {
            HiddenLineSettingsProps hline;
            HiddenLineStyleProps visible;
            visible.ovrColor = true;
            visible.color = 0;
            visible.pattern = LinePixels::Solid;
            visible.width = 3;
            hline.visible = visible;
            hline.transThreshold = 1.0;
            s.props.hline = hline;
        }
        {
            LightSettingsProps lights;
            SolarLightProps solar;
            solar.dirX = 0.7623;
            solar.dirY = 0.0505;
            solar.dirZ = -0.6453;
            solar.intensity = 1.95;
            solar.alwaysEnabled = true;
            lights.solar = solar;
            AmbientLightProps ambient;
            ambient.intensity = 0.2;
            lights.ambient = ambient;
            lights.portraitIntensity = 0.0;
            lights.specularIntensity = 0.0;
            lights.numCels = 2;
            s.props.lights = lights;
        }
        styles.push_back(std::move(s));
    }

    // 7. Outdoorsy（:132-148）。
    {
        RenderingStyle s{"Outdoorsy", DisplayStyle3dSettingsProps{}};
        s.props.environment = defaultEnvironment();
        s.props.viewflags = renderingStyleViewFlags();
        {
            LightSettingsProps lights;
            SolarLightProps solar = defaultSolarDir();
            solar.intensity = 1.05;
            lights.solar = solar;
            AmbientLightProps ambient;
            ambient.intensity = 0.25;
            lights.ambient = ambient;
            HemisphereLightsProps hemi;
            hemi.upperColor = dqCommon::RgbColorProps{206, 233, 255};
            hemi.intensity = 0.5;
            lights.hemisphere = hemi;
            lights.portraitIntensity = 0.0;
            s.props.lights = lights;
        }
        styles.push_back(std::move(s));
    }

    // 8. Schematic（:149-163）。
    {
        RenderingStyle s{"Schematic", DisplayStyle3dSettingsProps{}};
        s.props.environment = EnvironmentProps{};
        s.props.backgroundColor = 16777215;
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.visEdges = true;
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            SolarLightProps solar;
            solar.dirX = 0.0;
            solar.dirY = -0.6178171353958787;
            solar.dirZ = -0.7863218089378106;
            solar.intensity = 1.95;
            solar.alwaysEnabled = true;
            lights.solar = solar;
            AmbientLightProps ambient;
            ambient.intensity = 0.65;
            lights.ambient = ambient;
            lights.portraitIntensity = 0.0;
            lights.specularIntensity = 0.0;
            s.props.lights = lights;
        }
        {
            HiddenLineSettingsProps hline = hlineBase();
            HiddenLineStyleProps visible;
            visible.ovrColor = true;
            visible.color = 0;
            visible.pattern = LinePixels::Solid;
            visible.width = 1;
            hline.visible = visible;
            s.props.hline = hline;
        }
        styles.push_back(std::move(s));
    }

    // 9. Soft（:164-181）。
    {
        RenderingStyle s{"Soft", DisplayStyle3dSettingsProps{}};
        s.props.environment = defaultEnvironment();
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.ambientOcclusion = true;
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            SolarLightProps solar = defaultSolarDir();
            solar.intensity = 0.0;
            lights.solar = solar;
            AmbientLightProps ambient;
            ambient.intensity = 0.75;
            lights.ambient = ambient;
            HemisphereLightsProps hemi;
            hemi.intensity = 0.3;
            lights.hemisphere = hemi;
            lights.portraitIntensity = 0.5;
            lights.specularIntensity = 0.4;
            s.props.lights = lights;
        }
        {
            dqCommon::AmbientOcclusion::Props ao;  // 视觉面未移植（计划文档 E2）
            ao.bias = 0.25;
            ao.zLengthCap = 0.0025;
            ao.maxDistance = 100.0;
            ao.intensity = 1.0;
            ao.texelStepSize = 1.0;
            ao.blurDelta = 1.5;
            ao.blurSigma = 2.0;
            ao.blurTexelStepSize = 1.0;
            s.props.ao = ao;
        }
        styles.push_back(std::move(s));
    }

    // 10. Moonlit（:182-205）。
    {
        RenderingStyle s{"Moonlit", DisplayStyle3dSettingsProps{}};
        {
            EnvironmentProps env;
            SkyBoxProps sky;
            sky.display = true;
            sky.groundColor = 2435876;
            sky.zenithColor = 0;
            sky.nadirColor = 3880;
            sky.skyColor = 3481088;
            env.sky = sky;
            env.ground = defaultGround();
            s.props.environment = env;
        }
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.visEdges = true;
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            SolarLightProps solar = defaultSolarDir();
            solar.intensity = 3.0;
            solar.alwaysEnabled = true;
            lights.solar = solar;
            AmbientLightProps ambient;
            ambient.intensity = 0.05;
            lights.ambient = ambient;
            HemisphereLightsProps hemi;
            hemi.lowerColor = dqCommon::RgbColorProps{83, 100, 87};
            lights.hemisphere = hemi;
            lights.portraitIntensity = 0.0;
            lights.specularIntensity = 0.0;
            s.props.lights = lights;
        }
        s.props.monochromeMode = dqCommon::MonochromeMode::Flat;  // 0
        {
            HiddenLineSettingsProps hline = hlineBase();
            HiddenLineStyleProps visible;
            visible.ovrColor = true;
            visible.color = 0;
            visible.pattern = LinePixels::Invalid;  // -1
            visible.width = 0;
            hline.visible = visible;
            s.props.hline = hline;
        }
        s.props.monochromeColor = 7897479;
        styles.push_back(std::move(s));
    }

    // 11. Thematic: Height（:206-213——视觉面未移植，计划文档 E2）。
    {
        RenderingStyle s{"Thematic: Height", DisplayStyle3dSettingsProps{}};
        ViewFlagProps vf = renderingStyleViewFlags();
        vf.thematicDisplay = true;
        s.props.viewflags = vf;
        ThematicDisplayProps thematic;
        thematic.axis = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
        ThematicGradientSettingsProps grad;
        grad.mode = dqCommon::ThematicGradientMode::SteppedWithDelimiter;
        thematic.gradientSettings = grad;
        s.props.thematic = thematic;
        s.props.lights = LightSettingsProps{};  // lights: {}
        styles.push_back(std::move(s));
    }

    // 12. Thematic: Slope（:214-231）。
    {
        RenderingStyle s{"Thematic: Slope", DisplayStyle3dSettingsProps{}};
        ViewFlagProps vf = renderingStyleViewFlags();
        vf.thematicDisplay = true;
        s.props.viewflags = vf;
        ThematicDisplayProps thematic;
        thematic.displayMode = dqCommon::ThematicDisplayMode::Slope;
        thematic.rangeMin = 0.0;
        thematic.rangeMax = 90.0;
        thematic.axis = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
        ThematicGradientSettingsProps grad;
        grad.mode = dqCommon::ThematicGradientMode::Smooth;
        grad.colorScheme = dqCommon::ThematicGradientColorScheme::Custom;
        std::vector<GradientKeyColorProps> keys;
        keys.push_back(GradientKeyColorProps{0.0, 0x404040});
        keys.push_back(GradientKeyColorProps{1.0, 0xffffff});
        grad.customKeys = keys;
        thematic.gradientSettings = grad;
        s.props.thematic = thematic;
        s.props.lights = LightSettingsProps{};
        styles.push_back(std::move(s));
    }

    // 13. Gloss（:232-246）。
    {
        RenderingStyle s{"Gloss", DisplayStyle3dSettingsProps{}};
        s.props.environment = defaultEnvironment();
        {
            ViewFlagProps vf = renderingStyleViewFlags();
            vf.visEdges = true;
            s.props.viewflags = vf;
        }
        {
            LightSettingsProps lights;
            lights.solar = defaultSolarDir();
            lights.specularIntensity = 4.15;
            s.props.lights = lights;
        }
        {
            HiddenLineSettingsProps hline = hlineBase();
            HiddenLineStyleProps visible;
            visible.ovrColor = true;
            visible.color = 8026756;
            visible.pattern = LinePixels::Solid;
            visible.width = 1;
            hline.visible = visible;
            s.props.hline = hline;
        }
        styles.push_back(std::move(s));
    }

    // 14. Atmosphere（:247-268——atmosphere.display 段未移植，计划文档 E2）。
    {
        RenderingStyle s{"Atmosphere", DisplayStyle3dSettingsProps{}};
        EnvironmentProps env;
        SkyBoxProps sky;
        sky.display = true;
        env.sky = sky;
        GroundPlaneProps ground;
        ground.display = true;
        env.ground = ground;
        s.props.environment = env;
        s.props.viewflags = renderingStyleViewFlags();
        styles.push_back(std::move(s));
    }

    return styles;
}

}  // namespace

std::vector<RenderingStyle> const& renderingStyles()
{
    static std::vector<RenderingStyle> const s_styles = makeRenderingStyles();
    return s_styles;
}

// ViewAttributes.applyRenderingStyle（:283-286）。
bool applyRenderingStyle(dqApp::Viewport& vp, size_t index)
{
    std::vector<RenderingStyle> const& styles = renderingStyles();
    if (index >= styles.size())
        return false;
    RenderingStyle const& style = styles[index];
    if (style.name == "None")
        return false;
    vp.overrideDisplayStyle(style.props);
    return true;
}

}  // namespace Gui
