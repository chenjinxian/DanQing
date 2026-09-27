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

DumpTileFetcher::DumpTileFetcher(std::string const& dumpRoot)
    : m_dumpRoot(dumpRoot)
{
    // manifest 装载（唯一键域——trees/tiles 索引；stats 计数门在 loader 内）。
    if (auto manifest = loadDumpManifest(dumpRoot)) {
        m_manifest = std::move(*manifest);
        m_valid = true;
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

    // manifest 查表（线性——dump 规模为十数量级条目）。
    DumpManifestTileEntry const* tileEntry = nullptr;
    for (auto const& item : m_manifest.tiles) {
        if (item.treeId == treeId && item.contentId == contentId) {
            tileEntry = &item;
            break;
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
    if (!readFileBytes(m_dumpRoot + "/" + tileEntry->file, bytes)) {
        entry.ok = false;
        entry.error = "DumpTileFetcher: cannot open " + tileEntry->file
                      + " (dump root: " + m_dumpRoot + ")";
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
