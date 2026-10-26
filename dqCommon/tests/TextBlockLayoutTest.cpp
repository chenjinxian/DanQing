// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextBlockLayout tests
// Ported from: itwinjs-core core/backend/src/test/annotations/TextBlock.test.ts
//              （布局 describe：document-width/linebreak/word-wrapping 组）+
//              AnnotationTestUtils.ts 的 doLayout/computeTextRangeAsStringLength 桩。
//              日文断行（UAX#14 专属）与 grapheme offsets 组随宿主字形面补齐
//              （EQUIVALENCE 登记——TextBlockLayout.h 文件头）。
#include <gtest/gtest.h>

#include <dqCommon/TextBlockLayout.h>

#include <memory>
#include <utility>

namespace {

using namespace dqCommon;

// Ported from: AnnotationTestUtils.computeTextRangeAsStringLength —
// layout/justification = [0,0, chars.length, textHeight]。
static TextLayoutRanges computeTextRangeAsStringLength(
    ComputeRangesForTextLayoutArgs const& args)
{
    dqGeom::Range2d range(0.0, 0.0, static_cast<double>(args.chars.size()), args.textHeight);
    return TextLayoutRanges{range, range};
}

// Ported from: AnnotationTestUtils.doLayout。
static TextBlockLayout doLayout(TextBlock const& textBlock)
{
    LayoutTextBlockArgs args;
    args.textBlock = &textBlock;
    args.computeTextRange = computeTextRangeAsStringLength;
    args.findFontId = [](std::string const&, std::optional<FontType>) { return FontId{0}; };
    return layoutTextBlock(args);
}

static std::unique_ptr<TextRun> makeTextRun(std::string content)
{
    TextRun::Props p;
    p.content = std::move(content);
    return TextRun::create(&p);
}

// it("produces one line per paragraph if document width <= 0")（:458-505）。
TEST(TextBlockLayout_DocumentWidth, OneLinePerParagraphWhenWidthNonPositive)
{
    double const lineSpacingFactor = 0.5;
    double const paragraphSpacingFactor = 0.25;
    double const textHeight = 1.0;  // TextStyleSettings.defaultProps.textHeight
    TextStyleSettingsProps so;
    so.paragraphSpacingFactor = paragraphSpacingFactor;
    so.lineSpacingFactor = lineSpacingFactor;
    so.textHeight = textHeight;
    TextBlock::Props bp;
    bp.styleOverrides = so;
    auto textBlock = TextBlock::create(&bp);

    for (int i = 0; i < 4; ++i) {
        auto layout = doLayout(*textBlock);
        if (i == 0) {
            EXPECT_TRUE(layout.range.isNull());
        } else {
            ASSERT_EQ(layout.lines.size(), static_cast<size_t>(i));
            EXPECT_DOUBLE_EQ(layout.range.low.x, 0.0);
            EXPECT_DOUBLE_EQ(layout.range.low.y,
                             static_cast<double>(-i - ((i - 1) * (lineSpacingFactor + paragraphSpacingFactor)))
                                 * textHeight);
            EXPECT_DOUBLE_EQ(layout.range.high.x, i * 3.0);
            EXPECT_DOUBLE_EQ(layout.range.high.y, 0.0);
        }

        for (size_t l = 0; l < layout.lines.size(); ++l) {
            auto const& line = layout.lines[l];
            ASSERT_EQ(line.runs().size(), l + 1);
            EXPECT_DOUBLE_EQ(line.range.low.x, 0.0);
            EXPECT_DOUBLE_EQ(line.range.low.y, 0.0);
            EXPECT_DOUBLE_EQ(line.range.high.y, textHeight);
            EXPECT_DOUBLE_EQ(line.range.high.x, 3.0 * (static_cast<double>(l) + 1.0));
            for (auto const& run : line.runs()) {
                EXPECT_EQ(run.charOffset, 0u);
                EXPECT_EQ(run.numChars, 3u);
                EXPECT_DOUBLE_EQ(run.range.low.x, 0.0);
                EXPECT_DOUBLE_EQ(run.range.low.y, 0.0);
                EXPECT_DOUBLE_EQ(run.range.high.x, 3.0);
                EXPECT_DOUBLE_EQ(run.range.high.y, textHeight);
            }
        }

        auto& p = textBlock->appendParagraph();
        for (int j = 0; j <= i; ++j)
            p.getChildren().push_back(makeTextRun("Run"));
    }
}

// it("produces a new line for each LineBreakRun")（:507-531）。
TEST(TextBlockLayout_DocumentWidth, NewLineForEachLineBreakRun)
{
    double const lineSpacingFactor = 0.5;
    double const textHeight = 1.0;
    TextStyleSettingsProps so;
    so.lineSpacingFactor = lineSpacingFactor;
    so.textHeight = textHeight;
    TextBlock::Props bp;
    bp.styleOverrides = so;
    auto textBlock = TextBlock::create(&bp);

    textBlock->appendRun(makeTextRun("abc"));
    textBlock->appendRun(LineBreakRun::create());
    textBlock->appendRun(makeTextRun("def"));
    textBlock->appendRun(makeTextRun("ghi"));
    textBlock->appendRun(LineBreakRun::create());
    textBlock->appendRun(makeTextRun("jkl"));

    auto tb = doLayout(*textBlock);
    ASSERT_EQ(tb.lines.size(), 3u);
    EXPECT_EQ(tb.lines[0].runs().size(), 2u);
    EXPECT_EQ(tb.lines[1].runs().size(), 3u);
    EXPECT_EQ(tb.lines[2].runs().size(), 1u);

    EXPECT_DOUBLE_EQ(tb.range.low.x, 0.0);
    EXPECT_DOUBLE_EQ(tb.range.high.x, 6.0);
    EXPECT_DOUBLE_EQ(tb.range.high.y, 0.0);
    // paragraphSpacingFactor 不作用于 linebreak，lineSpacingFactor 作用。
    EXPECT_DOUBLE_EQ(tb.range.low.y, -(lineSpacingFactor * 2.0 + textHeight * 3.0));
}

// word-wrapping expectLines（:1024-1042）——void 形（ASSERT_* 需 void 返回；
// 参考 helper 返回 layout 供后续断言——移植用例未消费该返回值，登记）。
static void expectLines(std::string const& input, double width,
                        std::vector<std::string> const& expectedLines)
{
    TextStyleSettingsProps so;
    so.paragraphSpacingFactor = 0.0;
    so.lineSpacingFactor = 0.0;
    so.textHeight = 1.0;
    TextBlock::Props bp;
    bp.styleOverrides = so;
    auto textBlock = TextBlock::create(&bp);
    textBlock->width = width;
    auto run = makeTextRun(input);
    TextRun const* rawRun = run.get();
    textBlock->appendRun(std::move(run));

    auto layout = doLayout(*textBlock);
    std::string const content = rawRun->content;
    for (auto const& line : layout.lines)
        for (auto const& r : line.runs()) {
            auto const* src = r.source->toTextRun();
            ASSERT_NE(src, nullptr);
            EXPECT_EQ(src->content, content);
        }

    ASSERT_EQ(layout.lines.size(), expectedLines.size());
    for (size_t i = 0; i < expectedLines.size(); ++i) {
        std::string actual;
        for (auto const& r : layout.lines[i].runs()) {
            auto const* src = r.source->toTextRun();
            actual += src->content.substr(r.charOffset, r.numChars);
        }
        EXPECT_EQ(actual, expectedLines[i]) << "line " << i;
    }
}

// it("splits paragraphs into multiple lines if runs exceed the document width")（:1042）。
TEST(TextBlockLayout_WordWrapping, SplitsParagraphsWhenRunsExceedWidth)
{
    auto textBlock = TextBlock::create();
    textBlock->width = 6.0;
    textBlock->appendRun(makeTextRun("ab"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 1u);
    textBlock->appendRun(makeTextRun("cd"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 1u);

    textBlock->appendRun(makeTextRun("ef"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 1u);
    textBlock->appendRun(makeTextRun("ghi"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 2u);

    textBlock->appendRun(makeTextRun("jklmnop"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 3u);

    textBlock->appendRun(makeTextRun("q"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 4u);
    textBlock->appendRun(makeTextRun("r"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 4u);
    textBlock->appendRun(makeTextRun("stu"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 4u);

    textBlock->appendRun(makeTextRun("vwxyz"));
    EXPECT_EQ(doLayout(*textBlock).lines.size(), 5u);
}

// it("splits a single TextRun at word boundaries if it exceeds the document width")（:1073）。
TEST(TextBlockLayout_WordWrapping, SplitsSingleTextRunAtWordBoundaries)
{
    expectLines("a bc def ghij klmno pqrstu vwxyz", 5.0,
                      {"a bc ", "def ", "ghij ", "klmno ", "pqrstu ", "vwxyz"});

    std::string const fox = "The quick brown fox jumped over the lazy dog";
    expectLines(fox, 50.0, {fox});
    expectLines(fox, 40.0,
                {"The quick brown fox jumped over the ", "lazy dog"});
    expectLines(fox, 30.0,
                {"The quick brown fox jumped ", "over the lazy dog"});
    expectLines(fox, 20.0,
                {"The quick brown fox ", "jumped over the ", "lazy dog"});
    expectLines(fox, 10.0,
                {"The quick ", "brown fox ", "jumped ", "over the ", "lazy dog"});
}

// it("considers consecutive whitespace part of a single 'word'")（:1119）。
TEST(TextBlockLayout_WordWrapping, ConsecutiveWhitespaceIsSingleWord)
{
    expectLines("a b  c   d    e     f      ", 3.0,
                      {"a ", "b  ", "c   ", "d    ", "e     ", "f      "});
}

}  // namespace
