// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text block layout engine implementation
// Ported from: itwinjs-core core/backend/src/annotations/TextBlockLayout.ts
#include <dqCommon/TextBlockLayout.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace dqCommon {

// --- Range2d helpers（参考 Range2d 方法在既有 dqGeom::Range2d 上的等价面） ---

namespace {
// Ported from: Range2d.setZero 消费面（LineLayout.computeRanges :618-619）。
void setRangeZero(dqGeom::Range2d& r)
{
    r.low.x = r.low.y = 0.0;
    r.high.x = r.high.y = 0.0;
}

// Ported from: Range2d.cloneTranslated({x,y}) — Range3d.h 既有 CloneTranslated(dx,dy)。
dqGeom::Range2d translated(dqGeom::Range2d const& r, double dx, double dy)
{
    return r.CloneTranslated(dx, dy);
}

// Ported from: scaleRange (TextBlockLayout.ts:183-186)。
void scaleRange(dqGeom::Range2d& range, double scale)
{
    range.low.x *= scale;
    range.low.y *= scale;
    range.high.x *= scale;
    range.high.y *= scale;
}

// Ported from: Geometry.isAlmostEqualNumber(a, b, smallMetricDistance)。
bool isAlmostEqualNumber(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

constexpr double kSmallMetricDistance = 1.0e-6;  // Geometry.smallMetricDistance

// Ported from: split() 的缺省断行实现（EQUIVALENCE——文件头登记）：
// UAX#14 的 ASCII 空白邻接子集——每段 = 词 + 其后空白（下一断点在空白后）。
std::vector<LineBreakSegment> defaultLineBreaks(std::string const& source)
{
    std::vector<LineBreakSegment> segments;
    if (source.empty())
        return segments;
    uint32_t index = 0;
    size_t i = 0;
    while (i < source.size()) {
        // 找下一断点：连续非空白后跟的空白末尾（或串尾）。
        size_t j = i;
        while (j < source.size() && !std::isspace(static_cast<unsigned char>(source[j])))
            ++j;
        while (j < source.size() && std::isspace(static_cast<unsigned char>(source[j])))
            ++j;
        segments.push_back({source.substr(i, j - i), index});
        index += static_cast<uint32_t>(j - i);
        i = j;
    }
    return segments;
}
}  // namespace

// overridesStyle 的 Props 形（TextBlockComponent.overridesStyle :108-110 同式
// ——任一字段在场）。
bool propsOverrideStyle(TextStyleSettingsProps const& o)
{
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

// applyBlockSettings（:158-206）——块级设置应用到目标样式（值语义经
// clone 等价 TS 别名+reassign 形）。
namespace {
TextStyleSettings applyBlockSettingsImpl(TextStyleSettings target,
                                         TextStyleSettings const& source, bool isLeader)
{
    TextStyleSettingsProps const sp = source.toJSON();
    double const lineSpacingFactor = *sp.lineSpacingFactor;
    double const paragraphSpacingFactor = *sp.paragraphSpacingFactor;
    double const widthFactor = *sp.widthFactor;
    TextJustification const justification = *sp.justification;
    TextFrameStyleProps const frame = *sp.frame;
    TextBlockMargins const margins = *sp.margins;
    TextLeaderStyleProps const leader = *sp.leader;

    bool const leaderShouldChange = !isLeader && !target.leaderEquals(leader);

    if (lineSpacingFactor != target.lineSpacingFactor
        || paragraphSpacingFactor != target.paragraphSpacingFactor
        || widthFactor != target.widthFactor || justification != target.justification
        || !target.frameEquals(frame) || !target.marginsEqual(margins) || leaderShouldChange) {
        TextStyleSettingsProps cloneProps;
        cloneProps.lineSpacingFactor = lineSpacingFactor;
        cloneProps.paragraphSpacingFactor = paragraphSpacingFactor;
        cloneProps.widthFactor = widthFactor;
        cloneProps.justification = justification;
        cloneProps.frame = frame;
        cloneProps.margins = margins;
        if (leaderShouldChange)
            cloneProps.leader = leader;
        target = target.clone(&cloneProps);
    }
    return target;
}
}  // namespace

// ---------------------------------------------------------------------------
// TextStyleResolver
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TextStyleResolver ctor (:227-233)
TextStyleResolver::TextStyleResolver(TextBlock const& textBlock, uint64_t textStyleId,
                                     FindTextStyle const& findTextStyle)
{
    // 参考缺省 createFindTextStyleImpl = iModel 查 AnnotationTextStyle，缺席回
    // TextStyleSettings.fromJSON()——注入面缺席时同回退（:53-60）。
    blockSettings = findTextStyle ? findTextStyle(textStyleId) : TextStyleSettings::defaults();

    // if (args.textBlock.styleOverrides) — TS truthiness = 空对象也 true；C++
    // overridesStyle() 语义（空 overrides clone 无效果——等价）。
    if (textBlock.overridesStyle())
        blockSettings = blockSettings.clone(&textBlock.getStyleOverrides());
}

// Ported from: itwinjs-core TextStyleResolver.resolveSettings (:238-245)
TextStyleSettings TextStyleResolver::resolveSettings(TextStyleSettingsProps const& overrides,
                                                     bool isLeader) const
{
    // if (overrides) — TS 空对象 truthiness=true；clone 空对象无效果（每字段
    // ?? 链落 blockSettings 本值——等价）。
    TextStyleSettings settings = propsOverrideStyle(overrides) ? blockSettings.clone(&overrides)
                                                           : blockSettings;
    return applyBlockSettingsImpl(settings, blockSettings, isLeader);
}

// Ported from: itwinjs-core TextStyleResolver.resolveMarkerText (:247-250)
std::string TextStyleResolver::resolveMarkerText(TextStyleSettingsProps const& overrides,
                                                 uint32_t index) const
{
    ListMarker const marker = overrides.listMarker ? *overrides.listMarker
                                                   : blockSettings.listMarker;
    return getMarkerText(marker, index);
}

// Ported from: itwinjs-core TextStyleResolver.resolveIndentation (:253-258)
double TextStyleResolver::resolveIndentation(TextStyleSettingsProps const& styleOverrides,
                                             uint32_t depth) const
{
    TextStyleSettings const overrides = resolveSettings(styleOverrides);
    return overrides.indentation + overrides.tabInterval * static_cast<double>(depth);
}

// ---------------------------------------------------------------------------
// LayoutContext
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core LayoutContext ctor (:282-283)
LayoutContext::LayoutContext(TextStyleResolver resolver,
                             ComputeRangesForTextLayout computeTextRange, FindFontId findFontId,
                             LineBreakFn lineBreaks)
    : m_resolver(resolver)
    , m_computeTextRange(std::move(computeTextRange))
    , m_findFontId(std::move(findFontId))
    , m_lineBreaks(lineBreaks ? std::move(lineBreaks) : defaultLineBreaks)
{
}

// Ported from: itwinjs-core LayoutContext.findFontId (:285-291)
FontId LayoutContext::findFontId(std::string const& name, std::optional<FontType> type) const
{
    auto const it = m_fontIds.find(name);
    if (it != m_fontIds.end())
        return it->second;
    FontId const id = m_findFontId ? m_findFontId(name, type) : 0;
    m_fontIds.emplace(name, id);
    return id;
}

// Ported from: itwinjs-core LayoutContext.computeRangeForText (:293-323)
TextLayoutRanges LayoutContext::computeRangeForText(std::string const& chars,
                                                    TextStyleSettings const& style,
                                                    std::string const& baselineShift) const
{
    if (chars.empty()) {
        return TextLayoutRanges{
            dqGeom::Range2d(0.0, 0.0, 0.0, style.textHeight),
            dqGeom::Range2d(),
        };
    }

    FontId const fontId = findFontId(style.font.name, style.font.type);
    ComputeRangesForTextLayoutArgs args;
    args.chars = chars;
    args.fontId = fontId;
    args.baselineShift = baselineShift;
    args.bold = style.isBold;
    args.italic = style.isItalic;
    args.textHeight = style.textHeight;
    args.widthFactor = m_resolver.blockSettings.widthFactor;

    TextLayoutRanges ranges = m_computeTextRange ? m_computeTextRange(args)
                                                 : TextLayoutRanges{};

    if (baselineShift != BaselineShift::None) {
        bool const isSub = baselineShift == BaselineShift::Subscript;
        double const scale = isSub ? style.subScriptScale : style.superScriptScale;
        double const offsetFactor =
            isSub ? style.subScriptOffsetFactor : style.superScriptOffsetFactor;
        double const offsetY = style.textHeight * offsetFactor;

        scaleRange(ranges.layout, scale);
        ranges.layout = translated(ranges.layout, 0.0, offsetY);

        scaleRange(ranges.justification, scale);
        ranges.justification = translated(ranges.justification, 0.0, offsetY);
    }

    return ranges;
}

// Ported from: itwinjs-core LayoutContext.computeRangeForTextRun (:325-338)
TextLayoutRanges LayoutContext::computeRangeForTextRun(TextStyleSettings const& style,
                                                       TextBlockComponent const& run,
                                                       uint32_t charOffset,
                                                       uint32_t numChars) const
{
    std::string content;
    std::string shift = BaselineShift::None;
    if (auto const* tr = run.toTextRun()) {
        content = tr->content;
        shift = tr->baselineShift;
    } else if (auto const* fr = run.toFieldRun()) {
        content = fr->getCachedContent();
    }
    return computeRangeForText(content.substr(charOffset, numChars), style, shift);
}

// Ported from: itwinjs-core LayoutContext.computeRangeForFractionRun (:340-363)
LayoutContext::FractionRanges LayoutContext::computeRangeForFractionRun(
    TextStyleSettings const& style, FractionRun const& source) const
{
    dqGeom::Range2d numerator =
        computeRangeForText(source.numerator, style, BaselineShift::None).layout;
    scaleRange(numerator, style.stackedFractionScale);

    dqGeom::Range2d denominator =
        computeRangeForText(source.denominator, style, BaselineShift::None).layout;
    scaleRange(denominator, style.stackedFractionScale);

    double const numLen = numerator.XLength();
    double const denomLen = denominator.XLength();
    switch (style.stackedFractionType) {
        case StackedFractionType::Horizontal: {
            if (numLen > denomLen)
                denominator = translated(denominator, (numLen - denomLen) / 2.0, 0.0);
            else
                numerator = translated(numerator, (denomLen - numLen) / 2.0, 0.0);
            numerator = translated(numerator, 0.0, 1.5 * denominator.YLength());
            break;
        }
        case StackedFractionType::Diagonal: {
            numerator = translated(numerator, 0.0, denominator.YLength());
            denominator = translated(denominator, numLen, 0.0);
            break;
        }
    }

    dqGeom::Range2d layout = numerator.clone();
    layout.ExtendRange(denominator);
    return FractionRanges{layout, numerator, denominator};
}

// Ported from: itwinjs-core LayoutContext.computeRangeForTabRun (:365-372)
dqGeom::Range2d LayoutContext::computeRangeForTabRun(TextStyleSettings const& style,
                                                     TabRun const& source,
                                                     double lengthFromLastTab) const
{
    double const interval =
        source.getStyleOverrides().tabInterval ? *source.getStyleOverrides().tabInterval
                                               : style.tabInterval;
    double const tabEndX = interval - std::fmod(lengthFromLastTab, interval);
    dqGeom::Range2d range(0.0, 0.0, 0.0, style.textHeight);
    range.ExtendXY(tabEndX, range.low.y);
    return range;
}

// Ported from: itwinjs-core split() (:378-394) — 断行器注入形。
std::vector<LineBreakSegment> LayoutContext::split(std::string const& source) const
{
    if (source.empty())
        return {};
    return m_lineBreaks(source);
}

// ---------------------------------------------------------------------------
// RunLayout
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core RunLayout.create (:440-471)
RunLayout RunLayout::create(TextBlockComponent& source, LayoutContext& context,
                            TextStyleSettingsProps const& cumulativeOverrides)
{
    RunLayout rl;
    rl.source = &source;
    rl.style = context.textStyleResolver().resolveSettings(cumulativeOverrides);
    rl.fontId = context.findFontId(rl.style.font.name, rl.style.font.type);
    rl.charOffset = 0;
    rl.numChars = 0;

    switch (source.getKind()) {
        case TextBlockComponent::Kind::Field:
        case TextBlockComponent::Kind::Text: {
            std::string const content = source.getKind() == TextBlockComponent::Kind::Text
                                            ? source.toTextRun()->content
                                            : source.toFieldRun()->getCachedContent();
            rl.numChars = static_cast<uint32_t>(content.size());
            TextLayoutRanges const ranges =
                context.computeRangeForTextRun(rl.style, source, rl.charOffset, rl.numChars);
            rl.range = ranges.layout;
            rl.justificationRange = ranges.justification;
            break;
        }
        case TextBlockComponent::Kind::Fraction: {
            rl.numChars = 1;
            auto const ranges = context.computeRangeForFractionRun(
                rl.style, *source.toFractionRun());
            rl.range = ranges.layout;
            rl.numeratorRange = ranges.numerator;
            rl.denominatorRange = ranges.denominator;
            break;
        }
        default: {  // "linebreak" or "tab"（:463-468——零宽 textHeight 高盒）
            rl.range = dqGeom::Range2d(0.0, 0.0, 0.0, rl.style.textHeight);
            break;
        }
    }
    return rl;
}

// Ported from: itwinjs-core RunLayout.stringify (:474-476)
std::string RunLayout::stringify() const
{
    if (source && source->getKind() == TextBlockComponent::Kind::Text)
        return source->toTextRun()->content.substr(charOffset, numChars);
    return source ? source->stringify() : std::string();
}

// Ported from: itwinjs-core RunLayout.cloneForWrap (:518-528)
RunLayout RunLayout::cloneForWrap(TextLayoutRanges const& ranges, uint32_t charOffset_,
                                  uint32_t numChars_) const
{
    RunLayout copy = *this;
    copy.charOffset = charOffset_;
    copy.numChars = numChars_;
    copy.range = ranges.layout;
    copy.justificationRange = ranges.justification;
    return copy;
}

// Ported from: itwinjs-core RunLayout.split (:530-545)
std::vector<RunLayout> RunLayout::split(LayoutContext& context) const
{
    if (!canWrap() || charOffset > 0)
        return {*this};

    std::string const myText =
        source->toTextRun()->content.substr(charOffset, numChars);
    auto const segments = context.split(myText);
    if (segments.size() <= 1)
        return {*this};

    std::vector<RunLayout> out;
    out.reserve(segments.size());
    for (auto const& segment : segments) {
        out.push_back(cloneForWrap(
            context.computeRangeForText(segment.segment, style,
                                        source->toTextRun()->baselineShift),
            segment.index, static_cast<uint32_t>(segment.segment.size())));
    }
    return out;
}

// Ported from: itwinjs-core RunLayout.toResult (:547-565)
RunLayoutResult RunLayout::toResult() const
{
    RunLayoutResult result;
    result.fontId = fontId;
    result.characterOffset = charOffset;
    result.characterCount = numChars;
    result.range = range;
    result.offsetFromLine = offsetFromLine;
    result.textStyle = style.toJSON();
    result.justificationRange = justificationRange;
    result.numeratorRange = numeratorRange;
    result.denominatorRange = denominatorRange;
    return result;
}

// ---------------------------------------------------------------------------
// LineLayout
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core LineLayout ctor (:582-588)
LineLayout::LineLayout(TextBlockComponent const& source_, TextStyleSettingsProps const& style,
                       LayoutContext* context, uint32_t depth_)
    : source(&source_)
    , depth(depth_)
{
    offsetFromDocument = dqGeom::Point2d::From(
        context ? context->textStyleResolver().resolveIndentation(style, depth) : 0.0, 0.0);
}

// Ported from: itwinjs-core LineLayout.stringify (:591-595)
std::string LineLayout::stringify() const
{
    std::string out;
    for (auto const& run : m_runs)
        out += run.stringify();
    return out;
}

// Ported from: itwinjs-core LineLayout.append (:611-614)
void LineLayout::append(RunLayout run)
{
    m_runs.push_back(std::move(run));
    computeRanges();
}

// Ported from: itwinjs-core LineLayout.computeRanges (:616-671)
void LineLayout::computeRanges()
{
    setRangeZero(runRange);
    lengthFromLastTab = 0.0;

    double lineHeight = 0.0;
    RunLayout const* tallestNonFractionRun = nullptr;
    for (auto const& run : m_runs) {
        double const runHeight = run.range.YLength();
        lineHeight = std::max(lineHeight, runHeight);
        bool const isFraction =
            run.source && run.source->getKind() == TextBlockComponent::Kind::Fraction;
        if (!isFraction
            && (!tallestNonFractionRun || runHeight > tallestNonFractionRun->range.YLength()))
            tallestNonFractionRun = &run;
    }

    // The baseline for the line is the bottom of the tallest non-fraction run.
    double baseline = 0.0;
    if (tallestNonFractionRun)
        baseline = (lineHeight + tallestNonFractionRun->range.YLength()) / 2.0;

    for (auto& run : m_runs) {
        double const runHeight = run.range.YLength();
        double yOffset = lineHeight - baseline;

        bool const isFraction =
            run.source && run.source->getKind() == TextBlockComponent::Kind::Fraction;
        if (isFraction) {
            double const denominatorHeight =
                run.denominatorRange ? run.denominatorRange->YLength() : 0.0;
            if (tallestNonFractionRun && run.style.textHeight <= tallestNonFractionRun->style.textHeight)
                yOffset = (lineHeight - baseline) - denominatorHeight / 2.0;
            else
                yOffset = (lineHeight - runHeight) / 2.0;
        }

        run.offsetFromLine = dqGeom::Point2d::From(runRange.high.x, yOffset);

        dqGeom::Range2d const runLayoutRange = translated(run.range, run.offsetFromLine.x,
                                                          run.offsetFromLine.y);
        runRange.ExtendRange(runLayoutRange);

        bool const isLineBreak =
            run.source && run.source->getKind() == TextBlockComponent::Kind::LineBreak;
        if (!isLineBreak) {
            auto const runJustificationRange =
                run.justificationRange
                    ? translated(*run.justificationRange, run.offsetFromLine.x,
                                 run.offsetFromLine.y)
                    : runLayoutRange;
            justificationRange.ExtendRange(runJustificationRange);
        }

        if (run.source && run.source->getKind() == TextBlockComponent::Kind::Tab)
            lengthFromLastTab = 0.0;
        else
            lengthFromLastTab += run.range.XLength();
    }

    range.SetFrom(runRange);

    if (m_marker) {
        double const indentation = range.low.x;
        double const x = indentation - (m_marker->style.tabInterval / 2.0)
                       - m_marker->range.XLength();
        double const runHeight = m_marker->range.YLength();
        m_marker->offsetFromLine = dqGeom::Point2d::From(
            x, (lineHeight - runHeight) / 2.0);  // Center the marker vertically.
        dqGeom::Range2d const markerRange = translated(m_marker->range,
                                                       m_marker->offsetFromLine.x,
                                                       m_marker->offsetFromLine.y);
        range.ExtendRange(markerRange);
    }
}

// Ported from: itwinjs-core LineLayout.toResult (:673-678)
LineLayoutResult LineLayout::toResult() const
{
    LineLayoutResult result;
    for (auto const& run : m_runs)
        result.runs.push_back(run.toResult());
    if (m_marker)
        result.marker = m_marker->toResult();
    result.range = range;
    result.justificationRange = justificationRange;
    result.offsetFromDocument = offsetFromDocument;
    return result;
}

// ---------------------------------------------------------------------------
// TextBlockLayout
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core TextBlockLayout ctor (:717-726)
TextBlockLayout::TextBlockLayout(TextBlock const& source_, LayoutContext& context)
    : source(&source_)
{
    m_context = std::unique_ptr<LayoutContext>(&context);  // 接管调用方 new 的 context
    LayoutContext& ctx = *m_context;
    if (source->width > 0) {
        textRange.low.x = 0.0;
        textRange.high.x = source->width;
    }

    populateLines(ctx);
    justifyLines(ctx);
    applyMargins(ctx.textStyleResolver().blockSettings.toJSON().margins.value(),
                 ctx.textStyleResolver().blockSettings.textHeight);
}

// Ported from: itwinjs-core TextBlockLayout.toResult (:728-732)
TextBlockLayoutResult TextBlockLayout::toResult() const
{
    TextBlockLayoutResult result;
    for (auto const& line : lines)
        result.lines.push_back(line.toResult());
    result.range = range;
    return result;
}

// Ported from: itwinjs-core TextBlockLayout.stringify (:735-737)
std::string TextBlockLayout::stringify() const
{
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0)
            out += "\n";
        out += lines[i].stringify();
    }
    return out;
}

