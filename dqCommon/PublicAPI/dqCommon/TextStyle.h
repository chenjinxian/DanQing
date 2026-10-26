// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text style settings
// Ported from: itwinjs-core core/common/src/annotation/TextStyle.ts
#pragma once

#include "ColorDef.h"
#include "DqCommon.h"
#include "Fonts.h"

#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Predefined markers for list items in text annotations.
// Ported from: itwinjs-core ListMarkerEnumerator (TextStyle.ts:17-27) —
// 参考枚举值为字符串（"A"/"I"/"1"/"•"/…）且 ListMarker.enumerator 亦接受任意
// 字符串——C++ 形以 string 常量承载（§3.4 字符串字面量联合适配，等值比较
// 语义与参考逐位一致）。
namespace ListMarkerEnumerator {
    inline constexpr char const* Letter = "A";       // English Alphabet
    inline constexpr char const* RomanNumeral = "I";
    inline constexpr char const* Number = "1";
    inline constexpr char const* Bullet = "\xE2\x80\xA2";       // • (U+2022)
    inline constexpr char const* Circle = "\xE2\x97\x8B";       // ○ (U+25CB)
    inline constexpr char const* Square = "\xE2\x96\xA0";       // ■ (U+25A0)
    inline constexpr char const* Dash = "\xE2\x80\x93";         // – (U+2013, EM dash)
}

// A settings to specify how to mark or denote the start of a list item in a List.
// Ported from: itwinjs-core ListMarker (TextStyle.ts:32-39).
// `case` 为 C++ 关键字——最小区分改名 letterCase（§3.4 碰撞例外）。
struct ListMarker {
    // One of the predefined ListMarkerEnumerator values, or any arbitrary string.
    std::string enumerator = ListMarkerEnumerator::Number;
    // The punctuation to follow the enumerator: "parenthesis" | "period"（缺席 =
    // 无标点——getMarkerText 的空串臂）。
    std::optional<std::string> terminator;
    // "upper" | "lower" — ignored for non-alphabetic/roman enumerators.
    std::optional<std::string> letterCase;  // ← 参考名 `case`

    bool operator==(ListMarker const& other) const noexcept
    {
        return enumerator == other.enumerator && terminator == other.terminator
            && letterCase == other.letterCase;
    }
};

// Predefined shapes that can be computed and drawn around the margins of a
// TextBlock, in reference array order (textAnnotationFrameShapes, :44).
// Ported from: itwinjs-core TextAnnotationFrameShape (TextStyle.ts:44-49)
enum class TextAnnotationFrameShape : int32_t {
    None = 0,
    Line,
    Rectangle,
    Circle,
    EquilateralTriangle,
    Diamond,
    Square,
    Pentagon,
    Hexagon,
    Octagon,
    Capsule,
    RoundedRectangle,
};

// Predefined shapes usable as leader terminators (terminatorShapes, :54).
// Ported from: itwinjs-core TerminatorShape (TextStyle.ts:54-59)
enum class TerminatorShape : int32_t {
    OpenArrow = 0,
    ClosedArrow,
    ClosedArrowFilled,
    Circle,
    CircleFilled,
    Slash,
    None,
};

// Predefined shapes indicating the target point of a leader (targetPointShapes, :65).
// Ported from: itwinjs-core TargetPointShape (TextStyle.ts:65-70)
enum class TargetPointShape : int32_t {
    Cross = 0,
    Plus,
    Circle,
    Square,
    Rectangle,
};

// The color in which to draw the text / frame border: "subcategory" (use the
// hosting GeometryStream's SubCategory color) or a specific color.
// Ported from: itwinjs-core TextStyleColor (TextStyle.ts:112) —
// TS 字符串联合 → 标签结构（§3.4；等值/分支语义逐位一致）。
struct TextStyleColor {
    bool subcategory = true;
    ColorDefProps color = 0;  // valid iff !subcategory

    static TextStyleColor FromSubcategory() noexcept { return {}; }
    static TextStyleColor FromColor(ColorDef c) noexcept { return {false, c.getTbgr()}; }
    bool operator==(TextStyleColor const& other) const noexcept
    {
        return subcategory == other.subcategory && color == other.color;
    }
};

