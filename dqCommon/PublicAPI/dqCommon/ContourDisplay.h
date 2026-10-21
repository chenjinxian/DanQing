// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Contour display settings
// Ported from: itwinjs-core core/common/src/ContourDisplay.ts
#pragma once

#include "Export.h"
#include "LinePixels.h"
#include "RgbColor.h"
#include "DqCommon.h"

#include <dqBase/CompressedId64Set.h>

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
    // 压缩子类集（CompressedId64Set 串——ContourGroupProps.subCategories，
    // ContourDisplay.ts:260）。
    std::optional<std::string> subCategories;
    std::optional<std::string> name;
};

struct ContourDisplayProps {
    std::optional<std::vector<ContourGroupProps>> groups;
    std::optional<bool> displayContours;
};

// Contour line style.
// Ported from: itwinjs-core ContourStyle（ContourDisplay.ts:36-124——
//   默认 color=黑/pixelWidth=1/pattern=Solid；toJSON 省略默认段 :95-108）。
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

    // Ported from: itwinjs-core ContourStyle.toJSON（:95-108——逐段省略默认：
    //   color≠黑才写 / pixelWidth≠1 才写 / pattern≠Solid(0) 才写）。
    ContourStyleProps toJSON() const
    {
        ContourStyleProps p;
        if (!color.equals(RgbColor(0, 0, 0)))
            p.color = color.toJSON();
        if (1.0 != pixelWidth)
            p.pixelWidth = pixelWidth;
        if (LinePixels::Solid != pattern)
            p.pattern = pattern;
        return p;
    }

    bool equals(const ContourStyle& rhs) const noexcept
    {
        return color.equals(rhs.color) && pixelWidth == rhs.pixelWidth && pattern == rhs.pattern;
    }

    // Ported from: itwinjs-core ContourStyle.compare（:66-76——color.compareTo
    //   → compareNumbers(pixelWidth) → compareNumbers(pattern)；
    //   compareNumbers=lhs-rhs[Compare.ts:45-47]）。
    static int compare(ContourStyle const& lhs, ContourStyle const& rhs) noexcept
    {
        if (int const diff = lhs.color.compareTo(rhs.color); diff != 0)
            return diff;
        if (lhs.pixelWidth != rhs.pixelWidth)
            return lhs.pixelWidth < rhs.pixelWidth ? -1 : 1;
        if (lhs.pattern != rhs.pattern)
            return static_cast<uint32_t>(lhs.pattern) < static_cast<uint32_t>(rhs.pattern) ? -1 : 1;
        return 0;
    }
    int compare(ContourStyle const& rhs) const noexcept { return compare(*this, rhs); }

    // Ported from: itwinjs-core ContourStyle.clone（:118-123——
    //   create({...this, ...changedProps})：缺席段保持现值；C++ optional
    //   无 "显式 undefined→回默认" 区分[参考该臂语义登记——本节缺席段=
    //   保持现值]）。
    ContourStyle clone() const { return *this; }
    ContourStyle clone(ContourStyleProps const& changedProps) const
    {
        ContourStyle c = *this;
        if (changedProps.color) c.color = RgbColor::fromJSON(&*changedProps.color);
        if (changedProps.pixelWidth) c.pixelWidth = *changedProps.pixelWidth;
        if (changedProps.pattern) c.pattern = *changedProps.pattern;
        return c;
    }
};

