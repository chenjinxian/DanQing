// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — RPC-dump 元数据域实现（manifest 索引 + 树 props 映射）
// Authored: see DumpTileTreeProps.h（参考对齐面 = TileProps.ts 字段映射 +
//           iModelTileTreeParamsFromJSON IModelTileTree.ts:49-82）。
#include "dqApp/tile/DumpTileTreeProps.h"

#include <dqGeom/Point3d.h>

#include <fstream>
#include <iterator>

BEGIN_DQ_APP_NAMESPACE

namespace {

std::vector<uint8_t> readFileBytes(std::string const& path, bool* ok = nullptr)
{
    // 整块读（M-I(1)——原 istreambuf_iterator 逐字符抽取对 59MB manifest 是
    // 秒级开销；与 DumpTileFetcher.cpp readFileBytes 同款 seek/tellg/read）。
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        if (ok)
            *ok = false;
        return {};
    }
    if (ok)
        *ok = true;
    auto const size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), size)) {
        if (ok)
            *ok = false;
        return {};
    }
    return bytes;
}

std::string joinPath(std::string const& base, std::string const& rel)
{
    // dump 相对路径统一 '/'（manifest 契约——files/<n>）；Windows 接受 '/'。
    if (base.empty())
        return rel;
    if (base.back() == '/' || base.back() == '\\')
        return base + rel;
    return base + "/" + rel;
}

// manifest 文本直读为 std::string（M-I(1)——大 manifest 免 vector→string 的
// 二次 59MB 拷贝；json 解析走 string_view，不要求容器形态）。
bool readManifestText(std::string const& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open())
        return false;
    auto const size = in.tellg();
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (size > 0 && !in.read(&out[0], size))
        return false;
    return true;
}

// Range3dProps {low:[x,y,z], high:[x,y,z]} → dqGeom::Range3d。
// 参考语义 = Range3d.fromJSON（Point3d.fromJSON 逐轴赋值，**不重排** low/high
// ——空 range 的 ±extreme 反转域须原样存活供 isNull() 判定；CreateXYZXYZ 的
// min/max 校正会把它"修"成非空，故不用）。
bool parseRange3d(dumpjson::JsonValue const& json, dqGeom::Range3d& out)
{
    if (json.type != dumpjson::JsonValue::Type::Object)
        return false;
    dumpjson::JsonValue const* low = json.find("low");
    dumpjson::JsonValue const* high = json.find("high");
    if (!low || !high || low->type != dumpjson::JsonValue::Type::Array
        || high->type != dumpjson::JsonValue::Type::Array
        || low->arr.size() < 3 || high->arr.size() < 3)
        return false;
    out = dqGeom::Range3d(
        dqGeom::Point3d::From(low->arr[0].number, low->arr[1].number,
                              low->arr[2].number),
        dqGeom::Point3d::From(high->arr[0].number, high->arr[1].number,
                              high->arr[2].number));
    return true;
}

// TransformProps 3×4 数组形 → dqGeom::Transform。
// Ported from: itwinjs-core core-geometry Transform.setFromJSON 的
//              isArrayOfNumberArray(json,3,4) 分支（Transform.ts:86-94——
//              matrix = 3×3 行值、origin = 每行第 4 列）；dump 实态即此形
//              （纯平移）——{origin,matrix} 对象形与 12 数平铺形登记未移植
//              （采集域无此形态）。缺段/非数组 → false。
bool parseTransform3x4(dumpjson::JsonValue const& json, dqGeom::Transform& out)
{
    if (json.type != dumpjson::JsonValue::Type::Array || json.arr.size() < 3)
        return false;
    double m[9];
    double t[3];
    for (int row = 0; row < 3; ++row) {
        auto const& r = json.arr[static_cast<size_t>(row)];
        if (r.type != dumpjson::JsonValue::Type::Array || r.arr.size() < 4)
            return false;
        for (int col = 0; col < 3; ++col)
            m[row * 3 + col] = r.arr[static_cast<size_t>(col)].number;
        t[row] = r.arr[3].number;
    }
    out = dqGeom::Transform::CreateOriginAndMatrix(
        dqGeom::Point3d::From(t[0], t[1], t[2]),
        dqGeom::Matrix3d::CreateRowValues(m[0], m[1], m[2], m[3], m[4], m[5],
                                          m[6], m[7], m[8]));
    return true;
}