LineLayout& TextBlockLayout::backLine() { return lines.back(); }

// Ported from: itwinjs-core TextBlockLayout.populateLines (:743-761)
void TextBlockLayout::populateLines(LayoutContext& context)
{
    TextBlock const& doc = *source;
    if (doc.getChildren().empty())
        return;

    LineLayout curLine(*doc.getChildren()[0], doc.getChildren()[0]->getStyleOverrides(),
                       &context);
    uint32_t childIndex = 0;
    for (auto const& child : doc.getChildren()) {
        curLine = populateComponent(*child, childIndex++, context, doc.width,
                                    std::move(curLine), &doc, doc.getStyleOverrides());
    }

    if (curLine.runs().size() > 0)
        flushLine(context, std::move(curLine), doc.getStyleOverrides());
}

// Ported from: itwinjs-core TextBlockLayout.populateComponent (:763-839)
LineLayout TextBlockLayout::populateComponent(TextBlockComponent& component,
                                              uint32_t componentIndex, LayoutContext& context,
                                              double docWidth, LineLayout curLine,
                                              TextBlockComponent const* parent,
                                              TextStyleSettingsProps const& cumulativeOverrides,
                                              uint32_t depth)
{
    // cumulativeOverrides = {...cumulativeOverrides, ...component.styleOverrides}
    // —— 逐键 component 胜（spread 合并）。
    TextStyleSettingsProps overrides = cumulativeOverrides;
    if (propsOverrideStyle(component.getStyleOverrides())) {
        auto const& src = component.getStyleOverrides();
#define DQ_MERGE(field) if (src.field.has_value()) overrides.field = src.field;
        DQ_MERGE(color) DQ_MERGE(font) DQ_MERGE(textHeight) DQ_MERGE(lineSpacingFactor)
        DQ_MERGE(paragraphSpacingFactor) DQ_MERGE(isBold) DQ_MERGE(isItalic)
        DQ_MERGE(isUnderlined) DQ_MERGE(stackedFractionScale) DQ_MERGE(stackedFractionType)
        DQ_MERGE(subScriptOffsetFactor) DQ_MERGE(subScriptScale)
        DQ_MERGE(superScriptOffsetFactor) DQ_MERGE(superScriptScale) DQ_MERGE(widthFactor)
        DQ_MERGE(leader) DQ_MERGE(tabInterval) DQ_MERGE(frame) DQ_MERGE(margins)
        DQ_MERGE(indentation) DQ_MERGE(listMarker) DQ_MERGE(justification)
#undef DQ_MERGE
    }

    switch (component.getKind()) {
        case TextBlockComponent::Kind::List: {
            List& list = *component.toList();
            // Flush any runs in the current line before starting the list.
            if (curLine.runs().size() > 0) {
                TextBlockComponent* firstChild = list.getChildren().empty()
                    ? nullptr : list.getChildren()[0].get();
                curLine = flushLine(context, std::move(curLine), overrides, firstChild, true,
                                    depth + 1);
            } else {
                curLine.offsetFromDocument = dqGeom::Point2d::From(
                    context.textStyleResolver().resolveIndentation(overrides, depth + 1),
                    curLine.offsetFromDocument.y);
                curLine.depth = depth + 1;
            }

            // Iterate list items, setting the marker and populating contents.
            uint32_t index = 0;
            for (auto const& child : list.getChildren()) {
                std::string const markerContent =
                    context.textStyleResolver().resolveMarkerText(overrides, index + 1);
                auto markerRun = TextRun::create();
                markerRun->content = markerContent;
                TextRun& markerRef = *markerRun;
                curLine.adoptSource(std::move(markerRun));
                curLine.setMarker(RunLayout::create(markerRef, context, overrides));

                curLine = populateComponent(*child, index, context, docWidth,
                                            std::move(curLine), &list, overrides, depth + 1);
                ++index;
            }

            // Flush the line if a sibling follows.
            TextBlockComponent const* nextSibling = nullptr;
            if (parent) {
                if (auto* p = parent->toParagraph()) {
                    if (componentIndex + 1 < p->getChildren().size())
                        nextSibling = p->getChildren()[componentIndex + 1].get();
                } else if (auto* l = parent->toList()) {
                    if (componentIndex + 1 < l->getChildren().size())
                        nextSibling = l->getChildren()[componentIndex + 1].get();
                } else if (auto* tb = parent->toTextBlock()) {
                    if (componentIndex + 1 < tb->getChildren().size())
                        nextSibling = tb->getChildren()[componentIndex + 1].get();
                }
            }
            if (nextSibling)  // 参考 `if (curLine && nextSibling)`（C++ curLine 恒在）
                curLine = flushLine(context, std::move(curLine), overrides, nextSibling, true,
                                    depth);
            break;
        }
        case TextBlockComponent::Kind::Paragraph: {
            Paragraph& para = *component.toParagraph();
            uint32_t index = 0;
            for (auto const& child : para.getChildren()) {
                curLine = populateComponent(*child, index, context, docWidth,
                                            std::move(curLine), &para, overrides, depth);
                ++index;
            }
            TextBlockComponent const* nextSibling = nullptr;
            if (parent) {
                if (auto* p = parent->toParagraph()) {
                    if (componentIndex + 1 < p->getChildren().size())
                        nextSibling = p->getChildren()[componentIndex + 1].get();
                } else if (auto* l = parent->toList()) {
                    if (componentIndex + 1 < l->getChildren().size())
                        nextSibling = l->getChildren()[componentIndex + 1].get();
                } else if (auto* tb = parent->toTextBlock()) {
                    if (componentIndex + 1 < tb->getChildren().size())
                        nextSibling = tb->getChildren()[componentIndex + 1].get();
                }
            }
            if (nextSibling)
                curLine = flushLine(context, std::move(curLine), overrides, nextSibling, true,
                                    depth);
            break;
        }
        case TextBlockComponent::Kind::Text:
        case TextBlockComponent::Kind::Field: {
            RunLayout layout = RunLayout::create(component, context, overrides);
            if (docWidth > 0) {
                for (auto& piece : layout.split(context))
                    curLine = populateRun(std::move(curLine), std::move(piece), context,
                                          overrides, docWidth);
            } else {
                curLine = populateRun(std::move(curLine), std::move(layout), context, overrides,
                                      docWidth);
            }
            break;
        }
        case TextBlockComponent::Kind::Fraction:
        case TextBlockComponent::Kind::Tab: {
            RunLayout layout = RunLayout::create(component, context, overrides);
            curLine = populateRun(std::move(curLine), std::move(layout), context, overrides,
                                  docWidth);
            break;
        }
        case TextBlockComponent::Kind::LineBreak: {
            RunLayout layout = RunLayout::create(component, context, overrides);
            curLine.append(std::move(layout));
            curLine = flushLine(context, std::move(curLine), overrides, nullptr, false, depth);
            break;
        }
        default:
            break;
    }

    return curLine;
}

