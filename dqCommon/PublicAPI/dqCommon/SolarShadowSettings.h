// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Solar shadow settings
// Ported from: itwinjs-core core/common/src/SolarShadows.ts
#pragma once

#include "ColorByName.h"
#include "Export.h"
#include "RgbColor.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct SolarShadowSettingsProps {
    std::optional<uint32_t> color;  // ColorDefProps
    std::optional<double> bias;
};

// Solar shadow rendering settings.
// Ported from: itwinjs-core SolarShadowSettings
class DQ_COMMON_EXPORT SolarShadowSettings {
public:
    RgbColor color = RgbColor(127, 127, 127);  // grey
    double bias = 0.001;

    static const SolarShadowSettings& defaults() noexcept
    {
        static const SolarShadowSettings s_default;
        return s_default;
    }

    static SolarShadowSettings fromJSON(const SolarShadowSettingsProps* props = nullptr)
    {
        SolarShadowSettings s;
        if (!props) return s;
        if (props->color) s.color = RgbColor::fromColorDef(ColorDef::fromTbgr(*props->color));
        if (props->bias) s.bias = *props->bias;
        return s;
    }

    SolarShadowSettingsProps toJSON() const
    {
        SolarShadowSettingsProps p;
        p.color = color.toColorDef().getTbgr();
        p.bias = bias;
        return p;
    }

    bool equals(const SolarShadowSettings& rhs) const noexcept
    {
        return color.equals(rhs.color) && bias == rhs.bias;
    }

    SolarShadowSettings clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
