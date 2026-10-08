// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Thematic display settings
// Ported from: itwinjs-core core/common/src/ThematicDisplay.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "GradientKeyColor.h"
#include "TextureProps.h"
#include "DqCommon.h"

#include <dqGeom/Range3d.h>
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
    // Ported from: itwinjs-core ThematicDisplayProps.range（Range1dProps——JSON
    // 文本形为 [low,high] 数组/空数组=null；C++ props 以 Range1d 承载，缺席=
    // undefined）。M-S 归位：原 rangeMin/rangeMax 双 double 无参考对应面。
    std::optional<dqGeom::Range1d> range;
    std::optional<dqGeom::Vector3d> axis;
    std::optional<dqGeom::Vector3d> sunDirection;
    std::optional<ThematicDisplaySensorSettingsProps> sensorSettings;
};

// Thematic gradient settings.
// Ported from: itwinjs-core ThematicGradientSettings
class DQ_COMMON_EXPORT ThematicGradientSettings {
public:
    ThematicGradientMode mode = ThematicGradientMode::Smooth;
    // 参考默认 10（ThematicDisplay.ts:100——"Cannot be less than 2"）。
    int stepCount = 10;
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

    // Ported from: itwinjs-core ThematicGradientSettings.textureTransparency
    //（@alpha，:128-148——getPass 的 MultiplySurfaceAndGradient 分路消费面）。
    TextureTransparency textureTransparency() const noexcept
    {
        auto transp = TextureTransparency::Opaque;
        if (colorScheme == ThematicGradientColorScheme::Custom) {
            bool haveOpaque = false;
            bool haveTransparent = false;
            for (auto const& key : customKeys) {
                bool const isOpq = key.color.isOpaque();
                haveOpaque = haveOpaque || isOpq;
                haveTransparent = haveTransparent || !isOpq;
            }
            if (haveTransparent)
                transp = haveOpaque ? TextureTransparency::Mixed : TextureTransparency::Translucent;
        }

        if (transp != TextureTransparency::Mixed)
            if (marginColor.isOpaque() != (transp == TextureTransparency::Opaque))
                transp = TextureTransparency::Mixed;

        return transp;
    }

    static ThematicGradientSettings fromJSON(const ThematicGradientSettingsProps* props = nullptr)
    {
        ThematicGradientSettings s;
        if (!props) return s;
        // 参考构造校验（ThematicDisplay.ts:189-226）：
        if (props->mode) s.mode = *props->mode;
        if (static_cast<uint8_t>(s.mode) > static_cast<uint8_t>(ThematicGradientMode::IsoLines))
            s.mode = ThematicGradientMode::Smooth;

        if (props->stepCount) s.stepCount = *props->stepCount;
        if (s.stepCount < 2)
            s.stepCount = 2;

        if (props->marginColor) s.marginColor = ColorDef::fromTbgr(*props->marginColor);

        if (props->colorScheme) s.colorScheme = *props->colorScheme;
        if (static_cast<uint8_t>(s.colorScheme) > static_cast<uint8_t>(ThematicGradientColorScheme::Custom))
            s.colorScheme = ThematicGradientColorScheme::BlueRed;

        if (props->customKeys) {
            s.customKeys.clear();
            for (const auto& k : *props->customKeys)
                s.customKeys.push_back(GradientKeyColor(k));
        }

        // Enforce 2 entries in custom color keys if violated（:216-221——
        // 内置白→黑两键；computeTbgrFromComponents(r,b,g) 参数序与参考 :220
        // 的 (keyValue[1], keyValue[3], keyValue[2]) 怪癖 1:1[白/黑对称不可见]）。
        if (s.colorScheme == ThematicGradientColorScheme::Custom && s.customKeys.size() < 2) {
            s.customKeys.clear();
            s.customKeys.push_back(GradientKeyColor(0.0, ColorDef::fromTbgr(
                ColorDef::computeTbgrFromComponents(255, 255, 255))));
            s.customKeys.push_back(GradientKeyColor(1.0, ColorDef::fromTbgr(
                ColorDef::computeTbgrFromComponents(0, 0, 0))));
        }

        if (props->colorMix) s.colorMix = *props->colorMix;
        if (props->transparencyMode) s.transparencyMode = *props->transparencyMode;
        return s;
    }