// applyTabShift（:396-401）。
namespace {
void applyTabShift(RunLayout& run, LineLayout const& parent, LayoutContext& context)
{
    if (run.source && run.source->getKind() == TextBlockComponent::Kind::Tab) {
        run.range = context.computeRangeForTabRun(
            run.style, *run.source->toTabRun(), parent.lengthFromLastTab);
    }
}
}  // namespace

// Ported from: itwinjs-core TextBlockLayout.populateRun (:841-874)
LineLayout TextBlockLayout::populateRun(LineLayout curLine, RunLayout&& run,
                                        LayoutContext& context,
                                        TextStyleSettingsProps const& cumulativeOverrides,
                                        double docWidth)
{
    applyTabShift(run, curLine, context);

    if (docWidth <= 0) {
        curLine.append(std::move(run));
        return curLine;
    }

    double const runWidth = run.justificationRange ? run.justificationRange->XLength()
                                                   : run.range.XLength();
    double const lineWidth = curLine.runRange.XLength();
    double const newWidth = runWidth + lineWidth + curLine.offsetFromDocument.x;

    if (newWidth < docWidth
        || isAlmostEqualNumber(newWidth, docWidth, kSmallMetricDistance)) {
        curLine.append(std::move(run));
        return curLine;
    }

    if (curLine.runs().empty()) {
        curLine.append(std::move(run));
        curLine = flushLine(context, std::move(curLine), cumulativeOverrides, nullptr, false,
                            curLine.depth);
    } else {
        curLine = flushLine(context, std::move(curLine), cumulativeOverrides, nullptr, false,
                            curLine.depth);
        applyTabShift(run, curLine, context);  // Recompute tab shift.
        curLine.append(std::move(run));
    }
    return curLine;
}

