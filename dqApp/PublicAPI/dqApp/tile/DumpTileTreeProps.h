// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — RPC-dump 元数据域（manifest 索引 + IModelTileTreeProps 解析）
//
// Authored: no reference equivalent exists — replaying RPC bytes captured from
// the real itwinjs backend is the host-side seam of the §8.2 zero-network
// protocol（2026-09-27 用户指令：请求获取归宿主层/本地文件；DanQing 只做请求
// 获取之后的加载/解析/渲染）。dump 资产由仓外 danqing-rpc-tools collector 采集
// （阶段1 M-D Task 1，third_party/tile-sample-assets/rpc-dumps/，§11.11 只读）。
//
// 参考对齐面（字段映射的唯一规范来源）：
//   - IModelTileTreeProps / TileTreeProps / TileProps
//     (core/common/src/tile/TileProps.ts:23-70)；
//   - 其参考消费 iModelTileTreeParamsFromJSON
//     (core/frontend/src/internal/tile/IModelTileTree.ts:49-82) ——
//     PrimaryTreeSupplier.createTileTree (PrimaryTileTree.ts:63-80) 的
//     requestTileTreeProps 返回值（TileAdmin.ts:648-657）离线对应物。
//
// 本文件持 dump 的元数据三件：manifest.json 索引（trees/tiles——DumpTileFetcher
// 的字节回放同用此索引）、树 props JSON → DumpTreeProps 映射、按 treeId 分发。
// 公共头依据 §8.4：DisplayTestApp 宿主（samples）跨模块消费。
#pragma once

#include "../Export.h"

#include <dqGeom/Range3d.h>
#include <dqRender/tile/ImdlTileTree.h>  // ImdlTreeMetadata / ImdlTileMetadata

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

// ---------------------------------------------------------------------------
// dumpjson — manifest/props 的手写 JSON 解析（照 dqRender/src/tile/TilesetJson.h
// 的 tilejson::JsonParser 模式逐字同构——字符串扫描器 + 插入序对象；该解析器
// 为 dqRender 内部件，§8.4 跨模块不可 include，故按其模式在 dqApp 侧持副本；
// 溯源见 TilesetJson.h:1-6 的 string-scanner 历史）。
// ---------------------------------------------------------------------------
namespace dumpjson {

struct JsonValue {
    enum class Type : uint8_t { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;  // insertion-ordered

