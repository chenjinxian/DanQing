// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Terrain settings
// Ported from: itwinjs-core core/common/src/TerrainSettings.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// Terrain height origin mode.
// Ported from: itwinjs-core TerrainHeightOriginMode
enum class TerrainHeightOriginMode : uint8_t {
    Geodetic = 0,
    Geoid = 1,
    Ground = 2,
};

// JSON persistence.
struct TerrainProps {
    std::optional<std::string> providerName;
    std::optional<std::string> dataSource;
    std::optional<double> exaggeration;
    std::optional<bool> applyLighting;
    std::optional<double> heightOrigin;
    std::optional<TerrainHeightOriginMode> heightOriginMode;
};

// Terrain rendering settings.
// Ported from: itwinjs-core TerrainSettings
class DQ_COMMON_EXPORT TerrainSettings {
public:
    std::string providerName = "CesiumWorldTerrain";
    std::string dataSource;
    double exaggeration = 1.0;
    bool applyLighting = false;
    double heightOrigin = 0.0;
    TerrainHeightOriginMode heightOriginMode = TerrainHeightOriginMode::Geodetic;

    static const TerrainSettings& defaults() noexcept
    {
        static const TerrainSettings s_default;
        return s_default;
    }

    static TerrainSettings fromJSON(const TerrainProps* props = nullptr)
    {
        TerrainSettings s;
        if (!props) return s;
        if (props->providerName) s.providerName = *props->providerName;
        if (props->dataSource) s.dataSource = *props->dataSource;
        if (props->exaggeration) s.exaggeration = std::clamp(*props->exaggeration, 0.1, 100.0);
        if (props->applyLighting) s.applyLighting = *props->applyLighting;
        if (props->heightOrigin) s.heightOrigin = *props->heightOrigin;
        if (props->heightOriginMode) s.heightOriginMode = *props->heightOriginMode;
        return s;
    }

    TerrainProps toJSON() const
    {
        TerrainProps p;
        p.providerName = providerName;
        p.dataSource = dataSource;
        p.exaggeration = exaggeration;
        p.applyLighting = applyLighting;
        p.heightOrigin = heightOrigin;
        p.heightOriginMode = heightOriginMode;
        return p;
    }

    bool equals(const TerrainSettings& rhs) const noexcept
    {
        return providerName == rhs.providerName && dataSource == rhs.dataSource &&
               exaggeration == rhs.exaggeration && applyLighting == rhs.applyLighting &&
               heightOrigin == rhs.heightOrigin && heightOriginMode == rhs.heightOriginMode;
    }

    TerrainSettings clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
