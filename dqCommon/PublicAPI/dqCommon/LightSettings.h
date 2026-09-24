// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Light settings for 3D scenes
//
// Ported from: itwinjs-core core/common/src/LightSettings.ts
#pragma once

#include "Export.h"
#include "RgbColor.h"
#include "DqCommon.h"

#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Solar directional light properties.
// Ported from: itwinjs-core SolarLightProps
struct SolarLightProps {
    std::optional<double> intensity;
    std::optional<double> dirX, dirY, dirZ;
    std::optional<bool> alwaysEnabled;
    std::optional<double> timePoint;
};

// Solar directional light.
// Ported from: itwinjs-core SolarLight
class DQ_COMMON_EXPORT SolarLight {
public:
    dqGeom::Vector3d direction;
    double intensity = 1.0;
    bool alwaysEnabled = false;
    std::optional<double> timePoint;

    SolarLight();
    explicit SolarLight(const SolarLightProps& props);

    bool equals(const SolarLight& rhs) const noexcept;
    SolarLightProps toJSON() const;
};

// Ambient light properties.
// Ported from: itwinjs-core AmbientLightProps
struct AmbientLightProps {
    std::optional<RgbColorProps> color;
    std::optional<double> intensity;
};

// Ambient light.
// Ported from: itwinjs-core AmbientLight
class DQ_COMMON_EXPORT AmbientLight {
public:
    RgbColor color{0, 0, 0};
    double intensity = 0.2;

    AmbientLight() = default;
    explicit AmbientLight(const AmbientLightProps& props);

    bool equals(const AmbientLight& rhs) const noexcept;
    AmbientLightProps toJSON() const;
};

// Hemisphere lights properties.
// Ported from: itwinjs-core HemisphereLightsProps
struct HemisphereLightsProps {
    std::optional<RgbColorProps> upperColor;
    std::optional<RgbColorProps> lowerColor;
    std::optional<double> intensity;
};

// Hemisphere lights (upper/lower pair along Z axis).
// Ported from: itwinjs-core HemisphereLights
class DQ_COMMON_EXPORT HemisphereLights {
public:
    RgbColor upperColor{143, 205, 255};
    RgbColor lowerColor{120, 143, 125};
    double intensity = 0.0;

    HemisphereLights() = default;
    explicit HemisphereLights(const HemisphereLightsProps& props);

    bool equals(const HemisphereLights& rhs) const noexcept;
    HemisphereLightsProps toJSON() const;
};

// Fresnel settings properties.
// Ported from: itwinjs-core FresnelSettingsProps
struct FresnelSettingsProps {
    std::optional<double> intensity;
    std::optional<bool> invert;
};

// Fresnel effect settings.
// Ported from: itwinjs-core FresnelSettings
class DQ_COMMON_EXPORT FresnelSettings {
public:
    double intensity = 0.0;
    bool invert = false;

    FresnelSettings() = default;
    FresnelSettings(double intensity_, bool invert_) : intensity(intensity_), invert(invert_) {}

    static FresnelSettings fromJSON(const FresnelSettingsProps& props) noexcept;
    FresnelSettingsProps toJSON() const;
    bool equals(const FresnelSettings& rhs) const noexcept { return intensity == rhs.intensity && invert == rhs.invert; }
};

// Light settings properties.
// Ported from: itwinjs-core LightSettingsProps
struct LightSettingsProps {
    std::optional<double> portraitIntensity;
    std::optional<SolarLightProps> solar;
    std::optional<HemisphereLightsProps> hemisphere;
    std::optional<AmbientLightProps> ambient;
    std::optional<double> specularIntensity;
    std::optional<int> numCels;
    std::optional<FresnelSettingsProps> fresnel;
};

// Describes lighting for a 3D scene.
// Ported from: itwinjs-core LightSettings
class DQ_COMMON_EXPORT LightSettings {
public:
    SolarLight solar;
    AmbientLight ambient;
    HemisphereLights hemisphere;
    double portraitIntensity = 0.3;
    double specularIntensity = 1.0;
    int numCels = 0;
    FresnelSettings fresnel;

    LightSettings() = default;

    static LightSettings fromJSON(const LightSettingsProps& props);
    LightSettingsProps toJSON() const;
    bool equals(const LightSettings& rhs) const noexcept;
};

END_DQ_COMMON_NAMESPACE
