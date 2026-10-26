// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextStyleSettings implementation
// Ported from: itwinjs-core core/common/src/annotation/TextStyle.ts
#include <dqCommon/TextStyle.h>

namespace dqCommon {

// Ported from: itwinjs-core TextStyleSettings.defaultProps (TextStyle.ts:395-440)
TextStyleSettingsProps TextStyleSettings::defaultProps()
{
    TextStyleSettingsProps p;
    p.color = TextStyleColor::FromSubcategory();
    p.font = FontFamilySelector{FontType::TrueType, ""};
    p.textHeight = 1.0;
    p.lineSpacingFactor = 0.5;
    p.paragraphSpacingFactor = 0.5;
    p.isBold = false;
    p.isItalic = false;
    p.isUnderlined = false;
    p.stackedFractionScale = 0.7;
    p.stackedFractionType = StackedFractionType::Horizontal;
    p.subScriptOffsetFactor = -0.15;
    p.subScriptScale = 2.0 / 3.0;
    p.superScriptOffsetFactor = 0.5;
    p.superScriptScale = 2.0 / 3.0;
    p.widthFactor = 1.0;
    TextLeaderStyleProps leader;
    leader.color = TextLeaderStyleProps::Color{};  // "inherit"
    leader.wantElbow = false;
    leader.elbowLength = 1.0;
    leader.terminatorShape = TerminatorShape::OpenArrow;
    leader.terminatorHeightFactor = 1.0;
    leader.terminatorWidthFactor = 1.0;
    leader.showLeaders = true;
    leader.showTerminators = true;
    leader.showTargetPoint = false;
    leader.targetPointShape = TargetPointShape::Cross;
    leader.targetPointOffsetFactor = 0.0;
    p.leader = leader;
    p.tabInterval = 4.0;
    p.indentation = 0.0;
    ListMarker marker;
    marker.enumerator = ListMarkerEnumerator::Number;
    marker.terminator = "period";
    marker.letterCase = "lower";
    p.listMarker = marker;
    TextFrameStyleProps frame;
    frame.shape = TextAnnotationFrameShape::None;
    frame.fillColor = TextAnnotationFillColor::None();
    frame.borderColor = TextStyleColor::FromColor(ColorDef::black);
    frame.borderWeight = 1.0;
    p.frame = frame;
    p.margins = TextBlockMargins{0.0, 0.0, 0.0, 0.0};
    p.justification = TextJustification::Left;
    return p;
}

// Ported from: itwinjs-core TextStyleSettings.defaults (TextStyle.ts:443)
TextStyleSettings const& TextStyleSettings::defaults()
{
    static TextStyleSettings const s_defaults = TextStyleSettings(defaultProps());
    return s_defaults;
}

// Ported from: itwinjs-core TextStyleSettings ctor (TextStyle.ts:445-501)
TextStyleSettings::TextStyleSettings(TextStyleSettingsProps const& props,
                                     TextStyleSettingsProps const* overrideDefaults)
{
    TextStyleSettingsProps const def = overrideDefaults ? *overrideDefaults : defaultProps();

    color = props.color.value_or(def.color.value());
    font.name = props.font ? props.font->name : def.font->name;
    font.type = props.font && props.font->type ? *props.font->type : *def.font->type;
    textHeight = props.textHeight.value_or(def.textHeight.value());
    lineSpacingFactor = props.lineSpacingFactor.value_or(def.lineSpacingFactor.value());
    paragraphSpacingFactor = props.paragraphSpacingFactor.value_or(def.paragraphSpacingFactor.value());
    isBold = props.isBold.value_or(def.isBold.value());
    isItalic = props.isItalic.value_or(def.isItalic.value());
    isUnderlined = props.isUnderlined.value_or(def.isUnderlined.value());
    stackedFractionScale = props.stackedFractionScale.value_or(def.stackedFractionScale.value());
    stackedFractionType = props.stackedFractionType.value_or(def.stackedFractionType.value());
    subScriptOffsetFactor = props.subScriptOffsetFactor.value_or(def.subScriptOffsetFactor.value());
    subScriptScale = props.subScriptScale.value_or(def.subScriptScale.value());
    superScriptOffsetFactor = props.superScriptOffsetFactor.value_or(def.superScriptOffsetFactor.value());
    superScriptScale = props.superScriptScale.value_or(def.superScriptScale.value());
    widthFactor = props.widthFactor.value_or(def.widthFactor.value());

    TextLeaderStyleProps const& pl = props.leader ? *props.leader : TextLeaderStyleProps{};
    TextLeaderStyleProps const& dl = def.leader ? *def.leader : TextLeaderStyleProps{};
    leaderColor = pl.color.has_value() ? *pl.color
                 : (dl.color.has_value() ? *dl.color : TextLeaderStyleProps::Color{});
    leaderWantElbow = pl.wantElbow.value_or(dl.wantElbow.value_or(false));
    leaderElbowLength = pl.elbowLength.value_or(dl.elbowLength.value_or(1.0));
    leaderTerminatorShape = pl.terminatorShape.value_or(dl.terminatorShape.value_or(TerminatorShape::OpenArrow));
    leaderTerminatorHeightFactor = pl.terminatorHeightFactor.value_or(dl.terminatorHeightFactor.value_or(1.0));
    leaderTerminatorWidthFactor = pl.terminatorWidthFactor.value_or(dl.terminatorWidthFactor.value_or(1.0));
    leaderShowLeaders = pl.showLeaders.value_or(dl.showLeaders.value_or(true));
    leaderShowTerminators = pl.showTerminators.value_or(dl.showTerminators.value_or(true));
    leaderShowTargetPoint = pl.showTargetPoint.value_or(dl.showTargetPoint.value_or(false));
    leaderTargetPointShape = pl.targetPointShape.value_or(dl.targetPointShape.value_or(TargetPointShape::Cross));
    leaderTargetPointOffsetFactor = pl.targetPointOffsetFactor.value_or(dl.targetPointOffsetFactor.value_or(0.0));

    tabInterval = props.tabInterval.value_or(def.tabInterval.value());
    indentation = props.indentation.value_or(def.indentation.value());
    listMarker = props.listMarker.value_or(def.listMarker.value());

    TextFrameStyleProps const& pf = props.frame ? *props.frame : TextFrameStyleProps{};
    TextFrameStyleProps const& df = def.frame ? *def.frame : TextFrameStyleProps{};
    frameShape = pf.shape.value_or(df.shape.value_or(TextAnnotationFrameShape::None));
    frameFillColor = pf.fillColor.value_or(df.fillColor.value_or(TextAnnotationFillColor::None()));
    frameBorderColor = pf.borderColor.value_or(df.borderColor.value_or(
        TextStyleColor::FromColor(ColorDef::black)));
    frameBorderWeight = pf.borderWeight.value_or(df.borderWeight.value_or(1.0));

    TextBlockMargins const& pm = props.margins ? *props.margins : TextBlockMargins{};
    TextBlockMargins const& dm = def.margins ? *def.margins : TextBlockMargins{};
    marginLeft = pm.left.value_or(dm.left.value_or(0.0));
    marginRight = pm.right.value_or(dm.right.value_or(0.0));
    marginTop = pm.top.value_or(dm.top.value_or(0.0));
    marginBottom = pm.bottom.value_or(dm.bottom.value_or(0.0));

    justification = props.justification.value_or(def.justification.value());
}

// Ported from: itwinjs-core TextStyleSettings.clone (TextStyle.ts:504-506)
TextStyleSettings TextStyleSettings::clone(TextStyleSettingsProps const* alteredProps) const
{
    // alteredProps ? new TextStyleSettings(alteredProps, this) : this（immutable——
    // 无改动时按值复制等价；this 作 defaults = 全字段已填充形态 = toJSON()）。
    if (!alteredProps)
        return *this;
    TextStyleSettingsProps const mine = toJSON();
    return TextStyleSettings(*alteredProps, &mine);
}

// Ported from: itwinjs-core TextStyleSettings.fromJSON (TextStyle.ts:509-511)
TextStyleSettings TextStyleSettings::fromJSON(TextStyleSettingsProps const* props)
{
    return props ? TextStyleSettings(*props) : defaults();
}

// Ported from: itwinjs-core TextStyleSettings.toJSON (TextStyle.ts:513-515) —
// structuredClone(this) = 全字段写出（Required 形）。
TextStyleSettingsProps TextStyleSettings::toJSON() const
{
    TextStyleSettingsProps p;
    p.color = color;
    p.font = font;
    p.textHeight = textHeight;
    p.lineSpacingFactor = lineSpacingFactor;
    p.paragraphSpacingFactor = paragraphSpacingFactor;
    p.isBold = isBold;
    p.isItalic = isItalic;
    p.isUnderlined = isUnderlined;
    p.stackedFractionScale = stackedFractionScale;
    p.stackedFractionType = stackedFractionType;
    p.subScriptOffsetFactor = subScriptOffsetFactor;
    p.subScriptScale = subScriptScale;
    p.superScriptOffsetFactor = superScriptOffsetFactor;
    p.superScriptScale = superScriptScale;
    p.widthFactor = widthFactor;
    TextLeaderStyleProps leader;
    leader.color = leaderColor;
    leader.wantElbow = leaderWantElbow;
    leader.elbowLength = leaderElbowLength;
    leader.terminatorShape = leaderTerminatorShape;
    leader.terminatorHeightFactor = leaderTerminatorHeightFactor;
    leader.terminatorWidthFactor = leaderTerminatorWidthFactor;
    leader.showLeaders = leaderShowLeaders;
    leader.showTerminators = leaderShowTerminators;
    leader.showTargetPoint = leaderShowTargetPoint;
    leader.targetPointShape = leaderTargetPointShape;
    leader.targetPointOffsetFactor = leaderTargetPointOffsetFactor;
    p.leader = leader;
    p.tabInterval = tabInterval;
    p.indentation = indentation;
    p.listMarker = listMarker;
    TextFrameStyleProps frame;
    frame.shape = frameShape;
    frame.fillColor = frameFillColor;
    frame.borderColor = frameBorderColor;
    frame.borderWeight = frameBorderWeight;
    p.frame = frame;
    p.margins = TextBlockMargins{marginLeft, marginRight, marginTop, marginBottom};
    p.justification = justification;
    return p;
}

// Ported from: itwinjs-core TextStyleSettings.leaderEquals (TextStyle.ts:521-533)
// 参考以 === 逐字段比较 this.leader.*（Required）对 other（可选）——缺席即不等。
bool TextStyleSettings::leaderEquals(TextLeaderStyleProps const& other) const noexcept
{
    bool const c = other.color.has_value() && *other.color == leaderColor;
    return c
        && other.wantElbow.has_value() && *other.wantElbow == leaderWantElbow
        && other.elbowLength.has_value() && *other.elbowLength == leaderElbowLength
        && other.terminatorShape.has_value() && *other.terminatorShape == leaderTerminatorShape
        && other.terminatorHeightFactor.has_value() && *other.terminatorHeightFactor == leaderTerminatorHeightFactor
        && other.terminatorWidthFactor.has_value() && *other.terminatorWidthFactor == leaderTerminatorWidthFactor
        && other.showLeaders.has_value() && *other.showLeaders == leaderShowLeaders
        && other.showTerminators.has_value() && *other.showTerminators == leaderShowTerminators
        && other.showTargetPoint.has_value() && *other.showTargetPoint == leaderShowTargetPoint
        && other.targetPointShape.has_value() && *other.targetPointShape == leaderTargetPointShape
        && other.targetPointOffsetFactor.has_value() && *other.targetPointOffsetFactor == leaderTargetPointOffsetFactor;
}

// Ported from: itwinjs-core TextStyleSettings.frameEquals (TextStyle.ts:535-540)
bool TextStyleSettings::frameEquals(TextFrameStyleProps const& other) const noexcept
{
    return other.shape.has_value() && *other.shape == frameShape
        && other.fillColor.has_value() && *other.fillColor == frameFillColor
        && other.borderColor.has_value() && *other.borderColor == frameBorderColor
        && other.borderWeight.has_value() && *other.borderWeight == frameBorderWeight;
}

// Ported from: itwinjs-core TextStyleSettings.marginsEqual (TextStyle.ts:542-546)
bool TextStyleSettings::marginsEqual(TextBlockMargins const& other) const noexcept
{
    return other.left == marginLeft && other.right == marginRight
        && other.top == marginTop && other.bottom == marginBottom;
}

// Ported from: itwinjs-core TextStyleSettings.equals (TextStyle.ts:548-562)
bool TextStyleSettings::equals(TextStyleSettings const& other) const noexcept
{
    return color == other.color && font.name == other.font.name && font.type == other.font.type
        && textHeight == other.textHeight && widthFactor == other.widthFactor
        && lineSpacingFactor == other.lineSpacingFactor && paragraphSpacingFactor == other.paragraphSpacingFactor
        && isBold == other.isBold && isItalic == other.isItalic && isUnderlined == other.isUnderlined
        && stackedFractionType == other.stackedFractionType && stackedFractionScale == other.stackedFractionScale
        && subScriptOffsetFactor == other.subScriptOffsetFactor && subScriptScale == other.subScriptScale
        && superScriptOffsetFactor == other.superScriptOffsetFactor && superScriptScale == other.superScriptScale
        && tabInterval == other.tabInterval && indentation == other.indentation
        && listMarker == other.listMarker
        && justification == other.justification
        && leaderColor == other.leaderColor && leaderWantElbow == other.leaderWantElbow
        && leaderElbowLength == other.leaderElbowLength
        && leaderTerminatorShape == other.leaderTerminatorShape
        && leaderTerminatorHeightFactor == other.leaderTerminatorHeightFactor
        && leaderTerminatorWidthFactor == other.leaderTerminatorWidthFactor
        && leaderShowLeaders == other.leaderShowLeaders
        && leaderShowTerminators == other.leaderShowTerminators
        && leaderShowTargetPoint == other.leaderShowTargetPoint
        && leaderTargetPointShape == other.leaderTargetPointShape
        && leaderTargetPointOffsetFactor == other.leaderTargetPointOffsetFactor
        && frameShape == other.frameShape && frameFillColor == other.frameFillColor
        && frameBorderColor == other.frameBorderColor && frameBorderWeight == other.frameBorderWeight
        && marginLeft == other.marginLeft && marginRight == other.marginRight
        && marginTop == other.marginTop && marginBottom == other.marginBottom;
}

// Ported from: itwinjs-core TextStyleSettings.getValidationErrors (TextStyle.ts:574-589)
std::vector<std::string> TextStyleSettings::getValidationErrors() const
{
    std::vector<std::string> errorMessages;
    // font name trim — reference `this.font.name.trim() === ""`.
    bool const nameBlank = font.name.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
    if (nameBlank)
        errorMessages.push_back("font name must be provided");
    if (textHeight <= 0)
        errorMessages.push_back("textHeight must be greater than 0");
    if (stackedFractionScale <= 0)
        errorMessages.push_back("stackedFractionScale must be greater than 0");
    return errorMessages;
}

}  // namespace dqCommon
