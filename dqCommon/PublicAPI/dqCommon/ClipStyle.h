// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Clip style settings
// Ported from: itwinjs-core core/common/src/ClipStyle.ts
#pragma once

#include "Export.h"
#include "FeatureSymbology.h"
#include "HiddenLine.h"
#include "RgbColor.h"
#include "DqCommon.h"
#include "ViewFlags.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct CutStyleProps {
    std::optional<ViewFlagsProperties> viewflags;
    std::optional<HiddenLineSettingsProps> hiddenLine;
    std::optional<FeatureAppearanceProps> appearance;
};

struct ClipIntersectionStyleProps {
    std::optional<RgbColorProps> color;
    std::optional<double> width;
};

struct ClipStyleProps {
    std::optional<bool> produceCutGeometry;
    std::optional<bool> colorizeIntersection;
    std::optional<CutStyleProps> cutStyle;
    std::optional<RgbColorProps> insideColor;
    std::optional<RgbColorProps> outsideColor;
    std::optional<ClipIntersectionStyleProps> intersectionStyle;
};

// Cut geometry style.
// Ported from: itwinjs-core CutStyle
class DQ_COMMON_EXPORT CutStyle {
public:
    ViewFlagsProperties viewflags;
    std::optional<HiddenLineSettings> hiddenLine;
    std::optional<FeatureAppearance> appearance;

    static const CutStyle& defaults() noexcept
    {
        static const CutStyle s_default;
        return s_default;
    }

    static CutStyle create(const ViewFlagsProperties* vf = nullptr,
                           const HiddenLineSettings* hl = nullptr,
                           const FeatureAppearance* app = nullptr)
    {
        CutStyle s;
        if (vf) s.viewflags = *vf;
        if (hl) s.hiddenLine = *hl;
        if (app) s.appearance = *app;
        return s;
    }

    static CutStyle fromJSON(const CutStyleProps* props = nullptr)
    {
        CutStyle s;
        if (!props) return s;
        if (props->viewflags) s.viewflags = *props->viewflags;
        if (props->hiddenLine) s.hiddenLine = HiddenLineSettings::fromJSON(*props->hiddenLine);
        if (props->appearance) s.appearance = FeatureAppearance::fromJSON(&*props->appearance);
        return s;
    }

    CutStyleProps toJSON() const
    {
        CutStyleProps p;
        p.viewflags = viewflags;
        if (hiddenLine) p.hiddenLine = HiddenLineSettingsProps{hiddenLine->visible.toJSON(), hiddenLine->hidden.toJSON(), hiddenLine->transparencyThreshold};
        if (appearance) p.appearance = appearance->toJSON();
        return p;
    }

    bool matchesDefaults() const { return equals(defaults()); }

    bool equals(const CutStyle& rhs) const noexcept
    {
        // Simplified comparison
        return hiddenLine.has_value() == rhs.hiddenLine.has_value() &&
               appearance.has_value() == rhs.appearance.has_value();
    }

    CutStyle() = default;
};

// Clip intersection style.
// Ported from: itwinjs-core ClipIntersectionStyle
class DQ_COMMON_EXPORT ClipIntersectionStyle {
public:
    RgbColor color = RgbColor(0, 0, 0);
    double width = 2.0;

    static const ClipIntersectionStyle& defaults() noexcept
    {
        static const ClipIntersectionStyle s_default;
        return s_default;
    }

    static ClipIntersectionStyle create(const RgbColor* c = nullptr, double w = 2.0)
    {
        ClipIntersectionStyle s;
        if (c) s.color = *c;
        s.width = w;
        return s;
    }

    static ClipIntersectionStyle fromJSON(const ClipIntersectionStyleProps* props = nullptr)
    {
        ClipIntersectionStyle s;
        if (!props) return s;
        if (props->color) s.color = RgbColor::fromJSON(&*props->color);
        if (props->width) s.width = *props->width;
        return s;
    }

    ClipIntersectionStyleProps toJSON() const
    {
        ClipIntersectionStyleProps p;
        p.color = color.toJSON();
        p.width = width;
        return p;
    }

    bool matchesDefaults() const { return equals(defaults()); }

    bool equals(const ClipIntersectionStyle& rhs) const noexcept
    {
        return color.equals(rhs.color) && width == rhs.width;
    }

    ClipIntersectionStyle() = default;
};

// Clip style.
// Ported from: itwinjs-core ClipStyle
class DQ_COMMON_EXPORT ClipStyle {
public:
    bool produceCutGeometry = false;
    bool colorizeIntersection = false;
    CutStyle cutStyle;
    std::optional<RgbColor> insideColor;
    std::optional<RgbColor> outsideColor;
    std::optional<ClipIntersectionStyle> intersectionStyle;

    static const ClipStyle& defaults() noexcept
    {
        static const ClipStyle s_default;
        return s_default;
    }

    static ClipStyle fromJSON(const ClipStyleProps* props = nullptr)
    {
        ClipStyle s;
        if (!props) return s;
        if (props->produceCutGeometry) s.produceCutGeometry = *props->produceCutGeometry;
        if (props->colorizeIntersection) s.colorizeIntersection = *props->colorizeIntersection;
        if (props->cutStyle) s.cutStyle = CutStyle::fromJSON(&*props->cutStyle);
        if (props->insideColor) s.insideColor = RgbColor::fromJSON(&*props->insideColor);
        if (props->outsideColor) s.outsideColor = RgbColor::fromJSON(&*props->outsideColor);
        if (props->intersectionStyle) s.intersectionStyle = ClipIntersectionStyle::fromJSON(&*props->intersectionStyle);
        return s;
    }

    ClipStyleProps toJSON() const
    {
        ClipStyleProps p;
        p.produceCutGeometry = produceCutGeometry;
        p.colorizeIntersection = colorizeIntersection;
        p.cutStyle = cutStyle.toJSON();
        if (insideColor) p.insideColor = insideColor->toJSON();
        if (outsideColor) p.outsideColor = outsideColor->toJSON();
        if (intersectionStyle) p.intersectionStyle = intersectionStyle->toJSON();
        return p;
    }

    bool matchesDefaults() const { return equals(defaults()); }

    bool equals(const ClipStyle& rhs) const noexcept
    {
        return produceCutGeometry == rhs.produceCutGeometry &&
               colorizeIntersection == rhs.colorizeIntersection;
    }

    ClipStyle() = default;
};

END_DQ_COMMON_NAMESPACE
