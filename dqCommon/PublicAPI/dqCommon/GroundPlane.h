// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Ground plane settings
// Ported from: itwinjs-core core/common/src/GroundPlane.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct GroundPlaneProps {
    bool display = false;
    std::optional<double> elevation;
    std::optional<uint32_t> aboveColor;  // ColorDefProps
    std::optional<uint32_t> belowColor;  // ColorDefProps
};

// Ground plane display settings.
// Ported from: itwinjs-core GroundPlane
class DQ_COMMON_EXPORT GroundPlane {
public:
    double elevation = -0.01;
    ColorDef aboveColor = ColorDef::from(30, 60, 30);   // darkGreen
    ColorDef belowColor = ColorDef::from(60, 30, 0);    // darkBrown

    static const GroundPlane& defaults() noexcept
    {
        static const GroundPlane s_default;
        return s_default;
    }

    static GroundPlane fromJSON(const GroundPlaneProps* props = nullptr)
    {
        GroundPlane gp;
        if (!props) return gp;
        if (props->elevation) gp.elevation = *props->elevation;
        if (props->aboveColor) gp.aboveColor = ColorDef::fromTbgr(*props->aboveColor);
        if (props->belowColor) gp.belowColor = ColorDef::fromTbgr(*props->belowColor);
        return gp;
    }

    GroundPlaneProps toJSON(bool display = false) const
    {
        GroundPlaneProps props;
        props.display = display;
        props.elevation = elevation;
        props.aboveColor = aboveColor.getTbgr();
        props.belowColor = belowColor.getTbgr();
        return props;
    }

    GroundPlane clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
