// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text block layout engine
// Ported from: itwinjs-core core/backend/src/annotations/TextBlockLayout.ts
//
// 布局引擎为纯 CPU 逻辑（dqCommon 宿主——RenderSchedule M-U 先例）。参考的两个
// 原生依赖面以注入缝承载（参考 @internal "chiefly for tests" 注入形 1:1）：
// - ComputeRangesForTextLayout：字形度量（参考缺省 = IModelJsNative
//   DgnDb.computeRangesForText——imodel-native 字形引擎，不在移植范围；宿主侧
//   Y-c 以系统字体实现，IGlyphGeometryProvider 同型 DI）。
// - FindFontId（参考缺省 = IModelDb.fontMap.findId）。
// - LineBreakFn：断行段切分（参考 split() 直用 npm linebreak = Unicode UAX#14；
//   DanQing 缺省 = 空白邻接断行 ASCII 子集，EQUIVALENCE 登记——发散=非 ASCII/
//   无空格文本的断行机会集不同，验证法 = TextBlockLayoutTest 直移注入桩）。
// TODO: UAX#14 断行随宿主字形面落地复核。
#pragma once

#include "DqCommon.h"
#include "TextBlock.h"
#include "TextBlockLayoutResult.h"
#include "TextStyle.h"

#include <dqGeom/Point2d.h>
#include <dqGeom/Range3d.h>  // dqGeom::Range2d 宿主

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// The layout and justification ranges of a string of text.
// Ported from: itwinjs-core TextLayoutRanges (TextBlockLayout.ts:17-20)
struct TextLayoutRanges {
    dqGeom::Range2d layout;
    dqGeom::Range2d justification;
};

// Arguments to ComputeRangesForTextLayout.
// Ported from: itwinjs-core ComputeRangesForTextLayoutArgs (:25-33)。
// baselineShift 以 string 承载（BaselineShift 值域，TextBlock.h）。
struct ComputeRangesForTextLayoutArgs {
    std::string chars;
    bool bold = false;
    bool italic = false;
    std::string baselineShift;  // BaselineShift 值域
    FontId fontId = 0;
    double widthFactor = 1.0;
    double textHeight = 1.0;
};

// A function that uses a font to compute the layout and justification ranges of
// a string of text.
// Ported from: itwinjs-core ComputeRangesForTextLayout (:37-38)
using ComputeRangesForTextLayout =
    std::function<TextLayoutRanges(ComputeRangesForTextLayoutArgs const&)>;

// A function that looks up the font Id for a font family name (+ optional type).
// Ported from: itwinjs-core FindFontId (:41-42)
using FindFontId = std::function<FontId(std::string const& name, std::optional<FontType> type)>;

// Looks up a named text style.（参考 FindTextStyle :45——id 为
// AnnotationTextStyle 元素 Id；DanQing 以 uint64 元素 Id 承载。）
// Ported from: itwinjs-core FindTextStyle (TextBlockLayout.ts:45)
using FindTextStyle = std::function<TextStyleSettings(uint64_t textStyleId)>;

// A function splitting a string into line-break segments [{segment, index}]——
// 参考 split() 的 linebreak 库注入等价形（见文件头 EQUIVALENCE 注）。
struct LineBreakSegment {
    std::string segment;
    uint32_t index = 0;
};
using LineBreakFn = std::function<std::vector<LineBreakSegment>(std::string const&)>;

// Arguments to layoutTextBlock/computeLayoutTextBlockResult。
// Ported from: itwinjs-core LayoutTextArgs/LayoutTextBlockArgs (:68-91) —
// iModel 面并入 findTextStyle/computeTextRange/findFontId 三注入缝（缺省原生
// 面不在移植范围——文件头登记）。
struct LayoutTextBlockArgs {
    TextBlock const* textBlock = nullptr;
    // The text style resolver seed（参考 TextStyleResolverArgs.textStyleId + iModel
    // ——以 findTextStyle 注入）。
    uint64_t textStyleId = 0;
    FindTextStyle findTextStyle;
    ComputeRangesForTextLayout computeTextRange;
    FindFontId findFontId;
    LineBreakFn lineBreaks;
};

// Resolves the effective style of components/leaders, taking into account
// overrides of the instance and its parent(s).
// Ported from: itwinjs-core TextStyleResolver (TextBlockLayout.ts:221-275)
class DQ_COMMON_EXPORT TextStyleResolver {
public:
    // The resolved style of the TextBlock.
    TextStyleSettings blockSettings;

    // Ported from: itwinjs-core TextStyleResolver ctor (:227-233) —
    // blockSettings = findTextStyle(id)，再叠 textBlock.styleOverrides。
    TextStyleResolver(TextBlock const& textBlock, uint64_t textStyleId,
                      FindTextStyle const& findTextStyle);

