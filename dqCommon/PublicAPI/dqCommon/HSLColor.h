// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — HSL (Hue, Saturation, Lightness) color
//
// Ported from: itwinjs-core core/common/src/HSLColor.ts
// Immutable color defined by Hue [0..1], Saturation [0..1], Lightness [0..1].
#pragma once

#include "DqCommon.h"

#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declaration — ColorDef defined in ColorDef.h
class ColorDef;

// An immutable color defined by Hue, Saturation, and Lightness.
// Ported from: itwinjs-core core/common/src/HSLColor.ts
class DQ_COMMON_EXPORT HSLColor {
public:
    double h = 0.0;  // Hue [0..1]
    double s = 0.0;  // Saturation [0..1]
    double l = 0.0;  // Lightness [0..1]

    constexpr HSLColor() noexcept = default;
    constexpr HSLColor(double hue, double saturation, double lightness) noexcept
        : h(hue), s(saturation), l(lightness)
    {
    }

    // clone with optional overrides — ported from: itwinjs-core HSLColor.clone()
    HSLColor clone(std::optional<double> hue = std::nullopt,
                   std::optional<double> saturation = std::nullopt,
                   std::optional<double> lightness = std::nullopt) const noexcept
    {
        return HSLColor(hue.value_or(h), saturation.value_or(s), lightness.value_or(l));
    }

    // Convert to ColorDef — ported from: itwinjs-core HSLColor.toColorDef()
    ColorDef toColorDef(int transparency = 0) const;

    // Create from ColorDef — ported from: itwinjs-core HSLColor.fromColorDef()
    static HSLColor fromColorDef(const ColorDef& colorDef);
};

END_DQ_COMMON_NAMESPACE