// Ported from: itwinjs-core TextBlockLayout.flushLine (:876-923)
LineLayout TextBlockLayout::flushLine(LayoutContext& context, LineLayout curLine,
                                      TextStyleSettingsProps const& cumulativeOverrides,
                                      TextBlockComponent const* next, bool newParagraph,
                                      uint32_t depth)
{
    TextBlockComponent const* effectiveNext = next ? next : curLine.source;

    // Guarantee each layout line has at least one run.
    if (curLine.runs().empty()) {
        if (lines.empty() || backLine().runs().empty())
            return LineLayout(*effectiveNext, cumulativeOverrides, &context, depth);

        bool const srcIsLineBreak = curLine.source
            && curLine.source->getKind() == TextBlockComponent::Kind::LineBreak;
        if (!srcIsLineBreak) {
            LineLayout newLine(*effectiveNext, cumulativeOverrides, &context, depth);
            newLine.offsetFromDocument =
                dqGeom::Point2d::From(newLine.offsetFromDocument.x,
                                      newLine.offsetFromDocument.y
                                          - context.textStyleResolver().blockSettings.paragraphSpacingFactor
                                                * context.textStyleResolver().blockSettings.textHeight);
            return newLine;
        }

        // 克隆 linebreak run 并追加（curLine.source.clone()——收养保存活）。
        auto runClone = curLine.source->clone();
        TextBlockComponent& cloneRef = *runClone;
        curLine.adoptSource(std::move(runClone));
        curLine.append(RunLayout::create(cloneRef, context, cumulativeOverrides));
    }

    // Line origin is its baseline.
    dqGeom::Point2d lineOffset = curLine.offsetFromDocument;
    lineOffset = dqGeom::Point2d::From(lineOffset.x, lineOffset.y - curLine.range.YLength());

    if (!lines.empty()) {
        lineOffset = dqGeom::Point2d::From(
            lineOffset.x,
            lineOffset.y + backLine().offsetFromDocument.y
                - context.textStyleResolver().blockSettings.lineSpacingFactor
                      * context.textStyleResolver().blockSettings.textHeight);
    }

    curLine.offsetFromDocument = lineOffset;
    textRange.ExtendRange(translated(curLine.range, lineOffset.x, lineOffset.y));

    lines.push_back(std::move(curLine));
    if (newParagraph) {
        LineLayout newLine(*effectiveNext, cumulativeOverrides, &context, depth);
        newLine.offsetFromDocument =
            dqGeom::Point2d::From(newLine.offsetFromDocument.x,
                                  newLine.offsetFromDocument.y
                                      - context.textStyleResolver().blockSettings.paragraphSpacingFactor
                                            * context.textStyleResolver().blockSettings.textHeight);
        return newLine;
    }
    return LineLayout(*effectiveNext, cumulativeOverrides, &context, depth);
}