    // Ported from: itwinjs-core TextStyleResolver.resolveSettings (:238-245)
    TextStyleSettings resolveSettings(TextStyleSettingsProps const& overrides,
                                      bool isLeader = false) const;

    // Ported from: itwinjs-core TextStyleResolver.resolveMarkerText (:247-250)
    std::string resolveMarkerText(TextStyleSettingsProps const& overrides, uint32_t index) const;

    // Ported from: itwinjs-core TextStyleResolver.resolveIndentation (:253-258)
    double resolveIndentation(TextStyleSettingsProps const& styleOverrides, uint32_t depth) const;
};

class RunLayout;
class LineLayout;

// Layout context: style resolver + glyph metrics + font-id cache + line breaks.
// Ported from: itwinjs-core LayoutContext (TextBlockLayout.ts:278-371)
class DQ_COMMON_EXPORT LayoutContext {
public:
    // Ported from: itwinjs-core LayoutContext ctor (:282-283) —
    // resolver 值持有（TS 引用语义的 C++ 所有权适配——TextBlockLayout 持
    // unique_ptr<LayoutContext>，resolver 随 context 存活）。
    LayoutContext(TextStyleResolver resolver, ComputeRangesForTextLayout computeTextRange,
                  FindFontId findFontId, LineBreakFn lineBreaks);

    // Ported from: itwinjs-core LayoutContext.findFontId (:285-291) —
    // 名称缓存（Map<string, FontId>）。
    FontId findFontId(std::string const& name, std::optional<FontType> type) const;

    // Ported from: itwinjs-core LayoutContext.computeRangeForText (:293-323) —
    // 空串 = 零宽 layout + 空 justification；非 none baselineShift → 缩放+平移。
    TextLayoutRanges computeRangeForText(std::string const& chars, TextStyleSettings const& style,
                                         std::string const& baselineShift) const;

    // Ported from: itwinjs-core LayoutContext.computeRangeForTextRun (:325-338)
    TextLayoutRanges computeRangeForTextRun(TextStyleSettings const& style,
                                            TextBlockComponent const& run, uint32_t charOffset,
                                            uint32_t numChars) const;

    // Ported from: itwinjs-core LayoutContext.computeRangeForFractionRun (:340-363)
    struct FractionRanges {
        dqGeom::Range2d layout;
        dqGeom::Range2d numerator;
        dqGeom::Range2d denominator;
    };
    FractionRanges computeRangeForFractionRun(TextStyleSettings const& style,
                                              FractionRun const& source) const;

    // Ported from: itwinjs-core LayoutContext.computeRangeForTabRun (:365-372)
    dqGeom::Range2d computeRangeForTabRun(TextStyleSettings const& style, TabRun const& source,
                                          double lengthFromLastTab) const;

    // split() 断行段切分（:378-394 的注入形——文件头 EQUIVALENCE 登记）。
    std::vector<LineBreakSegment> split(std::string const& source) const;

    TextStyleResolver const& textStyleResolver() const noexcept { return m_resolver; }

private:
    TextStyleResolver m_resolver;
    ComputeRangesForTextLayout m_computeTextRange;
    FindFontId m_findFontId;
    LineBreakFn m_lineBreaks;
    mutable std::map<std::string, FontId> m_fontIds;
};

// The layout of a single run within a line of text.
// Ported from: itwinjs-core RunLayout (TextBlockLayout.ts:400-542)
class DQ_COMMON_EXPORT RunLayout {
public:
    TextBlockComponent const* source = nullptr;
    uint32_t charOffset = 0;
    uint32_t numChars = 0;
    dqGeom::Range2d range;
    std::optional<dqGeom::Range2d> justificationRange;
    std::optional<dqGeom::Range2d> denominatorRange;
    std::optional<dqGeom::Range2d> numeratorRange;
    dqGeom::Point2d offsetFromLine;  // WritableXAndY {x,y}
    TextStyleSettings style;
    FontId fontId = 0;

    // Ported from: itwinjs-core RunLayout.create (:440-471)
    static RunLayout create(TextBlockComponent& source, LayoutContext& context,
                            TextStyleSettingsProps const& cumulativeOverrides);

    // Debug string（text 臂子串，其余 source.stringify）。
    std::string stringify() const;

    // Ported from: itwinjs-core RunLayout.canWrap (:514-516) — text 臂。
    bool canWrap() const noexcept { return source && source->getKind() == TextBlockComponent::Kind::Text; }

    // Ported from: itwinjs-core RunLayout.split (:525-545) — word-wrap 段切分。
    std::vector<RunLayout> split(LayoutContext& context) const;

    // Ported from: itwinjs-core RunLayout.toResult (:547-565)
    RunLayoutResult toResult() const;

private:
    RunLayout cloneForWrap(TextLayoutRanges const& ranges, uint32_t charOffset,
                           uint32_t numChars) const;
};

