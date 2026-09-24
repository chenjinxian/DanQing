// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Gradient symbology
//
// Ported from: itwinjs-core core/common/src/Gradient.ts
// Multi-color area fill defined by a range of colors that vary by position.
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "GradientKeyColor.h"
#include "Image.h"
#include "ThematicDisplay.h"
#include "DqCommon.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Gradient flags.
// Ported from: itwinjs-core Gradient.Flags
enum class GradientFlags : uint8_t {
    None = 0,
    Invert = 1,
    Outline = 2,
};

// Gradient modes.
// Ported from: itwinjs-core Gradient.Mode
enum class GradientMode : uint8_t {
    None = 0,
    Linear = 1,
    Curved = 2,
    Cylindrical = 3,
    Spherical = 4,
    Hemispherical = 5,
    Thematic = 6,
};

// GradientKeyColor / GradientKeyColorProps now live in GradientKeyColor.h (included above).
// ThematicGradientSettings(+Props) come from ThematicDisplay.h (included above) and are
// stored by value in std::optional below (TD-8: shared_ptr removed to satisfy §5/§9).

// Gradient symbology (JSON).
// Ported from: itwinjs-core Gradient.SymbProps
struct GradientSymbProps {
    GradientMode mode = GradientMode::None;
    GradientFlags flags = GradientFlags::None;
    double angleDegrees = 0.0;
    std::optional<double> tint;
    double shift = 0.0;
    std::vector<GradientKeyColorProps> keys;
    // Settings applicable to ThematicDisplay. (ref line 83)
    std::optional<ThematicGradientSettingsProps> thematicSettings;
};

// Arguments supplied to GradientSymb::produceImage.
// Ported from: itwinjs-core Gradient.ProduceImageArgs (interface lines 89-98)
struct ProduceImageArgs {
    int width = 1;
    int height = 1;
    // Include the thematic margin color in the top/bottom rows when mode is Thematic.
    bool includeThematicMargin = false;
};

// Multi-color area fill defined by a range of colors that vary by position.
// Ported from: itwinjs-core Gradient.Symb
class DQ_COMMON_EXPORT GradientSymb {
public:
    GradientMode mode = GradientMode::None;
    GradientFlags flags = GradientFlags::None;
    double angleDegrees = 0.0;
    std::optional<double> tint;
    double shift = 0.0;
    std::vector<GradientKeyColor> keys;
    // Settings applicable to ThematicDisplay. (ref line 110)
    std::optional<ThematicGradientSettings> thematicSettings;

    // Create from JSON.
    // Ported from: itwinjs-core Gradient.Symb.fromJSON()
    static GradientSymb fromJSON(const GradientSymbProps& props);

    // Convert to JSON.
    // Ported from: itwinjs-core Gradient.Symb.toJSON()
    GradientSymbProps toJSON() const;

    // clone.
    // Ported from: itwinjs-core Gradient.Symb.clone()
    GradientSymb clone() const { return *this; }

    // Equality.
    // Ported from: itwinjs-core Gradient.Symb.equals()
    bool equals(const GradientSymb& other) const noexcept { return compare(*this, other) == 0; }

    // Compare two gradient symbologies.
    // Ported from: itwinjs-core Gradient.Symb.compareSymb()
    static int compare(const GradientSymb& lhs, const GradientSymb& rhs) noexcept;

    // Compare to another.
    int compareTo(const GradientSymb& other) const noexcept { return compare(*this, other); }

    // Map a value [0,1] to a color.
    // Ported from: itwinjs-core Gradient.Symb.mapColor()
    ColorDef mapColor(double value) const noexcept;

    // Whether any key color has translucency.
    // Ported from: itwinjs-core Gradient.Symb.hasTranslucency
    bool hasTranslucency() const noexcept;

    // Whether the Outline flag is set.
    // Ported from: itwinjs-core Gradient.Symb.isOutlined
    bool isOutlined() const noexcept { return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(GradientFlags::Outline)) != 0; }

    // Create a Gradient.Symb for ThematicDisplay. (ref lines 140-158)
    // Ported from: itwinjs-core Gradient.Symb.createThematic()
    static GradientSymb createThematic(const ThematicGradientSettings& settings);

    // Produce an image suitable for thematic rendering (chiefly useful for the WebGL renderer).
    // (ref lines 307-359)
    // Ported from: itwinjs-core Gradient.Symb.getThematicImageForRenderer()
    std::optional<ImageBuffer> getThematicImageForRenderer(int maxDimension) const;

    // Produce a bitmap image, forcing width=1 for thematic mode and including margin.
    // (ref lines 367-372)
    // Ported from: itwinjs-core Gradient.Symb.getImage()
    std::optional<ImageBuffer> getImage(int width, int height) const;

    // Produce a bitmap image from this gradient. (ref lines 375-525)
    // Ported from: itwinjs-core Gradient.Symb.produceImage()
    std::optional<ImageBuffer> produceImage(const ProduceImageArgs& args) const;
};

END_DQ_COMMON_NAMESPACE
