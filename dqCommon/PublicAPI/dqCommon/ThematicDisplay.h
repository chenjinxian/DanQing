// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Thematic display settings
// Ported from: itwinjs-core core/common/src/ThematicDisplay.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "GradientKeyColor.h"
#include "DqCommon.h"

#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Thematic gradient mode.
// Ported from: itwinjs-core ThematicGradientMode
enum class ThematicGradientMode : uint8_t {
    Smooth = 0,
    Stepped = 1,
    SteppedWithDelimiter = 2,
    IsoLines = 3,
};

// Thematic gradient transparency mode.
// Ported from: itwinjs-core ThematicGradientTransparencyMode
enum class ThematicGradientTransparencyMode : uint8_t {
    SurfaceOnly = 0,
    MultiplySurfaceAndGradient = 1,
};

// Thematic gradient color scheme.
// Ported from: itwinjs-core ThematicGradientColorScheme
enum class ThematicGradientColorScheme : uint8_t {
    BlueRed = 0,
    RedBlue = 1,
    Monochrome = 2,
    Topographic = 3,
    SeaMountain = 4,
    Custom = 5,
};

// Thematic display mode.
// Ported from: itwinjs-core ThematicDisplayMode
enum class ThematicDisplayMode : uint8_t {
    Height = 0,
    InverseDistanceWeightedSensors = 1,
    Slope = 2,
    HillShade = 3,
};

// JSON persistence.
struct ThematicGradientSettingsProps {
    std::optional<ThematicGradientMode> mode;
    std::optional<int> stepCount;
    std::optional<uint32_t> marginColor;
    std::optional<ThematicGradientColorScheme> colorScheme;
    std::optional<std::vector<GradientKeyColorProps>> customKeys;
    std::optional<double> colorMix;
    std::optional<ThematicGradientTransparencyMode> transparencyMode;
};

struct ThematicDisplaySensorProps {
    std::optional<dqGeom::Point3d> position;
    std::optional<double> value;
};

struct ThematicDisplaySensorSettingsProps {
    std::optional<std::vector<ThematicDisplaySensorProps>> sensors;
    std::optional<double> distanceCutoff;
};

struct ThematicDisplayProps {
    std::optional<ThematicDisplayMode> displayMode;
    std::optional<ThematicGradientSettingsProps> gradientSettings;
    std::optional<double> rangeMin;
    std::optional<double> rangeMax;
    std::optional<dqGeom::Vector3d> axis;
    std::optional<dqGeom::Vector3d> sunDirection;
    std::optional<ThematicDisplaySensorSettingsProps> sensorSettings;
};

// Thematic gradient settings.
// Ported from: itwinjs-core ThematicGradientSettings
class DQ_COMMON_EXPORT ThematicGradientSettings {
public:
    ThematicGradientMode mode = ThematicGradientMode::Smooth;
    int stepCount = 0;
    ColorDef marginColor = ColorDef::black;
    ThematicGradientColorScheme colorScheme = ThematicGradientColorScheme::BlueRed;
    std::vector<GradientKeyColor> customKeys;
    double colorMix = 0.0;
    ThematicGradientTransparencyMode transparencyMode = ThematicGradientTransparencyMode::SurfaceOnly;

    static constexpr double margin() noexcept { return 0.001; }
    static constexpr double contentRange() noexcept { return 1.0 - 2.0 * margin(); }
    static constexpr double contentMax() noexcept { return 1.0 - margin(); }

    static const ThematicGradientSettings& defaults() noexcept
    {
        static const ThematicGradientSettings s_default;
        return s_default;
    }

