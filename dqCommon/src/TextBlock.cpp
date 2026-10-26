// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextBlock document model implementation
// Ported from: itwinjs-core core/common/src/annotation/TextBlock.ts
#include <dqCommon/TextBlock.h>

#include <cctype>
#include <cstdlib>

namespace dqCommon {

// ---------------------------------------------------------------------------
// TextBlockComponent
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TextBlockComponent.clearStyleOverrides (:103-105)
void TextBlockComponent::clearStyleOverrides(ClearTextStyleOptions const*)
{
    setStyleOverrides(TextStyleSettingsProps{});
}

// Ported from: itwinjs-core TextBlockComponent.overridesStyle (:108-110)
bool TextBlockComponent::overridesStyle() const noexcept
{
    auto const& o = m_styleOverrides;
    return o.color.has_value() || o.font.has_value() || o.textHeight.has_value()
        || o.lineSpacingFactor.has_value() || o.paragraphSpacingFactor.has_value()
        || o.isBold.has_value() || o.isItalic.has_value() || o.isUnderlined.has_value()
        || o.stackedFractionScale.has_value() || o.stackedFractionType.has_value()
        || o.subScriptOffsetFactor.has_value() || o.subScriptScale.has_value()
        || o.superScriptOffsetFactor.has_value() || o.superScriptScale.has_value()
        || o.widthFactor.has_value() || o.leader.has_value() || o.tabInterval.has_value()
        || o.frame.has_value() || o.margins.has_value() || o.indentation.has_value()
        || o.listMarker.has_value() || o.justification.has_value();
}

// Ported from: itwinjs-core TextBlockComponent.isWhitespace (:125-127)
bool TextBlockComponent::isWhitespace() const
{
    std::string const s = stringify();
    for (char const c : s)
        if (!std::isspace(static_cast<unsigned char>(c)))
            return false;
    return true;
}

// Ported from: itwinjs-core TextBlockComponent.toJSON (:130-134)
TextStyleSettingsProps TextBlockComponent::toJSON() const
{
    return m_styleOverrides;  // structuredClone = 值拷贝
}

// Ported from: itwinjs-core TextBlockComponent.equals (:137-141)
bool TextBlockComponent::equals(TextBlockComponent const& other) const
{
    TextStyleSettings const mine = TextStyleSettings::fromJSON(&m_styleOverrides);
    TextStyleSettings const theirs = TextStyleSettings::fromJSON(&other.m_styleOverrides);
    return mine.equals(theirs);
}

// instanceof 类型缝（§3.4——kind 判别后静态下转；-fno-rtti 无 dynamic_cast）。
#define DQ_TEXTBLOCK_TO_IMPL(T, K)                          \
    T* TextBlockComponent::to##T() noexcept                 \
    {                                                       \
        return m_kind == Kind::K ? static_cast<T*>(this) : nullptr; \
    }
DQ_TEXTBLOCK_TO_IMPL(TextRun, Text)
DQ_TEXTBLOCK_TO_IMPL(FractionRun, Fraction)
DQ_TEXTBLOCK_TO_IMPL(TabRun, Tab)
DQ_TEXTBLOCK_TO_IMPL(LineBreakRun, LineBreak)
DQ_TEXTBLOCK_TO_IMPL(FieldRun, Field)
DQ_TEXTBLOCK_TO_IMPL(Paragraph, Paragraph)
DQ_TEXTBLOCK_TO_IMPL(List, List)
DQ_TEXTBLOCK_TO_IMPL(TextBlock, TextBlock)
#undef DQ_TEXTBLOCK_TO_IMPL
#define DQ_TEXTBLOCK_TO_IMPL_CONST(T, K)                         T const* TextBlockComponent::to##T() const noexcept          {                                                                return m_kind == Kind::K ? static_cast<T const*>(this) : nullptr;     }
DQ_TEXTBLOCK_TO_IMPL_CONST(TextRun, Text)
DQ_TEXTBLOCK_TO_IMPL_CONST(FractionRun, Fraction)
DQ_TEXTBLOCK_TO_IMPL_CONST(TabRun, Tab)
DQ_TEXTBLOCK_TO_IMPL_CONST(LineBreakRun, LineBreak)
DQ_TEXTBLOCK_TO_IMPL_CONST(FieldRun, Field)
DQ_TEXTBLOCK_TO_IMPL_CONST(Paragraph, Paragraph)
DQ_TEXTBLOCK_TO_IMPL_CONST(List, List)
DQ_TEXTBLOCK_TO_IMPL_CONST(TextBlock, TextBlock)
#undef DQ_TEXTBLOCK_TO_IMPL_CONST

// ---------------------------------------------------------------------------
// TextRun
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TextRun ctor (:209-213)
TextRun::TextRun(Props const* props)
    : TextBlockComponent(Kind::Text,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
    , content(props ? props->content : std::string())
    , baselineShift(props && props->baselineShift ? *props->baselineShift
                                                  : std::string(BaselineShift::None))
{
}

// Ported from: itwinjs-core TextRun.create (:228-230)
std::unique_ptr<TextRun> TextRun::create(Props const* props)
{
    return std::unique_ptr<TextRun>(new TextRun(props));
}

// Ported from: itwinjs-core TextRun.clone (:215-217) — new TextRun(this.toJSON())
std::unique_ptr<TextBlockComponent> TextRun::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    p.content = content;
    p.baselineShift = baselineShift;
    return create(&p);
}

// Ported from: itwinjs-core TextRun.equals (:241-243)
bool TextRun::equals(TextBlockComponent const& other) const
{
    auto const* o = other.toTextRun();
    return o && content == o->content && baselineShift == o->baselineShift
        && TextBlockComponent::equals(other);
}

// ---------------------------------------------------------------------------
// FractionRun
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core FractionRun ctor (:270-274)
FractionRun::FractionRun(Props const* props)
    : TextBlockComponent(Kind::Fraction,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
    , numerator(props ? props->numerator : std::string())
    , denominator(props ? props->denominator : std::string())
{
}

// Ported from: itwinjs-core FractionRun.create (:289-291)
std::unique_ptr<FractionRun> FractionRun::create(Props const* props)
{
    return std::unique_ptr<FractionRun>(new FractionRun(props));
}

// Ported from: itwinjs-core FractionRun.clone (:285-287)
std::unique_ptr<TextBlockComponent> FractionRun::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    p.numerator = numerator;
    p.denominator = denominator;
    return create(&p);
}

// Ported from: itwinjs-core FractionRun.stringify (:298-301)
std::string FractionRun::stringify(TextBlockStringifyOptions const* options,
                                   TextBlockStringifyContext const*) const
{
    std::string const sep = options && options->fractionSeparator ? *options->fractionSeparator : "/";
    return numerator + sep + denominator;
}

// Ported from: itwinjs-core FractionRun.equals (:303-305)
bool FractionRun::equals(TextBlockComponent const& other) const
{
    auto const* o = other.toFractionRun();
    return o && numerator == o->numerator && denominator == o->denominator
        && TextBlockComponent::equals(other);
}

// ---------------------------------------------------------------------------
// LineBreakRun
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core LineBreakRun ctor (:323-325)
LineBreakRun::LineBreakRun(Props const* props)
    : TextBlockComponent(Kind::LineBreak,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
{
}

// Ported from: itwinjs-core LineBreakRun.create (:334-336)
std::unique_ptr<LineBreakRun> LineBreakRun::create(Props const* props)
{
    return std::unique_ptr<LineBreakRun>(new LineBreakRun(props));
}

// Ported from: itwinjs-core LineBreakRun.clone (:338-340)
std::unique_ptr<TextBlockComponent> LineBreakRun::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    return create(&p);
}

// Ported from: itwinjs-core LineBreakRun.stringify (:347-349)
std::string LineBreakRun::stringify(TextBlockStringifyOptions const* options,
                                    TextBlockStringifyContext const*) const
{
    return options && options->lineBreak ? *options->lineBreak : " ";
}

// Ported from: itwinjs-core LineBreakRun.equals (:351-353)
bool LineBreakRun::equals(TextBlockComponent const& other) const
{
    return other.toLineBreakRun() && TextBlockComponent::equals(other);
}

// ---------------------------------------------------------------------------
// TabRun
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TabRun ctor (:368-370 无 props 字段——super 链)
TabRun::TabRun(Props const* props)
    : TextBlockComponent(Kind::Tab,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
{
}

// Ported from: itwinjs-core TabRun.create (:383-385)
std::unique_ptr<TabRun> TabRun::create(Props const* props)
{
    return std::unique_ptr<TabRun>(new TabRun(props));
}

// Ported from: itwinjs-core TabRun.clone (:379-381)
std::unique_ptr<TextBlockComponent> TabRun::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    return create(&p);
}

// Ported from: itwinjs-core TabRun.stringify (:396-402)
std::string TabRun::stringify(TextBlockStringifyOptions const* options,
                              TextBlockStringifyContext const*) const
{
    if (options && options->tabsAsSpaces && *options->tabsAsSpaces > 0)
        return std::string(*options->tabsAsSpaces, ' ');
    return "\t";
}

// Ported from: itwinjs-core TabRun.equals (:404-406)
bool TabRun::equals(TextBlockComponent const& other) const
{
    return other.toTabRun() && TextBlockComponent::equals(other);
}

// ---------------------------------------------------------------------------
// FieldRun
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core FieldRun ctor (:466-473)
FieldRun::FieldRun(Props const& props)
    : TextBlockComponent(Kind::Field, props.styleOverrides ? &*props.styleOverrides : nullptr)
    , propertyHost(props.propertyHost)
    , propertyPath(props.propertyPath)
    , formatOptions(props.formatOptions)
    , m_cachedContent(props.cachedContent ? *props.cachedContent : invalidContentIndicator)
{
}

// Ported from: itwinjs-core FieldRun.create (:476-484) — host/path/format 深拷贝。
std::unique_ptr<FieldRun> FieldRun::create(Props const& props)
{
    return std::unique_ptr<FieldRun>(new FieldRun(props));
}

// Ported from: itwinjs-core FieldRun.setCachedContent (:462-464)
void FieldRun::setCachedContent(std::string const* content)
{
    m_cachedContent = content ? *content : invalidContentIndicator;
}

// Ported from: itwinjs-core FieldRun.clone (:507-509)
std::unique_ptr<TextBlockComponent> FieldRun::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    p.propertyHost = propertyHost;
    p.propertyPath = propertyPath;
    p.formatOptions = formatOptions;
    // toJSON 语义（:487-504）：cachedContent 恒 invalidContentIndicator 时不写。
    if (m_cachedContent != invalidContentIndicator)
        p.cachedContent = m_cachedContent;
    return create(p);
}