// ---------------------------------------------------------------------------
// manifest 流式读取器（M-I(1)——大 manifest 免 DOM 物化）。
//
// 大 manifest（joeshouse-v1 59MB/173,876 瓦）的 DOM 物化是打开链的残余
// 成本主体（/MDd /Od 实测：JSON DOM 构建 13.2s vs 同一原语免 DOM 结构遍历
// 3.6s——Temp/dqi_bench 探针 bench_parser2，机器负载同况对拍）。本读取器
// 用同一 dumpjson 原语（skipWs/consume/literal/parseString/parseNumber）
// 直读结构，不物化 JsonValue；props 文件/imodel.json（小文件）仍走 DOM
// 解析器。语义与原 DOM 路径（parseJsonDocument + parseManifest）逐分支
// 同构：
//   - 全文有效性：单个 JSON 值 + 仅余空白（parseJsonDocument 的
//     p.pos != text.size() 门）；
//   - 顶层须对象；trees/tiles 须存在且为数组（元素逐条校验）；
//   - 字段必需性/类型容错逐项同构：treeId/contentId 类型检查为 String
//     （非同型 → 整个 manifest 失败）；其余字段按 DOM find 的取值语义
//     直取（.str/.number——非同型值为空/0，不失败）；
//   - 同键重复：首键优先（DOM find 首命中）；
//   - stats 完整性门：仅对出现的键比对（缺省容错——采集器总写出，防御
//     未来格式；stats 非对象 → 门静默跳过——DOM 对非对象 find 落空同语义）；
//   - provenance.iModel：对象才置值；name 仅字符串；extents 坏形态（非
//     对象/low-high 缺轴）忽略该字段不置值；其余键全结构校验后丢弃；
//   - 未知键（provenance 的 defaultView/sweep 等嵌套结构——真实 dump 域）：
//     全结构校验后丢弃。
// ---------------------------------------------------------------------------
class ManifestStreamReader {
public:
    explicit ManifestStreamReader(std::string_view text)
        : m_p{text}
    {
    }

    bool run(DumpManifest& out, std::optional<DumpIModelInfo>* iModelInfoOut)
    {
        // 顶层对象（parseJsonDocument + doc->type == Object 门）。
        if (!m_p.consume('{'))
            return false;
        bool hasTrees = false;
        bool hasTiles = false;
        bool hasProvenance = false;
        double statTrees = 0.0;
        double statTiles = 0.0;
        bool hasStatTrees = false;
        bool hasStatTiles = false;
        bool statsSeen = false;
        if (!objectBody([&](std::string const& key) -> bool {
                if (key == "trees" && !hasTrees) {
                    hasTrees = true;
                    return readTrees(out);  // 非数组/元素坏 → 失败（DOM 同）
                }
                if (key == "tiles" && !hasTiles) {
                    hasTiles = true;
                    return readTiles(out);
                }
                if (key == "stats" && !statsSeen) {
                    statsSeen = true;
                    return statsValue(statTrees, hasStatTrees, statTiles, hasStatTiles);
                }
                if (key == "provenance" && !hasProvenance) {
                    hasProvenance = true;
                    if (iModelInfoOut != nullptr)
                        return provenanceValue(*iModelInfoOut);
                    return skipValue();
                }
                return skipValue();
            }))
            return false;

        // 全文有效性门（parseJsonDocument 的 p.pos != text.size() 同款）。
        m_p.skipWs();
        if (m_p.failed || m_p.pos != m_p.text.size())
            return false;

        // trees/tiles 必需（DOM 的 find + type 检查门——缺失/非数组 → 失败；
        // 本读取器在读到时已逐条校验，此处只补"键缺失"形态）。
        if (!hasTrees || !hasTiles)
            return false;

        // stats 计数门（语义不变——仅对出现的键比对；n->number 直取，
        // 非数值 stats 值为 0 同 DOM）。
        if (hasStatTrees && static_cast<size_t>(statTrees) != out.trees.size())
            return false;
        if (hasStatTiles && static_cast<size_t>(statTiles) != out.tiles.size())
            return false;
        return true;
    }

private:
    dumpjson::JsonParser m_p;

