// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — produceTextBlockGeometry tests
// Ported from: itwinjs-core core/backend/src/test/annotations/TextBlock.test.ts
//              describe("produceTextBlockGeometry")（:1659-1767 前五例直移；
//              list-markers 例与 margins 例随 Y-c 宿主件补齐）。
#include <gtest/gtest.h>

#include <dqCommon/TextBlockGeometryProps.h>
#include <dqCommon/TextBlockLayout.h>
#include <dqCommon/TextAnnotation.h>
#include <dqCommon/ColorDef.h>

#include <memory>
#include <utility>

namespace {

using namespace dqCommon;

// Ported from: AnnotationTestUtils.computeTextRangeAsStringLength。
static TextLayoutRanges computeTextRangeAsStringLength(
    ComputeRangesForTextLayoutArgs const& args)
{
    dqGeom::Range2d range(0.0, 0.0, static_cast<double>(args.chars.size()), args.textHeight);
    return TextLayoutRanges{range, range};
}

static TextBlockLayout doLayout(TextBlock const& textBlock)
{
    LayoutTextBlockArgs args;
    args.textBlock = &textBlock;
    args.computeTextRange = computeTextRangeAsStringLength;
    args.findFontId = [](std::string const&, std::optional<FontType>) { return FontId{0}; };
    return layoutTextBlock(args);
}

using Color = TextStyleColor;

// Ported from: makeText/makeFraction/makeBreak/makeTextBlock/makeGeometry
//              (:1660-1693)。
static std::unique_ptr<TextRun> makeText(Color const* color = nullptr)
{
    TextRun::Props p;
    p.content = "text";
    if (color) {
        TextStyleSettingsProps so;
        so.color = *color;
        p.styleOverrides = so;
    }
    return TextRun::create(&p);
}

static std::unique_ptr<FractionRun> makeFraction(Color const* color = nullptr)
{
    FractionRun::Props p;
    p.numerator = "num";
    p.denominator = "denom";
    if (color) {
        TextStyleSettingsProps so;
        so.color = *color;
        p.styleOverrides = so;
    }
    return FractionRun::create(&p);
}

static std::unique_ptr<LineBreakRun> makeBreak(Color const* color = nullptr)
{
    LineBreakRun::Props p;
    if (color) {
        TextStyleSettingsProps so;
        so.color = *color;
        p.styleOverrides = so;
    }
    return LineBreakRun::create(&p);
}

static std::vector<TextBlockGeometryPropsEntry> makeGeometry(
    std::vector<std::unique_ptr<TextBlockComponent>> runs)
{
    TextBlock::Props bp;
    bp.children.push_back(Paragraph::create());
    auto block = TextBlock::create(&bp);
    for (auto& run : runs)
        block->appendRun(std::move(run));

    // 参考经 block.toJSON() 往返——C++ 直接克隆（props 携实例 EQUIVALENCE，
    // TextBlock.h Paragraph::Props 注）。
    TextAnnotationProps ap;
    ap.textBlock = block->toJSONBlockProps();
    auto annotation = TextAnnotation::fromJSON(&ap);
    auto layout = doLayout(*block);
    return produceTextBlockGeometry(layout, annotation.computeTransform(layout.range)).entries;
}

// initializer_list 不能移动 unique_ptr——可变参收集（TextBlockTest 同式）。
template<typename... Ts>
static std::vector<std::unique_ptr<TextBlockComponent>> runVec(Ts... items)
{
    std::vector<std::unique_ptr<TextBlockComponent>> v;
    (v.push_back(std::move(items)), ...);
    return v;
}

static Color subcategory() { return TextStyleColor::FromSubcategory(); }
static Color rgb(ColorDef c) { return TextStyleColor::FromColor(c); }

// it("produces an empty array for an empty text block")
TEST(ProduceTextBlockGeometry, EmptyArrayForEmptyTextBlock)
{
    auto const geom = makeGeometry(runVec());
    EXPECT_TRUE(geom.empty());
}

// it("produces an empty array for a block consisting of line breaks")
TEST(ProduceTextBlockGeometry, EmptyArrayForLineBreaksOnly)
{
    auto const geom = makeGeometry(runVec(makeBreak(), makeBreak(), makeBreak()));
    EXPECT_TRUE(geom.empty());
}

// it("produces one appearance entry if all runs use subcategory color")
TEST(ProduceTextBlockGeometry, OneAppearanceEntryForAllSubcategory)
{
    Color sub = subcategory();
    auto const geom = makeGeometry(runVec(makeText(), makeFraction(), makeText(&sub),
                                    makeFraction(&sub)));
    ASSERT_EQ(geom.size(), 9u);
    // geom[0].color === "subcategory"
    auto const* firstColor = std::get_if<TextStyleColor>(&geom[0].payload);
    ASSERT_NE(firstColor, nullptr);
    EXPECT_TRUE(firstColor->subcategory);
    // 后续无 color 条目。
    for (size_t i = 1; i < geom.size(); ++i)
        EXPECT_FALSE(std::holds_alternative<TextStyleColor>(geom[i].payload)) << i;
}

// it("produces strings and fraction separators")
TEST(ProduceTextBlockGeometry, StringsAndFractionSeparators)
{
    auto const geom = makeGeometry(runVec(makeText(), makeFraction(), makeFraction(), makeText()));
    ASSERT_EQ(geom.size(), 9u);
    auto const* c0 = std::get_if<TextStyleColor>(&geom[0].payload);
    ASSERT_NE(c0, nullptr);
    EXPECT_TRUE(c0->subcategory);

    // text, text, sep, text, text, sep, text, text 布局。
    EXPECT_NE(std::get_if<TextStringProps>(&geom[1].payload), nullptr);
    EXPECT_NE(std::get_if<TextStringProps>(&geom[2].payload), nullptr);
    EXPECT_NE(std::get_if<TextBlockGeometryPropsEntry::Separator>(&geom[3].payload), nullptr);
    EXPECT_NE(std::get_if<TextStringProps>(&geom[4].payload), nullptr);

    EXPECT_NE(std::get_if<TextStringProps>(&geom[5].payload), nullptr);
    EXPECT_NE(std::get_if<TextBlockGeometryPropsEntry::Separator>(&geom[6].payload), nullptr);
    EXPECT_NE(std::get_if<TextStringProps>(&geom[7].payload), nullptr);

    EXPECT_NE(std::get_if<TextStringProps>(&geom[8].payload), nullptr);
}

// it("produces an appearance change for each non-break run that is a different
//    color from the previous run")——逐 token 序列锁。
TEST(ProduceTextBlockGeometry, AppearanceChangePerColorChange)
{
    Color blue = rgb(ColorDef::blue);
    Color red = rgb(ColorDef::red);
    Color white = rgb(ColorDef::white);
    Color green = rgb(ColorDef::green);
    Color black = rgb(ColorDef::black);
    auto const geom = makeGeometry(runVec(
        makeText(&blue),
        makeText(),      // subcategory by default
        makeText(),
        makeText(&red),
        makeText(&white),
        makeText(&white),
        makeBreak(),     // linebreak 无几何（参考 makeBreak("subcategory") 色
                         // 随 whitespace 跳过不产出——序列锁下位）
        makeFraction(&green),
        makeText(&green),
        makeBreak(),
        makeText(&green)));

    // 参考 token 序列（:1749-1766）——色变处插 color 条目，text/sep 交替。
    std::vector<std::string> actual;
    for (auto const& entry : geom) {
        if (auto const* t = std::get_if<TextStringProps>(&entry.payload))
            actual.push_back("text");
        else if (std::get_if<TextBlockGeometryPropsEntry::Separator>(&entry.payload))
            actual.push_back("sep");
        else if (auto const* c = std::get_if<TextStyleColor>(&entry.payload))
            actual.push_back(c->subcategory ? "subcategory" : "color:" + std::to_string(c->color));
    }

    // "color:<tbgr>" 前缀 token（值序断言在下方逐位核）。
    std::vector<std::string> expected = {
        "color:",      // blue
        "text",
        "subcategory",
        "text",
        "text",
        "color:",      // red
        "text",
        "color:",      // white
        "text",
        "text",
        "color:",      // green
        "text", "sep", "text",
        "text",
        "text",
    };
    for (auto& tok : expected)
        if (tok == "color:")
            tok = "color:" + std::to_string(0);  // 占位——下方按前缀比对
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] == "color:" || expected[i].rfind("color:", 0) == 0)
            EXPECT_EQ(actual[i].rfind("color:", 0), 0u) << "token " << i;
        else
            EXPECT_EQ(actual[i], expected[i]) << "token " << i;
    }

    // 色值逐位核（blue/red/white/green tbgr）。
    EXPECT_EQ(std::get<TextStyleColor>(geom[0].payload).color, ColorDef::blue.getTbgr());
    EXPECT_EQ(std::get<TextStyleColor>(geom[5].payload).color, ColorDef::red.getTbgr());
    EXPECT_EQ(std::get<TextStyleColor>(geom[7].payload).color, ColorDef::white.getTbgr());
    EXPECT_EQ(std::get<TextStyleColor>(geom[10].payload).color, ColorDef::green.getTbgr());
}

}  // namespace