    // Ported from: itwinjs-core ThematicGradientSettings.toJSON（:232-257——
    // **省略默认值**：mode=Smooth/stepCount=10/marginColor=0/colorScheme=
    // BlueRed/colorMix=0/transparencyMode=SurfaceOnly/空 customKeys 均不写出）。
    ThematicGradientSettingsProps toJSON() const
    {
        ThematicGradientSettingsProps p;
        if (mode != ThematicGradientMode::Smooth)
            p.mode = mode;
        if (stepCount != 10)
            p.stepCount = stepCount;
        if (marginColor.getTbgr() != 0)
            p.marginColor = marginColor.getTbgr();
        if (colorScheme != ThematicGradientColorScheme::BlueRed)
            p.colorScheme = colorScheme;
        if (colorMix != 0.0)
            p.colorMix = colorMix;
        if (transparencyMode != ThematicGradientTransparencyMode::SurfaceOnly)
            p.transparencyMode = transparencyMode;
        if (!customKeys.empty()) {
            std::vector<GradientKeyColorProps> keys;
            for (const auto& k : customKeys) {
                GradientKeyColorProps kp;
                kp.value = k.value;
                kp.color = k.color.getTbgr();
                keys.push_back(kp);
            }
            p.customKeys = std::move(keys);
        }
        return p;
    }