    // --- 原语包装 ---

    // 对象成员遍历（parseObject 逐分支同构——'{' 已由调用方消费；键
    // parseString → ':' → 值（onKey 消费）→ ','/'}'）。
    template <typename OnKey>
    bool objectBody(OnKey&& onKey)
    {
        m_p.skipWs();
        if (m_p.pos < m_p.text.size() && m_p.text[m_p.pos] == '}') {
            m_p.pos++;
            return true;
        }
        while (true) {
            m_p.skipWs();
            std::string key = m_p.parseString();
            if (m_p.failed)
                return false;
            if (!m_p.consume(':'))
                return false;
            if (!onKey(key))
                return false;
            m_p.skipWs();
            if (m_p.pos < m_p.text.size() && m_p.text[m_p.pos] == ',') {
                m_p.pos++;
                continue;
            }
            if (m_p.pos < m_p.text.size() && m_p.text[m_p.pos] == '}') {
                m_p.pos++;
                return true;
            }
            m_p.failed = true;
            return false;
        }
    }

    // 数组成员遍历（parseArray 逐分支同构——'[' 已由调用方消费）。

    void skipWs() { m_p.skipWs(); }
    char peek()
    {
        return m_p.pos < m_p.text.size() ? m_p.text[m_p.pos] : '\0';
    }

    // 值丢弃扫描（结构校验全程——parseValue 的丢弃对应物；失败置 failed）。
    bool skipValue()
    {
        m_p.skipWs();
        if (m_p.pos >= m_p.text.size()) {
            m_p.failed = true;
            return false;
        }
        char const c = peek();
        if (c == '{')
            return skipObject();
        if (c == '[')
            return skipArray();
        if (c == '"') {
            m_p.parseString();
            return !m_p.failed;
        }
        if (m_p.literal("true") || m_p.literal("false") || m_p.literal("null"))
            return true;
        m_p.parseNumber();
        return !m_p.failed;
    }

    bool skipObject()
    {
        // '{' 由本函数消费（skipValue 只 peek 分派——parseObject 的
        // consume('{') 对应物）。
        if (!m_p.consume('{'))
            return false;
        return objectBody([this](std::string const&) { return skipValue(); });
    }

    bool skipArray()
    {
        // '[' 由本函数消费（skipValue 只 peek 分派——parseArray 的
        // consume('[') 对应物）。
        if (!m_p.consume('['))
            return false;
        m_p.skipWs();
        if (peek() == ']') {
            m_p.pos++;
            return true;
        }
        while (true) {
            if (!skipValue())
                return false;
            m_p.skipWs();
            if (peek() == ',') {
                m_p.pos++;
                continue;
            }
            if (peek() == ']') {
                m_p.pos++;
                return true;
            }
            m_p.failed = true;
            return false;
        }
    }

    // 松散字符串直取（DOM 的 v->str 语义——字符串取其值，其他同型值跳过
    // 且取空串，不失败；仅结构坏才失败）。
    bool looseString(std::string& out)
    {
        m_p.skipWs();
        if (peek() == '"') {
            out = m_p.parseString();
            return !m_p.failed;
        }
        if (!skipValue())
            return false;
        out.clear();  // DOM：非字符串 JsonValue 的 .str == ""
        return true;
    }

