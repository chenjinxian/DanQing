// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Map layer settings
// Ported from: itwinjs-core core/common/src/MapLayerSettings.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Model map layer drape target (bit flags).
// Ported from: itwinjs-core ModelMapLayerDrapeTarget
enum class ModelMapLayerDrapeTarget : uint8_t {
    Globe = 1,
    RealityData = 2,
    IModel = 4,
};

// Map sub-layer.
// Ported from: itwinjs-core MapSubLayerSettings
struct MapSubLayerProps {
    std::string name;
    std::optional<std::string> title;
    std::optional<bool> visible;
    std::optional<std::string> id;
    std::optional<std::string> parent;
};

// Map sub-layer settings.
// Ported from: itwinjs-core MapSubLayerSettings
class DQ_COMMON_EXPORT MapSubLayerSettings {
public:
    std::string name;
    std::string title;
    bool visible = true;
    std::string id;
    std::string parent;

    static MapSubLayerSettings fromJSON(const MapSubLayerProps& props)
    {
        MapSubLayerSettings s;
        s.name = props.name;
        if (props.title) s.title = *props.title;
        if (props.visible) s.visible = *props.visible;
        if (props.id) s.id = *props.id;
        if (props.parent) s.parent = *props.parent;
        return s;
    }

    MapSubLayerProps toJSON() const
    {
        MapSubLayerProps p;
        p.name = name;
        p.title = title;
        p.visible = visible;
        p.id = id;
        p.parent = parent;
        return p;
    }

    bool equals(const MapSubLayerSettings& rhs) const noexcept
    {
        return name == rhs.name && visible == rhs.visible && id == rhs.id;
    }
};

// JSON persistence for image map layers.
struct ImageMapLayerProps {
    std::string name;
    std::string url;
    std::string formatId;
    std::optional<bool> visible;
    std::optional<double> transparency;
    std::optional<bool> transparentBackground;
    std::optional<std::vector<MapSubLayerProps>> subLayers;
};

// Image map layer settings (WMS, WMTS, ArcGIS, etc.).
// Ported from: itwinjs-core ImageMapLayerSettings
class DQ_COMMON_EXPORT ImageMapLayerSettings {
public:
    std::string name;
    std::string url;
    std::string formatId;
    bool visible = true;
    double transparency = 0.0;
    bool transparentBackground = true;
    std::vector<MapSubLayerSettings> subLayers;

    static ImageMapLayerSettings fromJSON(const ImageMapLayerProps& props)
    {
        ImageMapLayerSettings s;
        s.name = props.name;
        s.url = props.url;
        s.formatId = props.formatId;
        if (props.visible) s.visible = *props.visible;
        if (props.transparency) s.transparency = *props.transparency;
        if (props.transparentBackground) s.transparentBackground = *props.transparentBackground;
        if (props.subLayers) {
            for (const auto& sl : *props.subLayers)
                s.subLayers.push_back(MapSubLayerSettings::fromJSON(sl));
        }
        return s;
    }

    ImageMapLayerProps toJSON() const
    {
        ImageMapLayerProps p;
        p.name = name;
        p.url = url;
        p.formatId = formatId;
        p.visible = visible;
        p.transparency = transparency;
        p.transparentBackground = transparentBackground;
        std::vector<MapSubLayerProps> sv;
        for (const auto& sl : subLayers)
            sv.push_back(sl.toJSON());
        p.subLayers = std::move(sv);
        return p;
    }

    bool equals(const ImageMapLayerSettings& rhs) const noexcept
    {
        return name == rhs.name && url == rhs.url && formatId == rhs.formatId &&
               visible == rhs.visible && transparency == rhs.transparency;
    }
};

// JSON persistence for model map layers.
struct ModelMapLayerProps {
    std::string name;
    uint64_t modelId = 0;
    std::optional<bool> visible;
    std::optional<double> transparency;
    std::optional<ModelMapLayerDrapeTarget> drapeTarget;
};

// Model map layer settings.
// Ported from: itwinjs-core ModelMapLayerSettings
class DQ_COMMON_EXPORT ModelMapLayerSettings {
public:
    std::string name;
    uint64_t modelId = 0;
    bool visible = true;
    double transparency = 0.0;
    ModelMapLayerDrapeTarget drapeTarget = ModelMapLayerDrapeTarget::Globe;

    static ModelMapLayerSettings fromJSON(const ModelMapLayerProps& props)
    {
        ModelMapLayerSettings s;
        s.name = props.name;
        s.modelId = props.modelId;
        if (props.visible) s.visible = *props.visible;
        if (props.transparency) s.transparency = *props.transparency;
        if (props.drapeTarget) s.drapeTarget = *props.drapeTarget;
        return s;
    }

    ModelMapLayerProps toJSON() const
    {
        ModelMapLayerProps p;
        p.name = name;
        p.modelId = modelId;
        p.visible = visible;
        p.transparency = transparency;
        p.drapeTarget = drapeTarget;
        return p;
    }

    bool equals(const ModelMapLayerSettings& rhs) const noexcept
    {
        return name == rhs.name && modelId == rhs.modelId &&
               visible == rhs.visible && drapeTarget == rhs.drapeTarget;
    }
};

END_DQ_COMMON_NAMESPACE
