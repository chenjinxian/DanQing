// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — FeatureAppearance implementation
//
// Ported from: itwinjs-core core/common/src/FeatureSymbology.ts
#include "dqCommon/FeatureSymbology.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

struct FeatureAppearanceDefaultsAccess {
    static FeatureAppearance Create() noexcept { return FeatureAppearance{}; }
};

static const FeatureAppearance s_defaults = FeatureAppearanceDefaultsAccess::Create();

const FeatureAppearance& FeatureAppearance::defaults() noexcept
{
    return s_defaults;
}

// Helper: clamp and fix rounding errors in transparency
static std::optional<double> TransparencyFromJSON(std::optional<double> transp)
{
    if (!transp.has_value())
        return std::nullopt;

    double t = std::max(0.0, std::min(1.0, transp.value()));
    constexpr double smallDelta = 0.0001;
    if (1.0 - t < smallDelta)
        t = 1.0;
    else if (t < smallDelta)
        t = 0.0;
    return t;
}

// Ported from: itwinjs-core FeatureAppearance.fromJSON()
FeatureAppearance FeatureAppearance::fromJSON(const FeatureAppearanceProps* props)
{
    if (!props)
        return defaults();

    // Check if all props match defaults
    if (!props->rgb.has_value() && !props->lineRgb.has_value() && !props->lineRgbIsFalse &&
        !props->weight.has_value() && !props->linePixels.has_value() &&
        !props->transparency.has_value() && !props->lineTransparency.has_value() && !props->lineTransparencyIsFalse &&
        !props->ignoresMaterial && !props->nonLocatable && !props->emphasized)
    {
        return defaults();
    }

    FeatureAppearance app;
    if (props->rgb.has_value())
        app.m_rgb = RgbColor::fromJSON(&props->rgb.value());

    if (props->lineRgbIsFalse) {
        app.m_lineRgbIsFalse = true;
    } else if (props->lineRgb.has_value()) {
        app.m_lineRgb = RgbColor::fromJSON(&props->lineRgb.value());
    }

    app.m_transparency = TransparencyFromJSON(props->transparency);

    if (props->lineTransparencyIsFalse) {
        app.m_lineTransparencyIsFalse = true;
    } else {
        app.m_lineTransparency = TransparencyFromJSON(props->lineTransparency);
    }

    app.m_weight = props->weight;
    app.m_linePixels = props->linePixels;
    app.m_ignoresMaterial = props->ignoresMaterial;
    app.m_nonLocatable = props->nonLocatable;
    app.m_emphasized = props->emphasized;

    // clamp weight to [1, 32]
    if (app.m_weight.has_value())
        app.m_weight = std::max(1.0, std::min(32.0, app.m_weight.value()));

    // Set viewDependentTransparency only if transparency is actually overridden
    if (props->viewDependentTransparency &&
        (app.m_transparency.has_value() || app.getEffectiveLineTransparency().has_value()))
    {
        app.m_viewDependentTransparency = true;
    }

    return app;
}

// Ported from: itwinjs-core FeatureAppearance.fromRgb()
FeatureAppearance FeatureAppearance::fromRgb(const ColorDef& color)
{
    FeatureAppearanceProps props;
    const auto c = color.getColors();
    props.rgb = RgbColorProps{c.r, c.g, c.b};
    return fromJSON(&props);
}

// Ported from: itwinjs-core FeatureAppearance.fromRgba()
FeatureAppearance FeatureAppearance::fromRgba(const ColorDef& color, bool viewDependentTransparency)
{
    FeatureAppearanceProps props;
    const auto c = color.getColors();
    props.rgb = RgbColorProps{c.r, c.g, c.b};
    props.transparency = c.t / 255.0;
    props.viewDependentTransparency = viewDependentTransparency;
    return fromJSON(&props);
}

// Ported from: itwinjs-core FeatureAppearance.fromTransparency()
FeatureAppearance FeatureAppearance::fromTransparency(double transparency, bool viewDependent)
{
    FeatureAppearanceProps props;
    props.transparency = transparency;
    props.viewDependentTransparency = viewDependent;
    return fromJSON(&props);
}

// Ported from: itwinjs-core FeatureAppearance.overridesSymbology
bool FeatureAppearance::overridesSymbology() const noexcept
{
    return overridesRgb() || overridesTransparency() || overridesWeight() || overridesLinePixels() || m_ignoresMaterial ||
           m_emphasized || overridesNonLocatable() || getEffectiveLineRgb().has_value() || getEffectiveLineTransparency().has_value();
}

// Ported from: itwinjs-core FeatureAppearance.isFullyTransparent
bool FeatureAppearance::isFullyTransparent() const noexcept
{
    const double surf = m_transparency.value_or(0.0);
    const double line = getEffectiveLineTransparency().value_or(0.0);
    return surf >= 1.0 && line >= 1.0;
}

// Ported from: itwinjs-core FeatureAppearance.getLineRgb()
std::optional<RgbColor> FeatureAppearance::getEffectiveLineRgb() const noexcept
{
    if (m_lineRgbIsFalse)
        return std::nullopt;
    if (m_lineRgb.has_value())
        return m_lineRgb;
    return m_rgb;
}

// Ported from: itwinjs-core FeatureAppearance.getLineTransparency()
std::optional<double> FeatureAppearance::getEffectiveLineTransparency() const noexcept
{
    if (m_lineTransparencyIsFalse)
        return std::nullopt;
    if (m_lineTransparency.has_value())
        return m_lineTransparency;
    return m_transparency;
}