    JsonValue const* find(char const* key) const
    {
        for (auto const& kv : obj)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }
};

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
        char c = text[pos];
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
        skipWs();
        if (pos >= text.size() || text[pos] != '"') { failed = true; return out; }
        pos++;
        while (pos < text.size()) {
            char c = text[pos++];
            if (c == '"') return out;
            if (c == '\\' && pos < text.size()) {
                char e = text[pos++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (text.size() - pos < 4) { failed = true; return out; }
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = text[pos++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= unsigned(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= unsigned(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= unsigned(h - 'A' + 10);
                            else { failed = true; return out; }
                        }
                        // UTF-8 encode (BMP only — manifest/props keys are ASCII)
                        if (code < 0x80) {
                            out += char(code);
                        } else if (code < 0x800) {
                            out += char(0xC0 | (code >> 6));
                            out += char(0x80 | (code & 0x3F));
                        } else {
                            out += char(0xE0 | (code >> 12));
                            out += char(0x80 | ((code >> 6) & 0x3F));
                            out += char(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: failed = true; return out;
                }
            } else {
                out += c;
            }
        }
        failed = true;
        return out;
    }

    JsonValue parseNumber()
    {
        JsonValue v;
        v.type = JsonValue::Type::Number;
        size_t start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) pos++;
        bool any = false;
        while (pos < text.size()) {
            char c = text[pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E'
                || c == '-' || c == '+') {
                pos++;
                if (c >= '0' && c <= '9') any = true;
            } else break;
        }
        if (!any) { failed = true; return v; }
        v.number = std::strtod(std::string(text.substr(start, pos - start)).c_str(), nullptr);
        return v;
    }
};

inline std::optional<JsonValue> parseJsonDocument(std::string_view text)
{
    JsonParser p{text};
    JsonValue doc = p.parseValue();
    p.skipWs();
    if (p.failed || p.pos != text.size())
        return std::nullopt;
    return doc;
}

}  // namespace dumpjson

// ---------------------------------------------------------------------------
// manifest.json 索引（采集器契约——阶段1 M-D Task 1；字段名 1:1）。
// ---------------------------------------------------------------------------

// manifest trees[] 条目。
struct DumpManifestTreeEntry {
    std::string treeId;
    std::string iModelId;
    uint32_t formatVersion = 0;
    uint64_t byteLength = 0;
    std::string file;
    std::string propsFile;  // file 的别名双给（Task 1 评审钉死②）；解析优先显式 propsFile，缺省回退 file
};

// manifest tiles[] 条目。瓦键 = (treeId, contentId) 原样串——contentId 是前端
// 请求键（getTileRequestProps TileAdmin.ts:694-706；"-b-6-0-0-0-1" 形态），
// 不是 props.rootTile.contentId 的 "0/0/0/0/1" 形态（Task 1 评审钉死①）。
struct DumpManifestTileEntry {
    std::string treeId;
    std::string contentId;
    std::string guid;
    std::string iModelId;
    std::string changesetId;
    uint64_t byteLength = 0;
    std::string sha256;
    std::string file;
};

struct DumpManifest {
    std::vector<DumpManifestTreeEntry> trees;
    std::vector<DumpManifestTileEntry> tiles;
};

// <dumpRoot>/manifest.json → DumpManifest。缺失/坏 JSON/必需字段缺失/stats
// 计数与数组不一致 → nullopt（完整性门）。
std::optional<DumpManifest> DQ_APP_EXPORT
loadDumpManifest(std::string const& dumpRoot);

// ---------------------------------------------------------------------------
// 树 props 映射产物：树级 ImdlTreeMetadata + 根瓦 props。
// 字段映射对齐 TileProps.ts（§0：字段名 1:1）：
//   - metadata：contentRange（TileTreeProps.contentRange，:51）/ tileScreenSize
//     （IModelTileTreeProps.tileScreenSize，:67，缺省 512——TileProps.ts:66 与
//     iModelTileTreeParamsFromJSON IModelTileTree.ts:52 的 ?? 512）/ is2d（非
//     树 props 字段——参考来自视图侧 PrimaryTreeId.is3d → options.is3d，
//     PrimaryTileTree.ts:68；离线 dump 全为空间树 → false）。
//   - rootTile：TileProps（:23-36）→ ImdlTileMetadata 六字段
//     （contentId/range/contentRange/isLeaf/sizeMultiplier/emptySubRangeMask）。
//   - rootMaximumSize：TileProps.maximumSize（:31）——ImdlTileMetadata 无载体，
//     是 ImdlTile 构造的 maximumSize 实参（参考 RootTile 由
//     iModelTileParamsFromJSON(params.rootTile) 携带，IModelTileTree.ts:406）。
//
// 未消费的 props 字段（解析期忽略，登记——载体归后续里程碑，与 M-C 登记同源）：
//   location（TransformProps——ImdlTileTree 无变换载体）、maxTilesToSkip、
//   maxInitialTilesToSkip（**载体在、注入路径缺**：ImdlTileTree.h:349-350 的
//   预算成员存在，但树构造器无 props 入参，props 值无法到达——预算仍为
//   ?? 0 / TileAdmin 缺省，IModelTileTree.ts:390-391）、contentIdQualifier、
//   geometryGuid、transformNodeRanges、extentsBasis/baseExtents（TileProps.ts
//   之外的后端扩展域，参考类型亦无）。
//   formatVersion 原在本清单——M-D(3) 起已消费（metadata.formatVersion →
//   ImdlTileTree 的 ContentIdProvider 方案选择，IModelTileTree.ts:396-398）。
// ---------------------------------------------------------------------------
struct DumpTreeProps {
    dqRender::ImdlTreeMetadata metadata;
    dqRender::ImdlTileMetadata rootTile;
    double rootMaximumSize = 0.0;  // TileProps.maximumSize（0 = undisplayable 语义）
    std::string id;                // TileTreeProps.id 原文（不做强校验——参考 requestTileTreeProps 同样信任返回值）
};

// 按 treeId 提供树 props（manifest trees[].propsFile → IModelTileTreeProps
// JSON → DumpTreeProps）。M-D Task 3 的离线树装配（PrimaryTileTreeSupplier
// 离线模式）经此取 props。
class DQ_APP_EXPORT DumpTileTreeProps {
public:
    // 装载 <dumpRoot>/manifest.json；缺失/坏 manifest → nullopt。
    static std::optional<DumpTileTreeProps> load(std::string const& dumpRoot);

    // 未知 treeId / props 文件缺失或坏 → nullopt（NotFound 语义）。
    std::optional<DumpTreeProps> byTreeId(std::string const& treeId) const;

    // manifest 计数（契约测试的 ① 面——与 manifest.json stats 域一致）。
    size_t getTreeCount() const noexcept { return m_manifest.trees.size(); }
    size_t getTileCount() const noexcept { return m_manifest.tiles.size(); }

private:
    DumpTileTreeProps() = default;

    std::string m_dumpRoot;
    DumpManifest m_manifest;
};

END_DQ_APP_NAMESPACE