    // 松散数值直取（DOM 的 v->number 语义——数值取其值，其他同型值取 0
    // 不失败；仅结构坏才失败）。
    bool looseNumber(double& out)
    {
        m_p.skipWs();
        char const c = peek();
        if (c != '{' && c != '[' && c != '"') {
            // parseValue 的分派序：非 { [ " 且非 true/false/null 字面量 →
            // parseNumber（无数字 → failed——与 DOM 同为文档失败）。
            if (m_p.literal("true") || m_p.literal("false")
                || m_p.literal("null")) {
                out = 0.0;
                return true;
            }
            out = m_p.parseNumber().number;
            return !m_p.failed;
        }
        if (!skipValue())
            return false;
        out = 0.0;
        return true;
    }

    // --- 结构域读取 ---

    bool readTrees(DumpManifest& out)
    {
        m_p.skipWs();
        if (peek() != '[') {
            m_p.failed = true;
            return false;
        }
        m_p.pos++;
        m_p.skipWs();
        if (peek() == ']') {
            m_p.pos++;
            return true;
        }
        while (true) {
            DumpManifestTreeEntry item;
            if (!treeEntry(item))
                return false;
            out.trees.push_back(std::move(item));
            m_p.skipWs();
            if (peek() == ',') {
                m_p.pos++;
                continue;
            }
            if (peek() == ']') {
                m_p.pos++;
                return true;
            }
            m_p.failed = true;
            return false;
        }
    }

    bool treeEntry(DumpManifestTreeEntry& item)
    {
        // 元素须对象（DOM：entry.type != Object → false）。
        if (!m_p.consume('{'))
            return false;
        bool seenTreeId = false;
        bool seenIModelId = false;
        bool seenFormatVersion = false;
        bool seenByteLength = false;
        bool seenFile = false;
        bool seenPropsFile = false;
        bool treeIdOk = false;
        if (!objectBody([&](std::string const& key) -> bool {
                if (key == "treeId" && !seenTreeId) {
                    seenTreeId = true;
                    // DOM：treeId->type != String → false（必需字符串字段）。
                    m_p.skipWs();
                    if (peek() != '"') {
                        if (!skipValue())
                            return false;
                        return false;
                    }
                    item.treeId = m_p.parseString();
                    treeIdOk = !m_p.failed;
                    return treeIdOk;
                }
                if (key == "iModelId" && !seenIModelId) {
                    seenIModelId = true;
                    return looseString(item.iModelId);
                }
                if (key == "formatVersion" && !seenFormatVersion) {
                    seenFormatVersion = true;
                    double v = 0.0;
                    if (!looseNumber(v))
                        return false;
                    item.formatVersion = static_cast<uint32_t>(v);
                    return true;
                }
                if (key == "byteLength" && !seenByteLength) {
                    seenByteLength = true;
                    double v = 0.0;
                    if (!looseNumber(v))
                        return false;
                    item.byteLength = static_cast<uint64_t>(v);
                    return true;
                }
                if (key == "file" && !seenFile) {
                    seenFile = true;
                    return looseString(item.file);
                }
                if (key == "propsFile" && !seenPropsFile) {
                    seenPropsFile = true;
                    return looseString(item.propsFile);
                }
                return skipValue();
            }))
            return false;

        // propsFile 与 file 是同值别名（Task 1 评审钉死②）——优先显式
        // propsFile，缺省回退 file；都空 → 失败（树条目必须可达 props 文件）。
        if (item.propsFile.empty())
            item.propsFile = item.file;
        if (item.propsFile.empty())
            return false;
        return treeIdOk;
    }