    // Ported from: itwinjs-core ThematicGradientSettings.equals（:150-158——
    // 逐字段 + customKeys 逐项 keyColorEquals）。
    bool equals(const ThematicGradientSettings& rhs) const noexcept
    {
        if (mode != rhs.mode || stepCount != rhs.stepCount ||
            !marginColor.equals(rhs.marginColor) ||
            colorScheme != rhs.colorScheme || customKeys.size() != rhs.customKeys.size() ||
            colorMix != rhs.colorMix || transparencyMode != rhs.transparencyMode)
            return false;
        for (size_t i = 0; i < customKeys.size(); ++i)
            if (!customKeys[i].equals(rhs.customKeys[i]))
                return false;
        return true;
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

    // Create a copy of this, optionally modifying some properties.
    // Ported from: itwinjs-core ThematicGradientSettings.clone（:263-278）。
    ThematicGradientSettings clone(const ThematicGradientSettingsProps* changedProps = nullptr) const
    {
        if (!changedProps) {
            auto self = toJSON();
            return ThematicGradientSettings::fromJSON(&self);
        }

        ThematicGradientSettingsProps props;
        props.mode = changedProps->mode ? *changedProps->mode : mode;
        props.stepCount = changedProps->stepCount ? *changedProps->stepCount : stepCount;
        props.marginColor = changedProps->marginColor ? *changedProps->marginColor : marginColor.getTbgr();
        props.colorScheme = changedProps->colorScheme ? *changedProps->colorScheme : colorScheme;
        if (changedProps->customKeys) {
            props.customKeys = changedProps->customKeys;
        } else {
            std::vector<GradientKeyColorProps> keys;
            for (const auto& k : customKeys) {
                GradientKeyColorProps kp;
                kp.value = k.value;
                kp.color = k.color.getTbgr();
                keys.push_back(kp);
            }
            props.customKeys = std::move(keys);
        }
        props.colorMix = changedProps->colorMix ? *changedProps->colorMix : colorMix;
        props.transparencyMode = changedProps->transparencyMode ? *changedProps->transparencyMode : transparencyMode;
        return ThematicGradientSettings::fromJSON(&props);
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
        // value clamp [0,1]（ThematicDisplay.ts:308-311）。
        if (s.value < 0.0)
            s.value = 0.0;
        else if (s.value > 1.0)
            s.value = 1.0;
        return s;
    }

    ThematicDisplaySensorProps toJSON() const
    {
        ThematicDisplaySensorProps p;
        p.position = position;
        p.value = value;
        return p;
    }

    // Ported from: itwinjs-core ThematicDisplaySensor.equals（:315-317——
    // position.isAlmostEqual）。
    bool equals(const ThematicDisplaySensor& rhs) const noexcept
    {
        return value == rhs.value && position.AlmostEqual(rhs.position);
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

    // Ported from: itwinjs-core ThematicDisplaySensorSettings.equals
    //（:371-387——distanceCutoff + 逐传感器 equals）。
    bool equals(const ThematicDisplaySensorSettings& rhs) const noexcept
    {
        if (distanceCutoff != rhs.distanceCutoff || sensors.size() != rhs.sensors.size())
            return false;
        for (size_t i = 0; i < sensors.size(); ++i)
            if (!sensors[i].equals(rhs.sensors[i]))
                return false;
        return true;
    }
};

// Thematic display settings.
// Ported from: itwinjs-core ThematicDisplay
class DQ_COMMON_EXPORT ThematicDisplay {
public:
    ThematicDisplayMode displayMode = ThematicDisplayMode::Height;
    ThematicGradientSettings gradientSettings;
    // Ported from: itwinjs-core ThematicDisplay.range（Range1d——默认 null
    // range[:505 Range1d.fromJSON() 无参=null]；Height=世界米 / Slope=度）。
    dqGeom::Range1d range;
    // 默认 {0,0,0}（ThematicDisplay.ts:471/473——原 (0,0,1) 系发散，M-S 归位）。
    dqGeom::Vector3d axis = dqGeom::Vector3d::From(0, 0, 0);
    dqGeom::Vector3d sunDirection = dqGeom::Vector3d::From(0, 0, 0);
    ThematicDisplaySensorSettings sensorSettings;

    static const ThematicDisplay& defaults() noexcept
    {
        static const ThematicDisplay s_default;
        return s_default;
    }

    static ThematicDisplay fromJSON(const ThematicDisplayProps* props = nullptr)
    {
        ThematicDisplay td;
        if (props) {
            if (props->displayMode) td.displayMode = *props->displayMode;
            // displayMode 越界回 Height（ThematicDisplay.ts:505-507；enum class
            // uint8_t 无负值，<Height 臂退化不可达——保留判定对齐参考形）。
            if (static_cast<uint8_t>(td.displayMode) > static_cast<uint8_t>(ThematicDisplayMode::HillShade))
                td.displayMode = ThematicDisplayMode::Height;
            if (props->gradientSettings) td.gradientSettings = ThematicGradientSettings::fromJSON(&*props->gradientSettings);
            if (props->axis) td.axis = *props->axis;
            if (props->range) td.range = *props->range;
            if (props->sunDirection) td.sunDirection = *props->sunDirection;
            if (props->sensorSettings) td.sensorSettings = ThematicDisplaySensorSettings::fromJSON(&*props->sensorSettings);
        }
        // 构造校验（ThematicDisplay.ts:514-527）：非 Height 模式禁用
        // IsoLines/SteppedWithDelimiter（经 props round-trip 重建降级为
        // Smooth，参考 :519-523 同款）；Slope range 钳 [0,90] 度。
        if (td.displayMode != ThematicDisplayMode::Height) {
            if (td.gradientSettings.mode == ThematicGradientMode::IsoLines ||
                td.gradientSettings.mode == ThematicGradientMode::SteppedWithDelimiter) {
                auto gprops = td.gradientSettings.toJSON();
                gprops.mode = ThematicGradientMode::Smooth;
                td.gradientSettings = ThematicGradientSettings::fromJSON(&gprops);
            }
            if (td.displayMode == ThematicDisplayMode::Slope) {
                if (td.range.low < 0.0)
                    td.range.low = 0.0;
                if (td.range.high > 90.0)
                    td.range.high = 90.0;
            }
        }
        return td;
    }

    // Ported from: itwinjs-core ThematicDisplay.toJSON（:534-547——恒写
    // displayMode/gradientSettings/axis/sunDirection/range；sensorSettings
    // 仅 sensors 非空写）。
    ThematicDisplayProps toJSON() const
    {
        ThematicDisplayProps p;
        p.displayMode = displayMode;
        p.gradientSettings = gradientSettings.toJSON();
        p.axis = axis;
        p.sunDirection = sunDirection;
        p.range = range;
        if (!sensorSettings.sensors.empty())
            p.sensorSettings = sensorSettings.toJSON();
        return p;
    }

    // Ported from: itwinjs-core ThematicDisplay.equals（:479-494——逐字段；
    // range/axis/sunDirection 用 isAlmostEqual）。
    bool equals(const ThematicDisplay& rhs) const noexcept
    {
        return displayMode == rhs.displayMode &&
               gradientSettings.equals(rhs.gradientSettings) &&
               range.IsAlmostEqual(rhs.range) &&
               axis.AlmostEqual(rhs.axis) &&
               sunDirection.AlmostEqual(rhs.sunDirection) &&
               sensorSettings.equals(rhs.sensorSettings);
    }
};

END_DQ_COMMON_NAMESPACE
