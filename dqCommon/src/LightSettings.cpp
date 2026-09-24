// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — LightSettings implementation
//
// Ported from: itwinjs-core core/common/src/LightSettings.ts
#include "dqCommon/LightSettings.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqGeom;

static constexpr double kMaxIntensity = 5.0;

static double ExtractIntensity(std::optional<double> value, double defaultVal)
{
    if (!value.has_value())
        return defaultVal;
    return std::max(0.0, std::min(kMaxIntensity, value.value()));
}

// Default solar direction (looking down at 45 degrees)
static const Vector3d kDefaultSolarDirection = Vector3d::From(0.272166, 0.680414, 0.680414);

// SolarLight
SolarLight::SolarLight() : direction(kDefaultSolarDirection) {}

SolarLight::SolarLight(const SolarLightProps& props)
    : intensity(ExtractIntensity(props.intensity, 1.0))
    , alwaysEnabled(props.alwaysEnabled.value_or(false))
    , timePoint(props.timePoint)
{
    if (props.dirX.has_value() && props.dirY.has_value() && props.dirZ.has_value())
        direction = Vector3d::From(props.dirX.value(), props.dirY.value(), props.dirZ.value());
    else
        direction = kDefaultSolarDirection;
}

bool SolarLight::equals(const SolarLight& rhs) const noexcept
{
    return intensity == rhs.intensity && alwaysEnabled == rhs.alwaysEnabled &&
           direction.IsEqual(rhs.direction) && timePoint == rhs.timePoint;
}

SolarLightProps SolarLight::toJSON() const
{
    SolarLightProps props;
    if (!direction.IsEqual(kDefaultSolarDirection)) {
        props.dirX = direction.x;
        props.dirY = direction.y;
        props.dirZ = direction.z;
    }
    if (intensity != 1.0)
        props.intensity = intensity;
    if (alwaysEnabled)
        props.alwaysEnabled = true;
    props.timePoint = timePoint;
    return props;
}

// AmbientLight
AmbientLight::AmbientLight(const AmbientLightProps& props)
    : intensity(ExtractIntensity(props.intensity, 0.2))
{
    if (props.color.has_value())
        color = RgbColor::fromJSON(&props.color.value());
}

bool AmbientLight::equals(const AmbientLight& rhs) const noexcept
{
    return intensity == rhs.intensity && color.equals(rhs.color);
}

AmbientLightProps AmbientLight::toJSON() const
{
    AmbientLightProps props;
    if (color.r != 0 || color.g != 0 || color.b != 0)
        props.color = RgbColorProps{color.r, color.g, color.b};
    if (intensity != 0.2)
        props.intensity = intensity;
    return props;
}

// HemisphereLights
HemisphereLights::HemisphereLights(const HemisphereLightsProps& props)
    : intensity(ExtractIntensity(props.intensity, 0.0))
{
    if (props.upperColor.has_value())
        upperColor = RgbColor::fromJSON(&props.upperColor.value());
    if (props.lowerColor.has_value())
        lowerColor = RgbColor::fromJSON(&props.lowerColor.value());
}

bool HemisphereLights::equals(const HemisphereLights& rhs) const noexcept
{
    return intensity == rhs.intensity && upperColor.equals(rhs.upperColor) && lowerColor.equals(rhs.lowerColor);
}

HemisphereLightsProps HemisphereLights::toJSON() const
{
    HemisphereLightsProps props;
    if (!upperColor.equals(RgbColor(143, 205, 255)))
        props.upperColor = RgbColorProps{upperColor.r, upperColor.g, upperColor.b};
    if (!lowerColor.equals(RgbColor(120, 143, 125)))
        props.lowerColor = RgbColorProps{lowerColor.r, lowerColor.g, lowerColor.b};
    if (intensity != 0.0)
        props.intensity = intensity;
    return props;
}

// FresnelSettings
FresnelSettings FresnelSettings::fromJSON(const FresnelSettingsProps& props) noexcept
{
    const double intens = std::max(0.0, props.intensity.value_or(0.0));
    const bool inv = props.invert.value_or(false);
    return FresnelSettings(intens, inv);
}

FresnelSettingsProps FresnelSettings::toJSON() const
{
    FresnelSettingsProps props;
    if (intensity != 0.0)
        props.intensity = intensity;
    if (invert)
        props.invert = true;
    return props;
}

// LightSettings
LightSettings LightSettings::fromJSON(const LightSettingsProps& props)
{
    LightSettings settings;
    settings.solar = SolarLight(props.solar.value_or(SolarLightProps{}));
    settings.ambient = AmbientLight(props.ambient.value_or(AmbientLightProps{}));
    settings.hemisphere = HemisphereLights(props.hemisphere.value_or(HemisphereLightsProps{}));
    settings.portraitIntensity = ExtractIntensity(props.portraitIntensity, 0.3);
    settings.specularIntensity = ExtractIntensity(props.specularIntensity, 1.0);
    settings.numCels = props.numCels.value_or(0);
    settings.fresnel = FresnelSettings::fromJSON(props.fresnel.value_or(FresnelSettingsProps{}));
    return settings;
}

LightSettingsProps LightSettings::toJSON() const
{
    LightSettingsProps props;
    auto solarJson = solar.toJSON();
    if (solarJson.dirX.has_value() || solarJson.intensity.has_value() || solarJson.alwaysEnabled.has_value() || solarJson.timePoint.has_value())
        props.solar = solarJson;
    auto ambientJson = ambient.toJSON();
    if (ambientJson.color.has_value() || ambientJson.intensity.has_value())
        props.ambient = ambientJson;
    auto hemiJson = hemisphere.toJSON();
    if (hemiJson.upperColor.has_value() || hemiJson.lowerColor.has_value() || hemiJson.intensity.has_value())
        props.hemisphere = hemiJson;
    if (portraitIntensity != 0.3)
        props.portraitIntensity = portraitIntensity;
    if (specularIntensity != 1.0)
        props.specularIntensity = specularIntensity;
    if (numCels != 0)
        props.numCels = numCels;
    auto fresnelJson = fresnel.toJSON();
    if (fresnelJson.intensity.has_value() || fresnelJson.invert.has_value())
        props.fresnel = fresnelJson;
    return props;
}

bool LightSettings::equals(const LightSettings& rhs) const noexcept
{
    return portraitIntensity == rhs.portraitIntensity &&
           specularIntensity == rhs.specularIntensity &&
           numCels == rhs.numCels &&
           ambient.equals(rhs.ambient) &&
           solar.equals(rhs.solar) &&
           hemisphere.equals(rhs.hemisphere) &&
           fresnel.equals(rhs.fresnel);
}

END_DQ_COMMON_NAMESPACE