    bool readTiles(DumpManifest& out)
    {
        m_p.skipWs();
        if (peek() != '[') {
            m_p.failed = true;
            return false;
        }
        m_p.pos++;
        m_p.skipWs();
        if (peek() == ']') {
            m_p.pos++;
            return true;
        }
        while (true) {
            DumpManifestTileEntry item;
            if (!tileEntry(item))
                return false;
            out.tiles.push_back(std::move(item));
            m_p.skipWs();
            if (peek() == ',') {
                m_p.pos++;
                continue;
            }
            if (peek() == ']') {
                m_p.pos++;
                return true;
            }
            m_p.failed = true;
            return false;
        }
    }

    bool tileEntry(DumpManifestTileEntry& item)
    {
        if (!m_p.consume('{'))
            return false;
        bool seenTreeId = false;
        bool seenContentId = false;
        bool seenGuid = false;
        bool seenIModelId = false;
        bool seenChangesetId = false;
        bool seenByteLength = false;
        bool seenSha256 = false;
        bool seenFile = false;
        bool treeIdOk = false;
        bool contentIdOk = false;
        if (!objectBody([&](std::string const& key) -> bool {
                if (key == "treeId" && !seenTreeId) {
                    seenTreeId = true;
                    m_p.skipWs();
                    if (peek() != '"') {
                        if (!skipValue())
                            return false;
                        return false;
                    }
                    item.treeId = m_p.parseString();
                    treeIdOk = !m_p.failed;
                    return treeIdOk;
                }
                if (key == "contentId" && !seenContentId) {
                    seenContentId = true;
                    // 前端请求键原样（Task 1 评审钉死①）。
                    m_p.skipWs();
                    if (peek() != '"') {
                        if (!skipValue())
                            return false;
                        return false;
                    }
                    item.contentId = m_p.parseString();
                    contentIdOk = !m_p.failed;
                    return contentIdOk;
                }
                if (key == "guid" && !seenGuid) {
                    seenGuid = true;
                    return looseString(item.guid);
                }
                if (key == "iModelId" && !seenIModelId) {
                    seenIModelId = true;
                    return looseString(item.iModelId);
                }
                if (key == "changesetId" && !seenChangesetId) {
                    seenChangesetId = true;
                    // 快照 iModel 为空串——非异常（Task 1 报告③）。
                    return looseString(item.changesetId);
                }
                if (key == "byteLength" && !seenByteLength) {
                    seenByteLength = true;
                    double v = 0.0;
                    if (!looseNumber(v))
                        return false;
                    item.byteLength = static_cast<uint64_t>(v);
                    return true;
                }
                if (key == "sha256" && !seenSha256) {
                    seenSha256 = true;
                    return looseString(item.sha256);
                }
                if (key == "file" && !seenFile) {
                    seenFile = true;
                    return looseString(item.file);
                }
                return skipValue();
            }))
            return false;

        if (item.file.empty())
            return false;
        return treeIdOk && contentIdOk;
    }

    // stats 门域（DOM：非对象 stats 的 find 落空 → 门静默跳过；对象内
    // trees/tiles 键首键优先，其余键丢弃）。
    bool statsValue(double& statTrees, bool& hasStatTrees, double& statTiles,
                    bool& hasStatTiles)
    {
        m_p.skipWs();
        if (peek() != '{')
            return skipValue();
        m_p.pos++;
        bool seenTrees = false;
        bool seenTiles = false;
        return objectBody([&](std::string const& key) -> bool {
            if (key == "trees" && !seenTrees) {
                seenTrees = true;
                hasStatTrees = true;
                return looseNumber(statTrees);
            }
            if (key == "tiles" && !seenTiles) {
                seenTiles = true;
                hasStatTiles = true;
                return looseNumber(statTiles);
            }
            return skipValue();
        });
    }

    // provenance.iModel（M-E Task 2 语义——provenance 非对象/iModel 非对象
    // → 不置值；name 仅字符串；extents 坏形态忽略该字段）。
    bool provenanceValue(std::optional<DumpIModelInfo>& out)
    {
        m_p.skipWs();
        if (peek() != '{')
            return skipValue();
        m_p.pos++;
        bool seenIModel = false;
        return objectBody([&](std::string const& key) -> bool {
            if (key == "iModel" && !seenIModel) {
                seenIModel = true;
                return iModelValue(out);
            }
            return skipValue();
        });
    }