// What color to use when filling the frame around a TextBlock: "background"
// (GeometryParams.BackgroundFill = Outline), "none", "subcategory", or a color.
// Ported from: itwinjs-core TextAnnotationFillColor (TextStyle.ts:79) —
// TS 字符串联合 → 标签结构（§3.4 适配）。
struct TextAnnotationFillColor {
    enum class Source : int32_t { None = 0, Background, Subcategory, Rgb } source = Source::None;
    ColorDefProps rgb = 0;  // valid iff source == Rgb

    static TextAnnotationFillColor None() noexcept { return {}; }
    static TextAnnotationFillColor Background() noexcept { return {Source::Background, 0}; }
    static TextAnnotationFillColor FromSubcategory() noexcept { return {Source::Subcategory, 0}; }
    static TextAnnotationFillColor FromColor(ColorDef c) noexcept { return {Source::Rgb, c.getTbgr()}; }
    bool operator==(TextAnnotationFillColor const& other) const noexcept
    {
        return source == other.source && (source != Source::Rgb || rgb == other.rgb);
    }
};

// Multipliers that describe the margins around the content inside a TextBlock.
// All margins default to zero if absent.
// Ported from: itwinjs-core TextBlockMargins (TextStyle.ts:85-94)
struct TextBlockMargins {
    std::optional<double> left;
    std::optional<double> right;
    std::optional<double> top;
    std::optional<double> bottom;

    bool operator==(TextBlockMargins const& other) const noexcept
    {
        return left == other.left && right == other.right && top == other.top
            && bottom == other.bottom;
    }
};

// The relative alignment of text.
// Ported from: itwinjs-core TextJustification (TextStyle.ts:99)
enum class TextJustification : int32_t {
    Left = 0,
    Center,
    Right,
};

// How to separate the numerator and denominator of a FractionRun.
// Ported from: itwinjs-core StackedFractionType (TextStyle.ts:105)
enum class StackedFractionType : int32_t {
    Horizontal = 0,
    Diagonal,
};

// Describes how to draw the frame around a TextAnnotation.
// Ported from: itwinjs-core TextFrameStyleProps (TextStyle.ts:120-129)
struct TextFrameStyleProps {
    std::optional<TextAnnotationFrameShape> shape;                  // default "none"
    std::optional<TextAnnotationFillColor> fillColor;               // default "none"
    std::optional<TextStyleColor> borderColor;                      // default black
    std::optional<double> borderWeight;                             // default 1px

    bool operator==(TextFrameStyleProps const& other) const noexcept
    {
        const bool sc = (shape == other.shape);
        if (!sc) return false;
        // TextStyleColor 无 optional 比较的缺席语义——缺席==缺席、在场==在场。
        bool const fc = fillColor.has_value() == other.fillColor.has_value()
            && (!fillColor.has_value() || *fillColor == *other.fillColor);
        bool const bc = borderColor.has_value() == other.borderColor.has_value()
            && (!borderColor.has_value() || *borderColor == *other.borderColor);
        return fc && bc && borderWeight == other.borderWeight;
    }
};

// Properties describing the appearance of TextAnnotationLeaders.
// Ported from: itwinjs-core TextLeaderStyleProps (TextStyle.ts:135-185)
struct TextLeaderStyleProps {
    // "inherit"（父 TextStyleSettings 颜色）| TextStyleColor——参考
    // `TextStyleColor | "inherit"`：以第三个标签承载（§3.4 适配）。
    struct Color {
        enum class Source : int32_t { Inherit = 0, Subcategory, Rgb } source = Source::Inherit;
        ColorDefProps rgb = 0;  // valid iff source == Rgb

