// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text block document model
// Ported from: itwinjs-core core/common/src/annotation/TextBlock.ts
#pragma once

#include "DqCommon.h"
#include "TextString.h"
#include "TextField.h"
#include "TextStyle.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

class TextBlockComponent;
class List;
class Paragraph;
class TextBlock;
class TextRun;
class FractionRun;
class TabRun;
class LineBreakRun;
class FieldRun;

// Options supplied to clearStyleOverrides.
// Ported from: itwinjs-core ClearTextStyleOptions (TextBlock.ts:15-20)
struct ClearTextStyleOptions {
    // Controls whether the styleOverrides of any child components are retained.
    bool preserveChildrenOverrides = false;
};

// Options controlling how the content is formatted when converting a TextBlock
// to a string.
// Ported from: itwinjs-core TextBlockStringifyOptions (TextBlock.ts:35-56)
struct TextBlockStringifyOptions {
    std::optional<std::string> paragraphBreak;    // default " "
    std::optional<std::string> lineBreak;         // default " "
    std::optional<std::string> fractionSeparator; // default "/"
    // Number of spaces to use for tabs.（缺席/0 = 用 "\t"。）
    std::optional<uint32_t> tabsAsSpaces;
    std::optional<std::string> listMarkerBreak;   // default " "
};

// Context information useful when converting a TextBlock to a string.
// Ported from: itwinjs-core TextBlockStringifyContext (TextBlock.ts:62-65)
struct TextBlockStringifyContext {
    uint32_t depth = 0;
};

// Abstract representation of any of the building blocks that make up a
// TextBlock document — Runs and structural components.
// Ported from: itwinjs-core TextBlockComponent (TextBlock.ts:82-142)
class DQ_COMMON_EXPORT TextBlockComponent {
public:
    // 组件判别（参考 `type` 字符串字段 + instanceof 的 -fno-RTTI 适配——§3.4，
    // asClipShape 先例）。TextBlock 自身无 type（参考 TextBlockProps 无 type 字段）。
    enum class Kind : int32_t { Text, Fraction, Tab, LineBreak, Field, Paragraph, List, TextBlock };

    virtual ~TextBlockComponent() = default;

    // Deviations from the base TextStyleSettings of the applied annotation style.
    // Ported from: itwinjs-core TextBlockComponent.styleOverrides (:90-100) —
    // setter 深拷贝 = 值类型自动成立；非 const 访问器承载参考的
    // `component.styleOverrides.x = ...` 直接变异（测试 stringify lists 用）。
    TextStyleSettingsProps const& getStyleOverrides() const noexcept { return m_styleOverrides; }
    TextStyleSettingsProps& getStyleOverrides() noexcept { return m_styleOverrides; }
    void setStyleOverrides(TextStyleSettingsProps const& overrides) { m_styleOverrides = overrides; }

    // Reset any styleOverrides applied to this component.
    // Ported from: itwinjs-core TextBlockComponent.clearStyleOverrides (:103-105) —
    // 结构组件覆写为递归形（clearStyleOverrides 自由函数 :67-74）。
    virtual void clearStyleOverrides(ClearTextStyleOptions const* options = nullptr);

    // True if styleOverrides specifies any deviations.
    // Ported from: itwinjs-core TextBlockComponent.overridesStyle (:108-110) —
    // Object.keys().length > 0 = 任一字段在场。
    bool overridesStyle() const noexcept;

    // Create a deep copy of this component.
    // Ported from: itwinjs-core TextBlockComponent.clone (:113)
    virtual std::unique_ptr<TextBlockComponent> clone() const = 0;

    // String representation of this component and all of its sub-components.
    // Ported from: itwinjs-core TextBlockComponent.stringify (:116)
    virtual std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                                  TextBlockStringifyContext const* context = nullptr) const = 0;

    // True if this component has no content or children.
    // Ported from: itwinjs-core TextBlockComponent.isEmpty (:119)
    virtual bool isEmpty() const = 0;

    // True if the string representation consists only of whitespace characters.
    // Ported from: itwinjs-core TextBlockComponent.isWhitespace (:125-127) —
    // `/^\s*$/g` = 全空白或空串。
    bool isWhitespace() const;

    // Ported from: itwinjs-core TextBlockComponent.toJSON (:130-134)
    virtual TextStyleSettingsProps toJSON() const;

    // Returns true if `this` is equivalent to `other`.
    // Ported from: itwinjs-core TextBlockComponent.equals (:137-141) —
    // styleOverrides 经 TextStyleSettings.fromJSON(...).equals 比较；派生类
    // 覆写先判型再 super.equals（参考虚方法形）。
    virtual bool equals(TextBlockComponent const& other) const;

    Kind getKind() const noexcept { return m_kind; }

    // instanceof 的类型缝（§3.4）。
    TextRun* toTextRun() noexcept;
    FractionRun* toFractionRun() noexcept;
    TabRun* toTabRun() noexcept;
    LineBreakRun* toLineBreakRun() noexcept;
    FieldRun* toFieldRun() noexcept;
    Paragraph* toParagraph() noexcept;
    List* toList() noexcept;
    TextBlock* toTextBlock() noexcept;
    TextRun const* toTextRun() const noexcept;
    FractionRun const* toFractionRun() const noexcept;
    TabRun const* toTabRun() const noexcept;
    LineBreakRun const* toLineBreakRun() const noexcept;
    FieldRun const* toFieldRun() const noexcept;
    Paragraph const* toParagraph() const noexcept;
    List const* toList() const noexcept;
    TextBlock const* toTextBlock() const noexcept;

