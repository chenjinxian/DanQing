// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — NamedVSPS 持久化载体实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/NamedViews.ts
#include "NamedViews.h"

#include <dqApp/tile/DumpTileTreeProps.h>  // dumpjson（SavedViewsPanel 同源）

#include <algorithm>
#include <cstdio>

namespace Gui {

NamedVSPSList NamedVSPSList::create(
    std::vector<NamedViewStatePropsString> const* viewNames)
{
    NamedVSPSList viewList;
    viewList.populate(viewNames);
    return viewList;
}

void NamedVSPSList::clear()
{
    m_array.clear();
}

void NamedVSPSList::populate(
    std::vector<NamedViewStatePropsString> const* viewStateStrings)
{
    clear();
    if (viewStateStrings == nullptr || viewStateStrings->empty())
        return;
    for (auto const& vss : *viewStateStrings)
        insert(vss);
}

void NamedVSPSList::insert(NamedViewStatePropsString const& vsp)
{
    auto it = std::lower_bound(
        m_array.begin(), m_array.end(), vsp.name(),
        [](NamedViewStatePropsString const& a, std::string const& name) {
            return a.name() < name;  // compareStrings（字典序）
        });
    m_array.insert(it, vsp);
}

int NamedVSPSList::findName(std::string const& name) const
{
    for (size_t i = 0; i < m_array.size(); ++i) {
        if (m_array[i].name() == name)
            return static_cast<int>(i);
    }
    return -1;
}

void NamedVSPSList::removeName(std::string const& name)
{
    int const ndx = findName(name);
    if (ndx >= 0)
        m_array.erase(m_array.begin() + ndx);
}

namespace {

void appendEscaped(std::string& out, std::string const& s)
{
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

// NamedVSPSProps 的对象段（缩进层级由调用方管——字段顺序 1:1 构造序）。
void appendPropsObject(std::string& out, NamedVSPSProps const& p, char const* pad)
{
    out += pad;
    out += "{\n";
    auto field = [&](char const* key, std::optional<std::string> const& v) {
        if (!v)
            return;
        out += pad;
        out += "  ";
        appendEscaped(out, key);
        out += ": ";
        appendEscaped(out, *v);
        out += ",\n";
    };
    out += pad;
    out += "  \"_name\": ";
    appendEscaped(out, p._name);
    out += ",\n";
    out += pad;
    out += "  \"_viewStatePropsString\": ";
    appendEscaped(out, p._viewStatePropsString);
    out += ",\n";
    field("_selectedElements", p._selectedElements);
    field("_overrideElements", p._overrideElements);
    field("_displayTransforms", p._displayTransforms);
    // 末字段无尾逗号。
    auto const lastComma = out.rfind(",\n");
    if (lastComma != std::string::npos)
        out.replace(lastComma, 2, "\n");
    out += pad;
    out += "}";
}

}  // namespace

std::string NamedVSPSList::getPrintString() const
{
    if (m_array.empty())
        return "[]";
    std::string out = "[\n";
    for (size_t i = 0; i < m_array.size(); ++i) {
        appendPropsObject(out, m_array[i].props(), "  ");
        if (i + 1 < m_array.size())
            out += ",";
        out += "\n";
    }
    out += "]";
    return out;
}

void NamedVSPSList::loadFromString(std::string const& esvString)
{
    clear();
    if (esvString.empty())
        return;
    auto doc = dqApp::dumpjson::parseJsonDocument(esvString);
    if (!doc || doc->type != dqApp::dumpjson::JsonValue::Type::Array)
        return;
    for (auto const& obj : doc->arr) {
        if (obj.type != dqApp::dumpjson::JsonValue::Type::Object)
            continue;
        NamedVSPSProps props;
        if (dqApp::dumpjson::JsonValue const* v = obj.find("_name"))
            props._name = v->str;
        if (dqApp::dumpjson::JsonValue const* v = obj.find("_viewStatePropsString"))
            props._viewStatePropsString = v->str;
        auto strField = [&](char const* key, std::optional<std::string>& field) {
            if (dqApp::dumpjson::JsonValue const* v = obj.find(key)) {
                if (v->type == dqApp::dumpjson::JsonValue::Type::String)
                    field = v->str;
            }
        };
        strField("_selectedElements", props._selectedElements);
        strField("_overrideElements", props._overrideElements);
        strField("_displayTransforms", props._displayTransforms);
        m_array.push_back(NamedViewStatePropsString(std::move(props)));
    }
    // loadFromString 后重排序（参考 JSON.parse 后逐条 insert 的同语义——
    // SortedArray 不信任源序）。
    std::sort(m_array.begin(), m_array.end(),
              [](NamedViewStatePropsString const& a,
                 NamedViewStatePropsString const& b) {
                  return a.name() < b.name();
              });
}

}  // namespace Gui