// The layout of a single line within a paragraph of a text block.
// Ported from: itwinjs-core LineLayout (TextBlockLayout.ts:551-678)
class DQ_COMMON_EXPORT LineLayout {
public:
    TextBlockComponent const* source = nullptr;
    dqGeom::Range2d range{0.0, 0.0, 0.0, 0.0};               // 含 marker
    dqGeom::Range2d runRange{0.0, 0.0, 0.0, 0.0};            // 不含 marker
    dqGeom::Range2d justificationRange{0.0, 0.0, 0.0, 0.0};
    dqGeom::Point2d offsetFromDocument;
    uint32_t depth = 0;
    double lengthFromLastTab = 0.0;  // tab runs 的位移基准

    // Ported from: itwinjs-core LineLayout ctor (:582-588)
    LineLayout(TextBlockComponent const& source, TextStyleSettingsProps const& style,
               LayoutContext* context, uint32_t depth = 0);

    std::string stringify() const;

    std::vector<RunLayout> const& runs() const noexcept { return m_runs; }
    bool isEmpty() const noexcept { return m_runs.empty(); }
    RunLayout const& back() const noexcept { return m_runs.back(); }

    // marker（lists——参考 getter/setter :605-609）。
    RunLayout const* marker() const noexcept { return m_marker ? &*m_marker : nullptr; }
    void setMarker(std::optional<RunLayout> m) { m_marker = std::move(m); }

    // RunLayout.source 为非拥有指针——marker/linebreak 克隆等临时 run 由行
    // 收养（TS GC 语义的显式所有权适配；块内 run 由 args.textBlock 保证存活）。
    void adoptSource(std::unique_ptr<TextBlockComponent> owned) { m_owned.push_back(std::move(owned)); }

    // Ported from: itwinjs-core LineLayout.append (:611-614)
    void append(RunLayout run);

    // Ported from: itwinjs-core LineLayout.computeRanges (:616-671)
    void computeRanges();

    // Ported from: itwinjs-core LineLayout.toResult (:673-678)
    LineLayoutResult toResult() const;

private:
    std::vector<RunLayout> m_runs;
    std::optional<RunLayout> m_marker;
    std::vector<std::unique_ptr<TextBlockComponent>> m_owned;
};

// Lays out the contents of a TextBlock into lines containing runs.
// Ported from: itwinjs-core TextBlockLayout (TextBlockLayout.ts:686-960)
class DQ_COMMON_EXPORT TextBlockLayout {
public:
    TextBlock const* source = nullptr;

    // The range of text geometry（调试面）。
    dqGeom::Range2d textRange;
    // The range including margins.
    dqGeom::Range2d range;
    std::vector<LineLayout> lines;

    // Ported from: itwinjs-core TextBlockLayout ctor (:717-726) —
    // populateLines → justifyLines → applyMargins。
    TextBlockLayout(TextBlock const& source, LayoutContext& context);

    // Ported from: itwinjs-core TextBlockLayout.toResult (:728-732)
    TextBlockLayoutResult toResult() const;

    // Compute a string representation, primarily for debugging（行以 \n 连接）。
    std::string stringify() const;

    // 内部结构遍历面（测试/调试——参考 private 方法的测试缝形）。
    LineLayout flushLineForTest(LayoutContext& context, LineLayout curLine,
                                TextStyleSettingsProps const& cumulativeOverrides);

private:
    friend class TextBlockLayoutAccess;
    std::unique_ptr<LayoutContext> m_context;

    LineLayout& backLine();
    void populateLines(LayoutContext& context);
    LineLayout populateComponent(TextBlockComponent& component, uint32_t componentIndex,
                                 LayoutContext& context, double docWidth, LineLayout curLine,
                                 TextBlockComponent const* parent,
                                 TextStyleSettingsProps const& cumulativeOverrides,
                                 uint32_t depth = 0);
    LineLayout populateRun(LineLayout curLine, RunLayout&& run, LayoutContext& context,
                           TextStyleSettingsProps const& cumulativeOverrides, double docWidth);
    LineLayout flushLine(LayoutContext& context, LineLayout curLine,
                         TextStyleSettingsProps const& cumulativeOverrides,
                         TextBlockComponent const* next = nullptr, bool newParagraph = false,
                         uint32_t depth = 0);
    void justifyLines(LayoutContext& context);
    void applyMargins(TextBlockMargins const& margins, double textHeight);
};

// Lays out a TextBlock. Ported from: itwinjs-core layoutTextBlock (:95-103)
DQ_COMMON_EXPORT TextBlockLayout layoutTextBlock(LayoutTextBlockArgs const& args);

// Computes the layout result（layoutTextBlock + toResult，:110-115）。
// Ported from: itwinjs-core computeLayoutTextBlockResult (:110-115)
DQ_COMMON_EXPORT TextBlockLayoutResult computeLayoutTextBlockResult(LayoutTextBlockArgs const& args);

END_DQ_COMMON_NAMESPACE
