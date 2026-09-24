// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Analysis style
// Ported from: itwinjs-core core/common/src/AnalysisStyle.ts
#pragma once

#include "Export.h"
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
    double rangeMin = 0.0;
    double rangeMax = 1.0;
    std::optional<ThematicGradientSettingsProps> thematicSettings;
};

struct AnalysisStyleProps {
    std::optional<AnalysisStyleDisplacementProps> displacement;
    std::optional<AnalysisStyleThematicProps> scalar;
    std::optional<std::string> normalChannelName;
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
        AnalysisStyleDisplacementProps p;
        p.channelName = channelName;
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
    double rangeMin = 0.0;
    double rangeMax = 1.0;
    ThematicGradientSettings thematicSettings;

    static AnalysisStyleThematic fromJSON(const AnalysisStyleThematicProps& props)
    {
        AnalysisStyleThematic t;
        t.channelName = props.channelName;
        t.rangeMin = props.rangeMin;
        t.rangeMax = props.rangeMax;
        if (props.thematicSettings)
            t.thematicSettings = ThematicGradientSettings::fromJSON(&*props.thematicSettings);
        return t;
    }

    AnalysisStyleThematicProps toJSON() const
    {
        AnalysisStyleThematicProps p;
        p.channelName = channelName;
        p.rangeMin = rangeMin;
        p.rangeMax = rangeMax;
        p.thematicSettings = thematicSettings.toJSON();
        return p;
    }

    bool equals(const AnalysisStyleThematic& rhs) const noexcept
    {
        return channelName == rhs.channelName && rangeMin == rhs.rangeMin &&
               rangeMax == rhs.rangeMax && thematicSettings.equals(rhs.thematicSettings);
    }
};

// Analysis style for visualization of analysis results.
// Ported from: itwinjs-core AnalysisStyle
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
        AnalysisStyle s;
        if (!props) return s;
        if (props->displacement)
            s.displacement = AnalysisStyleDisplacement::fromJSON(*props->displacement);
        if (props->scalar)
            s.scalar = AnalysisStyleThematic::fromJSON(*props->scalar);
        s.normalChannelName = props->normalChannelName;
        return s;
    }

    AnalysisStyleProps toJSON() const
    {
        AnalysisStyleProps p;
        if (displacement) p.displacement = displacement->toJSON();
        if (scalar) p.scalar = scalar->toJSON();
        p.normalChannelName = normalChannelName;
        return p;
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
