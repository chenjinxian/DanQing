// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Sky box settings
// Ported from: itwinjs-core core/common/src/SkyBox.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// Sky box image type.
// Ported from: itwinjs-core SkyBoxImageType
enum class SkyBoxImageType : uint8_t {
    None = 0,
    Spherical = 1,
    Cube = 3,
};

// JSON persistence.
struct SkyBoxProps {
    bool display = false;
    std::optional<bool> twoColor;
    std::optional<uint32_t> skyColor;
    std::optional<uint32_t> groundColor;
    std::optional<uint32_t> zenithColor;
    std::optional<uint32_t> nadirColor;
    std::optional<double> skyExponent;
    std::optional<double> groundExponent;
};

// Sky gradient parameters.
// Ported from: itwinjs-core SkyGradient
class DQ_COMMON_EXPORT SkyGradient {
public:
    bool twoColor = false;
    ColorDef skyColor = ColorDef::from(142, 205, 255);
    ColorDef groundColor = ColorDef::from(143, 205, 125);
    ColorDef zenithColor = ColorDef::from(54, 117, 255);
    ColorDef nadirColor = ColorDef::from(40, 125, 0);
    double skyExponent = 4.0;
    double groundExponent = 4.0;

    static const SkyGradient& defaults() noexcept
    {
        static const SkyGradient s_default;
        return s_default;
    }

    static SkyGradient fromJSON(const SkyBoxProps* props = nullptr)
    {
        SkyGradient g;
        if (!props) return g;
        if (props->twoColor) g.twoColor = *props->twoColor;
        if (props->skyColor) g.skyColor = ColorDef::fromTbgr(*props->skyColor);
        if (props->groundColor) g.groundColor = ColorDef::fromTbgr(*props->groundColor);
        if (props->zenithColor) g.zenithColor = ColorDef::fromTbgr(*props->zenithColor);
        if (props->nadirColor) g.nadirColor = ColorDef::fromTbgr(*props->nadirColor);
        if (props->skyExponent) g.skyExponent = *props->skyExponent;
        if (props->groundExponent) g.groundExponent = *props->groundExponent;
        return g;
    }

    SkyBoxProps toJSON() const
    {
        SkyBoxProps p;
        p.twoColor = twoColor;
        p.skyColor = skyColor.getTbgr();
        p.groundColor = groundColor.getTbgr();
        p.zenithColor = zenithColor.getTbgr();
        p.nadirColor = nadirColor.getTbgr();
        p.skyExponent = skyExponent;
        p.groundExponent = groundExponent;
        return p;
    }

    bool equals(const SkyGradient& rhs) const noexcept
    {
        return twoColor == rhs.twoColor && skyColor.equals(rhs.skyColor) &&
               groundColor.equals(rhs.groundColor) && zenithColor.equals(rhs.zenithColor) &&
               nadirColor.equals(rhs.nadirColor) && skyExponent == rhs.skyExponent &&
               groundExponent == rhs.groundExponent;
    }

    SkyGradient clone() const { return *this; }
};

// Sky box (gradient-based).
// Ported from: itwinjs-core SkyBox
class DQ_COMMON_EXPORT SkyBox {
public:
    SkyGradient gradient;

    SkyBox() = default;
    explicit SkyBox(const SkyGradient& g) : gradient(g) {}

    static const SkyBox& defaults() noexcept
    {
        static const SkyBox s_default;
        return s_default;
    }

    static SkyBox fromJSON(const SkyBoxProps* props = nullptr)
    {
        return SkyBox(SkyGradient::fromJSON(props));
    }

    SkyBoxProps toJSON(bool display = false) const
    {
        auto p = gradient.toJSON();
        p.display = display;
        return p;
    }
};

END_DQ_COMMON_NAMESPACE
