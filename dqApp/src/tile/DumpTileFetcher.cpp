// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DumpTileFetcher 实现（本地 RPC-dump 回放，零网络 §8.2）
// Authored: see DumpTileFetcher.h（宿主侧取数缝——参考无对应物）。
#include "dqApp/tile/DumpTileFetcher.h"

#include <dqRender/WorkerProxy.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

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

// 瓦键 = treeId '\0' contentId（两段为 manifest 契约字符串，均不含 '\0'
// ——'\0' 分隔使拼接键无歧义）。
std::string tileKey(std::string const& treeId, std::string const& contentId)
{
    std::string key;
    key.reserve(treeId.size() + 1 + contentId.size());
    key.append(treeId);
    key.push_back('\0');
    key.append(contentId);
    return key;
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
    loadFallbacks();
    buildIndexes();
}

DumpTileFetcher::DumpTileFetcher(DumpManifest&& primaryManifest,
                                 std::string const& primaryRoot,
                                 std::vector<std::string> fallbackRoots)
    : m_dumpRoot(primaryRoot)
    , m_manifest(std::move(primaryManifest))
    // 主根有效性由装载侧保证（DumpTileTreeProps 仅经 load 构造——.h 前置
    // 条件）；装载门语义在 load 侧不变。
    , m_valid(true)
    , m_fallbackRoots(std::move(fallbackRoots))
{
    loadFallbacks();
    buildIndexes();
}

void DumpTileFetcher::loadFallbacks()
{
    // fallback 根（M-H Task 2 多根合并）：同一解析路径逐根装载；失败记
    // warning 不致命（主根语义），不进查找域。
    for (auto const& root : m_fallbackRoots) {
        if (auto manifest = loadDumpManifest(root)) {
            m_fallbacks.push_back(FallbackRoot{root, std::move(*manifest), {}});
        } else {
            m_fallbackWarnings.push_back(root);
        }
    }
}

void DumpTileFetcher::buildIndexes()
{
    // 每根一份索引（查找按根序逐根查——hitRoot 首命中语义不变）。建立点 =
    // 各 manifest 已定型（m_manifest 成员 + m_fallbacks 向量此后不再增长，
    // FallbackRoot 移动为 vector 缓冲转移——tiles 条目地址稳定）。
    m_primaryIndex.reserve(m_manifest.tiles.size());
    for (auto const& item : m_manifest.tiles)
        m_primaryIndex.emplace(tileKey(item.treeId, item.contentId), &item);

    for (auto& fallback : m_fallbacks) {
        fallback.index.reserve(fallback.manifest.tiles.size());
        for (auto const& item : fallback.manifest.tiles)
            fallback.index.emplace(tileKey(item.treeId, item.contentId), &item);
    }
}

// M-O(2) I11：运行期追加根——索引只为新根建立（既有根的 tiles 条目地址在
// m_fallbacks 向量增长（缓冲转移）下稳定——DumpManifest 的 tiles 堆数组
// 不随对象移动，见 buildIndexes 注释）。
void DumpTileFetcher::addFallbackRoot(DumpManifest&& manifest,
                                      std::string const& root)
{
    m_fallbackRoots.push_back(root);
    m_fallbacks.push_back(FallbackRoot{root, std::move(manifest), {}});
    auto& fallback = m_fallbacks.back();
    fallback.index.reserve(fallback.manifest.tiles.size());
    for (auto const& item : fallback.manifest.tiles)
        fallback.index.emplace(tileKey(item.treeId, item.contentId), &item);
}

