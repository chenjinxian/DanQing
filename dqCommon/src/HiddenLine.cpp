// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — HiddenLine implementation
//
// Ported from: itwinjs-core core/common/src/HiddenLine.ts
#include "dqCommon/HiddenLine.h"

#include <algorithm>

BEGIN_DQ_COMMON_NAMESPACE

static const HiddenLineStyle s_defaultVisible{};
static const HiddenLineStyle s_defaultHidden{[] {
    HiddenLineStyle s;
    s.pattern = LinePixels::HiddenLine;
    return s;
}()};
static const HiddenLineSettings s_defaultSettings{};

const HiddenLineStyle& HiddenLineStyle::defaultVisible() noexcept { return s_defaultVisible; }
const HiddenLineStyle& HiddenLineStyle::defaultHidden() noexcept { return s_defaultHidden; }
const HiddenLineSettings& HiddenLineSettings::defaults() noexcept { return s_defaultSettings; }

// Ported from: itwinjs-core HiddenLine.Style.fromJSON()
HiddenLineStyle HiddenLineStyle::fromJSON(const HiddenLineStyleProps* props, bool hidden)
{
    if (!props) {
        return hidden ? defaultHidden() : defaultVisible();
    }

    HiddenLineStyle style;

    // Color override: if ovrColor is not false and color is defined
    if ((props->color.has_value() && !props->ovrColor.has_value()) || (props->ovrColor.has_value() && props->ovrColor.value())) {
        if (props->color.has_value())
            style.color = ColorDef::fromTbgr(props->color.value());
    }

    // Pattern override
    if (props->pattern.has_value()) {
        const int pattern = static_cast<int>(props->pattern.value());
        if (static_cast<int>(LinePixels::Invalid) != pattern)
            style.pattern = props->pattern.value();
    } else if (hidden) {
        style.pattern = LinePixels::HiddenLine;
    }

    // Width override
    if (props->width.has_value()) {
        int w = props->width.value();
        if (w != 0) {
            w = std::max(1, w);
            style.width = std::min(32, w);
        }
    }

    return style;
}

// Ported from: itwinjs-core HiddenLine.Style.toJSON()
HiddenLineStyleProps HiddenLineStyle::toJSON() const
{
    HiddenLineStyleProps props;
    props.ovrColor = overridesColor();
    props.color = color.has_value() ? std::optional<ColorDefProps>(color->getTbgr()) : std::optional<ColorDefProps>(ColorDef::white.getTbgr());
    props.pattern = pattern.has_value() ? pattern : LinePixels::Invalid;
    props.width = width.has_value() ? width.value() : 0;
    return props;
}

// Ported from: itwinjs-core HiddenLine.Style.equals()
bool HiddenLineStyle::equals(const HiddenLineStyle& other) const noexcept
{
    if (this == &other)
        return true;
    if (overridesColor() != other.overridesColor() || pattern != other.pattern || width != other.width)
        return false;
    if (color.has_value() != other.color.has_value())
        return false;
    return !color.has_value() || color->equals(other.color.value());
}

// Ported from: itwinjs-core HiddenLine.Style.overrideColor()
HiddenLineStyle HiddenLineStyle::overrideColor(const std::optional<ColorDef>& newColor) const
{
    if (!color.has_value() && !newColor.has_value())
        return *this;
    if (color.has_value() && newColor.has_value() && color->equals(newColor.value()))
        return *this;

    HiddenLineStyleProps props;
    if (newColor.has_value()) {
        props.color = newColor->getTbgr();
        props.ovrColor = true;
    } else {
        props.ovrColor = false;
    }
    props.pattern = pattern;
    props.width = width;
    return fromJSON(&props);
}

HiddenLineStyle HiddenLineStyle::overridePattern(std::optional<LinePixels> newPattern) const
{
    if (newPattern == pattern)
        return *this;
    HiddenLineStyleProps props;
    if (color.has_value()) {
        props.color = color->getTbgr();
        props.ovrColor = true;
    }
    props.pattern = newPattern;
    props.width = width;
    return fromJSON(&props);
}

HiddenLineStyle HiddenLineStyle::overrideWidth(std::optional<int> newWidth) const
{
    if (newWidth == width)
        return *this;
    HiddenLineStyleProps props;
    if (color.has_value()) {
        props.color = color->getTbgr();
        props.ovrColor = true;
    }
    props.pattern = pattern;
    props.width = newWidth;
    return fromJSON(&props);
}

// Ported from: itwinjs-core HiddenLine.Settings.fromJSON()
HiddenLineSettings HiddenLineSettings::fromJSON(const HiddenLineSettingsProps& props)
{
    HiddenLineSettings settings;
    settings.visible = HiddenLineStyle::fromJSON(props.visible.has_value() ? &props.visible.value() : nullptr, false);
    settings.hidden = HiddenLineStyle::fromJSON(props.hidden.has_value() ? &props.hidden.value() : nullptr, true);
    settings.transparencyThreshold = props.transThreshold.value_or(1.0);
    return settings;
}

// Ported from: itwinjs-core HiddenLine.Settings.toJSON()
HiddenLineSettingsProps HiddenLineSettings::toJSON() const
{
    HiddenLineSettingsProps props;
    props.visible = visible.toJSON();
    props.hidden = hidden.toJSON();
    props.transThreshold = transparencyThreshold;
    return props;
}

// Ported from: itwinjs-core HiddenLine.Settings.equals()
bool HiddenLineSettings::equals(const HiddenLineSettings& other) const noexcept
{
    if (this == &other)
        return true;
    return visible.equals(other.visible) &&
           hidden.equals(other.hidden) &&
           transparencyThreshold == other.transparencyThreshold;
}

END_DQ_COMMON_NAMESPACE