// Contour definition (major + minor styles).
// Ported from: itwinjs-core Contour（ContourDisplay.ts:158-251——默认
//   majorStyle={pixelWidth:2}/minorStyle=默认/minorInterval=1/
//   majorIntervalCount=5/showGeometry=true；toJSON 省略默认段 :216-235）。
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

    // Ported from: itwinjs-core Contour.toJSON（:216-235——逐段省略默认：
    //   majorStyle≠{pixelWidth:2} / minorStyle≠默认 / minorInterval≠1 /
    //   majorIntervalCount≠5 / showGeometry≠true 才写）。
    ContourProps toJSON() const
    {
        ContourProps p;
        // 参考 :219——`!this.majorStyle.equals(ContourStyle.fromJSON({pixelWidth: 2}))`。
        ContourStyle defaultMajor;
        defaultMajor.pixelWidth = 2.0;
        if (!majorStyle.equals(defaultMajor))
            p.majorStyle = majorStyle.toJSON();
        if (!minorStyle.equals(ContourStyle()))
            p.minorStyle = minorStyle.toJSON();
        if (1.0 != minorInterval)
            p.minorInterval = minorInterval;
        if (5 != majorIntervalCount)
            p.majorIntervalCount = majorIntervalCount;
        if (true != showGeometry)
            p.showGeometry = showGeometry;
        return p;
    }

    bool equals(const Contour& rhs) const noexcept
    {
        return majorStyle.equals(rhs.majorStyle) && minorStyle.equals(rhs.minorStyle) &&
               minorInterval == rhs.minorInterval && majorIntervalCount == rhs.majorIntervalCount &&
               showGeometry == rhs.showGeometry;
    }

    // Ported from: itwinjs-core Contour.compare（:187-193——styles →
    //   compareNumbers(minorInterval/majorIntervalCount) →
    //   compareBooleans(showGeometry)[false<true——Compare.ts:50-52]）。
    static int compare(Contour const& lhs, Contour const& rhs) noexcept
    {
        if (int const diff = ContourStyle::compare(lhs.majorStyle, rhs.majorStyle); diff != 0)
            return diff;
        if (int const diff = ContourStyle::compare(lhs.minorStyle, rhs.minorStyle); diff != 0)
            return diff;
        if (lhs.minorInterval != rhs.minorInterval)
            return lhs.minorInterval < rhs.minorInterval ? -1 : 1;
        if (lhs.majorIntervalCount != rhs.majorIntervalCount)
            return lhs.majorIntervalCount < rhs.majorIntervalCount ? -1 : 1;
        if (lhs.showGeometry != rhs.showGeometry)
            return !lhs.showGeometry ? -1 : 1;
        return 0;
    }
    int compare(Contour const& rhs) const noexcept { return compare(*this, rhs); }

    // Ported from: itwinjs-core Contour.clone（:245-250——同 ContourStyle.clone 注）。
    Contour clone() const { return *this; }
    Contour clone(ContourProps const& changedProps) const
    {
        Contour c = *this;
        if (changedProps.majorStyle) c.majorStyle = ContourStyle::fromJSON(&*changedProps.majorStyle);
        if (changedProps.minorStyle) c.minorStyle = ContourStyle::fromJSON(&*changedProps.minorStyle);
        if (changedProps.minorInterval) c.minorInterval = *changedProps.minorInterval;
        if (changedProps.majorIntervalCount) c.majorIntervalCount = *changedProps.majorIntervalCount;
        if (changedProps.showGeometry) c.showGeometry = *changedProps.showGeometry;
        return c;
    }
};

// Contour group (contour definition + subcategory set + name).
// Ported from: itwinjs-core ContourGroup（ContourDisplay.ts:278-361——
//   空 subCategories=默认组[isDefaultGroup :298-300]；toJSON :334-345——
//   contourDef≠默认才写 / name 非空才写 / **subCategories 恒写**[:343]）。
class DQ_COMMON_EXPORT ContourGroup {
public:
    Contour contourDef;
    // 压缩子类集（CompressedId64Set 串——参考 _subCategories[:279]；
    //   OrderedId64Iterable 消费面经 CompressedId64Set::DecompressArray 展开）。
    std::string subCategories;
    std::string name;

    static ContourGroup fromJSON(const ContourGroupProps* props = nullptr)
    {
        ContourGroup g;
        if (!props) return g;
        if (props->contourDef) g.contourDef = Contour::fromJSON(&*props->contourDef);
        // Ported from: ContourDisplay.ts:319——
        //   `props?.subCategories ? CompressedId64Set.sortAndCompress(props.subCategories) : ""`
        // （fromJSON :330 先把压缩串转 iterable；sortAndCompress 归一化回压缩串）。
        if (props->subCategories && !props->subCategories->empty())
            g.subCategories = dqBase::CompressedId64Set::SortAndCompress(
                dqBase::CompressedId64Set::DecompressSet(*props->subCategories));
        if (props->name) g.name = *props->name;
        return g;
    }

    ContourGroupProps toJSON() const
    {
        ContourGroupProps p;
        if (!contourDef.equals(Contour::defaults()))
            p.contourDef = contourDef.toJSON();
        if (!name.empty())
            p.name = name;
        // 参考 :343——subCategories 恒写（含空串）。
        p.subCategories = subCategories;
        return p;
    }

    // Ported from: itwinjs-core ContourGroup.isDefaultGroup（:298-300——
    //   OrderedId64Iterable.isEmptySet(_subCategories)）。
    bool isDefaultGroup() const noexcept { return subCategories.empty(); }

    // Ported from: itwinjs-core ContourGroup.subCategoriesEqual（:303-305——
    //   压缩串 ===；Contours.ts:50 的 LUT 失效比对消费面）。
    bool subCategoriesEqual(ContourGroup const& rhs) const noexcept
    {
        return subCategories == rhs.subCategories;
    }

