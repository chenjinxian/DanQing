// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Map imagery settings
// Ported from: itwinjs-core core/common/src/MapImagerySettings.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "MapLayerSettings.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Base layer can be either a color or an image map layer.
using BaseLayerSettings = std::variant<ColorDef, ImageMapLayerSettings>;

// JSON persistence.
struct MapImageryProps {
    std::optional<uint32_t> backgroundBaseColor;  // ColorDefProps (if base is a color)
    std::optional<ImageMapLayerProps> backgroundBaseLayer;  // (if base is a layer)
    std::optional<std::vector<ImageMapLayerProps>> backgroundLayers;
    std::optional<std::vector<ImageMapLayerProps>> overlayLayers;
};

// Map imagery settings.
// Ported from: itwinjs-core MapImagerySettings
class DQ_COMMON_EXPORT MapImagerySettings {
public:
    BaseLayerSettings backgroundBase = ColorDef::black;
    std::vector<ImageMapLayerSettings> backgroundLayers;
    std::vector<ImageMapLayerSettings> overlayLayers;

    static MapImagerySettings fromJSON(const MapImageryProps* props = nullptr)
    {
        MapImagerySettings s;
        if (!props) return s;
        if (props->backgroundBaseColor)
            s.backgroundBase = ColorDef::fromTbgr(*props->backgroundBaseColor);
        else if (props->backgroundBaseLayer)
            s.backgroundBase = ImageMapLayerSettings::fromJSON(*props->backgroundBaseLayer);
        if (props->backgroundLayers) {
            for (const auto& lp : *props->backgroundLayers)
                s.backgroundLayers.push_back(ImageMapLayerSettings::fromJSON(lp));
        }
        if (props->overlayLayers) {
            for (const auto& lp : *props->overlayLayers)
                s.overlayLayers.push_back(ImageMapLayerSettings::fromJSON(lp));
        }
        return s;
    }

    MapImageryProps toJSON() const
    {
        MapImageryProps p;
        if (std::holds_alternative<ColorDef>(backgroundBase))
            p.backgroundBaseColor = std::get<ColorDef>(backgroundBase).getTbgr();
        else
            p.backgroundBaseLayer = std::get<ImageMapLayerSettings>(backgroundBase).toJSON();

        std::vector<ImageMapLayerProps> bv;
        for (const auto& l : backgroundLayers)
            bv.push_back(l.toJSON());
        p.backgroundLayers = std::move(bv);

        std::vector<ImageMapLayerProps> ov;
        for (const auto& l : overlayLayers)
            ov.push_back(l.toJSON());
        p.overlayLayers = std::move(ov);
        return p;
    }

    bool equals(const MapImagerySettings& rhs) const noexcept
    {
        // Simplified comparison — check variant index and layer counts
        const bool baseEq = backgroundBase.index() == rhs.backgroundBase.index();
        return baseEq && backgroundLayers.size() == rhs.backgroundLayers.size() &&
               overlayLayers.size() == rhs.overlayLayers.size();
    }

    MapImagerySettings clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