        bool operator==(Color const& other) const noexcept
        {
            return source == other.source && (source != Source::Rgb || rgb == other.rgb);
        }
    };
    std::optional<Color> color;                                     // default "inherit"
    std::optional<bool> wantElbow;                                  // default false
    std::optional<double> elbowLength;                              // default 1.0
    std::optional<TerminatorShape> terminatorShape;                 // default "openArrow"
    std::optional<double> terminatorHeightFactor;                   // default 1.0
    std::optional<double> terminatorWidthFactor;                    // default 1.0
    std::optional<bool> showLeaders;
    std::optional<bool> showTerminators;
    std::optional<bool> showTargetPoint;
    std::optional<TargetPointShape> targetPointShape;               // default "cross"
    std::optional<double> targetPointOffsetFactor;                  // default 0

    bool operator==(TextLeaderStyleProps const& other) const noexcept
    {
        auto const eqC = [&](std::optional<Color> const& a, std::optional<Color> const& b) {
            return a.has_value() == b.has_value() && (!a.has_value() || *a == *b);
        };
        return eqC(color, other.color) && wantElbow == other.wantElbow
            && elbowLength == other.elbowLength
            && terminatorShape == other.terminatorShape
            && terminatorHeightFactor == other.terminatorHeightFactor
            && terminatorWidthFactor == other.terminatorWidthFactor
            && showLeaders == other.showLeaders
            && showTerminators == other.showTerminators
            && showTargetPoint == other.showTargetPoint
            && targetPointShape == other.targetPointShape
            && targetPointOffsetFactor == other.targetPointOffsetFactor;
    }
};

// Serves both as the JSON representation of a TextStyleSettings, and a way for a
// TextBlockComponent to selectively override aspects of an AnnotationTextStyle's
// properties.
// Ported from: itwinjs-core TextStyleSettingsProps (TextStyle.ts:190-297)
struct TextStyleSettingsProps {
    std::optional<TextStyleColor> color;                            // default "subcategory"
    std::optional<FontFamilySelector> font;                         // default {name:"",TrueType}
    std::optional<double> textHeight;                               // default 1.0
    std::optional<double> lineSpacingFactor;                        // default 0.5
    std::optional<double> paragraphSpacingFactor;                   // default 0.5
    std::optional<bool> isBold;                                     // default false
    std::optional<bool> isItalic;                                   // default false
    std::optional<bool> isUnderlined;                               // default false
    std::optional<double> stackedFractionScale;                     // default 0.7
    std::optional<StackedFractionType> stackedFractionType;         // default "horizontal"
    std::optional<double> subScriptOffsetFactor;                    // default -0.15
    std::optional<double> subScriptScale;                           // default 2/3
    std::optional<double> superScriptOffsetFactor;                  // default 0.5
    std::optional<double> superScriptScale;                         // default 2/3
    std::optional<double> widthFactor;                              // default 1.0
    std::optional<TextLeaderStyleProps> leader;
    std::optional<double> tabInterval;                              // default 4
    std::optional<TextFrameStyleProps> frame;
    std::optional<TextBlockMargins> margins;
    std::optional<double> indentation;                              // default 0
    std::optional<ListMarker> listMarker;                           // default "1."
    std::optional<TextJustification> justification;                 // default "left"
};

// A description of the formatting to be applied to a TextBlockComponent.
// Named instances of these settings can be stored as AnnotationTextStyles in an
// iModel. Immutable — use clone to create a modified copy.
// Ported from: itwinjs-core TextStyleSettings (TextStyle.ts:317-590)
class DQ_COMMON_EXPORT TextStyleSettings {
public:
    // A fully-populated JSON representation of the default settings. A real font
    // must be provided before use.
    // Ported from: itwinjs-core TextStyleSettings.defaultProps (TextStyle.ts:395-440)
    static TextStyleSettingsProps defaultProps();

    // Settings initialized to all default values.
    // Ported from: itwinjs-core TextStyleSettings.defaults (TextStyle.ts:443)
    static TextStyleSettings const& defaults();

    // 缺省 = defaults()（参考 TextStyleSettings.defaults 静态实例 :443——
    // DanQing 以缺省构造对齐默认值语义）。
    TextStyleSettings()
        : TextStyleSettings(defaultProps())
    {
    }

