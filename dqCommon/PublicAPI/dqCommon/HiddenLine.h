// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Hidden line settings
//
// Ported from: itwinjs-core core/common/src/HiddenLine.ts
// Controls how edges and surfaces are drawn in hidden line/solid fill modes.
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "LinePixels.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Edge style properties (JSON).
// Ported from: itwinjs-core HiddenLine.StyleProps
struct HiddenLineStyleProps {
    std::optional<bool> ovrColor;
    std::optional<ColorDefProps> color;
    std::optional<LinePixels> pattern;
    std::optional<int> width;
};

// Describes how edges should be drawn.
// Ported from: itwinjs-core HiddenLine.Style
class DQ_COMMON_EXPORT HiddenLineStyle {
public:
    std::optional<ColorDef> color;
    std::optional<LinePixels> pattern;
    std::optional<int> width;

    HiddenLineStyle() = default;

    // Whether color is overridden.
    bool overridesColor() const noexcept { return color.has_value(); }

    static const HiddenLineStyle& defaultVisible() noexcept;
    static const HiddenLineStyle& defaultHidden() noexcept;

    static HiddenLineStyle fromJSON(const HiddenLineStyleProps* props = nullptr, bool hidden = false);

    HiddenLineStyleProps toJSON() const;

    bool equals(const HiddenLineStyle& other) const noexcept;

    HiddenLineStyle overrideColor(const std::optional<ColorDef>& newColor) const;
    HiddenLineStyle overridePattern(std::optional<LinePixels> newPattern) const;
    HiddenLineStyle overrideWidth(std::optional<int> newWidth) const;
};

// Hidden line settings properties (JSON).
// Ported from: itwinjs-core HiddenLine.SettingsProps
struct HiddenLineSettingsProps {
    std::optional<HiddenLineStyleProps> visible;
    std::optional<HiddenLineStyleProps> hidden;
    std::optional<double> transThreshold;
};

// Describes how visible/hidden edges and transparent surfaces should be rendered.
// Ported from: itwinjs-core HiddenLine.Settings
class DQ_COMMON_EXPORT HiddenLineSettings {
public:
    HiddenLineStyle visible;
    HiddenLineStyle hidden;
    double transparencyThreshold = 1.0;

    HiddenLineSettings() = default;

    static const HiddenLineSettings& defaults() noexcept;
    static HiddenLineSettings fromJSON(const HiddenLineSettingsProps& props);
    HiddenLineSettingsProps toJSON() const;

    bool equals(const HiddenLineSettings& other) const noexcept;
    bool matchesDefaults() const noexcept { return equals(defaults()); }
};

END_DQ_COMMON_NAMESPACE
