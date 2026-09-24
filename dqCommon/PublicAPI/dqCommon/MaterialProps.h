// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Material properties (JSON representation)
//
// Ported from: itwinjs-core core/common/src/MaterialProps.ts
// JSON representation for RenderMaterial persistence.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Color as RGB factors [0..1].
// Ported from: itwinjs-core RgbFactorProps
using RgbFactorProps = std::vector<double>;

// 2D point as [x, y].
// Ported from: itwinjs-core Point2dProps
using Point2dProps = std::vector<double>;

// Texture map units.
// Ported from: itwinjs-core TextureMapUnits
enum class TextureMapUnits : uint8_t {
    Relative = 0,
    Meters = 3,
    Millimeters = 4,
    Feet = 5,
    Inches = 6,
};

// normal map flags.
// Ported from: itwinjs-core NormalMapFlags
enum class NormalMapFlags : uint8_t {
    None = 0,
    GreenUp = 1 << 0,
    UseConstantLod = 1 << 1,
};

// Texture mapping mode.
// Ported from: itwinjs-core TextureMapping.Mode
enum class TextureMappingMode : int8_t {
    None = -1,
    Parametric = 0,
    ElevationDrape = 1,
    Planar = 2,
    DirectionalDrape = 3,
    Cubic = 4,
    Spherical = 5,
    Cylindrical = 6,
    Solid = 7,
    FrontProject = 8,
};

// Texture map properties (JSON).
// Ported from: itwinjs-core TextureMapProps
struct DQ_COMMON_EXPORT TextureMapProps {
    std::optional<double> pattern_angle;
    std::optional<bool> pattern_u_flip;
    std::optional<bool> pattern_flip;
    std::optional<Point2dProps> pattern_scale;
    std::optional<Point2dProps> pattern_offset;
    std::optional<TextureMapUnits> pattern_scalemode;
    std::optional<TextureMappingMode> pattern_mapping;
    std::optional<double> pattern_weight;
    std::optional<bool> pattern_useconstantlod;
    std::optional<int> pattern_constantlod_repetitions;
    std::optional<Point2dProps> pattern_constantlod_offset;
    std::optional<double> pattern_constantlod_mindistanceclamp;
    std::optional<double> pattern_constantlod_maxdistanceclamp;
    std::string TextureId;
};

// normal map properties (extends TextureMapProps).
// Ported from: itwinjs-core NormalMapProps
struct DQ_COMMON_EXPORT NormalMapProps : TextureMapProps {
    std::optional<NormalMapFlags> NormalFlags;
};

// Material asset texture maps.
// Ported from: itwinjs-core RenderMaterialAssetMapsProps
struct DQ_COMMON_EXPORT RenderMaterialAssetMapsProps {
    std::optional<TextureMapProps> Pattern;
    std::optional<NormalMapProps> normal;
    std::optional<TextureMapProps> Bump;
    std::optional<TextureMapProps> Diffuse;
    std::optional<TextureMapProps> Finish;
    std::optional<TextureMapProps> GlowColor;
    std::optional<TextureMapProps> Reflect;
    std::optional<TextureMapProps> Specular;
    std::optional<TextureMapProps> TranslucencyColor;
    std::optional<TextureMapProps> TransparentColor;
    std::optional<TextureMapProps> Displacement;
};

// Material asset properties (JSON).
// Ported from: itwinjs-core RenderMaterialAssetProps
struct DQ_COMMON_EXPORT RenderMaterialAssetProps {
    std::optional<bool> HasBaseColor;
    std::optional<RgbFactorProps> color;
    std::optional<bool> HasSpecularColor;
    std::optional<RgbFactorProps> specular_color;
    std::optional<bool> HasFinish;
    std::optional<double> finish;
    std::optional<bool> HasTransmit;
    std::optional<double> transmit;
    std::optional<bool> HasDiffuse;
    std::optional<double> diffuse;
    std::optional<bool> HasSpecular;
    std::optional<double> specular;
    std::optional<bool> HasReflect;
    std::optional<double> reflect;
    std::optional<bool> HasReflectColor;
    std::optional<RgbFactorProps> reflect_color;
    std::optional<double> pbr_normal;
    std::optional<RenderMaterialAssetMapsProps> Map;
};

END_DQ_COMMON_NAMESPACE
