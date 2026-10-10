// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Analysis style
// Ported from: itwinjs-core core/common/src/AnalysisStyle.ts
#pragma once

#include "Export.h"
#include "Gradient.h"
#include "ThematicDisplay.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct AnalysisStyleDisplacementProps {
    std::string channelName;
    std::optional<double> scale;
};

struct AnalysisStyleThematicProps {
    std::string channelName;
    // Ported from: itwinjs-core AnalysisStyleThematicProps.range（Range1dProps
    // ——JSON 文本形 [low,high]；M-S 随 ThematicDisplay.range 同族归位，
    // 原 rangeMin/rangeMax 双 double 无参考对应面）。
    dqGeom::Range1d range;
    std::optional<ThematicGradientSettingsProps> thematicSettings;
};

struct AnalysisStyleProps {
    std::optional<AnalysisStyleDisplacementProps> displacement;
    std::optional<AnalysisStyleThematicProps> scalar;
    std::optional<std::string> normalChannelName;
    // Legacy JSON keys, flat-carried (ref LegacyAnalysisStyleProps
    // AnalysisStyle.ts:155-162) converted by tryConvertLegacyProps (:164-192).
    // Filled by the JSON parse layer on encountering legacy keys; mutually
    // exclusive with displacement/scalar (same as ref).
    struct {
        std::optional<std::string> displacementChannelName;
        std::optional<std::string> scalarChannelName;
        std::optional<std::string> normalChannelName;
        std::optional<double> displacementScale;
        std::optional<dqGeom::Range1d> scalarRange;
        std::optional<ThematicGradientSettingsProps> scalarThematicSettings;
    } legacy;
};

// Analysis displacement style.
// Ported from: itwinjs-core AnalysisStyleDisplacement
class DQ_COMMON_EXPORT AnalysisStyleDisplacement {
public:
    std::string channelName;
    double scale = 1.0;

    static AnalysisStyleDisplacement fromJSON(const AnalysisStyleDisplacementProps& props)
    {
        AnalysisStyleDisplacement d;
        d.channelName = props.channelName;
        if (props.scale) d.scale = *props.scale;
        return d;
    }

    AnalysisStyleDisplacementProps toJSON() const
    {
        // Ported from: itwinjs-core AnalysisStyleDisplacement.toJSON (:50-55)--
        // scale default 1, omitted when ==1.
        AnalysisStyleDisplacementProps p;
        p.channelName = channelName;
        if (scale != 1.0)
            p.scale = scale;
        return p;
    }

    bool equals(const AnalysisStyleDisplacement& rhs) const noexcept
    {
        return channelName == rhs.channelName && scale == rhs.scale;
    }
};

// Analysis thematic style.
// Ported from: itwinjs-core AnalysisStyleThematic
class DQ_COMMON_EXPORT AnalysisStyleThematic {
public:
    std::string channelName;
    // Ported from: itwinjs-core AnalysisStyleThematic.range（AnalysisStyle.ts:92
    // ——Range1d；fromJSON 经 Range1d.fromJSON(props.range) :99）。
    dqGeom::Range1d range;
    ThematicGradientSettings thematicSettings;

private:
    // Ported from: itwinjs-core AnalysisStyleThematic._gradient (:95--lazy).
    mutable std::optional<GradientSymb> m_gradient;

public:
    static AnalysisStyleThematic fromJSON(const AnalysisStyleThematicProps& props)
    {
        AnalysisStyleThematic t;
        t.channelName = props.channelName;
        t.range = props.range;
        if (props.thematicSettings)
            t.thematicSettings = ThematicGradientSettings::fromJSON(&*props.thematicSettings);
        return t;
    }

    AnalysisStyleThematicProps toJSON() const
    {
        // Ported from: itwinjs-core AnalysisStyleThematic.toJSON (:109-119)--
        // thematicSettings equal to defaults is omitted.
        AnalysisStyleThematicProps p;
        p.channelName = channelName;
        p.range = range;
        if (!thematicSettings.equals(ThematicGradientSettings::defaults()))
            p.thematicSettings = thematicSettings.toJSON();
        return p;
    }

    // The gradient computed from thematicSettings (lazy cache -- ref _gradient :95).
    // Ported from: itwinjs-core AnalysisStyleThematic.gradient (:122-127).
    GradientSymb const& gradient() const
    {
        if (!m_gradient.has_value())
            m_gradient = GradientSymb::createThematic(thematicSettings);
        return *m_gradient;
    }