// Ported from: itwinjs-core equalRgb helper
static bool EqualRgb(const std::optional<RgbColor>& a, const std::optional<RgbColor>& b)
{
    if (a.has_value() != b.has_value())
        return false;
    if (!a.has_value())
        return true;
    return a->equals(b.value());
}

// Ported from: itwinjs-core equalTransparency helper
static bool EqualTransparency(std::optional<double> a, std::optional<double> b)
{
    if (a.has_value() != b.has_value())
        return false;
    if (!a.has_value())
        return true;
    return static_cast<int>(std::floor(a.value() * 255)) == static_cast<int>(std::floor(b.value() * 255));
}

// Ported from: itwinjs-core equalLineRgb helper
static bool EqualLineRgb(const std::optional<RgbColor>& a, bool aFalse, const std::optional<RgbColor>& b, bool bFalse)
{
    if (aFalse != bFalse)
        return false;
    return EqualRgb(a, b);
}

// Ported from: itwinjs-core equalLineTransparency helper
static bool EqualLineTransparency(std::optional<double> a, bool aFalse, std::optional<double> b, bool bFalse)
{
    if (aFalse != bFalse)
        return false;
    return EqualTransparency(a, b);
}

// Ported from: itwinjs-core FeatureAppearance.equals()
bool FeatureAppearance::equals(const FeatureAppearance& other) const noexcept
{
    if (this == &other)
        return true;
    return EqualRgb(m_rgb, other.m_rgb) &&
           m_weight == other.m_weight &&
           EqualTransparency(m_transparency, other.m_transparency) &&
           m_linePixels == other.m_linePixels &&
           m_ignoresMaterial == other.m_ignoresMaterial &&
           m_nonLocatable == other.m_nonLocatable &&
           m_emphasized == other.m_emphasized &&
           m_viewDependentTransparency == other.m_viewDependentTransparency &&
           EqualLineTransparency(m_lineTransparency, m_lineTransparencyIsFalse, other.m_lineTransparency, other.m_lineTransparencyIsFalse) &&
           EqualLineRgb(m_lineRgb, m_lineRgbIsFalse, other.m_lineRgb, other.m_lineRgbIsFalse);
}

// Ported from: itwinjs-core FeatureAppearance.extendAppearance()
FeatureAppearance FeatureAppearance::extendAppearance(const FeatureAppearance& base) const
{
    if (!overridesSymbology())
        return base;

    FeatureAppearanceProps props = base.toJSON();
    if (!props.rgb.has_value() && m_rgb.has_value())
        props.rgb = RgbColorProps{m_rgb->r, m_rgb->g, m_rgb->b};
    if (!props.transparency.has_value() && m_transparency.has_value())
        props.transparency = m_transparency;
    if (!props.linePixels.has_value() && m_linePixels.has_value())
        props.linePixels = m_linePixels;
    if (!props.weight.has_value() && m_weight.has_value())
        props.weight = m_weight;
    if (!props.ignoresMaterial && m_ignoresMaterial)
        props.ignoresMaterial = true;
    if (!props.nonLocatable && m_nonLocatable)
        props.nonLocatable = true;
    if (!props.emphasized && m_emphasized)
        props.emphasized = true;
    if (!props.lineRgb.has_value() && !props.lineRgbIsFalse && (m_lineRgb.has_value() || m_lineRgbIsFalse)) {
        if (m_lineRgbIsFalse)
            props.lineRgbIsFalse = true;
        else
            props.lineRgb = RgbColorProps{m_lineRgb->r, m_lineRgb->g, m_lineRgb->b};
    }
    if (!props.lineTransparency.has_value() && !props.lineTransparencyIsFalse && (m_lineTransparency.has_value() || m_lineTransparencyIsFalse)) {
        if (m_lineTransparencyIsFalse)
            props.lineTransparencyIsFalse = true;
        else
            props.lineTransparency = m_lineTransparency;
    }

    if (m_viewDependentTransparency && (props.transparency.has_value() || props.lineTransparency.has_value()))
        props.viewDependentTransparency = true;

    return fromJSON(&props);
}

// Ported from: itwinjs-core FeatureAppearance.toJSON()
FeatureAppearanceProps FeatureAppearance::toJSON() const
{
    FeatureAppearanceProps props;
    if (m_rgb.has_value())
        props.rgb = RgbColorProps{m_rgb->r, m_rgb->g, m_rgb->b};
    if (m_weight.has_value())
        props.weight = m_weight;
    if (m_transparency.has_value()) {
        props.transparency = m_transparency;
        if (m_viewDependentTransparency)
            props.viewDependentTransparency = true;
    }
    if (m_linePixels.has_value())
        props.linePixels = m_linePixels;
    if (m_ignoresMaterial)
        props.ignoresMaterial = true;
    if (m_nonLocatable)
        props.nonLocatable = true;
    if (m_emphasized)
        props.emphasized = true;
    if (m_lineTransparency.has_value())
        props.lineTransparency = m_lineTransparency;
    if (m_lineTransparencyIsFalse)
        props.lineTransparencyIsFalse = true;
    if (m_lineRgb.has_value()) {
        props.lineRgb = RgbColorProps{m_lineRgb->r, m_lineRgb->g, m_lineRgb->b};
        if (m_viewDependentTransparency)
            props.viewDependentTransparency = true;
    }
    if (m_lineRgbIsFalse)
        props.lineRgbIsFalse = true;
    return props;
}

END_DQ_COMMON_NAMESPACE