    // Ported from: itwinjs-core ContourGroup.compare（:308-310——
    //   compareStrings(name) || compareStrings(subCategories) || contourDef.compare；
    //   compareStrings=字典序[Compare.ts:55-57]）。
    int compare(ContourGroup const& rhs) const noexcept
    {
        if (name != rhs.name)
            return name < rhs.name ? -1 : 1;
        if (subCategories != rhs.subCategories)
            return subCategories < rhs.subCategories ? -1 : 1;
        return contourDef.compare(rhs.contourDef);
    }

    bool equals(const ContourGroup& rhs) const noexcept
    {
        return contourDef.equals(rhs.contourDef) && name == rhs.name &&
               subCategories == rhs.subCategories;
    }

    // Ported from: itwinjs-core ContourGroup.clone（:355-360——同
    //   ContourStyle.clone 注；subCategories 段=压缩串原样[参考 {...this} 散布
    //   经 getter 读回后 sortAndCompress 归一化——与既有归一化串恒等]）。
    ContourGroup clone() const { return *this; }
    ContourGroup clone(ContourGroupProps const& changedProps) const
    {
        ContourGroup g = *this;
        if (changedProps.contourDef) g.contourDef = Contour::fromJSON(&*changedProps.contourDef);
        if (changedProps.name) g.name = *changedProps.name;
        if (changedProps.subCategories) {
            g.subCategories = changedProps.subCategories->empty()
                ? std::string()
                : dqBase::CompressedId64Set::SortAndCompress(
                      dqBase::CompressedId64Set::DecompressSet(*changedProps.subCategories));
        }
        return g;
    }
};

// Contour display settings.
// Ported from: itwinjs-core ContourDisplay（ContourDisplay.ts:384-471——
//   maxContourGroups=5 :397；equals 的组比对截断于 maxContourGroups :410）。
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

    // Ported from: itwinjs-core ContourDisplay.toJSON（:440-450——groups 恒写
    //   [空组也写空数组] + displayContours 恒写）。
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

    // Ported from: itwinjs-core ContourDisplay.equals（:405-416——
    //   displayContours 不等→false；groups 长度不等→false；组比对**截断于
    //   maxContourGroups**[:410——越界组不参与显示，故不参与等价判定]）。
    bool equals(const ContourDisplay& rhs) const noexcept
    {
        if (displayContours != rhs.displayContours || groups.size() != rhs.groups.size())
            return false;
        for (size_t i = 0; i < groups.size() && static_cast<int>(i) < MaxContourGroups; ++i) {
            if (!groups[i].equals(rhs.groups[i]))
                return false;
        }
        return true;
    }

    // Ported from: itwinjs-core ContourDisplay.compare（:400-402——
    //   compareBooleans(displayContours) || compareArrays(groups, group.compare)）。
    int compare(ContourDisplay const& rhs) const noexcept
    {
        if (displayContours != rhs.displayContours)
            return !displayContours ? -1 : 1;
        size_t const n = std::min(groups.size(), rhs.groups.size());
        for (size_t i = 0; i < n; ++i) {
            if (int const diff = groups[i].compare(rhs.groups[i]); diff != 0)
                return diff;
        }
        if (groups.size() != rhs.groups.size())
            return groups.size() < rhs.groups.size() ? -1 : 1;
        return 0;
    }

    // Ported from: itwinjs-core ContourDisplay.withDisplayContours（:458-460——
    //   同旗标返回等价拷贝；异则 create({...this, displayContours})）。
    ContourDisplay withDisplayContours(bool display) const
    {
        if (display == displayContours)
            return *this;
        ContourDisplay d = *this;
        d.displayContours = display;
        return d;
    }

    // Ported from: itwinjs-core ContourDisplay.clone（:465-470——同
    //   ContourStyle.clone 注；groups 段为值对象数组[参考 NonFunctionPropertiesOf
    //   形]，DanQing 以 ContourGroupProps 数组承载[toJSON/fromJSON 往返无损]）。
    ContourDisplay clone() const { return *this; }
    ContourDisplay clone(ContourDisplayProps const& changedProps) const
    {
        ContourDisplay d = *this;
        if (changedProps.groups) {
            d.groups.clear();
            for (const auto& gp : *changedProps.groups)
                d.groups.push_back(ContourGroup::fromJSON(&gp));
        }
        if (changedProps.displayContours) d.displayContours = *changedProps.displayContours;
        return d;
    }
};

END_DQ_COMMON_NAMESPACE