    // Ported from: itwinjs-core AnalysisStyleThematic.equals（AnalysisStyle.
    // ts:131——range.isAlmostEqual）。
    bool equals(const AnalysisStyleThematic& rhs) const noexcept
    {
        return channelName == rhs.channelName && range.IsAlmostEqual(rhs.range) &&
               thematicSettings.equals(rhs.thematicSettings);
    }
};

// Legacy JSON representation (@internal exported strictly for tests).
// Ported from: itwinjs-core LegacyAnalysisStyleProps (:155-162)
struct LegacyAnalysisStyleProps {
    std::optional<std::string> displacementChannelName;
    std::optional<std::string> scalarChannelName;
    std::optional<std::string> normalChannelName;
    std::optional<double> displacementScale;
    std::optional<dqGeom::Range1d> scalarRange;
    std::optional<ThematicGradientSettingsProps> scalarThematicSettings;
};

// Convert legacy props to current props; pass-through when no legacy keys.
// Ported from: itwinjs-core tryConvertLegacyProps (:164-192)
inline AnalysisStyleProps tryConvertLegacyProps(AnalysisStyleProps const& input)
{
    if (input.displacement || input.scalar)
        return input;

    if (!input.legacy.displacementChannelName && !input.legacy.scalarChannelName)
        return input;

    AnalysisStyleProps output;
    output.normalChannelName = input.normalChannelName;
    if (input.legacy.displacementChannelName) {
        AnalysisStyleDisplacementProps disp;
        disp.channelName = *input.legacy.displacementChannelName;
        disp.scale = input.legacy.displacementScale;
        output.displacement = disp;
    }
    if (input.legacy.scalarChannelName && input.legacy.scalarRange) {
        AnalysisStyleThematicProps scalar;
        scalar.channelName = *input.legacy.scalarChannelName;
        scalar.range = *input.legacy.scalarRange;
        scalar.thematicSettings = input.legacy.scalarThematicSettings;
        output.scalar = scalar;
    }
    return output;
}

// Analysis style for visualization of analysis results.
// Ported from: itwinjs-core AnalysisStyle (:201-274)
class DQ_COMMON_EXPORT AnalysisStyle {
public:
    std::optional<AnalysisStyleDisplacement> displacement;
    std::optional<AnalysisStyleThematic> scalar;
    std::optional<std::string> normalChannelName;

    static const AnalysisStyle& defaults() noexcept
    {
        static const AnalysisStyle s_default;
        return s_default;
    }

    static AnalysisStyle fromJSON(const AnalysisStyleProps* props = nullptr)
    {
        // Ported from: itwinjs-core AnalysisStyle.fromJSON (:210-219)--no props,
        // or no displacement/scalar/normalChannelName (after legacy conversion)
        // yields defaults.
        AnalysisStyle s;
        if (!props)
            return s;

        auto const converted = tryConvertLegacyProps(*props);
        if (!converted.displacement && !converted.scalar &&
            !converted.normalChannelName.has_value())
            return s;

        if (converted.displacement)
            s.displacement = AnalysisStyleDisplacement::fromJSON(*converted.displacement);
        if (converted.scalar)
            s.scalar = AnalysisStyleThematic::fromJSON(*converted.scalar);
        s.normalChannelName = converted.normalChannelName;
        return s;
    }

    AnalysisStyleProps toJSON() const
    {
        // Ported from: itwinjs-core AnalysisStyle.toJSON (:231-246)--defaults
        // identity returns empty props.
        AnalysisStyleProps p;
        if (this == &defaults())
            return p;
        if (displacement) p.displacement = displacement->toJSON();
        if (scalar) p.scalar = scalar->toJSON();
        p.normalChannelName = normalChannelName;
        return p;
    }

    // Produce a copy identical except for properties explicitly specified by
    // changedProps (spread-merge of toJSON + changedProps).
    // Ported from: itwinjs-core AnalysisStyle.clone (:249-253).
    AnalysisStyle clone(AnalysisStyleProps const& changedProps) const
    {
        auto merged = toJSON();
        if (changedProps.displacement) merged.displacement = changedProps.displacement;
        if (changedProps.scalar) merged.scalar = changedProps.scalar;
        if (changedProps.normalChannelName)
            merged.normalChannelName = changedProps.normalChannelName;
        return fromJSON(&merged);
    }

    bool equals(const AnalysisStyle& rhs) const noexcept
    {
        const bool dispEq = displacement.has_value() == rhs.displacement.has_value() &&
                            (!displacement || displacement->equals(*rhs.displacement));
        const bool scalarEq = scalar.has_value() == rhs.scalar.has_value() &&
                              (!scalar || scalar->equals(*rhs.scalar));
        return dispEq && scalarEq && normalChannelName == rhs.normalChannelName;
    }
};

END_DQ_COMMON_NAMESPACE
