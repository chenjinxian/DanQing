// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextBlock tests
// Ported from: itwinjs-core core/common/src/test/annotations/TextBlock.test.ts
//              describe("TextBlockComponent") setStyle/stringify +
//              describe("TextBlock") appendParagraph/appendRun/adds-items.
//              （FieldRun describe 随 Y-a2 后续提交直移。）
#include <gtest/gtest.h>

#include <dqCommon/TextBlock.h>

#include <memory>
#include <utility>

namespace {

using namespace dqCommon;

// --- 构件助手（参考 makeTextRun/makeFractionRun/makeParagraph/makeList/
//     makeInnerList 的 C++ 形——children 为实例而非 JSON props，§3.4 登记
//     见 TextBlock.h Paragraph::Props 注） ---

static std::unique_ptr<TextBlockComponent> textRun(std::string content = {})
{
    TextRun::Props p;
    p.content = std::move(content);
    return TextRun::create(&p);
}

static std::unique_ptr<TextBlockComponent> textRun(TextStyleSettingsProps so, std::string content = {})
{
    TextRun::Props p;
    p.content = std::move(content);
    p.styleOverrides = std::move(so);
    return TextRun::create(&p);
}

static std::unique_ptr<TextBlockComponent> fractRun(std::string num, std::string den)
{
    FractionRun::Props p;
    p.numerator = std::move(num);
    p.denominator = std::move(den);
    return FractionRun::create(&p);
}

static std::unique_ptr<TextBlockComponent> tabRun() { return TabRun::create(); }
static std::unique_ptr<TextBlockComponent> lineBreakRun() { return LineBreakRun::create(); }

// initializer_list 不能移动 unique_ptr——可变参助手收集子代（push_back 逐个
// 移动；顺序 = 实参序）。
template<typename... Ts>
static std::vector<std::unique_ptr<TextBlockComponent>> runVec(Ts... items)
{
    std::vector<std::unique_ptr<TextBlockComponent>> v;
    (v.push_back(std::move(items)), ...);
    return v;
}

template<typename... Ts>
static std::vector<std::unique_ptr<Paragraph>> paraVec(Ts... items)
{
    std::vector<std::unique_ptr<Paragraph>> v;
    (v.push_back(std::move(items)), ...);
    return v;
}

static std::unique_ptr<Paragraph> paragraph(std::vector<std::unique_ptr<TextBlockComponent>> children = {},
                                            std::optional<TextStyleSettingsProps> so = {})
{
    Paragraph::Props p;
    if (so)
        p.styleOverrides = std::move(*so);
    p.children = std::move(children);
    return Paragraph::create(&p);
}

static std::unique_ptr<List> list(std::vector<std::unique_ptr<Paragraph>> children = {},
                                  std::optional<TextStyleSettingsProps> so = {})
{
    List::Props p;
    if (so)
        p.styleOverrides = std::move(*so);
    p.children = std::move(children);
    return List::create(&p);
}

// Paragraph props 携一个内嵌 List（参考 makeInnerList）。
static std::unique_ptr<Paragraph> innerListParagraph(std::vector<std::unique_ptr<Paragraph>> children,
                                                     std::optional<TextStyleSettingsProps> so = {})
{
    std::vector<std::unique_ptr<TextBlockComponent>> kids;
    kids.push_back(list(std::move(children), std::move(so)));
    return paragraph(std::move(kids));
}

static bool propsEquals(TextStyleSettingsProps const& a, TextStyleSettingsProps const& b)
{
    return TextStyleSettings::fromJSON(&a).equals(TextStyleSettings::fromJSON(&b));
}

// ---------------------------------------------------------------------------
// describe("TextBlockComponent") describe("setStyle")
// ---------------------------------------------------------------------------

// 参考 beforeEach 的块形（:73-84）+ getOverrides 助手（:56-65）。
struct StyleBlock {
    std::unique_ptr<TextBlock> block;

