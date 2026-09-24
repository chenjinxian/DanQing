// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Gradient key color (fraction/color pair)
// Ported from: itwinjs-core core/common/src/Gradient.ts (Gradient.KeyColor, Gradient.KeyColorProps)
//
// Extracted from Gradient.h to break the Gradient.h <-> ThematicDisplay.h include cycle:
// ThematicDisplay.h needs only GradientKeyColor/Props (for ThematicGradientSettings.customKeys);
// Gradient.h needs ThematicGradientSettings (for GradientSymb.thematicSettings). Keeping
// GradientKeyColor in its own header lets ThematicDisplay.h depend on it without pulling in all
// of Gradient.h, so Gradient.h may freely include ThematicDisplay.h (TD-8).
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "DqCommon.h"

BEGIN_DQ_COMMON_NAMESPACE

// Gradient fraction/color pair (JSON).
// Ported from: itwinjs-core Gradient.KeyColorProps
struct GradientKeyColorProps {
    double value = 0.0;
    ColorDefProps color = 0;
};

// Gradient fraction/color pair.
// Ported from: itwinjs-core Gradient.KeyColor
class DQ_COMMON_EXPORT GradientKeyColor {
public:
    double value;
    ColorDef color;

    GradientKeyColor() noexcept : value(0.0), color(ColorDef::black) {}
    GradientKeyColor(double v, const ColorDef& c) noexcept : value(v), color(c) {}
    GradientKeyColor(const GradientKeyColorProps& props)
        : value(props.value), color(ColorDef::fromTbgr(props.color))
    {
    }

    bool equals(const GradientKeyColor& other) const noexcept
    {
        return value == other.value && color.equals(other.color);
    }
};

END_DQ_COMMON_NAMESPACE
