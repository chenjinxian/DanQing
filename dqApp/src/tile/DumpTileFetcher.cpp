// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DumpTileFetcher 实现（本地 RPC-dump 回放，零网络 §8.2）
// Authored: see DumpTileFetcher.h（宿主侧取数缝——参考无对应物）。
#include "dqApp/tile/DumpTileFetcher.h"

#include <algorithm>
#include <fstream>
#include <iterator>

BEGIN_DQ_APP_NAMESPACE

namespace {

std::vector<std::string> splitNonEmpty(std::string const& text, char sep)
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        size_t const slash = text.find(sep, start);
        std::string const part = slash == std::string::npos
            ? text.substr(start)
            : text.substr(start, slash - start);
        if (!part.empty())
            parts.push_back(std::move(part));
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return parts;
}

bool readFileBytes(std::string const& path, std::vector<uint8_t>& out)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open())
        return false;
    auto const size = in.tellg();
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (size > 0
        && !in.read(reinterpret_cast<char*>(out.data()), size)) {
        out.clear();
        return false;
    }
    return true;
}

}  // namespace

DumpTileFetcher::DumpTileFetcher(std::string const& primaryRoot,
                                 std::vector<std::string> fallbackRoots)
    : m_dumpRoot(primaryRoot)
    , m_fallbackRoots(std::move(fallbackRoots))
{
    // manifest 装载（唯一键域——trees/tiles 索引；stats 计数门在 loader 内）。
    // 主根失败 → isValid()==false（现状语义不变）。
    if (auto manifest = loadDumpManifest(primaryRoot)) {
        m_manifest = std::move(*manifest);
        m_valid = true;
    }
    // fallback 根（M-H Task 2 多根合并）：同一解析路径逐根装载；失败记
    // warning 不致命（主根语义），不进查找域。
    for (auto const& root : m_fallbackRoots) {
        if (auto manifest = loadDumpManifest(root)) {
            m_fallbacks.push_back(FallbackRoot{root, std::move(*manifest)});
        } else {
            m_fallbackWarnings.push_back(root);
        }
    }
}

void DumpTileFetcher::fetch(std::string const& url, dqRender::Tile& tile,
                            std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
                            std::function<void(dqRender::Tile&, std::string const&)> onError)
{
    // 轮询契约（FileTileFetcher 先例）：读盘/查表同步做，投递只在
    // processCompleted()——从不在 fetch() 内联（TileAdmin process 周期语义）。
    Completed entry;
    entry.tile = &tile;
    entry.onComplete = std::move(onComplete);
    entry.onError = std::move(onError);

    // 请求轨迹（对账锁消费面）：每个 fetch() 调用一条，结果归类随分支落定。
    DumpRequestRecord record;
    record.outcome = DumpFetchOutcome::Error;

    if (!m_valid) {
        entry.ok = false;
        entry.error = "DumpTileFetcher: no manifest loaded (dump root: " + m_dumpRoot + ")";
        m_requestLog.push_back(std::move(record));
        m_completed.push_back(std::move(entry));
        return;
    }

    // url → (treeId, contentId)：末两段。ImdlTileTree::contentUrl 组合
    // "<treeId>/<contentId>"；brief 的 "<dumpRoot>/<treeId>/<contentId>" 形态
    // 同解析——前缀只是定位器，键域只有 manifest。
    std::vector<std::string> const parts = splitNonEmpty(url, '/');
    if (parts.size() < 2) {
        entry.ok = false;
        entry.error = "DumpTileFetcher: url does not carry <treeId>/<contentId>: " + url;
        record.contentId = url;  // 原样留痕（无法拆键的畸形 url）
        m_requestLog.push_back(std::move(record));
        m_completed.push_back(std::move(entry));
        return;
    }
    std::string const& treeId = parts[parts.size() - 2];
    std::string const& contentId = parts[parts.size() - 1];
    record.treeId = treeId;
    record.contentId = contentId;

    // manifest 查表（线性——dump 规模为十数量级条目）。多根合并查找序：
    // 主根 → fallback[0] → fallback[1]…，首命中即服务（字节读自命中根目录，
    // 命中源记 record.hitRoot——0=主根，i+1=fallbackRoots()[i]）。
    DumpManifestTileEntry const* tileEntry = nullptr;
    std::string const* hitFileRoot = nullptr;
    auto lookup = [&treeId, &contentId, &tileEntry, &hitFileRoot](
                      std::string const& root, DumpManifest const& manifest) {
        for (auto const& item : manifest.tiles) {
            if (item.treeId == treeId && item.contentId == contentId) {
                tileEntry = &item;
                hitFileRoot = &root;
                return true;
            }
        }
        return false;
    };
    if (lookup(m_dumpRoot, m_manifest)) {
        record.hitRoot = 0;
    } else {
        for (size_t i = 0; i < m_fallbacks.size(); ++i) {
            if (lookup(m_fallbacks[i].root, m_fallbacks[i].manifest)) {
                record.hitRoot = i + 1;
                break;
            }
        }
    }
    if (!tileEntry) {
        // NotFound 语义（前端请求键未采集进 dump——上层瓦片按请求失败处置）。
        record.outcome = DumpFetchOutcome::NotFound;
        entry.ok = false;
        entry.error = "DumpTileFetcher: content not found in dump manifest: "
                      + treeId + "/" + contentId;
        m_requestLog.push_back(std::move(record));
        m_completed.push_back(std::move(entry));
        return;
    }

    std::vector<uint8_t> bytes;
    if (!readFileBytes(*hitFileRoot + "/" + tileEntry->file, bytes)) {
        entry.ok = false;
        entry.error = "DumpTileFetcher: cannot open " + tileEntry->file
                      + " (dump root: " + *hitFileRoot + ")";
    } else if (bytes.size() != tileEntry->byteLength) {
        // 完整性下界（brief Step 3 决策点登记）：manifest.sha256 由采集侧
        // collector 已算（danqing-rpc-tools，Task 1 入库前全量复算一致）；
        // dqBase 仅有 imodel-native 移植的 SHA1/MD5、无 SHA256（自实现属 §7
        // 自创算法——两参考仓均无对应物），故回放侧完整性 = byteLength 精确
        // 一致 + 文件存在性；字节同一性由测试的磁盘直读逐字节比对钉死。
        entry.ok = false;
        entry.error = "DumpTileFetcher: byteLength mismatch for " + treeId + "/"
                      + contentId + " (manifest " + std::to_string(tileEntry->byteLength)
                      + ", disk " + std::to_string(bytes.size()) + ")";
    } else {
        record.outcome = DumpFetchOutcome::Completed;
        record.bytes = static_cast<uint64_t>(bytes.size());
        entry.ok = true;
        entry.data = std::move(bytes);
    }

    m_requestLog.push_back(std::move(record));
    m_completed.push_back(std::move(entry));
}

void DumpTileFetcher::processCompleted()
{
    // 从搬空的列表投递：回调可能重入 fetch()（FileTileFetcher 同款防重入）。
    auto pending = std::move(m_completed);
    m_completed.clear();
    for (auto& entry : pending) {
        if (!entry.tile)
            continue;
        if (entry.ok)
            entry.onComplete(*entry.tile, entry.data);
        else
            entry.onError(*entry.tile, entry.error);
    }
}

uint32_t DumpTileFetcher::getActiveCount() const noexcept
{
    return static_cast<uint32_t>(m_completed.size());
}

void DumpTileFetcher::cancelAll()
{
    m_completed.clear();
}

END_DQ_APP_NAMESPACE
