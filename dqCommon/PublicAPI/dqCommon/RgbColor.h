// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — RGB color (immutable)
//
// Ported from: itwinjs-core core/common/src/RgbColor.ts
// Immutable color with red, green, blue components each in [0, 255].
#pragma once

#include "ColorDef.h"
#include "Export.h"

#include <cstdint>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// JSON representation of an RgbColor.
// Ported from: itwinjs-core core/common/src/RgbColor.ts
struct RgbColorProps {
    int r = 255;
    int g = 255;
    int b = 255;
};

// An immutable representation of a color with red, green, and blue components.
// Ported from: itwinjs-core core/common/src/RgbColor.ts
class DQ_COMMON_EXPORT RgbColor {
public:
    int r;
    int g;
    int b;

    // Construct from red, green, blue (clamped to [0, 255]).
    // Ported from: itwinjs-core RgbColor constructor
    RgbColor(int red, int green, int blue) noexcept
        : r((std::max)(0, (std::min)(255, red)))
        , g((std::max)(0, (std::min)(255, green)))
        , b((std::max)(0, (std::min)(255, blue)))
    {
    }

    // Construct from ColorDef (ignores transparency).
    // Ported from: itwinjs-core RgbColor.fromColorDef()
    static RgbColor fromColorDef(const ColorDef& colorDef) noexcept;

    // Convert to ColorDef.
    // Ported from: itwinjs-core RgbColor.toColorDef()
    ColorDef toColorDef(int transparency = 0) const;

    // Convert to JSON.
    // Ported from: itwinjs-core RgbColor.toJSON()
    RgbColorProps toJSON() const noexcept { return {r, g, b}; }

    // Create from JSON.
    // Ported from: itwinjs-core RgbColor.fromJSON()
    static RgbColor fromJSON(const RgbColorProps* json) noexcept;

    // Equality.
    // Ported from: itwinjs-core RgbColor.equals()
    bool equals(const RgbColor& other) const noexcept
    {
        return r == other.r && g == other.g && b == other.b;
    }

    // Compare (ordered).
    // Ported from: itwinjs-core RgbColor.compareTo()
    int compareTo(const RgbColor& other) const noexcept;

    // To "#rrggbb" string.
    // Ported from: itwinjs-core RgbColor.toHexString()
    std::string toHexString() const;
};

END_DQ_COMMON_NAMESPACE