protected:
    // Ported from: itwinjs-core TextBlockComponent ctor (:86-88)
    explicit TextBlockComponent(Kind kind, TextStyleSettingsProps const* props = nullptr)
        : m_kind(kind)
    {
        if (props)
            m_styleOverrides = *props;
    }

private:
    Kind m_kind;
    TextStyleSettingsProps m_styleOverrides;
};

// Options supplied to clearStyleOverrides.
// Ported from: itwinjs-core ClearTextStyleOptions (TextBlock.ts:15-20) — 见前置声明。

// Describes whether the characters of a TextRun display normally, subscript,
// or superscript: "subscript" | "superscript" | "none".
// Ported from: itwinjs-core BaselineShift (TextBlock.ts:180)
namespace BaselineShift {
    inline constexpr char const* None = "none";
    inline constexpr char const* Subscript = "subscript";
    inline constexpr char const* Superscript = "superscript";
}

// A sequence of characters within a Paragraph sharing a single style.
// Ported from: itwinjs-core TextRun (TextBlock.ts:201-244)
class DQ_COMMON_EXPORT TextRun final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core TextRunProps (:185-196)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
        std::string content;                                  // default ""
        std::optional<std::string> baselineShift;             // default "none"
    };

    // Ported from: itwinjs-core TextRun.create (:228-230)
    static std::unique_ptr<TextRun> create(Props const* props = nullptr);

    std::string content;
    std::string baselineShift = BaselineShift::None;  // ← 参考名 baselineShift

    std::unique_ptr<TextBlockComponent> clone() const override;
    std::string stringify(TextBlockStringifyOptions const* = nullptr,
                          TextBlockStringifyContext const* = nullptr) const override { return content; }
    bool isEmpty() const override { return stringify().empty(); }
    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit TextRun(Props const* props);
};

// A Run containing a numeric ratio displayed as numerator/denominator.
// Ported from: itwinjs-core FractionRun (TextBlock.ts:262-306)
class DQ_COMMON_EXPORT FractionRun final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core FractionRunProps (:249-256)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
        std::string numerator;    // default ""
        std::string denominator;  // default ""
    };

    // Ported from: itwinjs-core FractionRun.create (:289-291)
    static std::unique_ptr<FractionRun> create(Props const* props = nullptr);

    std::string numerator;
    std::string denominator;

    std::unique_ptr<TextBlockComponent> clone() const override;
    // numerator + fractionSeparator + denominator（:298-301）
    std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                          TextBlockStringifyContext const* = nullptr) const override;
    bool isEmpty() const override { return stringify().empty(); }
    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit FractionRun(Props const* props);
};

// A Run representing the end of a line of text within a Paragraph.
// Ported from: itwinjs-core LineBreakRun (TextBlock.ts:319-354)
class DQ_COMMON_EXPORT LineBreakRun final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core LineBreakRunProps (:311-314)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
    };

    // Ported from: itwinjs-core LineBreakRun.create (:334-336)
    static std::unique_ptr<LineBreakRun> create(Props const* props = nullptr);

    std::unique_ptr<TextBlockComponent> clone() const override;
    // lineBreak 选项或缺省 " "（:347-349）
    std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                          TextBlockStringifyContext const* = nullptr) const override;
    bool isEmpty() const override { return stringify().empty(); }
    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit LineBreakRun(Props const* props);
};

// A Run used to shift the next tab stop (left-justified tabs only, :365).
// Ported from: itwinjs-core TabRun (TextBlock.ts:368-407)
class DQ_COMMON_EXPORT TabRun final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core TabRunProps (:359-362)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
    };

    // Ported from: itwinjs-core TabRun.create (:383-385)
    static std::unique_ptr<TabRun> create(Props const* props = nullptr);

    std::unique_ptr<TextBlockComponent> clone() const override;
    // tabsAsSpaces 个空格或 "\t"（:396-402）
    std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                          TextBlockStringifyContext const* = nullptr) const override;
    bool isEmpty() const override { return stringify().empty(); }
    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit TabRun(Props const* props);
};

