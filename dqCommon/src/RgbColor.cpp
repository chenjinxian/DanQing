// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — RgbColor implementation
//
// Ported from: itwinjs-core core/common/src/RgbColor.ts
#include "dqCommon/RgbColor.h"

BEGIN_DQ_COMMON_NAMESPACE

// Ported from: itwinjs-core RgbColor.fromColorDef()
RgbColor RgbColor::fromColorDef(const ColorDef& colorDef) noexcept
{
    const auto c = colorDef.getColors();
    return RgbColor(c.r, c.g, c.b);
}

// Ported from: itwinjs-core RgbColor.toColorDef()
ColorDef RgbColor::toColorDef(int transparency) const
{
    return ColorDef::from(r, g, b, transparency);
}

// Ported from: itwinjs-core RgbColor.fromJSON()
RgbColor RgbColor::fromJSON(const RgbColorProps* json) noexcept
{
    if (json == nullptr)
        return RgbColor(0xFF, 0xFF, 0xFF);
    return RgbColor(json->r, json->g, json->b);
}

// Ported from: itwinjs-core RgbColor.compareTo()
int RgbColor::compareTo(const RgbColor& other) const noexcept
{
    // Ported from: itwinjs-core compareNumbers pattern
    if (r != other.r)
        return r < other.r ? -1 : 1;
    if (g != other.g)
        return g < other.g ? -1 : 1;
    if (b != other.b)
        return b < other.b ? -1 : 1;
    return 0;
}

// Ported from: itwinjs-core RgbColor.toHexString()
std::string RgbColor::toHexString() const
{
    return toColorDef().toHexString();
}

END_DQ_COMMON_NAMESPACE