// Ported from: itwinjs-core FieldRun.equals (:521-560)
bool FieldRun::equals(TextBlockComponent const& other) const
{
    auto const* o = other.toFieldRun();
    if (!o || !TextBlockComponent::equals(other))
        return false;

    if (!(propertyHost == o->propertyHost))
        return false;
    if (!(propertyPath == o->propertyPath))
        return false;

    // formatOptions：双在场逐字段比较（参考 JSON.stringify 字符串比较——字段集
    // 固定形态下等价）；单在场不等。
    if (formatOptions && o->formatOptions) {
        if (!(*formatOptions == *o->formatOptions))
            return false;
    } else if (formatOptions || o->formatOptions) {
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Paragraph
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Paragraph ctor (:578-584) — children 逐个构造。
Paragraph::Paragraph(Props const* props)
    : TextBlockComponent(Kind::Paragraph,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
{
    if (props) {
        for (auto const& child : props->children)
            if (child)
                m_children.push_back(child->clone());
    }
}

// Ported from: itwinjs-core Paragraph.create (:587-589)
std::unique_ptr<Paragraph> Paragraph::create(Props const* props)
{
    return std::unique_ptr<Paragraph>(new Paragraph(props));
}

// Ported from: itwinjs-core clearStyleOverrides 自由函数 (:67-74) 经
// Paragraph.clearStyleOverrides (:591-593)。
void Paragraph::clearStyleOverrides(ClearTextStyleOptions const* options)
{
    setStyleOverrides(TextStyleSettingsProps{});
    if (!options || !options->preserveChildrenOverrides)
        for (auto const& child : m_children)
            if (child)
                child->clearStyleOverrides(options);
}

// Ported from: itwinjs-core Paragraph.clone (:599-601)
std::unique_ptr<TextBlockComponent> Paragraph::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    for (auto const& child : m_children)
        if (child)
            p.children.push_back(child->clone());
    return create(&p);
}

// Ported from: itwinjs-core Paragraph.toJSON (:603-608)
TextStyleSettingsProps Paragraph::toJSON() const
{
    return TextBlockComponent::toJSON();  // children 序列化归 JSON 解析层（Props 携实例）
}

// Ported from: itwinjs-core Paragraph.stringify (:611-613)
std::string Paragraph::stringify(TextBlockStringifyOptions const* options,
                                 TextBlockStringifyContext const* context) const
{
    std::string out;
    uint32_t index = 0;
    for (auto const& child : m_children) {
        if (!child)
            continue;
        bool const isList = child->getKind() == Kind::List;
        if (index > 0 && isList) {
            out += options && options->paragraphBreak ? *options->paragraphBreak : " ";
        }
        out += child->stringify(options, context);
        ++index;
    }
    return out;
}

// Ported from: itwinjs-core Paragraph.equals (:615-617)
bool Paragraph::equals(TextBlockComponent const& other) const
{
    return other.toParagraph() && TextBlockComponent::equals(other);
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core List ctor (:636-640)
List::List(Props const* props)
    : TextBlockComponent(Kind::List,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
{
    if (props) {
        for (auto const& child : props->children)
            if (child)
                m_children.push_back(std::unique_ptr<Paragraph>(
                    static_cast<Paragraph*>(child->clone().release())));
    }
}

// Ported from: itwinjs-core List.create (:643-645)
std::unique_ptr<List> List::create(Props const* props)
{
    return std::unique_ptr<List>(new List(props));
}

// Ported from: itwinjs-core clearStyleOverrides 自由函数 (:67-74) 经
// List.clearStyleOverrides (:647-649)。
void List::clearStyleOverrides(ClearTextStyleOptions const* options)
{
    setStyleOverrides(TextStyleSettingsProps{});
    if (!options || !options->preserveChildrenOverrides)
        for (auto const& child : m_children)
            if (child)
                child->clearStyleOverrides(options);
}

// Ported from: itwinjs-core List.clone (:655-657)
std::unique_ptr<TextBlockComponent> List::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    for (auto const& child : m_children)
        if (child)
            p.children.push_back(std::unique_ptr<Paragraph>(
                static_cast<Paragraph*>(child->clone().release())));
    return create(&p);
}

// Ported from: itwinjs-core List.toJSON (:659-665)
TextStyleSettingsProps List::toJSON() const
{
    return TextBlockComponent::toJSON();
}

// Ported from: itwinjs-core List.stringify (:668-676)
std::string List::stringify(TextBlockStringifyOptions const* options,
                            TextBlockStringifyContext const* context) const
{
    uint32_t const depth = context ? context->depth : 0;
    uint32_t index = 0;
    std::string out;
    for (auto const& child : m_children) {
        if (!child)
            continue;
        std::string const marker = getMarkerText(
            getStyleOverrides().listMarker.value_or(TextStyleSettings::defaultProps().listMarker.value()),
            index + 1);
        std::string const tab =
            (options && options->tabsAsSpaces && *options->tabsAsSpaces > 0
                 ? std::string(*options->tabsAsSpaces, ' ')
                 : std::string("\t"));
        std::string tabs;
        for (uint32_t i = 0; i < depth; ++i)
            tabs += tab;
        TextBlockStringifyContext childCtx;
        childCtx.depth = depth + 1;
        if (index > 0)
            out += options && options->paragraphBreak ? *options->paragraphBreak : " ";
        out += tabs + marker + (options && options->listMarkerBreak ? *options->listMarkerBreak : " ")
             + child->stringify(options, &childCtx);
        ++index;
    }
    return out;
}

// Ported from: itwinjs-core List.equals (:678-680)
bool List::equals(TextBlockComponent const& other) const
{
    return other.toList() && TextBlockComponent::equals(other);
}

// ---------------------------------------------------------------------------
// TextBlock
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TextBlock ctor (:709-714)
TextBlock::TextBlock(Props const* props)
    : TextBlockComponent(Kind::TextBlock,
                         props && props->styleOverrides ? &*props->styleOverrides : nullptr)
    , width(props && props->width ? *props->width : 0.0)
{
    if (props) {
        for (auto const& para : props->children)
            if (para)
                m_children.push_back(std::unique_ptr<Paragraph>(
                    static_cast<Paragraph*>(para->clone().release())));
    }
}

// Ported from: itwinjs-core TextBlock.create (:729-731)
std::unique_ptr<TextBlock> TextBlock::create(Props const* props)
{
    return std::unique_ptr<TextBlock>(new TextBlock(props));
}

// Ported from: itwinjs-core clearStyleOverrides 自由函数 (:67-74) 经
// TextBlock.clearStyleOverrides (:716-718)。
void TextBlock::clearStyleOverrides(ClearTextStyleOptions const* options)
{
    setStyleOverrides(TextStyleSettingsProps{});
    if (!options || !options->preserveChildrenOverrides)
        for (auto const& child : m_children)
            if (child)
                child->clearStyleOverrides(options);
}

// Ported from: itwinjs-core TextBlock.toJSON (:720-726) — TextBlockProps 形态。
TextBlock::Props TextBlock::toJSONBlockProps() const
{
    Props p;
    p.styleOverrides = TextBlockComponent::toJSON();
    p.width = width;
    for (auto const& para : m_children)
        if (para)
            p.children.push_back(std::unique_ptr<Paragraph>(
                static_cast<Paragraph*>(para->clone().release())));
    return p;
}

// 组件面（super.toJSON——styleOverrides）。
TextStyleSettingsProps TextBlock::toJSON() const
{
    return TextBlockComponent::toJSON();
}

// Ported from: itwinjs-core TextBlock.isEmpty (:734-736)
bool TextBlock::isEmpty() const
{
    for (auto const& child : m_children)
        if (child && !child->isEmpty())
            return false;
    return true;
}

// Ported from: itwinjs-core TextBlock.clone (:738-740)
std::unique_ptr<TextBlockComponent> TextBlock::clone() const
{
    Props p;
    p.styleOverrides = getStyleOverrides();
    p.width = width;
    for (auto const& para : m_children)
        if (para)
            p.children.push_back(std::unique_ptr<Paragraph>(
                static_cast<Paragraph*>(para->clone().release())));
    return create(&p);
}

// Ported from: itwinjs-core TextBlock.stringify (:743-745)
std::string TextBlock::stringify(TextBlockStringifyOptions const* options,
                                 TextBlockStringifyContext const* context) const
{
    std::string out;
    uint32_t index = 0;
    for (auto const& para : m_children) {
        if (!para)
            continue;
        if (index > 0)
            out += options && options->paragraphBreak ? *options->paragraphBreak : " ";
        out += para->stringify(options, context);
        ++index;
    }
    return out;
}

// Ported from: itwinjs-core TextBlock.appendParagraph (:752-758) —
// styleOverrides = {...seed?.styleOverrides, ...props?.styleOverrides}（props 逐键胜）。
Paragraph& TextBlock::appendParagraph(typename Paragraph::Props const* props, bool seedFromLast)
{
    Paragraph const* seed = nullptr;
    if (seedFromLast && !m_children.empty())
        seed = m_children.back().get();

    Paragraph::Props merged;
    if (seed)
        merged.styleOverrides = seed->getStyleOverrides();
    if (props) {
        if (!merged.styleOverrides)
            merged.styleOverrides = TextStyleSettingsProps{};
        auto& o = *merged.styleOverrides;
        auto const& src = *props;
        o.color = src.styleOverrides ? src.styleOverrides->color : o.color;
        // 逐键覆盖（spread 语义：props 在场键胜，缺席键保 seed 值）。
#define DQ_MERGE(field) o.field = (src.styleOverrides && src.styleOverrides->field.has_value()) \
            ? src.styleOverrides->field : o.field;
        DQ_MERGE(color) DQ_MERGE(font) DQ_MERGE(textHeight) DQ_MERGE(lineSpacingFactor)
        DQ_MERGE(paragraphSpacingFactor) DQ_MERGE(isBold) DQ_MERGE(isItalic)
        DQ_MERGE(isUnderlined) DQ_MERGE(stackedFractionScale) DQ_MERGE(stackedFractionType)
        DQ_MERGE(subScriptOffsetFactor) DQ_MERGE(subScriptScale)
        DQ_MERGE(superScriptOffsetFactor) DQ_MERGE(superScriptScale) DQ_MERGE(widthFactor)
        DQ_MERGE(leader) DQ_MERGE(tabInterval) DQ_MERGE(frame) DQ_MERGE(margins)
        DQ_MERGE(indentation) DQ_MERGE(listMarker) DQ_MERGE(justification)
#undef DQ_MERGE
        merged.children.clear();
        for (auto const& child : src.children)
            if (child)
                merged.children.push_back(child->clone());
    }
    m_children.push_back(Paragraph::create(&merged));
    return *m_children.back();
}

// Ported from: itwinjs-core TextBlock.appendRun (:763-766)
void TextBlock::appendRun(std::unique_ptr<TextBlockComponent> run)
{
    if (!run)
        return;
    if (m_children.empty())
        appendParagraph();
    m_children.back()->toParagraph()->getChildren().push_back(std::move(run));
}

// Ported from: itwinjs-core TextBlock.equals (:768-790)
bool TextBlock::equals(TextBlockComponent const& other) const
{
    auto const* o = other.toTextBlock();
    if (!o)
        return false;
    if (!TextBlockComponent::equals(other))
        return false;
    if (width != o->width)
        return false;
    if (m_children.size() != o->m_children.size())
        return false;
    for (size_t i = 0; i < m_children.size(); ++i)
        if (!m_children[i] || !o->m_children[i] || !m_children[i]->equals(*o->m_children[i]))
            return false;
    return true;
}

// ---------------------------------------------------------------------------
// traverseTextBlockComponent
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core traverseTextBlockComponent (:807-814) —
// 先序 DFS：yield {parent, child} 后递归 list/paragraph。
void traverseTextBlockComponent(
    TextBlockComponent& parent,
    std::function<void(TextBlockComponent&, TextBlockComponent&)> const& visit)
{
    if (auto* p = parent.toParagraph()) {
        auto& children = p->getChildren();
        for (auto const& child : children) {
            if (!child)
                continue;
            visit(parent, *child);
            if (child->getKind() == TextBlockComponent::Kind::List
                    || child->getKind() == TextBlockComponent::Kind::Paragraph)
                traverseTextBlockComponent(*child, visit);
        }
        return;
    }
    if (auto* l = parent.toList()) {
        auto& children = l->getChildren();
        for (auto const& child : children) {
            if (!child)
                continue;
            visit(parent, *child);
            if (child->getKind() == TextBlockComponent::Kind::List
                    || child->getKind() == TextBlockComponent::Kind::Paragraph)
                traverseTextBlockComponent(*child, visit);
        }
        return;
    }
    if (auto* tb = parent.toTextBlock()) {
        auto& children = tb->getChildren();
        for (auto const& child : children) {
            if (!child)
                continue;
            visit(parent, *child);
            if (child->getKind() == TextBlockComponent::Kind::List
                    || child->getKind() == TextBlockComponent::Kind::Paragraph)
                traverseTextBlockComponent(*child, visit);
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// getMarkerText / integerToRoman / integerToAlpha
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core integerToRoman (:858-872)
static std::string integerToRoman(uint32_t num)
{
    static constexpr uint32_t values[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static constexpr char const* symbols[] = {"M", "CM", "D", "CD", "C", "XC",
                                              "L", "XL", "X", "IX", "V", "IV", "I"};
    std::string roman;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        while (num >= values[i]) {
            roman += symbols[i];
            num -= values[i];
        }
    }
    return roman;
}

// Ported from: itwinjs-core integerToAlpha (:880-885)
static std::string integerToAlpha(uint32_t num)
{
    uint32_t const letterOffset = (num - 1) % 26;
    char const letter = static_cast<char>(65 + letterOffset);
    uint32_t const depth = (num + 25) / 26;  // Math.ceil(num / 26)
    return std::string(depth, letter);
}

// Ported from: itwinjs-core getMarkerText (:824-850)
std::string getMarkerText(ListMarker const& marker, uint32_t num)
{
    std::string markerString;
    if (marker.enumerator == ListMarkerEnumerator::Letter) {
        markerString = integerToAlpha(num);
    } else if (marker.enumerator == ListMarkerEnumerator::RomanNumeral) {
        markerString = integerToRoman(num);
    } else if (marker.enumerator == ListMarkerEnumerator::Number || marker.enumerator.empty()) {
        // `${num}` — Number 枚举与 undefined 缺省臂同为十进制串。
        markerString = std::to_string(num);
    } else {
        markerString = marker.enumerator;  // 任意自定义枚举串
    }

    if (marker.letterCase) {
        auto const mapCase = [&](std::string const& in, int (*fn)(int)) {
            std::string out = in;
            for (auto& c : out)
                c = static_cast<char>(fn(static_cast<unsigned char>(c)));
            return out;
        };
        markerString = *marker.letterCase == "upper" ? mapCase(markerString, std::toupper)
                                                     : mapCase(markerString, std::tolower);
    }

    std::string const terminator = marker.terminator
        ? (*marker.terminator == "period" ? "." : (*marker.terminator == "parenthesis" ? ")" : ""))
        : "";
    return markerString + terminator;
}

}  // namespace dqCommon
