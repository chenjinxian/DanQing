// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextStyleSettings tests
// Ported from: itwinjs-core core/common/src/test/annotations/TextStyle.test.ts
//              describe("TextStyleSettings") — 4 cases direct-mapped.
#include <gtest/gtest.h>

#include <dqCommon/TextStyle.h>

namespace {

// Ported from: TextStyle.test.ts:15-62 customProps（DeepRequired 全字段形态）。
// 颜色字段：参考的 TextStyleColor/TextAnnotationFillColor 数字形态（tbgr）。
dqCommon::TextStyleSettingsProps makeCustomProps()
{
    dqCommon::TextStyleSettingsProps p;
    p.color = dqCommon::TextStyleColor{false, 0xff007f};
    p.font = dqCommon::FontFamilySelector{dqCommon::FontType::TrueType, "customFont"};
    p.textHeight = 2.0;
    p.lineSpacingFactor = 1.0;
    p.paragraphSpacingFactor = 2.0;
    p.isBold = true;
    p.isItalic = true;
    p.isUnderlined = true;
    p.stackedFractionScale = 0.8;
    p.stackedFractionType = dqCommon::StackedFractionType::Diagonal;
    p.subScriptOffsetFactor = -0.2;
    p.subScriptScale = 0.5;
    p.superScriptOffsetFactor = 0.6;
    p.superScriptScale = 0.5;
    p.widthFactor = 2.0;
    dqCommon::TextFrameStyleProps frame;
    frame.shape = dqCommon::TextAnnotationFrameShape::Rectangle;
    frame.fillColor = dqCommon::TextAnnotationFillColor{  // ColorDef.green.tbgr
        dqCommon::TextAnnotationFillColor::Source::Rgb, dqCommon::ColorDef::green.getTbgr()};
    frame.borderColor = dqCommon::TextStyleColor{false, dqCommon::ColorDef::red.getTbgr()};
    frame.borderWeight = 2.0;
    p.frame = frame;
    p.margins = dqCommon::TextBlockMargins{1.0, 1.0, 1.0, 1.0};
    dqCommon::TextLeaderStyleProps leader;
    leader.color = dqCommon::TextLeaderStyleProps::Color{  // 0xff007f 数字形态
        dqCommon::TextLeaderStyleProps::Color::Source::Rgb, 0xff007f};
    leader.wantElbow = false;
    leader.elbowLength = 0.5;
    leader.terminatorShape = dqCommon::TerminatorShape::OpenArrow;  // terminatorShapes[0]
    leader.terminatorHeightFactor = 0.5;
    leader.terminatorWidthFactor = 0.5;
    leader.showLeaders = true;
    leader.showTerminators = true;
    leader.showTargetPoint = true;
    leader.targetPointShape = dqCommon::TargetPointShape::Cross;    // targetPointShapes[0]
    leader.targetPointOffsetFactor = 0.0;
    p.leader = leader;
    p.tabInterval = 7.0;
    p.indentation = 0.33;
    dqCommon::ListMarker marker;
    marker.enumerator = dqCommon::ListMarkerEnumerator::Letter;
    marker.terminator = "parenthesis";
    marker.letterCase = "lower";
    p.listMarker = marker;
    p.justification = dqCommon::TextJustification::Center;
    return p;
}

// it("returns defaults if no props provided")
// 参考 === 实例同一性：C++ 值语义——fromJSON(nullptr) 与 defaults 全字段等值，
// fromJSON({}) 为新实例（同值；同一性半边不适用，登记）。
TEST(TextStyleSettings, ReturnsDefaultsIfNoPropsProvided)
{
    EXPECT_TRUE(dqCommon::TextStyleSettings::fromJSON(nullptr).equals(
        dqCommon::TextStyleSettings::defaults()));
    dqCommon::TextStyleSettingsProps const empty;
    EXPECT_TRUE(dqCommon::TextStyleSettings::fromJSON(&empty).equals(
        dqCommon::TextStyleSettings::defaults()));
}

// it("round-trips through JSON") — defaults.toJSON() deep-equals defaultProps 等
// 三腿。deep-equal 经全字段 equals（toJSON 恒写全字段——形状固定，值逐字段
// equals 捕捉）。
TEST(TextStyleSettings, RoundTripsThroughJSON)
{
    auto const defaults = dqCommon::TextStyleSettings::defaults();
    // defaults.toJSON() deep-equal defaultProps
    EXPECT_TRUE(dqCommon::TextStyleSettings(defaults.toJSON()).equals(defaults));
    // fromJSON({}).toJSON() deep-equal defaultProps
    dqCommon::TextStyleSettingsProps const empty;
    auto const fromEmpty = dqCommon::TextStyleSettings::fromJSON(&empty);
    EXPECT_TRUE(dqCommon::TextStyleSettings(fromEmpty.toJSON()).equals(defaults));
    // fromJSON(customProps).toJSON() deep-equal customProps（逐字段核对）
    auto const custom = makeCustomProps();
    auto const settings = dqCommon::TextStyleSettings::fromJSON(&custom);
    auto const out = settings.toJSON();
    EXPECT_TRUE(out.color.has_value() && *out.color == *custom.color);
    EXPECT_TRUE(out.font.has_value() && out.font->name == custom.font->name
                && out.font->type == custom.font->type);
    EXPECT_EQ(out.textHeight, custom.textHeight);
    EXPECT_EQ(out.lineSpacingFactor, custom.lineSpacingFactor);
    EXPECT_EQ(out.paragraphSpacingFactor, custom.paragraphSpacingFactor);
    EXPECT_EQ(out.isBold, custom.isBold);
    EXPECT_EQ(out.isItalic, custom.isItalic);
    EXPECT_EQ(out.isUnderlined, custom.isUnderlined);
    EXPECT_EQ(out.stackedFractionScale, custom.stackedFractionScale);
    EXPECT_EQ(out.stackedFractionType, custom.stackedFractionType);
    EXPECT_EQ(out.subScriptOffsetFactor, custom.subScriptOffsetFactor);
    EXPECT_EQ(out.subScriptScale, custom.subScriptScale);
    EXPECT_EQ(out.superScriptOffsetFactor, custom.superScriptOffsetFactor);
    EXPECT_EQ(out.superScriptScale, custom.superScriptScale);
    EXPECT_EQ(out.widthFactor, custom.widthFactor);
    ASSERT_TRUE(out.frame.has_value());
    EXPECT_TRUE(*out.frame == *custom.frame);
    ASSERT_TRUE(out.margins.has_value());
    EXPECT_TRUE(*out.margins == *custom.margins);
    ASSERT_TRUE(out.leader.has_value());
    EXPECT_TRUE(*out.leader == *custom.leader);
    EXPECT_EQ(out.tabInterval, custom.tabInterval);
    EXPECT_EQ(out.indentation, custom.indentation);
    ASSERT_TRUE(out.listMarker.has_value());
    EXPECT_TRUE(*out.listMarker == *custom.listMarker);
    EXPECT_EQ(out.justification, custom.justification);
}

// it("compares for equality") — 逐字段偏离 defaults 均不等。
TEST(TextStyleSettings, ComparesForEquality)
{
    auto const custom = makeCustomProps();
    dqCommon::TextStyleSettingsProps const empty;
    EXPECT_TRUE(dqCommon::TextStyleSettings::fromJSON(&empty).equals(
        dqCommon::TextStyleSettings::defaults()));

    // Object.keys(customProps) 逐键单字段偏离——每键 fromJSON({key}).equals(defaults)
    // == false。
    std::vector<std::function<void(dqCommon::TextStyleSettingsProps&)>> const mutators = {
        [&](auto& p) { p.color = custom.color; },
        [&](auto& p) { p.font = custom.font; },
        [&](auto& p) { p.textHeight = custom.textHeight; },
        [&](auto& p) { p.lineSpacingFactor = custom.lineSpacingFactor; },
        [&](auto& p) { p.paragraphSpacingFactor = custom.paragraphSpacingFactor; },
        [&](auto& p) { p.isBold = custom.isBold; },
        [&](auto& p) { p.isItalic = custom.isItalic; },
        [&](auto& p) { p.isUnderlined = custom.isUnderlined; },
        [&](auto& p) { p.stackedFractionScale = custom.stackedFractionScale; },
        [&](auto& p) { p.stackedFractionType = custom.stackedFractionType; },
        [&](auto& p) { p.subScriptOffsetFactor = custom.subScriptOffsetFactor; },
        [&](auto& p) { p.subScriptScale = custom.subScriptScale; },
        [&](auto& p) { p.superScriptOffsetFactor = custom.superScriptOffsetFactor; },
        [&](auto& p) { p.superScriptScale = custom.superScriptScale; },
        [&](auto& p) { p.widthFactor = custom.widthFactor; },
        [&](auto& p) { p.leader = custom.leader; },
        [&](auto& p) { p.tabInterval = custom.tabInterval; },
        [&](auto& p) { p.frame = custom.frame; },
        [&](auto& p) { p.margins = custom.margins; },
        [&](auto& p) { p.indentation = custom.indentation; },
        [&](auto& p) { p.listMarker = custom.listMarker; },
        [&](auto& p) { p.justification = custom.justification; },
    };
    for (auto const& mutate : mutators) {
        dqCommon::TextStyleSettingsProps props;
        mutate(props);
        auto const settings = dqCommon::TextStyleSettings::fromJSON(&props);
        EXPECT_FALSE(settings.equals(dqCommon::TextStyleSettings::defaults()));
    }

    EXPECT_TRUE(dqCommon::TextStyleSettings::fromJSON(&custom).equals(
        dqCommon::TextStyleSettings::fromJSON(&custom)));
}

// it("returns validation error messages for invalid values")
TEST(TextStyleSettings, ReturnsValidationErrorsForInvalidValues)
{
    auto const custom = makeCustomProps();
    auto const validSettings = dqCommon::TextStyleSettings::fromJSON(&custom);
    EXPECT_TRUE(validSettings.getValidationErrors().empty());

    dqCommon::TextStyleSettingsProps altered;
    altered.font = dqCommon::FontFamilySelector{dqCommon::FontType::TrueType, ""};
    altered.textHeight = 0.0;
    altered.stackedFractionScale = 0.0;
    auto const invalidSettings = validSettings.clone(&altered);

    auto const errors = invalidSettings.getValidationErrors();
    ASSERT_EQ(errors.size(), 3u);
    EXPECT_EQ(errors[0], "font name must be provided");
    EXPECT_EQ(errors[1], "textHeight must be greater than 0");
    EXPECT_EQ(errors[2], "stackedFractionScale must be greater than 0");
}

}  // namespace
