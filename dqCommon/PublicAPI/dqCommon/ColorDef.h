// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Immutable color representation
//
// Ported from: itwinjs-core core/common/src/ColorDef.ts
// Internal format: 0xTTBBGGRR (transparency in high byte, red in low byte).
// Immutable — all modifications return new instances.
#pragma once

#include "ColorByName.h"
#include "Export.h"
#include "HSLColor.h"
#include "HSVColor.h"

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// JSON representation of a ColorDef — a uint32_t in 0xTTBBGGRR format.
// Ported from: itwinjs-core core/common/src/ColorDef.ts
using ColorDefProps = uint32_t;

// Color components extracted from a TBGR value.
struct ColorComponents {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t t = 0;  // transparency (0 = fully opaque, 255 = fully transparent)
};

// An immutable integer representation of a color.
// Ported from: itwinjs-core core/common/src/ColorDef.ts
class DQ_COMMON_EXPORT ColorDef {
public:
    // --- Static factory methods ---

    // create from optional value (number = TBGR, string = parse).
    // Ported from: itwinjs-core ColorDef.create()
    static ColorDef create() noexcept { return black; }
    static ColorDef create(ColorDefProps val) { return fromTbgr(val); }
    static ColorDef create(const std::string& val) { return fromString(val); }

    // create from red, green, blue, transparency (each 0-255).
    // Ported from: itwinjs-core ColorDef.from()
    static ColorDef from(int red, int green, int blue, int transparency = 0);

    // create from 0xTTBBGGRR value.
    // Ported from: itwinjs-core ColorDef.fromTbgr()
    static ColorDef fromTbgr(ColorDefProps tbgr);

    // create from 0xAABBGGRR value.
    // Ported from: itwinjs-core ColorDef.fromAbgr()
    static ColorDef fromAbgr(uint32_t abgr);

    // create from string (rgb/rgba/hsl/hsla/hex/name).
    // Ported from: itwinjs-core ColorDef.fromString()
    static ColorDef fromString(const std::string& val);

    // create from HSL values.
    // Ported from: itwinjs-core ColorDef.fromHSL()
    static ColorDef fromHSL(double h, double s, double l, int transparency = 0);

    // create from HSVColor.
    // Ported from: itwinjs-core ColorDef.fromHSV()
    static ColorDef fromHSV(const HSVColor& hsv, int transparency = 0);

    // --- Compute helpers (static, return TBGR value) ---

    // Ported from: itwinjs-core ColorDef.computeTbgr()
    static ColorDefProps computeTbgr(const std::string& val);
    static ColorDefProps computeTbgr(ColorDefProps val);

    // Ported from: itwinjs-core ColorDef.computeTbgrFromComponents()
    static ColorDefProps computeTbgrFromComponents(int red, int green, int blue, int transparency = 0);

    // Ported from: itwinjs-core ColorDef.computeTbgrFromString()
    static ColorDefProps computeTbgrFromString(const std::string& val);

    // Ported from: itwinjs-core ColorDef.tryComputeTbgrFromString()
    static std::optional<ColorDefProps> tryComputeTbgrFromString(const std::string& val);

    // Ported from: itwinjs-core ColorDef.computeTbgrFromHSL()
    static ColorDefProps computeTbgrFromHSL(double h, double s, double l, int transparency = 0);

    // --- Accessors ---

    // The TBGR value (0xTTBBGGRR).
    // Ported from: itwinjs-core ColorDef.tbgr
    ColorDefProps getTbgr() const noexcept { return m_tbgr; }

    // Get r,g,b,t components.
    // Ported from: itwinjs-core ColorDef.colors / ColorDef.getColors()
    ColorComponents getColors() const noexcept;
    static ColorComponents getColors(ColorDefProps tbgr) noexcept;

    // Get as 0xAABBGGRR (transparency → alpha).
    // Ported from: itwinjs-core ColorDef.getAbgr()
    uint32_t getAbgr() const noexcept { return getAbgr(m_tbgr); }
    static uint32_t getAbgr(ColorDefProps tbgr) noexcept;

    // Get as 0xRRGGBB (ignores transparency).
    // Ported from: itwinjs-core ColorDef.getRgb()
    uint32_t getRgb() const noexcept { return getRgb(m_tbgr); }
    static uint32_t getRgb(ColorDefProps tbgr) noexcept;