// A Run displaying the formatted value of a property of some Element.
// Ported from: itwinjs-core FieldRun (TextBlock.ts:440-561)
class DQ_COMMON_EXPORT FieldRun final : public TextBlockComponent {
public:
    // Display string used to signal an error in computing the field's value.
    // Ported from: itwinjs-core FieldRun.invalidContentIndicator (:442)
    static constexpr char const* invalidContentIndicator = "####";

    // Ported from: itwinjs-core FieldRunProps (:412-428)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
        FieldPropertyHost propertyHost;
        FieldPropertyPath propertyPath;
        std::optional<FieldFormatOptions> formatOptions;
        std::optional<std::string> cachedContent;
    };

    // Ported from: itwinjs-core FieldRun.create (:476-484)
    static std::unique_ptr<FieldRun> create(Props const& props);

    FieldPropertyHost propertyHost;
    FieldPropertyPath propertyPath;
    std::optional<FieldFormatOptions> formatOptions;

    // The field's most recently evaluated display string.
    // Ported from: itwinjs-core FieldRun.cachedContent (:457-459)
    std::string const& getCachedContent() const noexcept { return m_cachedContent; }

    // @internal Used by core-backend when re-evaluating field content.
    // Ported from: itwinjs-core FieldRun.setCachedContent (:462-464) —
    // undefined → invalidContentIndicator。
    void setCachedContent(std::string const* content);

    std::unique_ptr<TextBlockComponent> clone() const override;
    // cachedContent（:516-518）
    std::string stringify(TextBlockStringifyOptions const* = nullptr,
                          TextBlockStringifyContext const* = nullptr) const override { return getCachedContent(); }
    bool isEmpty() const override { return stringify().empty(); }
    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit FieldRun(Props const& props);

private:
    std::string m_cachedContent;
};

// A collection of Runs and Lists, laid out on a separate line.
// Ported from: itwinjs-core Paragraph (TextBlock.ts:574-618)
class DQ_COMMON_EXPORT Paragraph final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core ParagraphProps (:566-568)。
    // §3.4 EQUIVALENCE：参考 children 为 JSON props 数组（经 Run.fromJSON/List
    // .create 构造）；DanQing 的 props 直接携构件实例（unique_ptr 克隆持有）——
    // toJSON→create 往返链语义一致（children 值域同构），JSON 解析层（Y-c 几何
    // 生成接线）出现时补 Run.fromJSON 判别工厂。
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
        std::vector<std::unique_ptr<TextBlockComponent>> children;
    };

    // Ported from: itwinjs-core Paragraph.create (:587-589)
    static std::unique_ptr<Paragraph> create(Props const* props = nullptr);

    // Runs 与 Lists（拥有）。
    std::vector<std::unique_ptr<TextBlockComponent>> const& getChildren() const noexcept { return m_children; }
    std::vector<std::unique_ptr<TextBlockComponent>>& getChildren() noexcept { return m_children; }

    // Ported from: itwinjs-core Paragraph.clearStyleOverrides (:591-593)
    void clearStyleOverrides(ClearTextStyleOptions const* options = nullptr) override;

    bool isEmpty() const override { return m_children.empty(); }

    std::unique_ptr<TextBlockComponent> clone() const override;
    TextStyleSettingsProps toJSON() const override;

    // 子串表示拼接（嵌套 List 前插 paragraphBreak，:611-613）。
    std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                          TextBlockStringifyContext const* context = nullptr) const override;

    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit Paragraph(Props const* props);

private:
    std::vector<std::unique_ptr<TextBlockComponent>> m_children;
};

// A collection of list items (Paragraphs).
// Ported from: itwinjs-core List (TextBlock.ts:632-681)
class DQ_COMMON_EXPORT List final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core ListProps (:623-626)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
        std::vector<std::unique_ptr<Paragraph>> children;
    };

    // Ported from: itwinjs-core List.create (:643-645)
    static std::unique_ptr<List> create(Props const* props = nullptr);

    std::vector<std::unique_ptr<Paragraph>> const& getChildren() const noexcept { return m_children; }
    std::vector<std::unique_ptr<Paragraph>>& getChildren() noexcept { return m_children; }

    // Ported from: itwinjs-core List.clearStyleOverrides (:647-649)
    void clearStyleOverrides(ClearTextStyleOptions const* options = nullptr) override;

    bool isEmpty() const override { return m_children.empty(); }

    std::unique_ptr<TextBlockComponent> clone() const override;
    TextStyleSettingsProps toJSON() const override;

    // 逐 item：tab×depth + marker + listMarkerBreak + item 串；paragraphBreak 连接
    //（:668-676——marker 取 styleOverrides.listMarker ?? defaultProps.listMarker）。
    std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                          TextBlockStringifyContext const* context = nullptr) const override;

    bool equals(TextBlockComponent const& other) const override;

