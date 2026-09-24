// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Feature symbology (appearance overrides)
//
// Ported from: itwinjs-core core/common/src/FeatureSymbology.ts
// Defines FeatureAppearance for overriding feature visual properties.
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "LinePixels.h"
#include "RgbColor.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declaration for friend access
class FeatureOverrides;

// JSON representation of FeatureAppearance.
// Ported from: itwinjs-core FeatureAppearanceProps
struct DQ_COMMON_EXPORT FeatureAppearanceProps {
    std::optional<RgbColorProps> rgb;
    std::optional<RgbColorProps> lineRgb;  // RgbColorProps or false (use nullopt for false)
    bool lineRgbIsFalse = false;           // true means lineRgb=false (don't override line color)
    std::optional<double> weight;
    std::optional<double> transparency;
    std::optional<double> lineTransparency;
    bool lineTransparencyIsFalse = false;  // true means lineTransparency=false
    bool viewDependentTransparency = false;
    std::optional<LinePixels> linePixels;
    bool ignoresMaterial = false;
    bool nonLocatable = false;
    bool emphasized = false;
};

// Defines overrides for selected aspects of a Feature's symbology.
// Ported from: itwinjs-core core/common/src/FeatureSymbology.ts
class DQ_COMMON_EXPORT FeatureAppearance {
public:
    // Default constructor — needed for STL containers
    FeatureAppearance() noexcept = default;
    // Properties — ported from: itwinjs-core FeatureAppearance fields
    const std::optional<RgbColor>& getRgb() const noexcept { return m_rgb; }
    const std::optional<RgbColor>& getLineRgb() const noexcept { return m_lineRgb; }
    bool isLineRgbFalse() const noexcept { return m_lineRgbIsFalse; }
    std::optional<double> getWeight() const noexcept { return m_weight; }
    std::optional<double> getTransparency() const noexcept { return m_transparency; }
    std::optional<double> getLineTransparency() const noexcept { return m_lineTransparency; }
    bool isLineTransparencyFalse() const noexcept { return m_lineTransparencyIsFalse; }
    std::optional<LinePixels> getLinePixels() const noexcept { return m_linePixels; }
    bool getIgnoresMaterial() const noexcept { return m_ignoresMaterial; }
    bool getNonLocatable() const noexcept { return m_nonLocatable; }
    bool getEmphasized() const noexcept { return m_emphasized; }
    bool getViewDependentTransparency() const noexcept { return m_viewDependentTransparency; }

    // An appearance that overrides nothing.
    // Ported from: itwinjs-core FeatureAppearance.defaults
    static const FeatureAppearance& defaults() noexcept;

    // Create from JSON props.
    // Ported from: itwinjs-core FeatureAppearance.fromJSON()
    static FeatureAppearance fromJSON(const FeatureAppearanceProps* props = nullptr);

    // Create from ColorDef (RGB only, ignores transparency).
    // Ported from: itwinjs-core FeatureAppearance.fromRgb()
    static FeatureAppearance fromRgb(const ColorDef& color);

    // Create from ColorDef (RGB + transparency).
    // Ported from: itwinjs-core FeatureAppearance.fromRgba()
    static FeatureAppearance fromRgba(const ColorDef& color, bool viewDependentTransparency = false);

    // Create from transparency only.
    // Ported from: itwinjs-core FeatureAppearance.fromTransparency()
    static FeatureAppearance fromTransparency(double transparency, bool viewDependent = false);

    // Whether any aspect is overridden.
    bool overridesRgb() const noexcept { return m_rgb.has_value(); }
    bool overridesTransparency() const noexcept { return m_transparency.has_value(); }
    bool overridesLinePixels() const noexcept { return m_linePixels.has_value(); }
    bool overridesWeight() const noexcept { return m_weight.has_value(); }
    bool overridesNonLocatable() const noexcept { return m_nonLocatable; }

    // Whether any symbology is overridden.
    // Ported from: itwinjs-core FeatureAppearance.overridesSymbology
    bool overridesSymbology() const noexcept;

    // Whether any aspect is overridden.
    // Ported from: itwinjs-core FeatureAppearance.anyOverridden
    bool anyOverridden() const noexcept { return overridesSymbology() || overridesNonLocatable(); }

    // Whether this matches defaults.
    bool matchesDefaults() const noexcept { return equals(defaults()); }

    // Whether fully transparent.
    // Ported from: itwinjs-core FeatureAppearance.isFullyTransparent
    bool isFullyTransparent() const noexcept;

    // Get the color for linear geometry.
    // Ported from: itwinjs-core FeatureAppearance.getLineRgb()
    std::optional<RgbColor> getEffectiveLineRgb() const noexcept;

    // Get the transparency for linear geometry.
    // Ported from: itwinjs-core FeatureAppearance.getLineTransparency()
    std::optional<double> getEffectiveLineTransparency() const noexcept;

    // Equality.
    // Ported from: itwinjs-core FeatureAppearance.equals()
    bool equals(const FeatureAppearance& other) const noexcept;

    // Extend a base appearance with this one's overrides.
    // Ported from: itwinjs-core FeatureAppearance.extendAppearance()
    FeatureAppearance extendAppearance(const FeatureAppearance& base) const;

    // Convert to JSON.
    // Ported from: itwinjs-core FeatureAppearance.toJSON()
    FeatureAppearanceProps toJSON() const;

private:
    friend struct FeatureAppearanceDefaultsAccess;
    friend class FeatureOverrides;

    std::optional<RgbColor> m_rgb;
    std::optional<RgbColor> m_lineRgb;
    bool m_lineRgbIsFalse = false;
    std::optional<double> m_weight;
    std::optional<double> m_transparency;
    std::optional<double> m_lineTransparency;
    bool m_lineTransparencyIsFalse = false;
    std::optional<LinePixels> m_linePixels;
    bool m_ignoresMaterial = false;
    bool m_nonLocatable = false;
    bool m_emphasized = false;
    bool m_viewDependentTransparency = false;
};

END_DQ_COMMON_NAMESPACE