    // Get alpha (0=transparent, 255=opaque).
    // Ported from: itwinjs-core ColorDef.getAlpha()
    int getAlpha() const noexcept { return getAlpha(m_tbgr); }
    static int getAlpha(ColorDefProps tbgr) noexcept;

    // True if fully opaque.
    // Ported from: itwinjs-core ColorDef.isOpaque
    bool isOpaque() const noexcept { return isOpaque(m_tbgr); }
    static bool isOpaque(ColorDefProps tbgr) noexcept;

    // Get transparency (0=opaque, 255=transparent).
    // Ported from: itwinjs-core ColorDef.getTransparency()
    int getTransparency() const noexcept { return getTransparency(m_tbgr); }
    static int getTransparency(ColorDefProps tbgr) noexcept;

    // The "known name" for this color, or empty if not in ColorByName.
    // Ported from: itwinjs-core ColorDef.name / ColorDef.getName()
    std::string getName() const { return getName(m_tbgr); }
    static std::string getName(ColorDefProps tbgr);

    // --- Withers (return new ColorDef) ---

    // copy with specified alpha.
    // Ported from: itwinjs-core ColorDef.withAlpha()
    ColorDef withAlpha(int alpha) const;
    static ColorDefProps withAlpha(ColorDefProps tbgr, int alpha) noexcept;

    // copy with specified transparency.
    // Ported from: itwinjs-core ColorDef.withTransparency()
    ColorDef withTransparency(int transparency) const;
    static ColorDefProps withTransparency(ColorDefProps tbgr, int transparency) noexcept;

    // --- Conversions ---

    // To "#rrggbb" string.
    // Ported from: itwinjs-core ColorDef.toHexString()
    std::string toHexString() const { return toHexString(m_tbgr); }
    static std::string toHexString(ColorDefProps tbgr);

    // To "rgb(r,g,b)" string.
    // Ported from: itwinjs-core ColorDef.toRgbString()
    std::string toRgbString() const { return toRgbString(m_tbgr); }
    static std::string toRgbString(ColorDefProps tbgr);

    // To "rgba(r,g,b,a)" string.
    // Ported from: itwinjs-core ColorDef.toRgbaString()
    std::string toRgbaString() const { return toRgbaString(m_tbgr); }
    static std::string toRgbaString(ColorDefProps tbgr);

    // Convert to HSLColor.
    // Ported from: itwinjs-core ColorDef.toHSL()
    HSLColor toHSL() const;

    // Convert to HSVColor.
    // Ported from: itwinjs-core ColorDef.toHSV()
    HSVColor toHSV() const;

    // --- Operations ---

    // Linear interpolation between two colors.
    // Ported from: itwinjs-core ColorDef.lerp()
    ColorDef lerp(const ColorDef& color2, double weight) const;
    static ColorDefProps lerp(ColorDefProps tbgr1, ColorDefProps tbgr2, double weight) noexcept;

    // Inverse (255 - each component). Ignores transparency.
    // Ported from: itwinjs-core ColorDef.inverse()
    ColorDef inverse() const;
    static ColorDefProps inverse(ColorDefProps tbgr) noexcept;

    // Adjust for maximum contrast against another color.
    // Ported from: itwinjs-core ColorDef.adjustedForContrast()
    ColorDef adjustedForContrast(const ColorDef& other, std::optional<int> alpha = std::nullopt) const;

    // Equality.
    // Ported from: itwinjs-core ColorDef.equals()
    bool equals(const ColorDef& other) const noexcept { return m_tbgr == other.m_tbgr; }

    // Validation.
    // Ported from: itwinjs-core ColorDef.isValidColor()
    static bool isValidColor(const std::string& val);
    static bool isValidColor(uint32_t val) noexcept;

    // --- Well-known constants ---
    // Ported from: itwinjs-core ColorDef.black/white/red/green/blue
    static const ColorDef black;
    static const ColorDef white;
    static const ColorDef red;
    static const ColorDef green;
    static const ColorDef blue;

private:
    explicit constexpr ColorDef(ColorDefProps tbgr) noexcept : m_tbgr(tbgr) {}

    ColorDefProps m_tbgr = 0;
};

END_DQ_COMMON_NAMESPACE