    static ThematicGradientSettings fromJSON(const ThematicGradientSettingsProps* props = nullptr)
    {
        ThematicGradientSettings s;
        if (!props) return s;
        if (props->mode) s.mode = *props->mode;
        if (props->stepCount) s.stepCount = *props->stepCount;
        if (props->marginColor) s.marginColor = ColorDef::fromTbgr(*props->marginColor);
        if (props->colorScheme) s.colorScheme = *props->colorScheme;
        if (props->customKeys) {
            s.customKeys.clear();
            for (const auto& k : *props->customKeys)
                s.customKeys.push_back(GradientKeyColor(k));
        }
        if (props->colorMix) s.colorMix = *props->colorMix;
        if (props->transparencyMode) s.transparencyMode = *props->transparencyMode;
        return s;
    }

    ThematicGradientSettingsProps toJSON() const
    {
        ThematicGradientSettingsProps p;
        p.mode = mode;
        p.stepCount = stepCount;
        p.marginColor = marginColor.getTbgr();
        p.colorScheme = colorScheme;
        std::vector<GradientKeyColorProps> keys;
        for (const auto& k : customKeys) {
            GradientKeyColorProps kp;
            kp.value = k.value;
            kp.color = k.color.getTbgr();
            keys.push_back(kp);
        }
        p.customKeys = std::move(keys);
        p.colorMix = colorMix;
        p.transparencyMode = transparencyMode;
        return p;
    }

    bool equals(const ThematicGradientSettings& rhs) const noexcept
    {
        return mode == rhs.mode && stepCount == rhs.stepCount &&
               colorScheme == rhs.colorScheme && colorMix == rhs.colorMix &&
               transparencyMode == rhs.transparencyMode;
    }

    // Compares two sets of thematic gradient settings for ordering.
    // Returns 0 if equivalent, negative if lhs < rhs, positive if lhs > rhs.
    // Ported from: itwinjs-core ThematicGradientSettings.compare (lines 165-187)
    static int compare(const ThematicGradientSettings& lhs, const ThematicGradientSettings& rhs) noexcept
    {
        if (lhs.mode != rhs.mode)
            return static_cast<int>(lhs.mode) < static_cast<int>(rhs.mode) ? -1 : 1;
        if (lhs.stepCount != rhs.stepCount)
            return lhs.stepCount < rhs.stepCount ? -1 : 1;
        if (lhs.marginColor.getTbgr() != rhs.marginColor.getTbgr())
            return lhs.marginColor.getTbgr() < rhs.marginColor.getTbgr() ? -1 : 1;
        if (lhs.colorScheme != rhs.colorScheme)
            return static_cast<int>(lhs.colorScheme) < static_cast<int>(rhs.colorScheme) ? -1 : 1;
        if (lhs.colorMix != rhs.colorMix)
            return lhs.colorMix < rhs.colorMix ? -1 : 1;
        if (lhs.customKeys.size() != rhs.customKeys.size())
            return lhs.customKeys.size() < rhs.customKeys.size() ? -1 : 1;
        if (lhs.transparencyMode != rhs.transparencyMode)
            return static_cast<int>(lhs.transparencyMode) < static_cast<int>(rhs.transparencyMode) ? -1 : 1;
        for (size_t i = 0; i < lhs.customKeys.size(); ++i) {
            const auto a = lhs.customKeys[i].color.getTbgr();
            const auto b = rhs.customKeys[i].color.getTbgr();
            if (a != b)
                return a < b ? -1 : 1;
        }
        return 0;
    }
};

// Thematic display sensor.
// Ported from: itwinjs-core ThematicDisplaySensor
class DQ_COMMON_EXPORT ThematicDisplaySensor {
public:
    dqGeom::Point3d position;
    double value = 0.0;

    static ThematicDisplaySensor fromJSON(const ThematicDisplaySensorProps* props = nullptr)
    {
        ThematicDisplaySensor s;
        if (!props) return s;
        if (props->position) s.position = *props->position;
        if (props->value) s.value = *props->value;
        return s;
    }

    ThematicDisplaySensorProps toJSON() const
    {
        ThematicDisplaySensorProps p;
        p.position = position;
        p.value = value;
        return p;
    }

