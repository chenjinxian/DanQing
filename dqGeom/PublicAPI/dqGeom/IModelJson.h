// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — IModelJson (geometry JSON serialization)
//
// Ported from: itwinjs-core core/geometry/src/serialization/IModelJsonSchema.ts
//              itwinjs-core core/geometry/src/serialization/SerializationHelpers.ts
//
// W-a2 子集裁决（E3）：仅移植 indexedMesh 读入面（Reader.parseIndexedMesh +
// parsePolyfaceAuxData + SerializationHelpers.announceZeroBasedIndicesFromSignedOne
// BasedIndices）与配套 Writer 面（handleIndexedPolyface + handlePolyfaceAuxData +
// emit）——Cantilever 资产（samples/DisplayTestApp/assets/cantilever.imjs）与
// PolyfaceAuxData.test.ts round-trip/Compress 锁是唯一消费场景。其余几何种类
// （lineSegment/arc/bcurve/bsurf/cone/sphere/sweep 族……）不移植——TODO：消费到时按
// IModelJsonSchema.ts:1274-1328 的分派表补齐。
//
// JSON DOM（imodeljson::JsonValue/JsonParser）：TS `any` 的 C++ 承载——字符串扫描器
// 模式与 dqRender/src/tile/TilesetJson.h 的 tilejson::JsonParser 逐字同构（DanQing
// 第三处同型实例；dqGeom 不得依赖 dqRender/dqApp，模块内自带）。
#pragma once

#include "GeometryQuery.h"
#include "IndexedPolyface.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

namespace imodeljson {

// JsonValue — TS `any` 的 C++ 承载（插入序对象；与 tilejson::JsonValue 同构）。
struct JsonValue {
    enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;  // insertion-ordered

    // hasOwnProperty(key) 语义（对象形态才查）。
    JsonValue const* find(char const* key) const
    {
        for (auto const& kv : obj)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }
};

// JsonParser — 字符串扫描器（照 tilejson::JsonParser 模式；JSON.parse 的严格子集：
// 数字经 strtod、字符串带转义、true/false/null 字面量）。
struct JsonParser {
    std::string_view text;
    size_t pos = 0;
    bool failed = false;

    void skipWs()
    {
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos++;
            else break;
        }
    }

    bool consume(char c)
    {
        skipWs();
        if (pos < text.size() && text[pos] == c) {
            pos++;
            return true;
        }
        failed = true;
        return false;
    }

    bool literal(char const* lit)
    {
        skipWs();
        size_t const n = std::strlen(lit);
        if (text.size() - pos >= n && text.substr(pos, n) == lit) {
            pos += n;
            return true;
        }
        return false;
    }

    JsonValue parseValue()
    {
        JsonValue v;
        skipWs();
        if (pos >= text.size()) { failed = true; return v; }
        char const c = text[pos];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') { v.type = JsonValue::Type::String; v.str = parseString(); return v; }
        if (literal("true"))  { v.type = JsonValue::Type::Bool; v.boolean = true;  return v; }
        if (literal("false")) { v.type = JsonValue::Type::Bool; v.boolean = false; return v; }
        if (literal("null"))  { v.type = JsonValue::Type::Null; return v; }
        return parseNumber();
    }

    JsonValue parseObject()
    {
        JsonValue v;
        v.type = JsonValue::Type::Object;
        if (!consume('{')) return v;
        skipWs();
        if (pos < text.size() && text[pos] == '}') { pos++; return v; }
        while (true) {
            skipWs();
            std::string key = parseString();
            if (failed) return v;
            if (!consume(':')) return v;
            v.obj.emplace_back(std::move(key), parseValue());
            if (failed) return v;
            skipWs();
            if (pos < text.size() && text[pos] == ',') { pos++; continue; }
            if (pos < text.size() && text[pos] == '}') { pos++; return v; }
            failed = true;
            return v;
        }
    }

    JsonValue parseArray()
    {
        JsonValue v;
        v.type = JsonValue::Type::Array;
        if (!consume('[')) return v;
        skipWs();
        if (pos < text.size() && text[pos] == ']') { pos++; return v; }
        while (true) {
            v.arr.push_back(parseValue());
            if (failed) return v;
            skipWs();
            if (pos < text.size() && text[pos] == ',') { pos++; continue; }
            if (pos < text.size() && text[pos] == ']') { pos++; return v; }
            failed = true;
            return v;
        }
    }