    static StyleBlock make()
    {
        StyleBlock sb;
        TextStyleSettingsProps blockSo;
        blockSo.widthFactor = 1234.0;
        TextBlock::Props bp;
        bp.styleOverrides = blockSo;
        sb.block = TextBlock::create(&bp);

        TextStyleSettingsProps paraSo;
        paraSo.textHeight = 42.0;
        Paragraph::Props pp;
        pp.styleOverrides = paraSo;
        auto& para = sb.block->appendParagraph(&pp);

        TextStyleSettingsProps runSo;
        runSo.font = FontFamilySelector{FontType::TrueType, "Consolas"};
        para.getChildren().push_back(textRun(std::move(runSo)));

        TextStyleSettingsProps listSo;
        ListMarker lm;
        lm.enumerator = "*";
        lm.terminator = "period";
        lm.letterCase = "lower";
        listSo.listMarker = lm;
        auto lst = list({}, listSo);

        TextStyleSettingsProps liSo;
        liSo.textHeight = 21.0;
        auto listPara = paragraph({}, liSo);
        TextStyleSettingsProps listRunSo;
        listRunSo.font = FontFamilySelector{FontType::TrueType, "Verdana"};
        listPara->getChildren().push_back(textRun(std::move(listRunSo)));
        lst->getChildren().push_back(std::move(listPara));
        para.getChildren().push_back(std::move(lst));
        return sb;
    }

