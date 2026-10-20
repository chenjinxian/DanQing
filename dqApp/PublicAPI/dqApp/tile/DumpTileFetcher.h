// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — 本地 RPC-dump 回放取数器（零网络——§8.2/TD-24 清退后的取数面）
//
// Authored: no reference equivalent exists — replaying captured RPC bytes is
// the host-side seam replacing itwinjs's TileAdmin RPC layer
// (generateTileContent / requestTileTreeProps, TileAdmin.ts:648-707) under the
// §8.2 zero-network protocol. The url contract follows ImdlTileTree::contentUrl
// 的既有组合约定 "<treeId>/<contentId>"（<dumpRoot>/ 前缀同解析——取末两段为
// 键，manifest 是唯一键域）；键 = manifest tiles[] 的 (treeId, contentId)
// 原样串（前端请求键，Task 1 评审钉死①）。
//
// 投递契约与 FileTileFetcher 同款轮询语义：投递只在 processCompleted()
// （从不在 fetch() 内联——TileAdmin process 周期语义）。查表（manifest 哈希
// 索引）在 fetch() 内同步完成（UI 线程，O(1)）；读盘**异步**——文件 IO 经
// WorkerProxy（dqRender，P-b1 载体）后台线程执行，完成项在该线程的
// drainCompleted（TileAdmin::process 首段，UI 线程）入队，仍在同一 process
// 周期的 processCompleted() 投递（帧序不变量 M-F(1) 不变）。同步径仅在
// DANQING_NO_WORKER=1（P-b3 单线程对照）时保留——语义同旧实现。
// 公共头依据 §8.4：DisplayTestApp 宿主跨模块注入（TileAdmin::setFetcher）。
#pragma once

#include "../Export.h"
#include "DumpTileTreeProps.h"  // DumpManifest（dump 元数据域共享）

#include <dqRender/tile/ITileFetcher.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

// DumpTileFetcher — manifest 回放的 ITileFetcher（零网络：只有文件 I/O，不
// include 任何网络头——结构零网络，DumpTileFetcherTest.ZeroNetworkByConstruction
// 全仓原语扫描锁）。
class DQ_APP_EXPORT DumpTileFetcher : public dqRender::ITileFetcher {
public:
    // 请求结果（fetch 侧一次性归类——投递前即定，processCompleted 不再改）：
    // Completed = manifest 命中且字节精确回放；NotFound = 键不在 manifest
    // （前端派生键与采集键域的差异——上层按请求失败处置）；
    // Error = 键命中但文件缺失/byteLength 不符（dump 资产自洽性破口）。
    enum class DumpFetchOutcome : uint8_t { Completed, NotFound, Error };

    // 一条请求轨迹（M-E Task 4 完整加载对账锁的消费面——"凡 manifest 有键的
    // 瓦全部消费"的键级清单：requested vs manifest 键集合逐一对账）。
    struct DumpRequestRecord {
        std::string treeId;
        std::string contentId;
        DumpFetchOutcome outcome;
        uint64_t bytes = 0;  // Completed 时的回放字节数（== manifest byteLength）
        // 命中源（M-H Task 2 多根合并）：0 = 主根，i+1 = fallbackRoots()[i]。
        // 仅在 manifest 命中（Completed/Error）时有意义；NotFound 恒 0（无命中）。
        size_t hitRoot = 0;
    };

    // 装载 <primaryRoot>/manifest.json；失败 → isValid()==false（fetch 走
    // onError，不崩——MissingDumpFailsLoadGracefully 锁）。
    //
    // 多根合并（M-H Task 2——instances60 sweep∪drill 联合域）：fallbackRoots
    // 为有序只读备根清单（默认空 = 单根现状零行为变化）。查找序 primary →
    // fallback[0] → fallback[1]…，**首个"文件存在且大小 == byteLength"的候选
    // 源即服务**（首命中文件缺失/大小不符时继续下一根——bridge-edit
    // `-b-2-0-0-0-1` 主根 134MB 瓦本地未持有、sweep 根有副本的实态；全候选
    // 不可用 → Error）。字节读自命中根目录，命中源记 requestLog 的 hitRoot
    // ——outcome 语义不变。树 props 仍仅取自主根
    // （DumpTileTreeProps 不动——同 treeId 语义单源；getTreeCount/
    // getTileCount 同为主根计数）。fallback 根 manifest 缺失/坏 → 记
    // fallbackWarnings() 不致命（主根语义：isValid 只看主根），不进查找域。
    explicit DumpTileFetcher(std::string const& primaryRoot,
                             std::vector<std::string> fallbackRoots = {});

    // 已解析 manifest 移入构造（M-I(1) 打开性能——单次解析共享）：manifest
    // 必须来自成功装载（DumpTileTreeProps::load → takeManifest——load 侧的
    // 缺失/坏 JSON/stats 完整性门语义不变，props 仅经 load 构造）；本构造
    // 不重读盘、不重解析，isValid() 恒 true（主根有效性由装载侧保证——
    // 打开链 props 装载失败时在 fetcher 构造前已返回）。fallback 根仍按
    // 路径逐根装载（语义与路径构造器同）。观测面（树/瓦计数、fallback
    // 清单、requestLog、交付字节）与路径构造器在同构型下逐项同值
    //（SharedManifestConstructorMatchesRootConstructor 对拍锁）。
    explicit DumpTileFetcher(DumpManifest&& primaryManifest,
                             std::string const& primaryRoot,
                             std::vector<std::string> fallbackRoots = {});

