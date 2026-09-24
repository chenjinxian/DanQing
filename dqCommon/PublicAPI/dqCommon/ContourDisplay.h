// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Contour display settings
// Ported from: itwinjs-core core/common/src/ContourDisplay.ts
#pragma once

#include "Export.h"
#include "LinePixels.h"
#include "RgbColor.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct ContourStyleProps {
    std::optional<RgbColorProps> color;
    std::optional<double> pixelWidth;
    std::optional<LinePixels> pattern;
};

struct ContourProps {
    std::optional<ContourStyleProps> majorStyle;
    std::optional<ContourStyleProps> minorStyle;
    std::optional<double> minorInterval;
    std::optional<int> majorIntervalCount;
    std::optional<bool> showGeometry;
};

struct ContourGroupProps {
    std::optional<ContourProps> contourDef;
    std::optional<std::string> name;
};

struct ContourDisplayProps {
    std::optional<std::vector<ContourGroupProps>> groups;
    std::optional<bool> displayContours;
};

// Contour line style.
// Ported from: itwinjs-core ContourStyle
class DQ_COMMON_EXPORT ContourStyle {
public:
    RgbColor color = RgbColor(0, 0, 0);
    double pixelWidth = 1.0;
    LinePixels pattern = LinePixels::Solid;

    static ContourStyle fromJSON(const ContourStyleProps* props = nullptr)
    {
        ContourStyle s;
        if (!props) return s;
        if (props->color) s.color = RgbColor::fromJSON(&*props->color);
        if (props->pixelWidth) s.pixelWidth = *props->pixelWidth;
        if (props->pattern) s.pattern = *props->pattern;
        return s;
    }

    ContourStyleProps toJSON() const
    {
        ContourStyleProps p;
        p.color = color.toJSON();
        p.pixelWidth = pixelWidth;
        p.pattern = pattern;
        return p;
    }

    bool equals(const ContourStyle& rhs) const noexcept
    {
        return color.equals(rhs.color) && pixelWidth == rhs.pixelWidth && pattern == rhs.pattern;
    }

    ContourStyle clone() const { return *this; }
};

// Contour definition (major + minor styles).
// Ported from: itwinjs-core Contour
class DQ_COMMON_EXPORT Contour {
public:
    ContourStyle majorStyle;  // default: pixelWidth=2
    ContourStyle minorStyle;
    double minorInterval = 1.0;
    int majorIntervalCount = 5;
    bool showGeometry = true;

    Contour()
    {
        majorStyle.pixelWidth = 2.0;
    }

    static const Contour& defaults() noexcept
    {
        static const Contour s_default;
        return s_default;
    }

    static Contour fromJSON(const ContourProps* props = nullptr)
    {
        Contour c;
        if (!props) return c;
        if (props->majorStyle) c.majorStyle = ContourStyle::fromJSON(&*props->majorStyle);
        if (props->minorStyle) c.minorStyle = ContourStyle::fromJSON(&*props->minorStyle);
        if (props->minorInterval) c.minorInterval = *props->minorInterval;
        if (props->majorIntervalCount) c.majorIntervalCount = *props->majorIntervalCount;
        if (props->showGeometry) c.showGeometry = *props->showGeometry;
        return c;
    }

    ContourProps toJSON() const
    {
        ContourProps p;
        p.majorStyle = majorStyle.toJSON();
        p.minorStyle = minorStyle.toJSON();
        p.minorInterval = minorInterval;
        p.majorIntervalCount = majorIntervalCount;
        p.showGeometry = showGeometry;
        return p;
    }

    bool equals(const Contour& rhs) const noexcept
    {
        return majorStyle.equals(rhs.majorStyle) && minorStyle.equals(rhs.minorStyle) &&
               minorInterval == rhs.minorInterval && majorIntervalCount == rhs.majorIntervalCount &&
               showGeometry == rhs.showGeometry;
    }

    Contour clone() const { return *this; }
};

// Contour group (contour definition + name).
// Ported from: itwinjs-core ContourGroup
class DQ_COMMON_EXPORT ContourGroup {
public:
    Contour contourDef;
    std::string name;

    static ContourGroup fromJSON(const ContourGroupProps* props = nullptr)
    {
        ContourGroup g;
        if (!props) return g;
        if (props->contourDef) g.contourDef = Contour::fromJSON(&*props->contourDef);
        if (props->name) g.name = *props->name;
        return g;
    }

    ContourGroupProps toJSON() const
    {
        ContourGroupProps p;
        p.contourDef = contourDef.toJSON();
        p.name = name;
        return p;
    }

    bool equals(const ContourGroup& rhs) const noexcept
    {
        return contourDef.equals(rhs.contourDef) && name == rhs.name;
    }

    ContourGroup clone() const { return *this; }
};

// Contour display settings.
// Ported from: itwinjs-core ContourDisplay
class DQ_COMMON_EXPORT ContourDisplay {
public:
    static constexpr int MaxContourGroups = 5;

    std::vector<ContourGroup> groups;
    bool displayContours = false;

    static const ContourDisplay& defaults() noexcept
    {
        static const ContourDisplay s_default;
        return s_default;
    }

    static ContourDisplay fromJSON(const ContourDisplayProps* props = nullptr)
    {
        ContourDisplay d;
        if (!props) return d;
        if (props->groups) {
            for (const auto& gp : *props->groups)
                d.groups.push_back(ContourGroup::fromJSON(&gp));
        }
        if (props->displayContours) d.displayContours = *props->displayContours;
        return d;
    }

    ContourDisplayProps toJSON() const
    {
        ContourDisplayProps p;
        std::vector<ContourGroupProps> gv;
        for (const auto& g : groups)
            gv.push_back(g.toJSON());
        p.groups = std::move(gv);
        p.displayContours = displayContours;
        return p;
    }

    bool equals(const ContourDisplay& rhs) const noexcept
    {
        if (displayContours != rhs.displayContours || groups.size() != rhs.groups.size())
            return false;
        for (size_t i = 0; i < groups.size(); ++i) {
            if (!groups[i].equals(rhs.groups[i]))
                return false;
        }
        return true;
    }

    ContourDisplay clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