    auto& getParagraph() const { return block->getChildren()[0]; }
    List& getList() const { return *getParagraph()->getChildren()[1]->toList(); }
};

// it("has overrides")
TEST(TextBlockComponent_SetStyle, HasOverrides)
{
    auto sb = StyleBlock::make();
    EXPECT_DOUBLE_EQ(sb.block->getStyleOverrides().widthFactor.value(), 1234.0);
    EXPECT_DOUBLE_EQ(sb.getParagraph()->getStyleOverrides().textHeight.value(), 42.0);
    EXPECT_EQ(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font->name, "Consolas");
    EXPECT_EQ(sb.getList().getStyleOverrides().listMarker->enumerator, "*");
    EXPECT_EQ(sb.getList().getStyleOverrides().listMarker->terminator, "period");
    EXPECT_EQ(sb.getList().getStyleOverrides().listMarker->letterCase, "lower");
    EXPECT_DOUBLE_EQ(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.value(), 21.0);
    EXPECT_EQ(sb.getList().getChildren()[0]->getChildren()[0]->getStyleOverrides().font->name, "Verdana");
}

// it("clears children's overrides by default when clearing block overrides")
TEST(TextBlockComponent_SetStyle, ClearBlockClearsChildrenByDefault)
{
    auto sb = StyleBlock::make();
    sb.block->clearStyleOverrides();
    EXPECT_FALSE(sb.block->getStyleOverrides().widthFactor.has_value());
    EXPECT_FALSE(sb.getParagraph()->getStyleOverrides().textHeight.has_value());
    EXPECT_FALSE(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font.has_value());
    EXPECT_FALSE(sb.getList().getStyleOverrides().listMarker.has_value());
    EXPECT_FALSE(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.has_value());
    EXPECT_FALSE(sb.getList().getChildren()[0]->getChildren()[0]->getStyleOverrides().font.has_value());
}

// it("clears children's overrides by default when clearing paragraph overrides")
TEST(TextBlockComponent_SetStyle, ClearParagraphClearsChildrenByDefault)
{
    auto sb = StyleBlock::make();
    sb.getParagraph()->clearStyleOverrides();
    EXPECT_TRUE(sb.block->getStyleOverrides().widthFactor.has_value());
    EXPECT_FALSE(sb.getParagraph()->getStyleOverrides().textHeight.has_value());
    EXPECT_FALSE(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font.has_value());
    EXPECT_FALSE(sb.getList().getStyleOverrides().listMarker.has_value());
    EXPECT_FALSE(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.has_value());
}

// it("clears children's overrides by default when clearing list overrides")
TEST(TextBlockComponent_SetStyle, ClearListClearsChildrenByDefault)
{
    auto sb = StyleBlock::make();
    sb.getList().clearStyleOverrides();
    EXPECT_TRUE(sb.block->getStyleOverrides().widthFactor.has_value());
    EXPECT_TRUE(sb.getParagraph()->getStyleOverrides().textHeight.has_value());
    EXPECT_TRUE(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font.has_value());
    EXPECT_FALSE(sb.getList().getStyleOverrides().listMarker.has_value());
    EXPECT_FALSE(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.has_value());
}

// it("does not clear children's overrides when clearing block overrides if
//    preserveChildrenStyles is true")
TEST(TextBlockComponent_SetStyle, PreserveChildrenOnBlockClear)
{
    auto sb = StyleBlock::make();
    ClearTextStyleOptions opts;
    opts.preserveChildrenOverrides = true;
    sb.block->clearStyleOverrides(&opts);
    EXPECT_FALSE(sb.block->getStyleOverrides().widthFactor.has_value());
    EXPECT_TRUE(sb.getParagraph()->getStyleOverrides().textHeight.has_value());
    EXPECT_TRUE(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font.has_value());
    EXPECT_TRUE(sb.getList().getStyleOverrides().listMarker.has_value());
    EXPECT_TRUE(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.has_value());
    EXPECT_TRUE(sb.getList().getChildren()[0]->getChildren()[0]->getStyleOverrides().font.has_value());
}

// it("does not clear children's overrides when clearing paragraph overrides if
//    preserveChildrenStyles is true")
TEST(TextBlockComponent_SetStyle, PreserveChildrenOnParagraphClear)
{
    auto sb = StyleBlock::make();
    ClearTextStyleOptions opts;
    opts.preserveChildrenOverrides = true;
    sb.getParagraph()->clearStyleOverrides(&opts);
    EXPECT_TRUE(sb.block->getStyleOverrides().widthFactor.has_value());
    EXPECT_FALSE(sb.getParagraph()->getStyleOverrides().textHeight.has_value());
    EXPECT_TRUE(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font.has_value());
    EXPECT_TRUE(sb.getList().getStyleOverrides().listMarker.has_value());
    EXPECT_TRUE(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.has_value());
}

// it("does not clear children's overrides when clearing list overrides if
//    preserveChildrenStyles is true")
TEST(TextBlockComponent_SetStyle, PreserveChildrenOnListClear)
{
    auto sb = StyleBlock::make();
    ClearTextStyleOptions opts;
    opts.preserveChildrenOverrides = true;
    sb.getList().clearStyleOverrides(&opts);
    EXPECT_TRUE(sb.block->getStyleOverrides().widthFactor.has_value());
    EXPECT_TRUE(sb.getParagraph()->getStyleOverrides().textHeight.has_value());
    EXPECT_TRUE(sb.getParagraph()->getChildren()[0]->getStyleOverrides().font.has_value());
    EXPECT_FALSE(sb.getList().getStyleOverrides().listMarker.has_value());
    EXPECT_TRUE(sb.getList().getChildren()[0]->getStyleOverrides().textHeight.has_value());
    EXPECT_TRUE(sb.getList().getChildren()[0]->getChildren()[0]->getStyleOverrides().font.has_value());
}

// it("handles empty text block")
TEST(TextBlockComponent_SetStyle, HandlesEmptyTextBlock)
{
    auto const empty = TextBlock::create();
    EXPECT_FALSE(empty->getStyleOverrides().widthFactor.has_value());
    empty->clearStyleOverrides();  // 不抛（C++ 无异常路径——调用即验证）
    SUCCEED();
}

// it("creates a deep copy of the style overrides")
TEST(TextBlockComponent_SetStyle, CreatesDeepCopyOfStyleOverrides)
{
    auto sb = StyleBlock::make();
    TextStyleSettingsProps original;
    original.widthFactor = 1234.0;
    original.textHeight = 42.0;
    original.font = FontFamilySelector{FontType::TrueType, "Consolas"};
    TextFrameStyleProps frame;
    frame.shape = TextAnnotationFrameShape::Rectangle;
    original.frame = frame;
    sb.block->setStyleOverrides(original);

    original.frame->shape = TextAnnotationFrameShape::Circle;  // 改原件

    // block 持副本——仍为 rectangle。
    EXPECT_TRUE(sb.block->getStyleOverrides().frame.has_value());
    EXPECT_EQ(*sb.block->getStyleOverrides().frame->shape, TextAnnotationFrameShape::Rectangle);
    EXPECT_EQ(*original.frame->shape, TextAnnotationFrameShape::Circle);
}

// ---------------------------------------------------------------------------
// describe("stringify")
// ---------------------------------------------------------------------------

// it("stringifies text runs")
TEST(TextBlockComponent_Stringify, TextRuns)
{
    auto run = TextRun::create();
    EXPECT_EQ(run->stringify(), "");
    run->content = "  with some leading whitespace";
    EXPECT_EQ(run->stringify(), "  with some leading whitespace");
    run->content = "with some trailing whitespace  ";
    EXPECT_EQ(run->stringify(), "with some trailing whitespace  ");
    run->content = "lorem";
    EXPECT_EQ(run->stringify(), "lorem");
}

// it("stringifies fraction runs")
TEST(TextBlockComponent_Stringify, FractionRuns)
{
    auto run = FractionRun::create();
    EXPECT_EQ(run->stringify(), "/");
    TextBlockStringifyOptions fopts;
    fopts.fractionSeparator = "F";
    EXPECT_EQ(run->stringify(&fopts), "F");

    run->numerator = "  ipsum";
    run->denominator = "  dolor";
    EXPECT_EQ(run->stringify(), "  ipsum/  dolor");
    EXPECT_EQ(run->stringify(&fopts), "  ipsumF  dolor");

    run->numerator = "ipsum  ";
    run->denominator = "dolor  ";
    EXPECT_EQ(run->stringify(), "ipsum  /dolor  ");
    EXPECT_EQ(run->stringify(&fopts), "ipsum  Fdolor  ");

    run->numerator = "ipsum";
    run->denominator = "dolor";
    EXPECT_EQ(run->stringify(), "ipsum/dolor");
    EXPECT_EQ(run->stringify(&fopts), "ipsumFdolor");

    run->numerator = "3.14159 is equal to >";
    run->denominator = "the number \xCF\x80!";  // π (U+03C0)
    EXPECT_EQ(run->stringify(), "3.14159 is equal to >/the number \xCF\x80!");
    EXPECT_EQ(run->stringify(&fopts), "3.14159 is equal to >Fthe number \xCF\x80!");
}

// it("stringifies line break runs")
TEST(TextBlockComponent_Stringify, LineBreakRuns)
{
    auto run = LineBreakRun::create();
    EXPECT_EQ(run->stringify(), " ");
    TextBlockStringifyOptions l1;
    l1.lineBreak = "L";
    EXPECT_EQ(run->stringify(&l1), "L");
    TextBlockStringifyOptions l2;
    l2.lineBreak = "\n";
    EXPECT_EQ(run->stringify(&l2), "\n");
}

// it("stringifies tab runs")
TEST(TextBlockComponent_Stringify, TabRuns)
{
    auto run = TabRun::create();
    EXPECT_EQ(run->stringify(), "\t");
    TextBlockStringifyOptions t4;
    t4.tabsAsSpaces = 4;
    EXPECT_EQ(run->stringify(&t4), "    ");
    TextBlockStringifyOptions t7;
    t7.tabsAsSpaces = 7;
    EXPECT_EQ(run->stringify(&t7), "       ");
}

// it("stringifies paragraphs")
TEST(TextBlockComponent_Stringify, Paragraphs)
{
    auto para0 = Paragraph::create();
    EXPECT_EQ(para0->stringify(), "");

    std::vector<std::unique_ptr<TextBlockComponent>> one;
    one.push_back(textRun("lorem"));
    auto para1 = paragraph(std::move(one));
    EXPECT_EQ(para1->stringify(), "lorem");

    std::vector<std::unique_ptr<TextBlockComponent>> many;
    many.push_back(fractRun("1", "\xCF\x80"));
    many.push_back(tabRun());
    many.push_back(textRun(" def   ghi"));
    many.push_back(lineBreakRun());
    many.push_back(textRun("j k l"));
    auto para2 = paragraph(std::move(many));
    EXPECT_EQ(para2->stringify(), "1/\xCF\x80\t def   ghi j k l");
    TextBlockStringifyOptions opts;
    opts.tabsAsSpaces = 4;
    opts.lineBreak = "L";
    opts.fractionSeparator = "F";
    EXPECT_EQ(para2->stringify(&opts), "1F\xCF\x80     def   ghiLj k l");
}

// it("stringifies lists")
TEST(TextBlockComponent_Stringify, Lists)
{
    auto empty = List::create();
    EXPECT_EQ(empty->stringify(), "");

    std::vector<std::unique_ptr<Paragraph>> oneP;
    std::vector<std::unique_ptr<TextBlockComponent>> oneR;
    oneR.push_back(textRun("lorem"));
    oneP.push_back(paragraph(std::move(oneR)));
    auto one = list(std::move(oneP));
    EXPECT_EQ(one->stringify(), "1. lorem");
    TextBlockStringifyOptions mOnly;
    mOnly.listMarkerBreak = "M";
    EXPECT_EQ(one->stringify(&mOnly), "1.Mlorem");

    std::vector<std::unique_ptr<Paragraph>> threeP;
    {
        std::vector<std::unique_ptr<TextBlockComponent>> r1;
        r1.push_back(textRun("lorem"));
        threeP.push_back(paragraph(std::move(r1)));
        std::vector<std::unique_ptr<TextBlockComponent>> r2;
        r2.push_back(fractRun("1", "\xCF\x80"));
        r2.push_back(tabRun());
        r2.push_back(textRun(" def   ghi"));
        r2.push_back(lineBreakRun());
        r2.push_back(textRun("j k l"));
        threeP.push_back(paragraph(std::move(r2)));
        threeP.push_back(paragraph());
    }
    auto many = list(std::move(threeP));
    EXPECT_EQ(many->stringify(), "1. lorem 2. 1/\xCF\x80\t def   ghi j k l 3. ");
    TextBlockStringifyOptions full;
    full.tabsAsSpaces = 4;
    full.lineBreak = "L";
    full.fractionSeparator = "F";
    full.listMarkerBreak = "M";
    full.paragraphBreak = "P";
    EXPECT_EQ(many->stringify(&full), "1.MloremP2.M1F\xCF\x80     def   ghiLj k lP3.M");

    // Unordered list marker
    many->getStyleOverrides().listMarker = ListMarker{"*"};
    EXPECT_EQ(many->stringify(), "* lorem * 1/\xCF\x80\t def   ghi j k l * ");
    EXPECT_EQ(many->stringify(&full), "*MloremP*M1F\xCF\x80     def   ghiLj k lP*M");

    // Alphabetic list marker
    ListMarker lm;
    lm.enumerator = ListMarkerEnumerator::Letter;
    lm.terminator = "parenthesis";
    lm.letterCase = "lower";
    many->getStyleOverrides().listMarker = lm;
    EXPECT_EQ(many->stringify(), "a) lorem b) 1/\xCF\x80\t def   ghi j k l c) ");
    EXPECT_EQ(many->stringify(&full), "a)MloremPb)M1F\xCF\x80     def   ghiLj k lPc)M");
}

// it("stringifies nested lists")
TEST(TextBlockComponent_Stringify, NestedLists)
{
    std::optional<TextStyleSettingsProps> letterSo, romanSo, bulletSo;
    {
        TextStyleSettingsProps s;
        ListMarker lm;
        lm.enumerator = ListMarkerEnumerator::Letter;
        lm.terminator = "period";
        lm.letterCase = "lower";
        s.listMarker = lm;
        letterSo = s;
    }
    {
        TextStyleSettingsProps s;
        ListMarker lm;
        lm.enumerator = ListMarkerEnumerator::RomanNumeral;
        lm.terminator = "period";
        lm.letterCase = "lower";
        s.listMarker = lm;
        romanSo = s;
    }
    {
        TextStyleSettingsProps s;
        ListMarker lm;
        lm.enumerator = ListMarkerEnumerator::Bullet;
        s.listMarker = lm;
        bulletSo = s;
    }

    // 内层 bullet list（Gala 的子表）。
    std::vector<std::unique_ptr<Paragraph>> bulletItems;
    bulletItems.push_back(paragraph(runVec(textRun("Sweet and crisp"))));
    bulletItems.push_back(paragraph(runVec(textRun("From New Zealand"))));
    auto bulletList = list(std::move(bulletItems), bulletSo);

    // roman list：Apples 段含 Gala 段（内嵌 bullet list）+ Fiji + Red Delicious。
    std::vector<std::unique_ptr<TextBlockComponent>> galaRuns;
    galaRuns.push_back(textRun("Gala"));
    galaRuns.push_back(std::move(bulletList));
    std::vector<std::unique_ptr<Paragraph>> romanItems;
    romanItems.push_back(paragraph(std::move(galaRuns)));
    romanItems.push_back(paragraph(runVec(textRun("Fiji"))));
    romanItems.push_back(paragraph(runVec(textRun("Red Delicious"))));
    auto romanList = list(std::move(romanItems), romanSo);

    // letter list：Oranges + Apples（含 roman list）。
    std::vector<std::unique_ptr<TextBlockComponent>> appleRuns;
    appleRuns.push_back(textRun("Apples"));
    appleRuns.push_back(std::move(romanList));
    std::vector<std::unique_ptr<Paragraph>> letterItems;
    letterItems.push_back(paragraph(runVec(textRun("Oranges"))));
    letterItems.push_back(paragraph(std::move(appleRuns)));
    auto letter = list(std::move(letterItems), letterSo);

    EXPECT_EQ(letter->stringify(),
              "a. Oranges b. Apples \ti. Gala \t\t\xE2\x80\xA2 Sweet and crisp"
              " \t\t\xE2\x80\xA2 From New Zealand \tii. Fiji \tiii. Red Delicious");
    TextBlockStringifyOptions opts;
    opts.tabsAsSpaces = 3;
    opts.listMarkerBreak = "M";
    opts.paragraphBreak = "P";
    EXPECT_EQ(letter->stringify(&opts),
              "a.MOrangesPb.MApplesP   i.MGalaP      \xE2\x80\xA2MSweet and crisp"
              "P      \xE2\x80\xA2MFrom New ZealandP   ii.MFijiP   iii.MRed Delicious");
}

// it("stringifies the whole TextBlock")
TEST(TextBlockComponent_Stringify, WholeTextBlock)
{
    std::optional<TextStyleSettingsProps> letterSo, romanSo, bulletSo;
    {
        TextStyleSettingsProps s;
        ListMarker lm;
        lm.enumerator = ListMarkerEnumerator::Letter;
        lm.terminator = "period";
        lm.letterCase = "lower";
        s.listMarker = lm;
        letterSo = s;
    }
    {
        TextStyleSettingsProps s;
        ListMarker lm;
        lm.enumerator = ListMarkerEnumerator::RomanNumeral;
        lm.terminator = "period";
        lm.letterCase = "lower";
        s.listMarker = lm;
        romanSo = s;
    }
    {
        TextStyleSettingsProps s;
        ListMarker lm;
        lm.enumerator = ListMarkerEnumerator::Bullet;
        s.listMarker = lm;
        bulletSo = s;
    }

    std::vector<std::unique_ptr<Paragraph>> bulletItems;
    bulletItems.push_back(paragraph(runVec(textRun("Sweet and crisp"))));
    bulletItems.push_back(paragraph(runVec(textRun("From New Zealand"))));
    auto bulletList = list(std::move(bulletItems), bulletSo);

    std::vector<std::unique_ptr<TextBlockComponent>> galaRuns;
    galaRuns.push_back(textRun("Gala"));
    galaRuns.push_back(std::move(bulletList));
    std::vector<std::unique_ptr<Paragraph>> romanItems;
    romanItems.push_back(paragraph(std::move(galaRuns)));
    romanItems.push_back(paragraph(runVec(textRun("Fiji"))));
    romanItems.push_back(paragraph(runVec(textRun("Red Delicious"))));
    auto romanList = list(std::move(romanItems), romanSo);

    std::vector<std::unique_ptr<TextBlockComponent>> appleRuns;
    appleRuns.push_back(textRun("Apples"));
    appleRuns.push_back(std::move(romanList));
    auto innerPara = paragraph(std::move(appleRuns));

    std::vector<std::unique_ptr<Paragraph>> letterItems;
    letterItems.push_back(paragraph(runVec(textRun("Oranges"))));
    letterItems.push_back(std::move(innerPara));
    auto innerListP = innerListParagraph(std::move(letterItems), letterSo);

    TextBlock::Props bp;
    bp.children.push_back(paragraph(runVec(fractRun("1", "\xCF\x80"), tabRun(),
                                     textRun(" def   ghi"), lineBreakRun(),
                                     textRun("j k l"))));
    bp.children.push_back(std::move(innerListP));
    auto block = TextBlock::create(&bp);

    EXPECT_EQ(block->stringify(),
              "1/\xCF\x80\t def   ghi j k l a. Oranges b. Apples \ti. Gala"
              " \t\t\xE2\x80\xA2 Sweet and crisp \t\t\xE2\x80\xA2 From New Zealand"
              " \tii. Fiji \tiii. Red Delicious");
    TextBlockStringifyOptions opts;
    opts.tabsAsSpaces = 3;
    opts.lineBreak = "L";
    opts.fractionSeparator = "F";
    opts.listMarkerBreak = "M";
    opts.paragraphBreak = "P";
    EXPECT_EQ(block->stringify(&opts),
              "1F\xCF\x80    def   ghiLj k lPa.MOrangesPb.MApplesP   i.MGalaP"
              "      \xE2\x80\xA2MSweet and crispP      \xE2\x80\xA2MFrom New Zealand"
              "P   ii.MFijiP   iii.MRed Delicious");
}

// ---------------------------------------------------------------------------
// describe("TextBlock") — appendParagraph / appendRun / adds items to list
// ---------------------------------------------------------------------------

// it("creates a paragraph with no overrides by default")
TEST(TextBlock_AppendParagraph, CreatesParagraphWithNoOverridesByDefault)
{
    TextStyleSettingsProps tbSo;
    tbSo.textHeight = 42.0;
    TextBlock::Props bp;
    bp.styleOverrides = tbSo;
    auto tb = TextBlock::create(&bp);

    auto& p = tb->appendParagraph();
    EXPECT_FALSE(p.getStyleOverrides().textHeight.has_value());
    auto& p2 = tb->appendParagraph();
    EXPECT_FALSE(p2.getStyleOverrides().textHeight.has_value());
    EXPECT_EQ(tb->getChildren().size(), 2u);
}

// it("uses the overrides of the last paragraph if one exists and seedFromLast is true")
TEST(TextBlock_AppendParagraph, SeedsFromLastParagraph)
{
    TextStyleSettingsProps tbSo;
    tbSo.textHeight = 42.0;
    TextBlock::Props bp;
    bp.styleOverrides = tbSo;
    auto tb = TextBlock::create(&bp);

    Paragraph::Props p1Props;
    TextStyleSettingsProps p1So;
    p1So.isBold = true;
    p1Props.styleOverrides = p1So;
    tb->appendParagraph(&p1Props);
    EXPECT_TRUE(propsEquals(tb->getChildren()[0]->getStyleOverrides(), p1So));

    auto& p2 = tb->appendParagraph(nullptr, true);
    EXPECT_TRUE(propsEquals(p2.getStyleOverrides(), p1So));
}

// it("creates a paragraph with no overrides if none exist even if seedFromLast is true")
TEST(TextBlock_AppendParagraph, SeedsNowhereWhenEmpty)
{
    TextStyleSettingsProps tbSo;
    tbSo.textHeight = 42.0;
    TextBlock::Props bp;
    bp.styleOverrides = tbSo;
    auto tb = TextBlock::create(&bp);
    auto& p1 = tb->appendParagraph(nullptr, true);
    EXPECT_FALSE(p1.getStyleOverrides().isBold.has_value());
    EXPECT_FALSE(p1.getStyleOverrides().textHeight.has_value());
}

// it("appends a paragraph IFF the text block is empty")
TEST(TextBlock_AppendRun, AppendsParagraphIffEmpty)
{
    auto tb = TextBlock::create();
    EXPECT_EQ(tb->getChildren().size(), 0u);

    tb->appendRun(textRun());
    EXPECT_EQ(tb->getChildren().size(), 1u);
    EXPECT_EQ(tb->getChildren()[0]->toParagraph()->getChildren().size(), 1u);

    tb->appendRun(textRun());
    EXPECT_EQ(tb->getChildren().size(), 1u);
    EXPECT_EQ(tb->getChildren()[0]->toParagraph()->getChildren().size(), 2u);
}

// it("adds items to list")
TEST(TextBlock, AddsItemsToList)
{
    std::vector<std::unique_ptr<Paragraph>> outerItems;
    outerItems.push_back(paragraph(runVec(textRun("item 1"), fractRun("1", "\xCF\x80"))));
    outerItems.push_back(paragraph(runVec(textRun("item 2"))));
    {
        std::vector<std::unique_ptr<Paragraph>> subItems;
        subItems.push_back(paragraph(runVec(textRun("sub item a"), fractRun("1", "\xCF\x80"))));
        subItems.push_back(paragraph(runVec(textRun("sub item b"))));
        subItems.push_back(paragraph(runVec(textRun("sub item c"))));
        auto sub = list(std::move(subItems));
        std::vector<std::unique_ptr<TextBlockComponent>> item3;
        item3.push_back(textRun("item 3"));
        item3.push_back(std::move(sub));
        outerItems.push_back(paragraph(std::move(item3)));
    }
    auto listPara = innerListParagraph(std::move(outerItems));

    TextBlock::Props bp;
    bp.children.push_back(std::move(listPara));
    auto tb = TextBlock::create(&bp);
    ASSERT_EQ(tb->getChildren().size(), 1u);
    auto const* para = tb->getChildren()[0]->toParagraph();
    ASSERT_NE(para, nullptr);
    auto const* lst = para->getChildren()[0]->toList();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(lst->getChildren().size(), 3u);

    auto const& li0 = lst->getChildren()[0];
    auto const& li1 = lst->getChildren()[1];
    auto const& li2 = lst->getChildren()[2];

    EXPECT_EQ(li0->getKind(), TextBlockComponent::Kind::Paragraph);
    EXPECT_EQ(li0->toParagraph()->getChildren().size(), 2u);
    EXPECT_EQ(li0->stringify(), "item 11/\xCF\x80");

    EXPECT_EQ(li1->getKind(), TextBlockComponent::Kind::Paragraph);
    EXPECT_EQ(li1->toParagraph()->getChildren().size(), 1u);
    EXPECT_EQ(li1->stringify(), "item 2");

    EXPECT_EQ(li2->getKind(), TextBlockComponent::Kind::Paragraph);
    EXPECT_EQ(li2->toParagraph()->getChildren().size(), 2u);
    EXPECT_EQ(li2->stringify(), "item 3 1. sub item a1/\xCF\x80 2. sub item b 3. sub item c");

    auto const* subList = li2->toParagraph()->getChildren()[1]->toList();
    ASSERT_NE(subList, nullptr);
    ASSERT_EQ(subList->getChildren().size(), 3u);
    EXPECT_EQ(subList->getChildren()[0]->toParagraph()->getChildren().size(), 2u);
    EXPECT_EQ(subList->getChildren()[0]->stringify(), "sub item a1/\xCF\x80");
    EXPECT_EQ(subList->getChildren()[1]->toParagraph()->getChildren().size(), 1u);
    EXPECT_EQ(subList->getChildren()[1]->stringify(), "sub item b");
    EXPECT_EQ(subList->getChildren()[2]->toParagraph()->getChildren().size(), 1u);
    EXPECT_EQ(subList->getChildren()[2]->stringify(), "sub item c");
}

}  // namespace
