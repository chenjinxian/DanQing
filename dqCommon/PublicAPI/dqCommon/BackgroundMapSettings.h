// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Background map settings
// Ported from: itwinjs-core core/common/src/BackgroundMapSettings.ts
#pragma once

#include "Export.h"
#include "GlobeMode.h"
#include "TerrainSettings.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <variant>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct BackgroundMapProps {
    std::optional<double> groundBias;
    std::variant<std::monostate, double, bool> transparency;  // number | false
    std::optional<bool> useDepthBuffer;
    std::optional<bool> applyTerrain;
    std::optional<TerrainProps> terrainSettings;
    std::optional<GlobeMode> globeMode;
    std::optional<bool> nonLocatable;
};

// Background map rendering settings.
// Ported from: itwinjs-core BackgroundMapSettings
class DQ_COMMON_EXPORT BackgroundMapSettings {
public:
    double groundBias = 0.0;
    double transparencyValue = 0.0;
    bool transparencyEnabled = false;  // false = no transparency override
    bool useDepthBuffer = false;
    bool applyTerrain = false;
    TerrainSettings terrainSettings;
    GlobeMode globeMode = GlobeMode::Ellipsoid;

    static const BackgroundMapSettings& defaults() noexcept
    {
        static const BackgroundMapSettings s_default;
        return s_default;
    }

    static BackgroundMapSettings fromJSON(const BackgroundMapProps* props = nullptr)
    {
        BackgroundMapSettings s;
        if (!props) return s;
        if (props->groundBias) s.groundBias = *props->groundBias;
        if (std::holds_alternative<double>(props->transparency)) {
            s.transparencyEnabled = true;
            s.transparencyValue = std::get<double>(props->transparency);
        } else if (std::holds_alternative<bool>(props->transparency)) {
            s.transparencyEnabled = false;
        }
        if (props->useDepthBuffer) s.useDepthBuffer = *props->useDepthBuffer;
        if (props->applyTerrain) s.applyTerrain = *props->applyTerrain;
        if (props->terrainSettings) s.terrainSettings = TerrainSettings::fromJSON(&*props->terrainSettings);
        if (props->globeMode) s.globeMode = *props->globeMode;
        return s;
    }

    BackgroundMapProps toJSON() const
    {
        BackgroundMapProps p;
        p.groundBias = groundBias;
        if (transparencyEnabled)
            p.transparency = transparencyValue;
        else
            p.transparency = false;
        p.useDepthBuffer = useDepthBuffer;
        p.applyTerrain = applyTerrain;
        p.terrainSettings = terrainSettings.toJSON();
        p.globeMode = globeMode;
        return p;
    }

    bool equals(const BackgroundMapSettings& rhs) const noexcept
    {
        return groundBias == rhs.groundBias &&
               transparencyEnabled == rhs.transparencyEnabled &&
               transparencyValue == rhs.transparencyValue &&
               useDepthBuffer == rhs.useDepthBuffer &&
               applyTerrain == rhs.applyTerrain &&
               terrainSettings.equals(rhs.terrainSettings) &&
               globeMode == rhs.globeMode;
    }

    BackgroundMapSettings clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
