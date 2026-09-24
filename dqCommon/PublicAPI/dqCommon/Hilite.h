// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Hilite settings
//
// Ported from: itwinjs-core core/common/src/Hilite.ts
// Settings for how hilited (selected) elements are displayed.
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "DqCommon.h"

#include <algorithm>
#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Width of the outline applied to hilited geometry.
// Ported from: itwinjs-core Hilite.Silhouette
enum class HiliteSilhouette : uint8_t {
    None = 0,
    Thin = 1,
    Thick = 2,
};

// Describes how the hilite effect is applied to elements.
// Ported from: itwinjs-core Hilite.Settings
class DQ_COMMON_EXPORT HiliteSettings {
public:
    ColorDef color;
    double visibleRatio;
    double hiddenRatio;
    HiliteSilhouette silhouette;

    // Ported from: itwinjs-core Hilite.Settings constructor
    HiliteSettings(const ColorDef& color = ColorDef::from(0x23, 0xbb, 0xfc),
                   double visibleRatio = 0.25,
                   double hiddenRatio = 0.0,
                   HiliteSilhouette silhouette = HiliteSilhouette::Thin)
        : color(color)
        , visibleRatio(clamp(visibleRatio))
        , hiddenRatio(clamp(hiddenRatio))
        , silhouette(silhouette)
    {
    }

    // Compare for equivalence.
    // Ported from: itwinjs-core Hilite.equalSettings
    bool equals(const HiliteSettings& other) const noexcept
    {
        return color.equals(other.color) &&
               visibleRatio == other.visibleRatio &&
               hiddenRatio == other.hiddenRatio &&
               silhouette == other.silhouette;
    }

private:
    static double clamp(double value) noexcept { return std::max(0.0, std::min(1.0, value)); }
};

END_DQ_COMMON_NAMESPACE
