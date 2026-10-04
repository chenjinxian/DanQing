// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — NamedVSPS 持久化载体（Saved Views 的存储条目）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/NamedViews.ts
//
// SortedArray 的 C++ 等价 = std::vector + 有序 insert（宿主层；compareStrings
// = 大小写敏感字典序 ⇔ std::string::operator<）。
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Gui {

// Ported from: itwinjs-core NamedViews.ts NamedVSPSProps（:11-17——
// JSON.stringify 直接消费的持久化字段名，含下划线）。
struct NamedVSPSProps {
    std::string _name;
    std::string _viewStatePropsString;
    std::optional<std::string> _selectedElements;
    std::optional<std::string> _overrideElements;
    std::optional<std::string> _displayTransforms;
};

// Ported from: itwinjs-core NamedViews.ts NamedViewStatePropsString（:21-41）。
class NamedViewStatePropsString {
public:
    explicit NamedViewStatePropsString(NamedVSPSProps props)
        : m_props(std::move(props))
    {}

    // TS get 属性面（:36-40）——§3.3 无前缀访问器。
    std::string const& name() const { return m_props._name; }
    std::string const& viewStatePropsString() const
    {
        return m_props._viewStatePropsString;
    }
    std::optional<std::string> const& selectedElements() const
    {
        return m_props._selectedElements;
    }
    std::optional<std::string> const& overrideElements() const
    {
        return m_props._overrideElements;
    }
    // DanQing 无 DisplayTransformProvider——保存侧恒 nullopt（恢复侧不可达，
    // SavedViewsPanel 的 EQUIVALENCE 登记）。
    std::optional<std::string> const& displayTransforms() const
    {
        return m_props._displayTransforms;
    }
    // 持久化面（getPrintString 的序列化源——参考 JSON.stringify 直读 _array）。
    NamedVSPSProps const& props() const { return m_props; }

private:
    NamedVSPSProps m_props;
};

// Ported from: itwinjs-core NamedViews.ts NamedVSPSList（:43-106——
// SortedArray<NamedViewStatePropsString> 的有序表：按 name 字典序）。
class NamedVSPSList {
public:
    // create（:49-53）——viewNames 可空（populate 的 undefined 语义）。
    static NamedVSPSList create(
        std::vector<NamedViewStatePropsString> const* viewNames = nullptr);

    void clear();  // :55-57
    // populate（:59-69）——空指针/空表 = 清空后返回。
    void populate(std::vector<NamedViewStatePropsString> const* viewStateStrings);
    // insert（SortedArray 语义——字典序定位插入）。
    void insert(NamedViewStatePropsString const& vsp);
    size_t length() const { return m_array.size(); }
    NamedViewStatePropsString const& get(size_t i) const { return m_array[i]; }
    // findName（:71-79）——命中下标，未命中 -1。
    int findName(std::string const& name) const;
    // removeName（:81-90）——未命中 no-op。
    void removeName(std::string const& name);
    // getPrintString（:92-95）——JSON.stringify(_array, null, "  ") 的等价
    //（2 空格缩进的 JSON 数组；字段名含下划线 1:1）。
    std::string getPrintString() const;
    // loadFromString（:97-105）——esvString 空串 = 清空；坏 JSON = 清空
    //（参考 JSON.parse throw 由调用方吞掉的面——持久化文件自产自销）。
    void loadFromString(std::string const& esvString);

private:
    std::vector<NamedViewStatePropsString> m_array;  // ← SortedArray._array
};

}  // namespace Gui