    bool isValid() const noexcept { return m_valid; }
    size_t getTreeCount() const noexcept { return m_manifest.trees.size(); }
    size_t getTileCount() const noexcept { return m_manifest.tiles.size(); }

    // 构造入参的 fallback 根清单（原序，含装载失败者）。
    std::vector<std::string> const& fallbackRoots() const noexcept
    {
        return m_fallbackRoots;
    }

    // manifest 装载失败的 fallback 根（warning 不致命——InvalidFallbackRoot
    // WarnsButPrimaryServes 锁）。
    std::vector<std::string> const& fallbackWarnings() const noexcept
    {
        return m_fallbackWarnings;
    }

    // M-O(2) I11：运行期追加 fallback 根（第二 iModel 叠加的取数扩围）。
    // EQUIVALENCE（§11.10，TiledGraphics.h 文件头同源登记）：参考源 =
    // BriefcaseConnection.openFile 后两连接各自后端取数；发散 = DanQing §8.2
    // 单 TileAdmin fetcher——第二包的瓦键经追加根进入同一查找域；验证法 =
    // TiledGraphicsTest 多根命中锁（两包键各自 hitRoot 命中）。
    // manifest 已解析形态优先（M-I(1) 单次解析共享——props 侧 takeManifest
    // 移交，不重读盘）。失败（路径形态装载失败）→ fallbackWarnings 不致命。
    void addFallbackRoot(DumpManifest&& manifest, std::string const& root);
    void addFallbackRoot(std::string const& root);

    // 全部被请求键的轨迹（每条至多一次；NotFound/Error 在 fetch() 内落定即
    // 追加，Completed 在后台读盘完成闭包落定后追加——列表序 = 结果落定序，
    // 非请求序；cancelAll() 丢弃未落定的旧代项。TileAdmin 对非 NotLoaded 态
    // 不重建请求（TileAdmin.cpp processRequestsForUser），故每键至多一条）。
    // 供完整加载对账测试只读遍历（消费面按键集合/逐键属性，不依赖列表序）。
    std::vector<DumpRequestRecord> const& requestLog() const noexcept
    {
        return m_requestLog;
    }

    // --- dqRender::ITileFetcher ---
    void fetch(std::string const& url, dqRender::Tile& tile,
               std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
               std::function<void(dqRender::Tile&, std::string const&)> onError) override;
    void processCompleted() override;
    uint32_t getActiveCount() const noexcept override;
    void cancelAll() override;

private:
    struct Completed {
        dqRender::Tile* tile = nullptr;
        bool ok = false;
        std::vector<uint8_t> data;
        std::string error;
        std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete;
        std::function<void(dqRender::Tile&, std::string const&)> onError;
    };

    std::string m_dumpRoot;
    DumpManifest m_manifest;
    bool m_valid = false;
    // 瓦键哈希索引（M-I(1)——大 dump 线性扫 1.8ms/键 → O(1)；键 =
    // treeId '\0' contentId，值 = manifest tiles[] 条目指针——manifest 在
    // 构造期定型后不再变更，索引在两构造器共有的 buildIndexes 尾段统一
    // 建立[此时各 manifest 已在最终位置——指针稳定]。emplace 首写不覆盖 =
    // 线性扫"首条目命中"同语义）。
    using TileKeyIndex = std::unordered_map<std::string, DumpManifestTileEntry const*>;
    TileKeyIndex m_primaryIndex;
    // 多根合并（M-H Task 2）：m_fallbackRoots 为构造入参原序（含失败者——
    // 观测面）；m_fallbacks 仅持装载成功的（根目录 + manifest + 索引——
    // 字节服务查找域）；m_fallbackWarnings 记装载失败根（warning 不致命）。
    struct FallbackRoot {
        std::string root;
        DumpManifest manifest;
        TileKeyIndex index;
    };
    std::vector<std::string> m_fallbackRoots;
    std::vector<FallbackRoot> m_fallbacks;
    std::vector<std::string> m_fallbackWarnings;
    std::vector<Completed> m_completed;
    std::vector<DumpRequestRecord> m_requestLog;

    // 异步读盘（2026-10-20 bridge-edit 卡顿取证续）：生命周期令牌（完成闭包
    // weak 持有——fetcher 被替换/销毁后旧完成项丢弃，防 UAF）；代际（
    // cancelAll 递增——旧代完成项丢弃）；在途读盘计数（getActiveCount 口径
    // = 已就绪待投递 + 在途）。
    std::shared_ptr<char> m_lifeToken = std::make_shared<char>(0);
    uint64_t m_ioGeneration = 0;
    std::atomic<uint32_t> m_inFlightReads{0};

    // 两构造器共有的尾段（M-I(1)）：fallback 逐根装载 + 全根索引建立
    //（索引必须在各 manifest 定型后建立——FallbackRoot 向量不再增长）。
    void loadFallbacks();
    void buildIndexes();
};

END_DQ_APP_NAMESPACE