    bool equals(const ThematicDisplaySensor& rhs) const noexcept
    {
        return position.IsEqual(rhs.position) && value == rhs.value;
    }
};

// Thematic display sensor settings.
// Ported from: itwinjs-core ThematicDisplaySensorSettings
class DQ_COMMON_EXPORT ThematicDisplaySensorSettings {
public:
    std::vector<ThematicDisplaySensor> sensors;
    double distanceCutoff = 0.0;

    static const ThematicDisplaySensorSettings& defaults() noexcept
    {
        static const ThematicDisplaySensorSettings s_default;
        return s_default;
    }

    static ThematicDisplaySensorSettings fromJSON(const ThematicDisplaySensorSettingsProps* props = nullptr)
    {
        ThematicDisplaySensorSettings s;
        if (!props) return s;
        if (props->sensors) {
            for (const auto& sp : *props->sensors)
                s.sensors.push_back(ThematicDisplaySensor::fromJSON(&sp));
        }
        if (props->distanceCutoff) s.distanceCutoff = *props->distanceCutoff;
        return s;
    }

    ThematicDisplaySensorSettingsProps toJSON() const
    {
        ThematicDisplaySensorSettingsProps p;
        std::vector<ThematicDisplaySensorProps> sv;
        for (const auto& s : sensors)
            sv.push_back(s.toJSON());
        p.sensors = std::move(sv);
        p.distanceCutoff = distanceCutoff;
        return p;
    }

    bool equals(const ThematicDisplaySensorSettings& rhs) const noexcept
    {
        return distanceCutoff == rhs.distanceCutoff && sensors.size() == rhs.sensors.size();
    }
};

// Thematic display settings.
// Ported from: itwinjs-core ThematicDisplay
class DQ_COMMON_EXPORT ThematicDisplay {
public:
    ThematicDisplayMode displayMode = ThematicDisplayMode::Height;
    ThematicGradientSettings gradientSettings;
    double rangeMin = 0.0;
    double rangeMax = 1.0;
    dqGeom::Vector3d axis = dqGeom::Vector3d::From(0, 0, 1);
    dqGeom::Vector3d sunDirection = dqGeom::Vector3d::From(0, 0, 1);
    ThematicDisplaySensorSettings sensorSettings;

    static const ThematicDisplay& defaults() noexcept
    {
        static const ThematicDisplay s_default;
        return s_default;
    }

    static ThematicDisplay fromJSON(const ThematicDisplayProps* props = nullptr)
    {
        ThematicDisplay td;
        if (!props) return td;
        if (props->displayMode) td.displayMode = *props->displayMode;
        if (props->gradientSettings) td.gradientSettings = ThematicGradientSettings::fromJSON(&*props->gradientSettings);
        if (props->rangeMin) td.rangeMin = *props->rangeMin;
        if (props->rangeMax) td.rangeMax = *props->rangeMax;
        if (props->axis) td.axis = *props->axis;
        if (props->sunDirection) td.sunDirection = *props->sunDirection;
        if (props->sensorSettings) td.sensorSettings = ThematicDisplaySensorSettings::fromJSON(&*props->sensorSettings);
        return td;
    }

    ThematicDisplayProps toJSON() const
    {
        ThematicDisplayProps p;
        p.displayMode = displayMode;
        p.gradientSettings = gradientSettings.toJSON();
        p.rangeMin = rangeMin;
        p.rangeMax = rangeMax;
        p.axis = axis;
        p.sunDirection = sunDirection;
        p.sensorSettings = sensorSettings.toJSON();
        return p;
    }

    bool equals(const ThematicDisplay& rhs) const noexcept
    {
        return displayMode == rhs.displayMode && rangeMin == rhs.rangeMin &&
               rangeMax == rhs.rangeMax && gradientSettings.equals(rhs.gradientSettings);
    }
};

END_DQ_COMMON_NAMESPACE