// Ported from: itwinjs-core TextBlockLayout.justifyLines (:925-953)
void TextBlockLayout::justifyLines(LayoutContext& context)
{
    TextStyleSettings const& block = context.textStyleResolver().blockSettings;
    if (lines.empty() || (lines.size() == 1 && source->width == 0)
        || block.justification == TextJustification::Left)
        return;

    double const docWidth = source->width;
    double minOffset = std::numeric_limits<double>::max();
    for (auto& line : lines) {
        double const lineWidth = line.justificationRange.XLength() + line.offsetFromDocument.x;
        double offset = docWidth - lineWidth;
        if (block.justification == TextJustification::Center)
            offset /= 2.0;
        line.offsetFromDocument = dqGeom::Point2d::From(line.offsetFromDocument.x + offset,
                                                        line.offsetFromDocument.y);
        minOffset = std::min(offset, minOffset);
    }

    if (minOffset < 0) {
        textRange.low.x += minOffset;
        textRange.high.x += minOffset;
    }
}

// Ported from: itwinjs-core TextBlockLayout.applyMargins (:955-960+)
void TextBlockLayout::applyMargins(TextBlockMargins const& margins, double textHeight)
{
    range = textRange.clone();
    if (range.isNull())
        return;

    // Disregard negative margins.
    double const right = margins.right && *margins.right >= 0 ? *margins.right * textHeight : 0.0;
    double const left = margins.left && *margins.left >= 0 ? *margins.left * textHeight : 0.0;
    double const top = margins.top && *margins.top >= 0 ? *margins.top * textHeight : 0.0;
    double const bottom = margins.bottom && *margins.bottom >= 0 ? *margins.bottom * textHeight : 0.0;

    double const xHigh = textRange.high.x + right;
    double const yHigh = textRange.high.y + top;
    double const xLow = textRange.low.x - left;
    double const yLow = textRange.low.y - bottom;

    range.ExtendXY(xHigh, yHigh);
    range.ExtendXY(xLow, yLow);
}

// Ported from: itwinjs-core layoutTextBlock (:95-103)
TextBlockLayout layoutTextBlock(LayoutTextBlockArgs const& args)
{
    // 参考缺省注入面（iModel 原生字形/字体表）不在移植范围——三缝为必选形
    //（文件头 EQUIVALENCE 登记）。context/resolver 所有权归 TextBlockLayout。
    auto* resolver = new TextStyleResolver(*args.textBlock, args.textStyleId, args.findTextStyle);
    auto* context = new LayoutContext(*resolver, args.computeTextRange, args.findFontId,
                                      args.lineBreaks);
    return TextBlockLayout(*args.textBlock, *context);  // ctor 接管 context
}

// Ported from: itwinjs-core computeLayoutTextBlockResult (:110-115)
TextBlockLayoutResult computeLayoutTextBlockResult(LayoutTextBlockArgs const& args)
{
    return layoutTextBlock(args).toResult();
}

}  // namespace dqCommon