protected:
    explicit List(Props const* props);

private:
    std::vector<std::unique_ptr<Paragraph>> m_children;
};

// A formatted text document consisting of a series of Paragraphs.
// Ported from: itwinjs-core TextBlock (TextBlock.ts:700-791)
class DQ_COMMON_EXPORT TextBlock final : public TextBlockComponent {
public:
    // Ported from: itwinjs-core TextBlockProps (:686-693)
    struct Props {
        std::optional<TextStyleSettingsProps> styleOverrides;
        // Lines exceeding this width (meters) wrap. ≤0 = no wrapping. Default 0.
        std::optional<double> width;
        std::vector<std::unique_ptr<Paragraph>> children;
    };

    // Ported from: itwinjs-core TextBlock.create (:729-731)
    static std::unique_ptr<TextBlock> create(Props const* props = nullptr);

    // The document width in meters（≤0 = 不换行，default 0）。
    double width = 0.0;

    std::vector<std::unique_ptr<Paragraph>> const& getChildren() const noexcept { return m_children; }
    std::vector<std::unique_ptr<Paragraph>>& getChildren() noexcept { return m_children; }

    // Ported from: itwinjs-core TextBlock.clearStyleOverrides (:716-718)
    void clearStyleOverrides(ClearTextStyleOptions const* options = nullptr) override;

    // Ported from: itwinjs-core TextBlock.toJSON (:720-726) —
    // {...super.toJSON(), width, children}（TextBlockProps 全形态；children 携
    // 实例克隆——§3.4 EQUIVALENCE 同 Props 注）。
    Props toJSONBlockProps() const;

    // 组件面（super.toJSON——styleOverrides）。
    TextStyleSettingsProps toJSON() const override;

    // True if every paragraph is empty.
    // Ported from: itwinjs-core TextBlock.isEmpty (:734-736)
    bool isEmpty() const override;

    std::unique_ptr<TextBlockComponent> clone() const override;

    // Paragraph 串以 paragraphBreak 连接（:743-745）。
    std::string stringify(TextBlockStringifyOptions const* options = nullptr,
                          TextBlockStringifyContext const* context = nullptr) const override;

    // Add and return a new paragraph（seedFromLast = 继承末子代 styleOverrides，
    // props 逐键优先——spread 合并 :752-758）。
    // Ported from: itwinjs-core TextBlock.appendParagraph (:752-758)
    Paragraph& appendParagraph(typename Paragraph::Props const* props = nullptr,
                               bool seedFromLast = false);

    // Append a run to the last Paragraph（无子代时先 appendParagraph，:763-766）。
    // Ported from: itwinjs-core TextBlock.appendRun (:763-766)
    void appendRun(std::unique_ptr<TextBlockComponent> run);

    bool equals(TextBlockComponent const& other) const override;

    // children 拥有制（unique_ptr）——拷贝禁用、移动默认（MSVC 对隐式删除
    // 特殊成员仍实例化成员 vector 拷贝赋值体，显式 delete 断开该链）。
    TextBlock(TextBlock&&) = default;
    TextBlock& operator=(TextBlock&&) = default;
    TextBlock(TextBlock const&) = delete;
    TextBlock& operator=(TextBlock const&) = delete;

protected:
    explicit TextBlock(Props const* props);

private:
    std::vector<std::unique_ptr<Paragraph>> m_children;
};

// Depth-first iteration over all components in a text block structure.
// Ported from: itwinjs-core traverseTextBlockComponent (:807-814) —
// TS 生成器 → C++ 回调遍历（§3.4 适配；yield 顺序 = 先序 DFS 逐位一致）。
DQ_COMMON_EXPORT void traverseTextBlockComponent(
    TextBlockComponent& parent,
    std::function<void(TextBlockComponent& parent, TextBlockComponent& child)> const& visit);

// Returns the formatted marker text for a list item（marker 类型 + item 序号 →
// 字符串；支持字母/罗马/数字与自定义枚举串，:824-850）。
// Ported from: itwinjs-core getMarkerText (TextBlock.ts:824-850)
DQ_COMMON_EXPORT std::string getMarkerText(ListMarker const& marker, uint32_t num);

END_DQ_COMMON_NAMESPACE
