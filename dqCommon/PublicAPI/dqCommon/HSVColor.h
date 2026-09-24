// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — HSV (Hue, Saturation, Value) color
//
// Ported from: itwinjs-core core/common/src/HSVColor.ts
// Immutable color defined by Hue [0..360], Saturation [0..100], Value [0..100].
#pragma once

#include "DqCommon.h"

#include <algorithm>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declaration
class ColorDef;

// HSV constants — ported from: itwinjs-core core/common/src/HSVColor.ts
enum class HSVConstants : int {
    VISIBILITY_GOAL = 40,
    HSV_SATURATION_WEIGHT = 4,
    HSV_VALUE_WEIGHT = 2,
};

// An immutable color defined by Hue, Saturation, and Value.
// Ported from: itwinjs-core core/common/src/HSVColor.ts
class DQ_COMMON_EXPORT HSVColor {
public:
    int h = 0;    // Hue [0..360]
    int s = 0;    // Saturation [0..100]
    int v = 0;    // Value [0..100]

    constexpr HSVColor() noexcept = default;
    constexpr HSVColor(int hue, int saturation, int value) noexcept
        : h(hue), s(saturation), v(value)
    {
    }

    // clone with optional overrides — ported from: itwinjs-core HSVColor.clone()
    HSVColor clone(std::optional<int> hue = std::nullopt,
                   std::optional<int> saturation = std::nullopt,
                   std::optional<int> value = std::nullopt) const noexcept
    {
        return HSVColor(hue.value_or(h), saturation.value_or(s), value.value_or(v));
    }

    // Convert to ColorDef — ported from: itwinjs-core HSVColor.toColorDef()
    ColorDef toColorDef(int transparency = 0) const;

    // Create from ColorDef — ported from: itwinjs-core HSVColor.fromColorDef()
    static HSVColor fromColorDef(const ColorDef& colorDef);

    // Adjust brightness/saturation — ported from: itwinjs-core HSVColor.adjusted()
    HSVColor adjusted(bool darkenColor, int delta) const noexcept
    {
        int weightedDelta;
        if (darkenColor) {
            weightedDelta = delta * static_cast<int>(HSVConstants::HSV_VALUE_WEIGHT);
            if (v >= weightedDelta)
                return HSVColor(h, s, v - weightedDelta);

            weightedDelta -= v;
            const int newS = (std::min)(s + weightedDelta, 100);
            return HSVColor(h, newS, 0);
        }

        weightedDelta = delta * static_cast<int>(HSVConstants::HSV_SATURATION_WEIGHT);
        if (s >= weightedDelta)
            return HSVColor(h, s - weightedDelta, v);

        weightedDelta -= s;
        const int newV = (std::min)(v + weightedDelta, 100);
        return HSVColor(h, 0, newV);
    }
};

END_DQ_COMMON_NAMESPACE