void DumpTileFetcher::addFallbackRoot(std::string const& root)
{
    m_fallbackRoots.push_back(root);
    if (auto manifest = loadDumpManifest(root)) {
        m_fallbacks.push_back(FallbackRoot{root, std::move(*manifest), {}});
        auto& fallback = m_fallbacks.back();
        fallback.index.reserve(fallback.manifest.tiles.size());
        for (auto const& item : fallback.manifest.tiles)
            fallback.index.emplace(tileKey(item.treeId, item.contentId), &item);
    } else {
        m_fallbackWarnings.push_back(root);
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

    // manifest 查表（哈希索引——M-I(1)；大 dump 线性扫 1.8ms/键 → O(1)）。
    // 多根合并查找序：主根 → fallback[0] → fallback[1]…；**源的可用性门** =
    // 文件存在且大小 == manifest byteLength（metadata 检查，微秒级；读盘仍
    // 异步）——首命中文件缺失时继续下一根（2026-10-20 bridge-edit 取证：
    // `-b-2-0-0-0-1` 在主根/drill 的 manifest 有键但 134MB 瓦文件本地未
    // 持有，首命中即报错会吞掉 sweep 根的有效副本 → 该 LOD 层整片缺失）。
    // EQUIVALENCE（多根为宿主回放缝，参考单后端）：验证法 =
    // DumpTileFetcherTest.MissingPrimaryFileFallsBackToNextRoot。
    // 命中源记 record.hitRoot——0=主根，i+1=fallbackRoots()[i]。
    struct Candidate {
        DumpManifestTileEntry const* entry;
        std::string const* root;
        size_t hitRoot;
    };
    std::vector<Candidate> candidates;
    std::string const key = tileKey(treeId, contentId);
    if (auto const it = m_primaryIndex.find(key); it != m_primaryIndex.end())
        candidates.push_back({it->second, &m_dumpRoot, 0});
    for (size_t i = 0; i < m_fallbacks.size(); ++i) {
        if (auto const fb = m_fallbacks[i].index.find(key);
            fb != m_fallbacks[i].index.end())
            candidates.push_back({fb->second, &m_fallbacks[i].root, i + 1});
    }
    if (candidates.empty()) {
        // NotFound 语义（前端请求键未采集进 dump——上层瓦片按请求失败处置）。
        record.outcome = DumpFetchOutcome::NotFound;
        entry.ok = false;
        entry.error = "DumpTileFetcher: content not found in dump manifest: "
                      + treeId + "/" + contentId;
        if (std::getenv("DANQING_TILE_TIME")) {
            std::fprintf(stderr, "[TT] fetchMiss %s/%s\n",
                         treeId.c_str(), contentId.c_str());
            std::fflush(stderr);
        }
        m_requestLog.push_back(std::move(record));
        m_completed.push_back(std::move(entry));
        return;
    }
    Candidate const* chosen = nullptr;
    for (auto const& candidate : candidates) {
        std::string const candidatePath =
            *candidate.root + "/" + candidate.entry->file;
        std::error_code ec;
        auto const size = std::filesystem::file_size(candidatePath, ec);
        if (!ec && size == candidate.entry->byteLength) {
            chosen = &candidate;
            break;
        }
    }
    if (chosen == nullptr) {
        // 全候选源的字节均不可用（文件缺失/大小不符）——Error（dump 资产
        // 自洽性破口），hitRoot 记首候选保持轨迹可读。
        record.outcome = DumpFetchOutcome::Error;
        record.hitRoot = candidates.front().hitRoot;
        entry.ok = false;
        entry.error = "DumpTileFetcher: no readable source for " + treeId + "/"
                      + contentId + " (" + std::to_string(candidates.size())
                      + " roots declare the key; none has the exact byteLength)";
        if (std::getenv("DANQING_TILE_TIME")) {
            std::fprintf(stderr, "[TT] fetchError %s/%s (%zu roots, none readable)\n",
                         treeId.c_str(), contentId.c_str(), candidates.size());
            std::fflush(stderr);
        }
        m_requestLog.push_back(std::move(record));
        m_completed.push_back(std::move(entry));
        return;
    }

    record.hitRoot = chosen->hitRoot;
    std::string const filePath = *chosen->root + "/" + chosen->entry->file;
    size_t const expectedBytes = chosen->entry->byteLength;

    // -------------------------------------------------------------------
    // 读盘下放（2026-10-20 bridge-edit 卡顿取证续：fetch 读盘 30-94ms/大瓦
    // 在 UI 线程卡死交互）。
    // EQUIVALENCE（§11.10）：参考源 = ImdlReader 的取数经宿主异步（XHR/
    // HTTP——浏览器侧 IO 从不阻塞主线程，core/frontend/src/tile/ImdlReader.ts
    // :44-70 的 arrayBuffer 异步读 + TileAdmin 的请求完成回调）；发散 = DanQing
    // 本地文件回放无网络层 (§8.2)，等价面 = WorkerProxy（P-b1 载体）后台
    // 读盘；完成项在 UI 线程的 WorkerProxy drain（TileAdmin::process 首段）入
    // m_completed，仍由 processCompleted() 投递；验证法 =
    // DumpTileFetcherTest.DeliveryDeferredUntilDrainAndProcessCompleted（fetch
    // 内零回调 + 字节同值）+ CancelAllDropsUndeliveredCompletion（代际）。
    // 轮询交付契约与帧序不变量（M-F(1)：交付不早于下一个选择趟）不变。
    // 生命周期：任务体只持 slot+路径（不触 fetcher）；完成闭包以 weak 令牌
    // 防 fetcher 先亡（fetcher 替换/teardown——setFetcher 删除旧实例）。
    // 取消：cancelAll() 递增代际，旧代完成的项丢弃（读盘本身不可中断——
    // 文件 IO 无副作用）。
    // noWorker（DANQING_NO_WORKER=1）→ 同步旧路（下方 readFileBytes 段）。
    // -------------------------------------------------------------------
    // 载体已终止（仅 teardown 边界可能——生产无人调用 singleton terminate）
    // 时走同步径：submit 对终止载体返回 0 并丢弃闭包（请求会悬挂），先门控。
    if (!dqRender::WorkerProxy::instance().isNoWorker()
        && !dqRender::WorkerProxy::instance().isTerminated() && m_lifeToken) {
        struct AsyncReadSlot {
            bool readOk = false;
            std::vector<uint8_t> bytes;
            double readMs = 0.0;
        };
        auto slot = std::make_shared<AsyncReadSlot>();
        std::weak_ptr<char> life = m_lifeToken;
        uint64_t const gen = m_ioGeneration;
        DumpTileFetcher* self = this;
        DumpRequestRecord rec = std::move(record);
        auto onC = std::move(entry.onComplete);
        auto onE = std::move(entry.onError);
        dqRender::Tile* tilePtr = &tile;
        std::string const treeIdCopy = treeId;
        std::string const contentIdCopy = contentId;
        m_inFlightReads.fetch_add(1);
        dqRender::WorkerProxy::instance().submit(
            [slot, filePath]() {
                auto const t0 = std::chrono::steady_clock::now();
                slot->readOk = readFileBytes(filePath, slot->bytes);
                slot->readMs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - t0).count() / 1000.0;
            },
            [self, life, gen, slot, rec = std::move(rec), onC = std::move(onC),
             onE = std::move(onE), tilePtr, expectedBytes, filePath,
             treeIdCopy, contentIdCopy]() mutable {
                if (!life.lock())
                    return;  // fetcher 已亡（setFetcher 替换/teardown）——丢弃
                self->m_inFlightReads.fetch_sub(1);
                if (gen != self->m_ioGeneration)
                    return;  // cancelAll 代际：旧代完成项丢弃
                Completed e;
                e.tile = tilePtr;
                e.onComplete = std::move(onC);
                e.onError = std::move(onE);
                if (std::getenv("DANQING_TILE_TIME")) {
                    std::fprintf(stderr, "[TT] fetchRead(worker) %s/%s %.2fms bytes=%llu hitRoot=%d\n",
                                 treeIdCopy.c_str(), contentIdCopy.c_str(), slot->readMs,
                                 static_cast<unsigned long long>(slot->bytes.size()),
                                 static_cast<int>(rec.hitRoot));
                    std::fflush(stderr);
                }
                if (!slot->readOk) {
                    e.ok = false;
                    e.error = "DumpTileFetcher: cannot open " + filePath;
                } else if (slot->bytes.size() != expectedBytes) {
                    e.ok = false;
                    e.error = "DumpTileFetcher: byteLength mismatch for " + treeIdCopy + "/"
                              + contentIdCopy + " (manifest "
                              + std::to_string(expectedBytes) + ", disk "
                              + std::to_string(slot->bytes.size()) + ")";
                } else {
                    rec.outcome = DumpFetchOutcome::Completed;
                    rec.bytes = static_cast<uint64_t>(slot->bytes.size());
                    e.ok = true;
                    e.data = std::move(slot->bytes);
                }
                if (!e.ok && std::getenv("DANQING_TILE_TIME")) {
                    std::fprintf(stderr, "[TT] fetchError %s/%s %s\n",
                                 treeIdCopy.c_str(), contentIdCopy.c_str(),
                                 e.error.c_str());
                    std::fflush(stderr);
                }
                self->m_requestLog.push_back(std::move(rec));
                self->m_completed.push_back(std::move(e));
            });
        return;
    }

    std::vector<uint8_t> bytes;
    // 诊断探针（§13.1 DANQING_TILE_TIME）：同步读盘耗时（noWorker 径——异步径
    // 的 worker 侧计时标记为 fetchRead(worker)）。
    auto const tRead0 = std::chrono::steady_clock::now();
    bool const readOk = readFileBytes(filePath, bytes);
    if (std::getenv("DANQING_TILE_TIME")) {
        auto const us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - tRead0).count();
        std::fprintf(stderr, "[TT] fetchRead %s/%s %.2fms bytes=%llu hitRoot=%d\n",
                     treeId.c_str(), contentId.c_str(), us / 1000.0,
                     static_cast<unsigned long long>(bytes.size()),
                     static_cast<int>(record.hitRoot));
        std::fflush(stderr);
    }
    if (!readOk) {
        entry.ok = false;
        entry.error = "DumpTileFetcher: cannot open " + chosen->entry->file
                      + " (dump root: " + *chosen->root + ")";
    } else if (bytes.size() != chosen->entry->byteLength) {
        // 完整性下界（brief Step 3 决策点登记）：manifest.sha256 由采集侧
        // collector 已算（danqing-rpc-tools，Task 1 入库前全量复算一致）；
        // dqBase 仅有 imodel-native 移植的 SHA1/MD5、无 SHA256（自实现属 §7
        // 自创算法——两参考仓均无对应物），故回放侧完整性 = byteLength 精确
        // 一致 + 文件存在性；字节同一性由测试的磁盘直读逐字节比对钉死。
        entry.ok = false;
        entry.error = "DumpTileFetcher: byteLength mismatch for " + treeId + "/"
                      + contentId + " (manifest " + std::to_string(chosen->entry->byteLength)
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
    // 口径 = 已就绪待投递 + 在途读盘（取消后旧代完成项被丢弃，在途计数在
    // 完成闭包处回落——排空尾部最多一个 drain 周期）。
    return static_cast<uint32_t>(m_completed.size()) + m_inFlightReads.load();
}

void DumpTileFetcher::cancelAll()
{
    // 代际递增：此后到达的旧代完成项丢弃（读盘不可中断——文件 IO 无副作用；
    // 完成闭包仍会执行并回落 in-flight 计数，仅在代际门处提前返回）。
    ++m_ioGeneration;
    m_completed.clear();
}

END_DQ_APP_NAMESPACE
