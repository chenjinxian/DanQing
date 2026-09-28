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
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        if (ok)
            *ok = false;
        return {};
    }
    if (ok)
        *ok = true;
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
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

bool parseManifest(std::string const& jsonText, DumpManifest& out,
                   std::optional<DumpIModelInfo>* iModelInfoOut)
{
    auto doc = dumpjson::parseJsonDocument(jsonText);
    if (!doc || doc->type != dumpjson::JsonValue::Type::Object)
        return false;

    dumpjson::JsonValue const* trees = doc->find("trees");
    dumpjson::JsonValue const* tiles = doc->find("tiles");
    if (!trees || !tiles || trees->type != dumpjson::JsonValue::Type::Array
        || tiles->type != dumpjson::JsonValue::Type::Array)
        return false;

    for (auto const& entry : trees->arr) {
        if (entry.type != dumpjson::JsonValue::Type::Object)
            return false;
        dumpjson::JsonValue const* treeId = entry.find("treeId");
        if (!treeId || treeId->type != dumpjson::JsonValue::Type::String)
            return false;
        DumpManifestTreeEntry item;
        item.treeId = treeId->str;
        if (auto const* v = entry.find("iModelId"))
            item.iModelId = v->str;
        if (auto const* v = entry.find("formatVersion"))
            item.formatVersion = static_cast<uint32_t>(v->number);
        if (auto const* v = entry.find("byteLength"))
            item.byteLength = static_cast<uint64_t>(v->number);
        if (auto const* v = entry.find("file"))
            item.file = v->str;
        // propsFile 与 file 是同值别名（Task 1 评审钉死②）——优先显式
        // propsFile，缺省回退 file。
        if (auto const* v = entry.find("propsFile"))
            item.propsFile = v->str;
        if (item.propsFile.empty())
            item.propsFile = item.file;
        if (item.propsFile.empty())
            return false;  // 树条目必须可达 props 文件
        out.trees.push_back(std::move(item));
    }

    for (auto const& entry : tiles->arr) {
        if (entry.type != dumpjson::JsonValue::Type::Object)
            return false;
        dumpjson::JsonValue const* treeId = entry.find("treeId");
        dumpjson::JsonValue const* contentId = entry.find("contentId");
        if (!treeId || !contentId || treeId->type != dumpjson::JsonValue::Type::String
            || contentId->type != dumpjson::JsonValue::Type::String)
            return false;
        DumpManifestTileEntry item;
        item.treeId = treeId->str;
        item.contentId = contentId->str;  // 前端请求键原样（评审钉死①）
        if (auto const* v = entry.find("guid"))
            item.guid = v->str;
        if (auto const* v = entry.find("iModelId"))
            item.iModelId = v->str;
        if (auto const* v = entry.find("changesetId"))
            item.changesetId = v->str;  // 快照 iModel 为空串——非异常（Task 1 报告③）
        if (auto const* v = entry.find("byteLength"))
            item.byteLength = static_cast<uint64_t>(v->number);
        if (auto const* v = entry.find("sha256"))
            item.sha256 = v->str;
        if (auto const* v = entry.find("file"))
            item.file = v->str;
        if (item.file.empty())
            return false;
        out.tiles.push_back(std::move(item));
    }

    // stats 计数门：数组计数与 stats 域一致（损坏 manifest 的廉价完整性探针；
    // stats 缺省容错——采集器总是写出，防御未来格式）。
    if (dumpjson::JsonValue const* stats = doc->find("stats")) {
        if (dumpjson::JsonValue const* n = stats->find("trees")) {
            if (static_cast<size_t>(n->number) != out.trees.size())
                return false;
        }
        if (dumpjson::JsonValue const* n = stats->find("tiles")) {
            if (static_cast<size_t>(n->number) != out.tiles.size())
                return false;
        }
    }

    // provenance.iModel → iModel 级元数据（M-E Task 2）。优雅语义：
    // provenance 缺失 / iModel 缺失 / iModel 非对象 → 不置值（nullopt——
    // 采集工具未写字段，非损坏）；iModel 为对象 → 消费其内可用的
    // name（字符串）与 extents（Range3dProps {low,high}——与 dump 内全部
    // 范围域同构）——extents 坏形态（非对象/low-high 缺轴）忽略该字段。
    if (iModelInfoOut) {
        if (dumpjson::JsonValue const* provenance = doc->find("provenance")) {
            if (provenance->type == dumpjson::JsonValue::Type::Object) {
                if (dumpjson::JsonValue const* imodel = provenance->find("iModel")) {
                    if (imodel->type == dumpjson::JsonValue::Type::Object) {
                        DumpIModelInfo info;
                        if (dumpjson::JsonValue const* name = imodel->find("name")) {
                            if (name->type == dumpjson::JsonValue::Type::String)
                                info.name = name->str;
                        }
                        if (dumpjson::JsonValue const* extents = imodel->find("extents"))
                            parseRange3d(*extents, info.extents);
                        *iModelInfoOut = std::move(info);
                    }
                }
            }
        }
    }
    return true;
}

}  // namespace

std::optional<DumpManifest> loadDumpManifest(std::string const& dumpRoot)
{
    bool ok = false;
    std::vector<uint8_t> const bytes =
        readFileBytes(joinPath(dumpRoot, "manifest.json"), &ok);
    if (!ok)
        return std::nullopt;
    DumpManifest manifest;
    if (!parseManifest(
            std::string(bytes.begin(), bytes.end()), manifest, nullptr))
        return std::nullopt;
    return manifest;
}

// ---------------------------------------------------------------------------
// DumpTileTreeProps
// ---------------------------------------------------------------------------

std::optional<DumpTileTreeProps> DumpTileTreeProps::load(std::string const& dumpRoot)
{
    bool ok = false;
    std::vector<uint8_t> const bytes =
        readFileBytes(joinPath(dumpRoot, "manifest.json"), &ok);
    if (!ok)
        return std::nullopt;
    DumpTileTreeProps props;
    if (!parseManifest(std::string(bytes.begin(), bytes.end()), props.m_manifest,
                       &props.m_iModelInfo))
        return std::nullopt;
    props.m_dumpRoot = dumpRoot;
    return props;
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