    std::string parseString()
    {
        std::string out;
        if (pos >= text.size() || text[pos] != '"') { failed = true; return out; }
        pos++;
        while (pos < text.size()) {
            char const c = text[pos++];
            if (c == '"') return out;
            if (c == '\\' && pos < text.size()) {
                char const e = text[pos++];
                switch (e) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case '/': out.push_back('/'); break;
                    case '\\': out.push_back('\\'); break;
                    case '"': out.push_back('"'); break;
                    case 'u': {
                        // 拨 4 位 hex → UTF-8（BMP；代理对按码点直通处理——测试资产无）
                        if (pos + 4 > text.size()) { failed = true; return out; }
                        uint32_t cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char const h = text[pos++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<uint32_t>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<uint32_t>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<uint32_t>(h - 'A' + 10);
                            else { failed = true; return out; }
                        }
                        if (cp < 0x80) {
                            out.push_back(static_cast<char>(cp));
                        } else if (cp < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: failed = true; return out;
                }
            } else {
                out.push_back(c);
            }
        }
        failed = true;
        return out;
    }

    JsonValue parseNumber()
    {
        JsonValue v;
        skipWs();
        size_t const start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) pos++;
        while (pos < text.size()) {
            char const c = text[pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
                pos++;
            else
                break;
        }
        if (pos == start) { failed = true; return v; }
        v.type = JsonValue::Type::Number;
        v.number = std::strtod(std::string(text.substr(start, pos - start)).c_str(), nullptr);
        return v;
    }
};

// parseJsonDocument — 全文解析（失败 → nullopt）。
inline std::optional<JsonValue> parseJsonDocument(std::string_view text)
{
    JsonParser p{text};
    JsonValue doc = p.parseValue();
    if (p.failed)
        return std::nullopt;
    return doc;
}

}  // namespace imodeljson

// ---------------------------------------------------------------------------
// SerializationHelpers（SerializationHelpers.ts 子集）
// ---------------------------------------------------------------------------
namespace SerializationHelpers {

// Ported from: SerializationHelpers.ts:292-326
// announceZeroBasedIndicesFromSignedOneBasedIndices —— 1-based blocked 索引 → 0-based：
// numPerBlock > 1 → 固定块（0=padding 跳过）；否则变长块（0=终止符，连续终止符合并，
// 缺尾终止符补一发）。announce 回调签名 (i0, flag)——flag=源索引为正。
using AnnounceZeroBasedIndex = std::function<void(int32_t i0, bool flag)>;
DQ_GEOM_EXPORT void announceZeroBasedIndicesFromSignedOneBasedIndices(
    std::vector<int32_t> const& sourceIndices, int numPerBlock,
    AnnounceZeroBasedIndex const& announceZeroBasedIndex,
    std::function<void()> const& terminateBlock = {});

}  // namespace SerializationHelpers

// ---------------------------------------------------------------------------
// IModelJson::Reader / Writer（IModelJsonSchema.ts 子集——E3）
// ---------------------------------------------------------------------------
namespace IModelJson {

class DQ_GEOM_EXPORT Reader {
public:
    Reader() = default;

    // Ported from: IModelJsonSchema.ts:1274-1328 parse 的数组与 indexedMesh 分支
    // （其余几何种类分支不移植——E3；返回 nullptr == 参考 undefined）。
    static dqBase::RefPtr<GeometryQuery> parse(imodeljson::JsonValue const& json);
    // Ported from: IModelJsonSchema.ts:940-952 parseArray。
    static std::vector<dqBase::RefPtr<GeometryQuery>> parseArray(imodeljson::JsonValue const& json);
    // Ported from: IModelJsonSchema.ts:979-1051 parseIndexedMesh
    // （edgeMateIndex/taggedNumericData 面不移植——DanQing PolyfaceData 无该两字段）。
    static dqBase::RefPtr<IndexedPolyface> parseIndexedMesh(imodeljson::JsonValue const& data);
    // Ported from: IModelJsonSchema.ts:955-976 parsePolyfaceAuxData。
    static std::optional<PolyfaceAuxData> parsePolyfaceAuxData(imodeljson::JsonValue const& data,
                                                               int numPerFace = 0);
};

class DQ_GEOM_EXPORT Writer {
public:
    Writer() = default;

    // Ported from: IModelJsonSchema.ts:1991-2001 emit（GeometryQuery 分派面——
    // 本子集仅支持 IndexedPolyface，其余返回 nullopt == 参考 undefined）。
    // §3.4 碰撞例外：参考方法名 `emit` 撞 Qt 关键字宏（QT_NO_KEYWORDS 在
    // DanQing 刻意未定义——samples/DisplayTestApp/CMakeLists.txt:155-156 注记），
    // 故以 `emitJson` 落名（最小可区分后缀）。
    std::optional<imodeljson::JsonValue> emitJson(GeometryQuery const& data);
    // Ported from: IModelJsonSchema.ts:2003-2006 toIModelJson。
    static std::optional<imodeljson::JsonValue> toIModelJson(GeometryQuery const& data)
    {
        Writer writer;
        return writer.emitJson(data);
    }

    // Ported from: IModelJsonSchema.ts:1753-1868 handleIndexedPolyface。
    imodeljson::JsonValue handleIndexedPolyface(IndexedPolyface const& pf);

private:
    // Ported from: IModelJsonSchema.ts:1731-1750 handlePolyfaceAuxData。
    imodeljson::JsonValue handlePolyfaceAuxData(PolyfaceAuxData const& auxData,
                                                IndexedPolyface const& pf);
};

}  // namespace IModelJson

END_DQ_GEOM_NAMESPACE
