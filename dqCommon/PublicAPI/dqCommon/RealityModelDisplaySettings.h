// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Reality model display settings
// Ported from: itwinjs-core core/common/src/RealityModelDisplaySettings.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// point cloud size mode.
enum class PointCloudSizeMode : uint8_t {
    Voxel = 0,
    Pixel = 1,
};

// point cloud shape.
enum class PointCloudShape : uint8_t {
    Square = 0,
    Round = 1,
};

// point cloud eye-dome lighting mode.
enum class PointCloudEDLMode : uint8_t {
    Off = 0,
    On = 1,
    Full = 2,
};

// JSON persistence.
struct PointCloudDisplayProps {
    std::optional<PointCloudSizeMode> sizeMode;
    std::optional<double> voxelScale;
    std::optional<double> minPixelsPerVoxel;
    std::optional<double> maxPixelsPerVoxel;
    std::optional<double> pixelSize;
    std::optional<PointCloudShape> shape;
    std::optional<PointCloudEDLMode> edlMode;
    std::optional<double> edlStrength;
    std::optional<double> edlRadius;
    std::optional<double> edlFilter;
    std::optional<double> edlMixWts1;
    std::optional<double> edlMixWts2;
    std::optional<double> edlMixWts4;
};

struct RealityModelDisplayProps {
    std::optional<PointCloudDisplayProps> pointCloud;
    std::optional<double> overrideColorRatio;
};

// point cloud display settings.
// Ported from: itwinjs-core PointCloudDisplaySettings
class DQ_COMMON_EXPORT PointCloudDisplaySettings {
public:
    PointCloudShape shape = PointCloudShape::Square;
    PointCloudSizeMode sizeMode = PointCloudSizeMode::Voxel;
    double pixelSize = 1.0;
    double voxelScale = 1.0;
    double minPixelsPerVoxel = 2.0;
    double maxPixelsPerVoxel = 8.0;
    PointCloudEDLMode edlMode = PointCloudEDLMode::Off;
    double edlStrength = 0.4;
    double edlRadius = 1.4;
    std::optional<double> edlFilter;
    std::optional<double> edlMixWts1;
    std::optional<double> edlMixWts2;
    std::optional<double> edlMixWts4;

    static const PointCloudDisplaySettings& defaults() noexcept
    {
        static const PointCloudDisplaySettings s_default;
        return s_default;
    }

    static PointCloudDisplaySettings fromJSON(const PointCloudDisplayProps* props = nullptr)
    {
        PointCloudDisplaySettings s;
        if (!props) return s;
        if (props->sizeMode) s.sizeMode = *props->sizeMode;
        if (props->voxelScale) s.voxelScale = *props->voxelScale;
        if (props->minPixelsPerVoxel) s.minPixelsPerVoxel = *props->minPixelsPerVoxel;
        if (props->maxPixelsPerVoxel) s.maxPixelsPerVoxel = *props->maxPixelsPerVoxel;
        if (props->pixelSize) s.pixelSize = *props->pixelSize;
        if (props->shape) s.shape = *props->shape;
        if (props->edlMode) s.edlMode = *props->edlMode;
        if (props->edlStrength) s.edlStrength = *props->edlStrength;
        if (props->edlRadius) s.edlRadius = *props->edlRadius;
        s.edlFilter = props->edlFilter;
        s.edlMixWts1 = props->edlMixWts1;
        s.edlMixWts2 = props->edlMixWts2;
        s.edlMixWts4 = props->edlMixWts4;
        return s;
    }

    PointCloudDisplayProps toJSON() const
    {
        PointCloudDisplayProps p;
        p.sizeMode = sizeMode;
        p.voxelScale = voxelScale;
        p.minPixelsPerVoxel = minPixelsPerVoxel;
        p.maxPixelsPerVoxel = maxPixelsPerVoxel;
        p.pixelSize = pixelSize;
        p.shape = shape;
        p.edlMode = edlMode;
        p.edlStrength = edlStrength;
        p.edlRadius = edlRadius;
        p.edlFilter = edlFilter;
        p.edlMixWts1 = edlMixWts1;
        p.edlMixWts2 = edlMixWts2;
        p.edlMixWts4 = edlMixWts4;
        return p;
    }

    bool equals(const PointCloudDisplaySettings& rhs) const noexcept
    {
        return shape == rhs.shape && sizeMode == rhs.sizeMode &&
               pixelSize == rhs.pixelSize && voxelScale == rhs.voxelScale &&
               edlMode == rhs.edlMode && edlStrength == rhs.edlStrength &&
               edlRadius == rhs.edlRadius;
    }

    PointCloudDisplaySettings clone() const { return *this; }
};

// Reality model display settings.
// Ported from: itwinjs-core RealityModelDisplaySettings
class DQ_COMMON_EXPORT RealityModelDisplaySettings {
public:
    double overrideColorRatio = 0.0;
    PointCloudDisplaySettings pointCloud;

    static const RealityModelDisplaySettings& defaults() noexcept
    {
        static const RealityModelDisplaySettings s_default;
        return s_default;
    }

    static RealityModelDisplaySettings fromJSON(const RealityModelDisplayProps* props = nullptr)
    {
        RealityModelDisplaySettings s;
        if (!props) return s;
        if (props->overrideColorRatio) s.overrideColorRatio = *props->overrideColorRatio;
        if (props->pointCloud) s.pointCloud = PointCloudDisplaySettings::fromJSON(&*props->pointCloud);
        return s;
    }

    RealityModelDisplayProps toJSON() const
    {
        RealityModelDisplayProps p;
        p.overrideColorRatio = overrideColorRatio;
        p.pointCloud = pointCloud.toJSON();
        return p;
    }

    bool equals(const RealityModelDisplaySettings& rhs) const noexcept
    {
        return overrideColorRatio == rhs.overrideColorRatio && pointCloud.equals(rhs.pointCloud);
    }

    RealityModelDisplaySettings clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