    bool iModelValue(std::optional<DumpIModelInfo>& out)
    {
        m_p.skipWs();
        if (peek() != '{')
            return skipValue();
        m_p.pos++;
        DumpIModelInfo info;
        bool seenName = false;
        bool seenExtents = false;
        if (!objectBody([&](std::string const& key) -> bool {
                if (key == "name" && !seenName) {
                    seenName = true;
                    // DOM：仅字符串 name 消费（非字符串忽略不置值）。
                    m_p.skipWs();
                    if (peek() != '"')
                        return skipValue();
                    info.name = m_p.parseString();
                    return !m_p.failed;
                }
                if (key == "extents" && !seenExtents) {
                    seenExtents = true;
                    range3dValue(info.extents);  // 失败忽略（不置值——DOM 同）
                    return !m_p.failed;
                }
                return skipValue();
            }))
            return false;
        out = std::move(info);
        return true;
    }

    // Range3dProps {low:[x,y,z], high:[x,y,z]}（与 parseRange3d 同条件：
    // 对象 + low/high 数组 ≥3 轴；失败不置值——返回 false 不置 failed）。
    bool range3dValue(dqGeom::Range3d& out)
    {
        m_p.skipWs();
        if (peek() != '{') {
            if (!skipValue())
                return false;
            return false;
        }
        m_p.pos++;
        double low[3] = {0.0, 0.0, 0.0};
        double high[3] = {0.0, 0.0, 0.0};
        bool lowOk = false;
        bool highOk = false;
        bool seenLow = false;
        bool seenHigh = false;
        if (!objectBody([&](std::string const& key) -> bool {
                if (key == "low" && !seenLow) {
                    seenLow = true;
                    lowOk = axis3Value(low);
                    return !m_p.failed;
                }
                if (key == "high" && !seenHigh) {
                    seenHigh = true;
                    highOk = axis3Value(high);
                    return !m_p.failed;
                }
                return skipValue();
            }))
            return false;
        if (!lowOk || !highOk)
            return false;
        out = dqGeom::Range3d(dqGeom::Point3d::From(low[0], low[1], low[2]),
                              dqGeom::Point3d::From(high[0], high[1], high[2]));
        return true;
    }

    // [x,y,z] 轴数组（DOM：数组且 size >= 3，元素取 .number——非数值元素
    // 为 0；形态坏 → false 不置 failed）。
    bool axis3Value(double xyz[3])
    {
        m_p.skipWs();
        if (peek() != '[') {
            if (!skipValue())
                return false;
            return false;
        }
        m_p.pos++;
        size_t n = 0;
        m_p.skipWs();
        if (peek() == ']') {
            m_p.pos++;
            return false;  // 空数组——< 3 轴（形态坏）
        }
        while (true) {
            double v = 0.0;
            if (!looseNumber(v))
                return false;
            if (n < 3)
                xyz[n] = v;
            ++n;
            m_p.skipWs();
            if (peek() == ',') {
                m_p.pos++;
                continue;
            }
            if (peek() == ']') {
                m_p.pos++;
                break;
            }
            m_p.failed = true;
            return false;
        }
        return n >= 3;
    }
};

}  // namespace

std::optional<DumpManifest> loadDumpManifest(std::string const& dumpRoot)
{
    std::string text;
    if (!readManifestText(joinPath(dumpRoot, "manifest.json"), text))
        return std::nullopt;
    DumpManifest manifest;
    if (!ManifestStreamReader(text).run(manifest, nullptr))
        return std::nullopt;
    return manifest;
}

// ---------------------------------------------------------------------------
// DumpTileTreeProps
// ---------------------------------------------------------------------------