    // Ported from: itwinjs-core TextStyleSettings ctor (TextStyle.ts:445-501) —
    // 每字段 `props.x ?? defaults.x`。defaults 缺省 = defaultProps。
    explicit TextStyleSettings(TextStyleSettingsProps const& props,
                               TextStyleSettingsProps const* overrides = nullptr);

    // Create a copy of these settings, modified according to `alteredProps`.
    // Ported from: itwinjs-core TextStyleSettings.clone (TextStyle.ts:504-506)
    TextStyleSettings clone(TextStyleSettingsProps const* alteredProps = nullptr) const;

    // Create settings from their JSON representation.
    // Ported from: itwinjs-core TextStyleSettings.fromJSON (TextStyle.ts:509-511)
    static TextStyleSettings fromJSON(TextStyleSettingsProps const* props);

    // A fully-populated JSON representation of these settings.
    // Ported from: itwinjs-core TextStyleSettings.toJSON (TextStyle.ts:513-515)
    TextStyleSettingsProps toJSON() const;

    // --- 字段（参考 readonly 成员 1:1，字段名逐字） ---
    TextStyleColor color;
    FontFamilySelector font;  // Required<FontFamilySelector> — type 恒在场
    double textHeight = 1.0;
    double lineSpacingFactor = 0.5;
    double paragraphSpacingFactor = 0.5;
    bool isBold = false;
    bool isItalic = false;
    bool isUnderlined = false;
    double stackedFractionScale = 0.7;
    StackedFractionType stackedFractionType = StackedFractionType::Horizontal;
    double subScriptOffsetFactor = -0.15;
    double subScriptScale = 2.0 / 3.0;
    double superScriptOffsetFactor = 0.5;
    double superScriptScale = 2.0 / 3.0;
    double widthFactor = 1.0;
    // Required<TextLeaderStyleProps> — 全字段必在（已填充默认）。
    TextLeaderStyleProps::Color leaderColor;
    bool leaderWantElbow = false;
    double leaderElbowLength = 1.0;
    TerminatorShape leaderTerminatorShape = TerminatorShape::OpenArrow;
    double leaderTerminatorHeightFactor = 1.0;
    double leaderTerminatorWidthFactor = 1.0;
    bool leaderShowLeaders = true;
    bool leaderShowTerminators = true;
    bool leaderShowTargetPoint = false;
    TargetPointShape leaderTargetPointShape = TargetPointShape::Cross;
    double leaderTargetPointOffsetFactor = 0.0;
    double tabInterval = 4.0;
    double indentation = 0.0;
    ListMarker listMarker;  // default {enumerator:"1", terminator:"period", case:"lower"}
    TextAnnotationFrameShape frameShape = TextAnnotationFrameShape::None;
    TextAnnotationFillColor frameFillColor = TextAnnotationFillColor::None();  // default "none"
    TextStyleColor frameBorderColor;      // default black（TextStyleColor 默认 =
                                          //  subcategory 标签——defaultProps 的
                                          //  frame.borderColor=black 由 ctor 显式覆盖）
    double frameBorderWeight = 1.0;
    double marginLeft = 0.0;
    double marginRight = 0.0;
    double marginTop = 0.0;
    double marginBottom = 0.0;
    TextJustification justification = TextJustification::Left;

    // Ported from: itwinjs-core TextStyleSettings.leaderEquals (TextStyle.ts:521-533)
    bool leaderEquals(TextLeaderStyleProps const& other) const noexcept;

    // Ported from: itwinjs-core TextStyleSettings.frameEquals (TextStyle.ts:535-540)
    bool frameEquals(TextFrameStyleProps const& other) const noexcept;

    // Ported from: itwinjs-core TextStyleSettings.marginsEqual (TextStyle.ts:542-546)
    bool marginsEqual(TextBlockMargins const& other) const noexcept;

    // Ported from: itwinjs-core TextStyleSettings.equals (TextStyle.ts:548-562)
    bool equals(TextStyleSettings const& other) const noexcept;

    // Universally-invalid value checks.
    // Ported from: itwinjs-core TextStyleSettings.getValidationErrors (TextStyle.ts:574-589)
    std::vector<std::string> getValidationErrors() const;
};

END_DQ_COMMON_NAMESPACE