std::optional<DumpTileTreeProps> DumpTileTreeProps::load(std::string const& dumpRoot)
{
    std::string text;
    if (!readManifestText(joinPath(dumpRoot, "manifest.json"), text))
        return std::nullopt;
    DumpTileTreeProps props;
    if (!ManifestStreamReader(text).run(props.m_manifest, &props.m_iModelInfo))
        return std::nullopt;
    props.m_dumpRoot = dumpRoot;
    return props;
}

DumpManifest DumpTileTreeProps::takeManifest()
{
    // 瓦键域移动（大头——大 dump 的解析成本主体）；树条目复制保留
    //（byTreeId 的 propsFile 查找面——每模型 1~2 条短串，成本可忽略）。
    DumpManifest out;
    out.trees = m_manifest.trees;
    out.tiles = std::move(m_manifest.tiles);  // 移出即清空源（vector 转移缓冲）
    return out;
}

std::optional<DumpTreeProps> DumpTileTreeProps::byTreeId(std::string const& treeId) const
{
    // manifest trees[].propsFile → IModelTileTreeProps JSON。
    DumpManifestTreeEntry const* entry = nullptr;
    for (auto const& item : m_manifest.trees) {
        if (item.treeId == treeId) {
            entry = &item;
            break;
        }
    }
    if (!entry)
        return std::nullopt;  // NotFound 语义

    bool ok = false;
    std::vector<uint8_t> const bytes =
        readFileBytes(joinPath(m_dumpRoot, entry->propsFile), &ok);
    if (!ok)
        return std::nullopt;
    auto doc = dumpjson::parseJsonDocument(std::string(bytes.begin(), bytes.end()));
    if (!doc || doc->type != dumpjson::JsonValue::Type::Object)
        return std::nullopt;

    DumpTreeProps out;

    // TileTreeProps.id（:43）——原文保留，不强校验（参考 requestTileTreeProps
    // 信任后端返回；dump 是采集 artifact）。
    if (dumpjson::JsonValue const* v = doc->find("id"))
        out.id = v->str;

    // IModelTileTreeProps.tileScreenSize（TileProps.ts:67）→ ?? 512
    // （iModelTileTreeParamsFromJSON IModelTileTree.ts:52；TileProps.ts:66 的
    // 缺省注释）。ImdlTreeMetadata 的成员缺省已是 512。
    if (dumpjson::JsonValue const* v = doc->find("tileScreenSize"))
        out.metadata.tileScreenSize = static_cast<uint32_t>(v->number);

    // IModelTileTreeProps.formatVersion（TileProps.ts:65）→ metadata 载体
    //（IModelTileTree.ts:396 消费——ContentIdProvider 的方案选择，M-D(3) 起
    // 接线；缺失保留 0 = DanQing legacy V1 id 路径，登记见 ImdlTileTree.h）。
    if (dumpjson::JsonValue const* v = doc->find("formatVersion"))
        out.metadata.formatVersion = static_cast<uint32_t>(v->number);

    // IModelTileTreeProps.maxInitialTilesToSkip（TileProps.ts:63）→ metadata
    // 载体（iModelTileTreeParamsFromJSON 的 destructure+params 透传
    // IModelTileTree.ts:51/:76 → 构造器 :390 消费——SelectParent 协议的初始
    // 跳级预算；缺失保留 0 = ?? 0 缺省。M-G(2) 接线：drill dump 无根瓦字节
    // 是采集实态，无此载体时根 NotFound 阻断整树下潜——RED 取证见
    // RpcDumpRender.Instances60DrillReplaysViewportChain 锁头）。
    if (dumpjson::JsonValue const* v = doc->find("maxInitialTilesToSkip"))
        out.metadata.maxInitialTilesToSkip = static_cast<uint32_t>(v->number);

    // TileTreeProps.location（TileProps.ts:46-47——"Transform tile coordinates
    // to iModel world coordinates"）→ DumpTreeProps.location（M-H Task 3 消费
    // ——装载侧 setIModelTransform；iModelTileTreeParamsFromJSON 的
    // Transform.fromJSON(props.location) IModelTileTree.ts:50/:71 →
    // TileTree.iModelTransform TileTree.ts:122）。缺失 → hasLocation=false
    // （恒等缺省——Transform.setFromJSON(undefined) Transform.ts:104-105）；
    // 字段在但畸形 → 装载失败（dump 自洽性破口）。
    if (dumpjson::JsonValue const* v = doc->find("location")) {
        if (v->type != dumpjson::JsonValue::Type::Null) {
            if (!parseTransform3x4(*v, out.location))
                return std::nullopt;
            out.hasLocation = true;
        }
    }

    // TileTreeProps.contentRange（:51）→ 仅在字段存在且为对象时置值
    // （IModelTileTree.ts:54-56；缺失/null 保留 null range——"unknown" 约定；
    // null 视同缺省不硬失败——参考 Range3d.setFromJSON 的 `if (!json) return`
    // 对 null 无操作，Range.ts:189-191）。
    if (dumpjson::JsonValue const* v = doc->find("contentRange")) {
        if (v->type == dumpjson::JsonValue::Type::Object
            && !parseRange3d(*v, out.metadata.contentRange))
            return std::nullopt;
    }

    // TileProps.rootTile（:45）——必需（:69 透传给 RootTile 构造）。
    dumpjson::JsonValue const* rootTile = doc->find("rootTile");
    if (!rootTile || rootTile->type != dumpjson::JsonValue::Type::Object)
        return std::nullopt;

    // TileProps.contentId（:25）——原文（"0/0/0/0/1" 形态；注意这不是 manifest
    // 的回放瓦键——参考侧 IModelTileTree 构造会用 contentIdProvider.rootContentId
    // 覆写它，IModelTileTree.ts:398）。
    if (dumpjson::JsonValue const* v = rootTile->find("contentId"))
        out.rootTile.contentId = v->str;

    // TileProps.range（:27）——必需。
    if (dumpjson::JsonValue const* v = rootTile->find("range")) {
        if (!parseRange3d(*v, out.rootTile.range))
            return std::nullopt;
    } else {
        return std::nullopt;
    }

    // TileProps.contentRange（:29，optional）——缺失/null → null（同上：null
    // 视同缺省，setFromJSON 对 null 无操作）。
    if (dumpjson::JsonValue const* v = rootTile->find("contentRange")) {
        if (v->type == dumpjson::JsonValue::Type::Object
            && !parseRange3d(*v, out.rootTile.contentRange))
            return std::nullopt;
    }

    // TileProps.maximumSize（:31）→ rootMaximumSize 载体（ImdlTileMetadata 无
    // 此字段；ImdlTile 构造的 maximumSize 实参）。
    if (dumpjson::JsonValue const* v = rootTile->find("maximumSize"))
        out.rootMaximumSize = v->number;

    // TileProps.isLeaf（:35，optional "Defaults to false"）。
    if (dumpjson::JsonValue const* v = rootTile->find("isLeaf"))
        out.rootTile.isLeaf = v->boolean;

    // TileProps.sizeMultiplier（:33，optional）——缺失 → 0 = 未设（DanQing
    // ImdlTileMetadata 约定：0 关断 magnification 分支）。
    if (dumpjson::JsonValue const* v = rootTile->find("sizeMultiplier"))
        out.rootTile.sizeMultiplier = v->number;

    // emptySubRangeMask——非 TileProps.ts 字段（参考经 V2 contentId 的 bisect
    // 编码携带；DanQing ImdlTileMetadata 有显式载体故接受 dump 直给；缺失 → 0
    // = 无空子域）。
    if (dumpjson::JsonValue const* v = rootTile->find("emptySubRangeMask"))
        out.rootTile.emptySubRangeMask = static_cast<uint32_t>(v->number);

    return out;
}

END_DQ_APP_NAMESPACE
